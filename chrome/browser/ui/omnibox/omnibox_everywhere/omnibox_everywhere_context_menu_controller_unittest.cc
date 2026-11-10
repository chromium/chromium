// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_context_menu_controller.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/autocomplete/chrome_aim_eligibility_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/new_tab_page/prefs/ntp_pref_names.h"
#include "chrome/browser/search_engines/ai_mode_button_service_factory.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/ntp_tiles/enterprise/enterprise_shortcuts_pref_names.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/search_engines/test_ai_mode_button_service.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/context_menu_data/context_menu_data.h"
#include "third_party/blink/public/common/context_menu_data/edit_flags.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_runner_handler.h"
#include "ui/views/test/menu_runner_test_api.h"
#include "ui/views/test/test_platform_native_widget.h"
#include "ui/views/view.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_MAC)
#include "ui/views/widget/native_widget_mac.h"
#elif defined(USE_AURA)
#include "ui/views/widget/native_widget_aura.h"
#endif

namespace omnibox_everywhere {

namespace {

using CommandId = OmniboxEverywhereContextMenuController::CommandId;

#if BUILDFLAG(IS_MAC)
using BaseTestNativeWidget =
    views::test::TestPlatformNativeWidget<views::NativeWidgetMac>;
#elif defined(USE_AURA)
using BaseTestNativeWidget =
    views::test::TestPlatformNativeWidget<views::NativeWidgetAura>;
#endif

class TestNativeWidget : public BaseTestNativeWidget {
 public:
  TestNativeWidget(views::internal::NativeWidgetDelegate* delegate,
                   base::RepeatingClosure on_show_emoji_panel)
      : BaseTestNativeWidget(delegate,
                             /*mock_capture=*/false,
                             /*destroyed=*/nullptr),
        on_show_emoji_panel_(std::move(on_show_emoji_panel)) {}
  ~TestNativeWidget() override = default;

  void ShowEmojiPanel() override {
    if (on_show_emoji_panel_) {
      on_show_emoji_panel_.Run();
    }
  }

 private:
  base::RepeatingClosure on_show_emoji_panel_;
};

class TestMenuRunnerHandler : public views::MenuRunnerHandler {
 public:
  explicit TestMenuRunnerHandler(ui::mojom::MenuSourceType* last_source_type)
      : last_source_type_(last_source_type) {}
  ~TestMenuRunnerHandler() override = default;

  void RunMenuAt(views::Widget* parent,
                 views::MenuButtonController* button_controller,
                 const gfx::Rect& bounds,
                 views::MenuAnchorPosition anchor,
                 ui::mojom::MenuSourceType source_type,
                 int32_t types) override {
    *last_source_type_ = source_type;
  }

 private:
  const raw_ptr<ui::mojom::MenuSourceType> last_source_type_;
};

class TestingAimEligibilityService : public ChromeAimEligibilityService {
 public:
  TestingAimEligibilityService(Profile* profile, bool is_fusebox_eligible)
      : ChromeAimEligibilityService(*profile->GetPrefs(),
                                    /*template_url_service=*/nullptr,
                                    /*url_loader_factory=*/nullptr,
                                    /*identity_manager=*/nullptr,
                                    /*configuration=*/{}),
        is_fusebox_eligible_(is_fusebox_eligible) {}

  variations::VariationsService* GetVariationsService() const override {
    return nullptr;
  }

  bool IsAimEligible() const override { return is_fusebox_eligible_; }
  bool IsFuseboxEligible() const override { return is_fusebox_eligible_; }
  bool IsAimAllowedByDse() const override { return is_fusebox_eligible_; }

 private:
  const bool is_fusebox_eligible_;
};

class FakeDelegate : public OmniboxEverywhereContextMenuController::Delegate {
 public:
  FakeDelegate() = default;
  ~FakeDelegate() override = default;

  void set_widget(views::Widget* widget) { widget_ = widget; }
  void set_web_contents(content::WebContents* web_contents) {
    web_contents_ = web_contents;
  }
  void set_profile(Profile* profile) { profile_ = profile; }
  int closed_count() const { return closed_count_; }

  // OmniboxEverywhereContextMenuController::Delegate:
  views::Widget* GetContextMenuHostWidget() override { return widget_; }
  content::WebContents* GetContextMenuWebContents() override {
    return web_contents_;
  }
  Profile* GetContextMenuProfile() override { return profile_; }
  void OnContextMenuClosed() override { ++closed_count_; }
#if BUILDFLAG(IS_WIN)
  void MinimizeFromContextMenu() override {}
  void CloseFromContextMenu() override {}
#endif  // BUILDFLAG(IS_WIN)

