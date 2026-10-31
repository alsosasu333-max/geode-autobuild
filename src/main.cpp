#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <algorithm>
#include <vector>

using namespace geode::prelude;

// ======================= НАСТРОЙКИ =======================

// Скорости: 1x (обычная), потом 6x ... 20x
static const std::vector<int> SPEEDS = {1, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};

// Объекты, вокруг которых ставится декор (шипы и пилы)
static const std::vector<int> TARGETS = {8, 39, 103, 392, 88, 89, 98, 397, 398, 399};

// 10 наборов ID декора. Если какой-то вид не нравится, меняй числа здесь.
static const std::vector<std::vector<int>> DECOR_SETS = {
    {18, 19, 20}, {21, 22, 23}, {24, 25, 26}, {27, 28, 29}, {30, 31, 32},
    {33, 34, 35}, {36, 37, 38}, {41, 42, 43}, {44, 45, 46}, {47, 48, 49},
};

// 10 расстановок. 10 наборов x 10 расстановок = 100 стилей
static const std::vector<std::vector<CCPoint>> PATTERNS = {
    {ccp(-30, 0), ccp(30, 0)},
    {ccp(0, 30), ccp(0, -30)},
    {ccp(-30, 30), ccp(30, 30)},
    {ccp(-30, -30), ccp(30, -30)},
    {ccp(-60, 0), ccp(-30, 0), ccp(30, 0), ccp(60, 0)},
    {ccp(0, 30), ccp(0, 60)},
    {ccp(-30, 0), ccp(30, 0), ccp(0, 30)},
    {ccp(-30, 30), ccp(30, 30), ccp(-30, -30), ccp(30, -30)},
    {ccp(-45, 0), ccp(45, 0), ccp(0, 45), ccp(0, -45)},
    {ccp(0, 60), ccp(-30, 30), ccp(30, 30)},
};

static int styleCount() {
    return static_cast<int>(DECOR_SETS.size() * PATTERNS.size());
}

// ======================= СКОРОСТЬ В УРОВНЕ =======================

class $modify(SpeedPlayLayer, PlayLayer) {
    struct Fields {
        int idx = 0;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        CCDirector::get()->getScheduler()->setTimeScale(1.f);
        auto win = CCDirector::get()->getWinSize();

        auto label = CCLabelBMFont::create("x1", "goldFont.fnt");
        label->setScale(0.7f);
        label->setPosition({win.width - 45, win.height - 70});
        label->setTag(7701);
        label->setZOrder(100);
        this->addChild(label);

        auto spr = ButtonSprite::create("SPEED");
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(SpeedPlayLayer::onSpeed));
        auto menu = CCMenu::create();
        menu->addChild(btn);
        menu->setPosition({win.width - 45, win.height - 30});
        menu->setZOrder(100);
        this->addChild(menu);

        return true;
    }

    void onSpeed(CCObject*) {
        m_fields->idx = (m_fields->idx + 1) % static_cast<int>(SPEEDS.size());
        int n = SPEEDS[m_fields->idx];
        CCDirector::get()->getScheduler()->setTimeScale(static_cast<float>(n));

        if (auto label = typeinfo_cast<CCLabelBMFont*>(this->getChildByTag(7701))) {
            label->setString(fmt::format("x{}", n).c_str());
            float t = n <= 1 ? 0.f : (n - 6) / 14.f; // зелёный -> жёлтый
            label->setColor({static_cast<GLubyte>(255 * t), 255, 0});
        }
    }

    void onQuit() {
        CCDirector::get()->getScheduler()->setTimeScale(1.f);
        PlayLayer::onQuit();
    }
};

// ======================= АВТОДЕКОР В РЕДАКТОРЕ =======================
// Кнопка AUTO открывает панель. Выбираешь стиль стрелками: декор сразу
// появляется в уровне как предпросмотр. OK оставляет его, CANCEL убирает.

class $modify(AutoDecorUI, EditorUI) {
    struct Fields {
        int style = 0;
        CCMenu* panel = nullptr;
        std::vector<Ref<GameObject>> preview;
    };