 private:
  raw_ptr<views::Widget> widget_ = nullptr;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  raw_ptr<Profile> profile_ = nullptr;
  int closed_count_ = 0;
};

bool HasCommand(const ui::SimpleMenuModel* model, int command_id) {
  return model && model->GetIndexOfCommandId(command_id).has_value();
}

std::vector<int> GetCommandIds(const ui::SimpleMenuModel* model) {
  std::vector<int> ids;
  for (size_t i = 0; i < model->GetItemCount(); ++i) {
    ids.push_back(model->GetCommandIdAt(i));
  }
  return ids;
}

base::OnceClosure CountingClosure(int* count) {
  return base::BindOnce([](int* count) { ++*count; }, count);
}

}  // namespace

class OmniboxEverywhereContextMenuControllerTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(omnibox::kOmniboxEverywhere);
    ChromeViewsTestBase::SetUp();
    views::ViewsDelegate::GetInstance()->set_native_widget_factory(
        base::BindRepeating(
            [](OmniboxEverywhereContextMenuControllerTest* test,
               const views::Widget::InitParams& init_params,
               views::internal::NativeWidgetDelegate* delegate)
                -> views::NativeWidget* {
              return new TestNativeWidget(
                  delegate,
                  base::BindRepeating(
                      [](OmniboxEverywhereContextMenuControllerTest* test) {
                        ++test->emoji_panel_shown_count_;
                      },
                      base::Unretained(test)));
            },
            base::Unretained(this)));
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    widget_->SetContentsView(std::make_unique<views::View>());
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    delegate_.set_widget(widget_.get());
    delegate_.set_web_contents(web_contents_.get());
    delegate_.set_profile(&profile_);
    controller_ =
        std::make_unique<OmniboxEverywhereContextMenuController>(delegate_);
    controller_->SetMenuRunnerFactoryForTesting(base::BindRepeating(
        [](base::RepeatingClosure* last_on_closed,
           ui::mojom::MenuSourceType* last_source_type, ui::MenuModel* model,
           base::RepeatingClosure on_closed) {
          *last_on_closed = on_closed;
          auto runner = std::make_unique<views::MenuRunner>(
              model,
              views::MenuRunner::HAS_MNEMONICS |
                  views::MenuRunner::CONTEXT_MENU,
              std::move(on_closed));
          views::test::MenuRunnerTestAPI(runner.get())
              .SetMenuRunnerHandler(
                  std::make_unique<TestMenuRunnerHandler>(last_source_type));
          return runner;
        },
        base::Unretained(&last_on_closed_),
        base::Unretained(&last_source_type_)));
  }

  void TearDown() override {
    controller_.reset();
    delegate_.set_web_contents(nullptr);
    delegate_.set_widget(nullptr);
    delegate_.set_profile(nullptr);
    web_contents_.reset();
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  int emoji_panel_shown_count() const { return emoji_panel_shown_count_; }

  void SetUpAimEligibilityService(bool is_fusebox_eligible) {
    AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        &profile_,
        base::BindRepeating(
            [](bool fusebox_eligible, content::BrowserContext* context)
                -> std::unique_ptr<KeyedService> {
              return std::make_unique<TestingAimEligibilityService>(
                  static_cast<TestingProfile*>(context), fusebox_eligible);
            },
            is_fusebox_eligible));
  }

  void SetUpAiModeButtonService() {
    AiModeButtonServiceFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          auto service = std::make_unique<TestAiModeButtonService>(
              /*template_url_service=*/nullptr);
          AiModeButtonUiConfig test_config(
              SearchEngineType::SEARCH_ENGINE_GOOGLE, u"AI Mode", u"Google",
              /*favicon_url=*/"", /*navigation_url=*/"",
              /*navigation_url_empty=*/"");
          service->current_ui_config_ = test_config;
          return service;
        }));
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  content::RenderViewHostTestEnabler rvh_enabler_;
  TestingProfile profile_;
  int emoji_panel_shown_count_ = 0;
  FakeDelegate delegate_;
  base::RepeatingClosure last_on_closed_;
  ui::mojom::MenuSourceType last_source_type_ =
      ui::mojom::MenuSourceType::kNone;
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<OmniboxEverywhereContextMenuController> controller_;
};

// Without a host widget there is nothing to anchor the menu to.
TEST_F(OmniboxEverywhereContextMenuControllerTest, NoMenuWithoutHostWidget) {
  delegate_.set_widget(nullptr);
  content::ContextMenuParams params;
  controller_->HandleContextMenu(params);
  EXPECT_FALSE(controller_->IsOpen());
  EXPECT_FALSE(controller_->model_for_testing());
}

// A non-editable, non-selected target shows only the settings entries.
TEST_F(OmniboxEverywhereContextMenuControllerTest, BackgroundMenu) {
  content::ContextMenuParams params;
  controller_->HandleContextMenu(params);

  ASSERT_TRUE(controller_->IsOpen());
  const ui::SimpleMenuModel* model = controller_->model_for_testing();
  EXPECT_TRUE(HasCommand(model, CommandId::kManageSearchEngines));
  EXPECT_TRUE(HasCommand(model, CommandId::kCustomizeKeyboardShortcut));
  EXPECT_TRUE(HasCommand(model, CommandId::kSettings));
  EXPECT_FALSE(HasCommand(model, CommandId::kCopy));
  EXPECT_FALSE(HasCommand(model, CommandId::kPaste));
}

// Selected, non-editable text offers Copy but no editing commands.
TEST_F(OmniboxEverywhereContextMenuControllerTest, SelectionMenu) {
  content::ContextMenuParams params;
  params.selection_text = u"selected";
  controller_->HandleContextMenu(params);

  const ui::SimpleMenuModel* model = controller_->model_for_testing();
  EXPECT_TRUE(HasCommand(model, CommandId::kCopy));
  EXPECT_TRUE(HasCommand(model, CommandId::kSelectAll));
  EXPECT_FALSE(HasCommand(model, CommandId::kCut));
  EXPECT_FALSE(HasCommand(model, CommandId::kPaste));
  EXPECT_FALSE(HasCommand(model, CommandId::kUndo));
}

// Editing commands are disabled when there is no WebContents to target.
TEST_F(OmniboxEverywhereContextMenuControllerTest,
       CommandsDisabledWithoutWebContents) {
  delegate_.set_web_contents(nullptr);
  EXPECT_FALSE(controller_->IsCommandIdEnabled(CommandId::kCopy));
  EXPECT_FALSE(controller_->IsCommandIdEnabled(CommandId::kPaste));
  EXPECT_FALSE(controller_->IsCommandIdEnabled(CommandId::kSelectAll));
  EXPECT_FALSE(controller_->IsCommandIdEnabled(CommandId::kSettings));
#if BUILDFLAG(IS_WIN)
  // Window controls only need the widget.
  EXPECT_TRUE(controller_->IsCommandIdEnabled(CommandId::kMinimize));
  EXPECT_TRUE(controller_->IsCommandIdEnabled(CommandId::kClose));
  delegate_.set_widget(nullptr);
  EXPECT_FALSE(controller_->IsCommandIdEnabled(CommandId::kMinimize));
  EXPECT_FALSE(controller_->IsCommandIdEnabled(CommandId::kClose));
#endif  // BUILDFLAG(IS_WIN)
}

// Closing the menu clears the open state and notifies the delegate once.
TEST_F(OmniboxEverywhereContextMenuControllerTest, CloseNotifiesDelegate) {
  controller_->HandleContextMenu(content::ContextMenuParams());
  ASSERT_TRUE(controller_->IsOpen());
  ASSERT_TRUE(last_on_closed_);

  last_on_closed_.Run();
  EXPECT_FALSE(controller_->IsOpen());
  EXPECT_FALSE(controller_->model_for_testing());
  EXPECT_EQ(1, delegate_.closed_count());
}

// Cancel() dismisses an open menu; Reset() also drops the model without
// notifying the delegate.
TEST_F(OmniboxEverywhereContextMenuControllerTest, CancelAndReset) {
  controller_->HandleContextMenu(content::ContextMenuParams());
  ASSERT_TRUE(controller_->IsOpen());

  controller_->Cancel();
  EXPECT_FALSE(controller_->IsOpen());

  controller_->HandleContextMenu(content::ContextMenuParams());
  ASSERT_TRUE(controller_->IsOpen());
  const int closed_count_before_reset = delegate_.closed_count();
  controller_->Reset();
  EXPECT_FALSE(controller_->IsOpen());
  EXPECT_FALSE(controller_->model_for_testing());
  EXPECT_EQ(closed_count_before_reset, delegate_.closed_count());
}

// The overflow menu shows exactly the background right-click menu's items, and
// closing it runs its callback once and notifies the delegate.
TEST_F(OmniboxEverywhereContextMenuControllerTest,
       OverflowMenuMatchesBackgroundMenu) {
  controller_->HandleContextMenu(content::ContextMenuParams());
  ASSERT_TRUE(controller_->model_for_testing());
  const std::vector<int> background_ids =
      GetCommandIds(controller_->model_for_testing());
  last_on_closed_.Run();

  int closed_count = 0;
  controller_->ShowOverflowMenu(gfx::Rect(100, 100, 28, 28),
                                ui::mojom::MenuSourceType::kMouse,
                                CountingClosure(&closed_count));
  ASSERT_TRUE(controller_->IsOpen());
  ASSERT_TRUE(controller_->model_for_testing());
  EXPECT_EQ(background_ids, GetCommandIds(controller_->model_for_testing()));
  EXPECT_TRUE(HasCommand(controller_->model_for_testing(),
                         CommandId::kManageSearchEngines));
  EXPECT_TRUE(
      HasCommand(controller_->model_for_testing(), CommandId::kSettings));
  EXPECT_EQ(0, closed_count);

  const int delegate_closed_before = delegate_.closed_count();
  last_on_closed_.Run();
  EXPECT_FALSE(controller_->IsOpen());
  EXPECT_EQ(1, closed_count);
  EXPECT_EQ(delegate_closed_before + 1, delegate_.closed_count());
}

// The overflow menu is run with the source it was triggered from, so a
// keyboard-opened menu selects its first item.
TEST_F(OmniboxEverywhereContextMenuControllerTest,
       OverflowMenuPassesSourceType) {
  controller_->ShowOverflowMenu(gfx::Rect(100, 100, 28, 28),
                                ui::mojom::MenuSourceType::kKeyboard,
                                base::DoNothing());
  EXPECT_EQ(ui::mojom::MenuSourceType::kKeyboard, last_source_type_);
  last_on_closed_.Run();

  controller_->ShowOverflowMenu(gfx::Rect(100, 100, 28, 28),
                                ui::mojom::MenuSourceType::kMouse,
                                base::DoNothing());
  EXPECT_EQ(ui::mojom::MenuSourceType::kMouse, last_source_type_);
}