    bool init(LevelEditorLayer* editor) {
        if (!EditorUI::init(editor)) return false;

        auto win = CCDirector::get()->getWinSize();

        // кнопка открытия
        auto openMenu = CCMenu::create();
        openMenu->setPosition({win.width - 60, win.height - 40});
        openMenu->setZOrder(100);
        auto openBtn = CCMenuItemSpriteExtra::create(ButtonSprite::create("AUTO"), this, menu_selector(AutoDecorUI::onOpen));
        openMenu->addChild(openBtn);
        this->addChild(openMenu);

        // панель выбора стиля (скрыта)
        auto panel = CCMenu::create();
        panel->setPosition({win.width - 110, win.height - 110});
        panel->setZOrder(100);
        panel->setVisible(false);

        auto label = CCLabelBMFont::create("Style 1/100", "bigFont.fnt");
        label->setScale(0.5f);
        label->setPosition({0, 30});
        label->setTag(7702);
        panel->addChild(label);

        auto prev = CCMenuItemSpriteExtra::create(ButtonSprite::create("<"), this, menu_selector(AutoDecorUI::onPrev));
        prev->setPosition({-70, 0});
        auto next = CCMenuItemSpriteExtra::create(ButtonSprite::create(">"), this, menu_selector(AutoDecorUI::onNext));
        next->setPosition({70, 0});
        auto ok = CCMenuItemSpriteExtra::create(ButtonSprite::create("OK"), this, menu_selector(AutoDecorUI::onOk));
        ok->setPosition({-35, -45});
        auto cancel = CCMenuItemSpriteExtra::create(ButtonSprite::create("CANCEL"), this, menu_selector(AutoDecorUI::onCancel));
        cancel->setPosition({35, -45});

        panel->addChild(prev);
        panel->addChild(next);
        panel->addChild(ok);
        panel->addChild(cancel);
        this->addChild(panel);
        m_fields->panel = panel;

        return true;
    }

    void clearPreview() {
        for (auto& obj : m_fields->preview) {
            m_editorLayer->removeObject(obj, true);
        }
        m_fields->preview.clear();
    }

    void showPreview() {
        this->clearPreview();

        int s = m_fields->style;
        auto& set = DECOR_SETS[s % DECOR_SETS.size()];
        auto& pattern = PATTERNS[(s / DECOR_SETS.size()) % PATTERNS.size()];

        std::vector<CCPoint> sources;
        for (auto obj : CCArrayExt<GameObject*>(m_editorLayer->m_objects)) {
            if (std::find(TARGETS.begin(), TARGETS.end(), obj->m_objectID) != TARGETS.end()) {
                sources.push_back(obj->getPosition());
            }
        }

        int k = 0;
        for (auto& p : sources) {
            for (auto& off : pattern) {
                int id = set[k++ % set.size()];
                if (auto created = m_editorLayer->createObject(id, p + off, false)) {
                    m_fields->preview.push_back(created);
                }
            }
        }

        if (m_fields->panel) {
            if (auto label = typeinfo_cast<CCLabelBMFont*>(m_fields->panel->getChildByTag(7702))) {
                label->setString(fmt::format("Style {}/{}", s + 1, styleCount()).c_str());
            }
        }
    }

    void onOpen(CCObject*) {
        if (!m_fields->panel || m_fields->panel->isVisible()) return;
        m_fields->panel->setVisible(true);
        this->showPreview();
    }

    void onPrev(CCObject*) {
        m_fields->style = (m_fields->style - 1 + styleCount()) % styleCount();
        this->showPreview();
    }

    void onNext(CCObject*) {
        m_fields->style = (m_fields->style + 1) % styleCount();
        this->showPreview();
    }

    void onOk(CCObject*) {
        int n = static_cast<int>(m_fields->preview.size());
        m_fields->preview.clear(); // объекты остаются в уровне
        m_fields->panel->setVisible(false);
        Notification::create(fmt::format("Applied: {} decor objects", n), NotificationIcon::Success)->show();
    }

    void onCancel(CCObject*) {
        this->clearPreview();
        m_fields->panel->setVisible(false);
    }
};