// The overflow menu doesn't inherit the previous right-click target, so
// editing items (including the macOS Writing Direction submenu) stay out.
TEST_F(OmniboxEverywhereContextMenuControllerTest,
       OverflowMenuAfterEditableMenuOmitsEditingItems) {
  content::ContextMenuParams params;
  params.is_editable = true;
  controller_->HandleContextMenu(params);
  ASSERT_TRUE(
      HasCommand(controller_->model_for_testing(), CommandId::kSelectAll));
  last_on_closed_.Run();

  controller_->ShowOverflowMenu(gfx::Rect(100, 100, 28, 28),
                                ui::mojom::MenuSourceType::kMouse,
                                base::DoNothing());
  const ui::SimpleMenuModel* model = controller_->model_for_testing();
  ASSERT_TRUE(model);
  EXPECT_EQ(CommandId::kManageSearchEngines, model->GetCommandIdAt(0));
  EXPECT_FALSE(HasCommand(model, CommandId::kSelectAll));
  EXPECT_FALSE(HasCommand(model, CommandId::kPaste));
#if BUILDFLAG(IS_MAC)
  EXPECT_FALSE(HasCommand(model, CommandId::kWritingDirectionMenu));
#endif  // BUILDFLAG(IS_MAC)
}

// Replacing the overflow menu with another menu, or resetting the controller,
// runs the pending overflow closed callback exactly once.
TEST_F(OmniboxEverywhereContextMenuControllerTest,
       OverflowMenuClosedCallbackRunsWhenReplacedOrReset) {
  int first_closed = 0;
  controller_->ShowOverflowMenu(gfx::Rect(100, 100, 28, 28),
                                ui::mojom::MenuSourceType::kMouse,
                                CountingClosure(&first_closed));
  controller_->HandleContextMenu(content::ContextMenuParams());
  EXPECT_EQ(1, first_closed);
  EXPECT_TRUE(controller_->IsOpen());

  int second_closed = 0;
  controller_->ShowOverflowMenu(gfx::Rect(100, 100, 28, 28),
                                ui::mojom::MenuSourceType::kMouse,
                                CountingClosure(&second_closed));
  controller_->Reset();
  EXPECT_EQ(1, second_closed);
  EXPECT_EQ(1, first_closed);
  EXPECT_FALSE(controller_->IsOpen());
}

TEST_F(OmniboxEverywhereContextMenuControllerTest,
       ContextMenuModelEditableElement) {
  content::ContextMenuParams params;
  params.is_editable = true;
  controller_->HandleContextMenu(params);

  const ui::SimpleMenuModel* model = controller_->model_for_testing();
  ASSERT_TRUE(model);

  size_t index = 0;
  if (ui::IsEmojiPanelSupported()) {
    EXPECT_EQ(model->GetCommandIdAt(index++), CommandId::kEmoji);
    EXPECT_EQ(model->GetTypeAt(index++),
              ui::MenuModel::ItemType::TYPE_SEPARATOR);
    EXPECT_TRUE(controller_->IsCommandIdEnabled(CommandId::kEmoji));
  }
  EXPECT_EQ(model->GetCommandIdAt(index++), CommandId::kUndo);
  EXPECT_EQ(model->GetTypeAt(index++), ui::MenuModel::ItemType::TYPE_SEPARATOR);
  EXPECT_EQ(model->GetCommandIdAt(index++), CommandId::kCut);
  EXPECT_EQ(model->GetCommandIdAt(index++), CommandId::kCopy);
  EXPECT_EQ(model->GetCommandIdAt(index++), CommandId::kPaste);
  EXPECT_EQ(model->GetCommandIdAt(index++), CommandId::kPasteAndSearch);
  EXPECT_EQ(model->GetCommandIdAt(index++), CommandId::kDelete);
  EXPECT_EQ(model->GetTypeAt(index++), ui::MenuModel::ItemType::TYPE_SEPARATOR);
  EXPECT_EQ(model->GetCommandIdAt(index++), CommandId::kSelectAll);
  EXPECT_EQ(model->GetTypeAt(index++), ui::MenuModel::ItemType::TYPE_SEPARATOR);
  EXPECT_EQ(model->GetCommandIdAt(index++), CommandId::kManageSearchEngines);
#if BUILDFLAG(IS_MAC)
  EXPECT_EQ(model->GetCommandIdAt(index++), CommandId::kWritingDirectionMenu);
#endif  // BUILDFLAG(IS_MAC)
  EXPECT_EQ(model->GetTypeAt(index++), ui::MenuModel::ItemType::TYPE_SEPARATOR);
  EXPECT_EQ(model->GetCommandIdAt(index++), CommandId::kShowShortcuts);
  EXPECT_EQ(model->GetCommandIdAt(index++),
            CommandId::kCustomizeKeyboardShortcut);
  EXPECT_EQ(model->GetCommandIdAt(index++), CommandId::kSettings);
#if BUILDFLAG(IS_WIN)
  EXPECT_EQ(model->GetTypeAt(index++), ui::MenuModel::ItemType::TYPE_SEPARATOR);
  EXPECT_EQ(model->GetCommandIdAt(index++), CommandId::kMinimize);
  EXPECT_EQ(model->GetCommandIdAt(index++), CommandId::kClose);
#endif  // BUILDFLAG(IS_WIN)
  EXPECT_EQ(model->GetItemCount(), index);
}

#if BUILDFLAG(IS_MAC)
TEST_F(OmniboxEverywhereContextMenuControllerTest,
       ContextMenuWritingDirectionSubmenu) {
  content::ContextMenuParams params;
  params.is_editable = true;
  params.writing_direction_default =
      blink::ContextMenuData::kCheckableMenuItemDisabled;
  params.writing_direction_left_to_right =
      blink::ContextMenuData::kCheckableMenuItemEnabled |
      blink::ContextMenuData::kCheckableMenuItemChecked;
  params.writing_direction_right_to_left =
      blink::ContextMenuData::kCheckableMenuItemEnabled;
  controller_->HandleContextMenu(params);

  const ui::SimpleMenuModel* model = controller_->model_for_testing();
  ASSERT_TRUE(model);
  std::optional<size_t> submenu_index =
      model->GetIndexOfCommandId(CommandId::kWritingDirectionMenu);
  ASSERT_TRUE(submenu_index.has_value());
  ui::MenuModel* submenu = model->GetSubmenuModelAt(*submenu_index);
  ASSERT_TRUE(submenu);
  ASSERT_EQ(submenu->GetItemCount(), 3u);
  EXPECT_EQ(submenu->GetCommandIdAt(0), CommandId::kWritingDirectionDefault);
  EXPECT_EQ(submenu->GetCommandIdAt(1), CommandId::kWritingDirectionLtr);
  EXPECT_EQ(submenu->GetCommandIdAt(2), CommandId::kWritingDirectionRtl);

  EXPECT_FALSE(
      controller_->IsCommandIdEnabled(CommandId::kWritingDirectionDefault));
  EXPECT_TRUE(controller_->IsCommandIdEnabled(CommandId::kWritingDirectionLtr));
  EXPECT_TRUE(controller_->IsCommandIdEnabled(CommandId::kWritingDirectionRtl));

  EXPECT_FALSE(
      controller_->IsCommandIdChecked(CommandId::kWritingDirectionDefault));
  EXPECT_TRUE(controller_->IsCommandIdChecked(CommandId::kWritingDirectionLtr));
  EXPECT_FALSE(
      controller_->IsCommandIdChecked(CommandId::kWritingDirectionRtl));

  controller_->ExecuteCommand(CommandId::kWritingDirectionRtl, 0);
}
#endif  // BUILDFLAG(IS_MAC)

TEST_F(OmniboxEverywhereContextMenuControllerTest, ContextMenuEmojiCommand) {
  // Non-editable context omits and disables kEmoji.
  content::ContextMenuParams non_editable_params;
  non_editable_params.is_editable = false;
  controller_->HandleContextMenu(non_editable_params);
  ASSERT_TRUE(controller_->model_for_testing());
  EXPECT_FALSE(controller_->model_for_testing()
                   ->GetIndexOfCommandId(CommandId::kEmoji)
                   .has_value());
  EXPECT_FALSE(controller_->IsCommandIdEnabled(CommandId::kEmoji));

  ui::Accelerator accelerator;
  EXPECT_FALSE(
      controller_->GetAcceleratorForCommandId(CommandId::kUndo, &accelerator));
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  EXPECT_TRUE(
      controller_->GetAcceleratorForCommandId(CommandId::kEmoji, &accelerator));
#endif

  // Editable context enables kEmoji and ExecuteCommand(kEmoji) triggers
  // ShowEmojiPanel() on the widget without invoking native OS global panels in
  // unit_tests.
  content::ContextMenuParams editable_params;
  editable_params.is_editable = true;
  controller_->HandleContextMenu(editable_params);
  if (ui::IsEmojiPanelSupported()) {
    EXPECT_TRUE(controller_->IsCommandIdEnabled(CommandId::kEmoji));
    EXPECT_EQ(emoji_panel_shown_count(), 0);
    controller_->ExecuteCommand(CommandId::kEmoji, 0);
    EXPECT_EQ(emoji_panel_shown_count(), 1);
  }
}

TEST_F(OmniboxEverywhereContextMenuControllerTest,
       ContextMenuModelNonEditableElementWithSelection) {
  content::ContextMenuParams params;
  params.is_editable = false;
  params.selection_text = u"selected text";
  controller_->HandleContextMenu(params);

  const ui::SimpleMenuModel* model = controller_->model_for_testing();
  ASSERT_TRUE(model);
  EXPECT_EQ(model->GetItemCount(), BUILDFLAG(IS_WIN) ? 12u : 9u);
  EXPECT_EQ(model->GetCommandIdAt(0), CommandId::kCopy);
  EXPECT_EQ(model->GetTypeAt(1), ui::MenuModel::ItemType::TYPE_SEPARATOR);
  EXPECT_EQ(model->GetCommandIdAt(2), CommandId::kSelectAll);
  EXPECT_EQ(model->GetTypeAt(3), ui::MenuModel::ItemType::TYPE_SEPARATOR);
  EXPECT_EQ(model->GetCommandIdAt(4), CommandId::kManageSearchEngines);
  EXPECT_EQ(model->GetTypeAt(5), ui::MenuModel::ItemType::TYPE_SEPARATOR);
  EXPECT_EQ(model->GetCommandIdAt(6), CommandId::kShowShortcuts);
  EXPECT_EQ(model->GetCommandIdAt(7), CommandId::kCustomizeKeyboardShortcut);
  EXPECT_EQ(model->GetCommandIdAt(8), CommandId::kSettings);
#if BUILDFLAG(IS_WIN)
  EXPECT_EQ(model->GetTypeAt(9), ui::MenuModel::ItemType::TYPE_SEPARATOR);
  EXPECT_EQ(model->GetCommandIdAt(10), CommandId::kMinimize);
  EXPECT_EQ(model->GetCommandIdAt(11), CommandId::kClose);
#endif  // BUILDFLAG(IS_WIN)
}

TEST_F(OmniboxEverywhereContextMenuControllerTest,
       ContextMenuModelNonEditableBackground) {
  // Right-clicking on empty background space (not editable, no selection text).
  content::ContextMenuParams params;
  params.is_editable = false;
  params.selection_text = u"";
  controller_->HandleContextMenu(params);

  const ui::SimpleMenuModel* model = controller_->model_for_testing();
  ASSERT_TRUE(model);
  EXPECT_EQ(model->GetItemCount(), BUILDFLAG(IS_WIN) ? 8u : 5u);
  EXPECT_EQ(model->GetCommandIdAt(0), CommandId::kManageSearchEngines);
  EXPECT_EQ(model->GetTypeAt(1), ui::MenuModel::ItemType::TYPE_SEPARATOR);
  EXPECT_EQ(model->GetCommandIdAt(2), CommandId::kShowShortcuts);
  EXPECT_EQ(model->GetCommandIdAt(3), CommandId::kCustomizeKeyboardShortcut);
  EXPECT_EQ(model->GetCommandIdAt(4), CommandId::kSettings);
#if BUILDFLAG(IS_WIN)
  EXPECT_EQ(model->GetTypeAt(5), ui::MenuModel::ItemType::TYPE_SEPARATOR);
  EXPECT_EQ(model->GetCommandIdAt(6), CommandId::kMinimize);
  EXPECT_EQ(model->GetCommandIdAt(7), CommandId::kClose);
#endif  // BUILDFLAG(IS_WIN)
}

#if BUILDFLAG(IS_WIN)
TEST_F(OmniboxEverywhereContextMenuControllerTest,
       ContextMenuOmitsWindowControlsWhenEphemeral) {
  base::ScopedClosureRunner reset_pref;
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, true);
    reset_pref.ReplaceClosure(base::BindOnce([]() {
      if (g_browser_process && g_browser_process->local_state()) {
        g_browser_process->local_state()->ClearPref(
            omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel);
      }
    }));
  }

  content::ContextMenuParams params;
  params.is_editable = false;
  controller_->HandleContextMenu(params);

  const ui::SimpleMenuModel* model = controller_->model_for_testing();
  ASSERT_TRUE(model);
  EXPECT_EQ(model->GetItemCount(), 5u);
  EXPECT_EQ(model->GetCommandIdAt(4), CommandId::kSettings);
}
#endif  // BUILDFLAG(IS_WIN)

TEST_F(OmniboxEverywhereContextMenuControllerTest,
       ContextMenuCommandEnablement) {
  SetUpAimEligibilityService(/*is_fusebox_eligible=*/true);
  SetUpAiModeButtonService();

  content::ContextMenuParams params;
  params.is_editable = true;
  params.edit_flags = blink::ContextMenuDataEditFlags::kCanCut |
                      blink::ContextMenuDataEditFlags::kCanCopy |
                      blink::ContextMenuDataEditFlags::kCanUndo |
                      blink::ContextMenuDataEditFlags::kCanDelete;
  controller_->HandleContextMenu(params);

  EXPECT_TRUE(controller_->IsCommandIdEnabled(CommandId::kUndo));
  EXPECT_TRUE(controller_->IsCommandIdEnabled(CommandId::kCut));
  EXPECT_TRUE(controller_->IsCommandIdEnabled(CommandId::kCopy));
  EXPECT_TRUE(controller_->IsCommandIdEnabled(CommandId::kPaste));
  EXPECT_TRUE(controller_->IsCommandIdEnabled(CommandId::kDelete));
  EXPECT_TRUE(controller_->IsCommandIdEnabled(CommandId::kSelectAll));
  EXPECT_TRUE(controller_->IsCommandIdEnabled(CommandId::kManageSearchEngines));
  EXPECT_TRUE(controller_->IsCommandIdEnabled(CommandId::kAlwaysShowAiMode));
  EXPECT_TRUE(controller_->IsCommandIdEnabled(CommandId::kShowShortcuts));
  EXPECT_TRUE(
      controller_->IsCommandIdEnabled(CommandId::kCustomizeKeyboardShortcut));
  EXPECT_TRUE(controller_->IsCommandIdEnabled(CommandId::kSettings));
#if BUILDFLAG(IS_WIN)
  EXPECT_TRUE(controller_->IsCommandIdEnabled(CommandId::kMinimize));
  EXPECT_TRUE(controller_->IsCommandIdEnabled(CommandId::kClose));
#endif  // BUILDFLAG(IS_WIN)

  EXPECT_TRUE(controller_->IsCommandIdEnabled(CommandId::kPaste));
  EXPECT_TRUE(controller_->IsCommandIdEnabled(CommandId::kPasteAndSearch));

  // Without edit flags, Cut, Copy, Undo, and Delete should be disabled if
  // selection is empty.
  params.edit_flags = 0;
  params.selection_text = u"";
  controller_->HandleContextMenu(params);

  EXPECT_FALSE(controller_->IsCommandIdEnabled(CommandId::kUndo));
  EXPECT_FALSE(controller_->IsCommandIdEnabled(CommandId::kCut));
  EXPECT_FALSE(controller_->IsCommandIdEnabled(CommandId::kCopy));
  EXPECT_FALSE(controller_->IsCommandIdEnabled(CommandId::kDelete));
  EXPECT_TRUE(controller_->IsCommandIdEnabled(CommandId::kPaste));
  EXPECT_TRUE(controller_->IsCommandIdEnabled(CommandId::kPasteAndSearch));
  EXPECT_TRUE(controller_->IsCommandIdEnabled(CommandId::kSelectAll));

  // When not editable and without kCanPaste flag, Paste is disabled.
  params.is_editable = false;
  controller_->HandleContextMenu(params);
  EXPECT_FALSE(controller_->IsCommandIdEnabled(CommandId::kPaste));

  // When not editable but kCanPaste flag is explicitly present, Paste is
  // enabled.
  params.edit_flags = blink::ContextMenuDataEditFlags::kCanPaste;
  controller_->HandleContextMenu(params);
  EXPECT_TRUE(controller_->IsCommandIdEnabled(CommandId::kPaste));
}

TEST_F(OmniboxEverywhereContextMenuControllerTest,
       ExecutePasteAndSearchCommand) {
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteText(u"https://example.com/");
  }

  EXPECT_TRUE(controller_->IsCommandIdEnabled(CommandId::kPasteAndSearch));
  controller_->ExecuteCommand(CommandId::kPasteAndSearch, 0);
}

TEST_F(OmniboxEverywhereContextMenuControllerTest,
       ContextMenuPasteAndSearchAndGoLabels) {
  content::ContextMenuParams params;
  params.is_editable = true;

  // 1. Search query on clipboard -> "Paste and Search for “8551957”".
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteText(u"8551957");
  }
  controller_->HandleContextMenu(params);
  const ui::SimpleMenuModel* model = controller_->model_for_testing();
  ASSERT_TRUE(model);
  std::optional<size_t> paste_and_go_index =
      model->GetIndexOfCommandId(CommandId::kPasteAndSearch);
  ASSERT_TRUE(paste_and_go_index.has_value());
  EXPECT_EQ(model->GetLabelAt(*paste_and_go_index),
            l10n_util::GetStringFUTF16(IDS_PASTE_AND_SEARCH, u"8551957"));

  // 2. URL on clipboard -> "Paste and Go to example.com".
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteText(u"https://example.com/");
  }
  controller_->HandleContextMenu(params);
  model = controller_->model_for_testing();
  ASSERT_TRUE(model);
  paste_and_go_index = model->GetIndexOfCommandId(CommandId::kPasteAndSearch);
  ASSERT_TRUE(paste_and_go_index.has_value());
  EXPECT_NE(model->GetLabelAt(*paste_and_go_index).find(u"example.com"),
            std::u16string::npos);

  // 3. Empty clipboard -> generic "Paste and Go".
  ui::Clipboard::GetForCurrentThread()->Clear(ui::ClipboardBuffer::kCopyPaste);
  controller_->HandleContextMenu(params);
  model = controller_->model_for_testing();
  ASSERT_TRUE(model);
  paste_and_go_index = model->GetIndexOfCommandId(CommandId::kPasteAndSearch);
  ASSERT_TRUE(paste_and_go_index.has_value());
  EXPECT_EQ(model->GetLabelAt(*paste_and_go_index),
            l10n_util::GetStringUTF16(IDS_PASTE_AND_GO_EMPTY));
}

TEST_F(OmniboxEverywhereContextMenuControllerTest,
       ContextMenuManageSearchEnginesSafetyGuard) {
  // Without WebContents / Profile, kManageSearchEngines is disabled.
  delegate_.set_web_contents(nullptr);
  delegate_.set_profile(nullptr);
  EXPECT_FALSE(
      controller_->IsCommandIdEnabled(CommandId::kManageSearchEngines));

  delegate_.set_web_contents(web_contents_.get());
  delegate_.set_profile(&profile_);
  EXPECT_TRUE(controller_->IsCommandIdEnabled(CommandId::kManageSearchEngines));

  delegate_.set_web_contents(nullptr);
  delegate_.set_profile(nullptr);
  EXPECT_FALSE(
      controller_->IsCommandIdEnabled(CommandId::kManageSearchEngines));
}

TEST_F(OmniboxEverywhereContextMenuControllerTest,
       ContextMenuAiModeFuseboxIneligible) {
  SetUpAimEligibilityService(/*is_fusebox_eligible=*/false);
  SetUpAiModeButtonService();

  EXPECT_FALSE(controller_->IsCommandIdEnabled(CommandId::kAlwaysShowAiMode));

  content::ContextMenuParams params;
  params.is_editable = false;
  params.selection_text = u"";
  controller_->HandleContextMenu(params);

  const ui::SimpleMenuModel* model = controller_->model_for_testing();
  ASSERT_TRUE(model);
  EXPECT_FALSE(
      model->GetIndexOfCommandId(CommandId::kAlwaysShowAiMode).has_value());
}

TEST_F(OmniboxEverywhereContextMenuControllerTest,
       ContextMenuAiModeFuseboxEligible) {
  SetUpAimEligibilityService(/*is_fusebox_eligible=*/true);
  SetUpAiModeButtonService();

  EXPECT_TRUE(controller_->IsCommandIdEnabled(CommandId::kAlwaysShowAiMode));

  content::ContextMenuParams params;
  params.is_editable = false;
  params.selection_text = u"";
  controller_->HandleContextMenu(params);

  const ui::SimpleMenuModel* model = controller_->model_for_testing();
  ASSERT_TRUE(model);
  EXPECT_TRUE(
      model->GetIndexOfCommandId(CommandId::kAlwaysShowAiMode).has_value());
}

TEST_F(OmniboxEverywhereContextMenuControllerTest,
       ContextMenuAlwaysShowAiModeToggle) {
  SetUpAimEligibilityService(/*is_fusebox_eligible=*/true);
  SetUpAiModeButtonService();

  profile_.GetPrefs()->SetBoolean(
      omnibox_everywhere::prefs::kOmniboxEverywhereShowAiMode, true);
  profile_.GetPrefs()->SetBoolean(omnibox::kShowAiModeOmniboxButton, true);
  EXPECT_TRUE(controller_->IsCommandIdChecked(CommandId::kAlwaysShowAiMode));

  controller_->ExecuteCommand(CommandId::kAlwaysShowAiMode, 0);
  EXPECT_FALSE(profile_.GetPrefs()->GetBoolean(
      omnibox_everywhere::prefs::kOmniboxEverywhereShowAiMode));
  EXPECT_FALSE(controller_->IsCommandIdChecked(CommandId::kAlwaysShowAiMode));
  // Verify that the browser omnibox preference remains unaffected.
  EXPECT_TRUE(
      profile_.GetPrefs()->GetBoolean(omnibox::kShowAiModeOmniboxButton));

  controller_->ExecuteCommand(CommandId::kAlwaysShowAiMode, 0);
  EXPECT_TRUE(profile_.GetPrefs()->GetBoolean(
      omnibox_everywhere::prefs::kOmniboxEverywhereShowAiMode));
  EXPECT_TRUE(controller_->IsCommandIdChecked(CommandId::kAlwaysShowAiMode));
  EXPECT_TRUE(
      profile_.GetPrefs()->GetBoolean(omnibox::kShowAiModeOmniboxButton));
}

TEST_F(OmniboxEverywhereContextMenuControllerTest,
       ContextMenuShowShortcutsToggle) {
  profile_.GetPrefs()->SetInteger(
      omnibox_everywhere::prefs::kOmniboxEverywhereShowShortcuts,
      static_cast<int>(
          omnibox_everywhere::prefs::ShowShortcutsPrefValue::kEnabled));
  EXPECT_TRUE(controller_->IsCommandIdChecked(CommandId::kShowShortcuts));

  controller_->ExecuteCommand(CommandId::kShowShortcuts, 0);
  EXPECT_EQ(profile_.GetPrefs()->GetInteger(
                omnibox_everywhere::prefs::kOmniboxEverywhereShowShortcuts),
            static_cast<int>(
                omnibox_everywhere::prefs::ShowShortcutsPrefValue::kDisabled));
  EXPECT_FALSE(controller_->IsCommandIdChecked(CommandId::kShowShortcuts));

  controller_->ExecuteCommand(CommandId::kShowShortcuts, 0);
  EXPECT_EQ(profile_.GetPrefs()->GetInteger(
                omnibox_everywhere::prefs::kOmniboxEverywhereShowShortcuts),
            static_cast<int>(
                omnibox_everywhere::prefs::ShowShortcutsPrefValue::kEnabled));
  EXPECT_TRUE(controller_->IsCommandIdChecked(CommandId::kShowShortcuts));
}

TEST_F(OmniboxEverywhereContextMenuControllerTest,
       ContextMenuShowShortcutsHiddenWhenNoShortcutsAvailable) {
  content::ContextMenuParams params;
  params.is_editable = false;

  // By default, personal shortcuts are enabled, so the option should be
  // present.
  controller_->HandleContextMenu(params);
  const ui::SimpleMenuModel* model = controller_->model_for_testing();
  ASSERT_TRUE(model);
  EXPECT_TRUE(
      model->GetIndexOfCommandId(CommandId::kShowShortcuts).has_value());

  // Without enterprise policy shortcuts, the option is always present.
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpPersonalShortcutsVisible,
                                  false);
  controller_->HandleContextMenu(params);
  model = controller_->model_for_testing();
  ASSERT_TRUE(model);
  EXPECT_TRUE(
      model->GetIndexOfCommandId(CommandId::kShowShortcuts).has_value());

  // Add enterprise shortcuts policy so both checkboxes exist.
  {
    ScopedListPrefUpdate update(
        profile_.GetPrefs(), ntp_tiles::prefs::kEnterpriseShortcutsPolicyList);
    update->Append(base::DictValue()
                       .Set("url", "https://corp.example.com")
                       .Set("title", "Corp"));
  }

  // When both checkboxes are unchecked, no shortcuts can be shown at all.
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpEnterpriseShortcutsVisible,
                                  false);
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpPersonalShortcutsVisible,
                                  false);

  controller_->HandleContextMenu(params);
  model = controller_->model_for_testing();
  ASSERT_TRUE(model);
  EXPECT_FALSE(
      model->GetIndexOfCommandId(CommandId::kShowShortcuts).has_value());
}

}  // namespace omnibox_everywhere
