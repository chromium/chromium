// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/user_action_tester.h"
#include "build/build_config.h"
#include "chrome/browser/ui/toolbar/test_toolbar_action_view_model.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_entry_view.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_browsertest.h"
#include "chrome/browser/ui/views/native_widget_factory.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension_features.h"

class ExtensionsMenuEntryViewBrowserTest : public ExtensionsToolbarBrowserTest {
 public:
  ExtensionsMenuEntryViewBrowserTest()
      : initial_extension_name_(u"Initial Extension Name"),
        initial_tooltip_(u"Initial tooltip") {
    feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionsMenuAccessControl);
  }
  ExtensionsMenuEntryViewBrowserTest(
      const ExtensionsMenuEntryViewBrowserTest&) = delete;
  ExtensionsMenuEntryViewBrowserTest& operator=(
      const ExtensionsMenuEntryViewBrowserTest&) = delete;
  ~ExtensionsMenuEntryViewBrowserTest() override = default;

 protected:
  // Helper to generate menu entry state with customizable action button
  // properties.
  ExtensionsMenuViewModel::MenuEntryState GenerateState(std::u16string name,
                                                        std::u16string tooltip,
                                                        bool is_enabled);

  HoverButton* action_button() {
    return menu_entry_->action_button_for_testing();
  }
  HoverButton* context_menu_button() {
    return menu_entry_->context_menu_button_for_testing();
  }

  // ExtensionsToolbarBrowserTest:
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  const std::u16string initial_extension_name_;
  const std::u16string initial_tooltip_;
  std::unique_ptr<TestToolbarActionViewModel> action_model_holder_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<ExtensionsMenuEntryView> menu_entry_ = nullptr;
  int action_callback_count_ = 0;

 private:
  base::test::ScopedFeatureList feature_list_;
};

void ExtensionsMenuEntryViewBrowserTest::SetUpOnMainThread() {
  ExtensionsToolbarBrowserTest::SetUpOnMainThread();

  widget_ = std::make_unique<views::Widget>();
  views::Widget::InitParams init_params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_MAC)
  init_params.native_widget = CreateNativeWidget(
      NativeWidgetType::kDesktopNativeWidgetAura, &init_params, widget_.get());
#endif
  widget_->Init(std::move(init_params));

  action_model_holder_ = std::make_unique<TestToolbarActionViewModel>("hello");

  std::unique_ptr<ExtensionsMenuEntryView> menu_entry =
      std::make_unique<ExtensionsMenuEntryView>(
          browser(), /*is_enterprise=*/false, action_model_holder_.get(),
          /*action_toggle_callback=*/
          base::BindRepeating(
              [](ExtensionsMenuEntryViewBrowserTest* test) {
                test->action_callback_count_++;
              },
              base::Unretained(this)),
          /*site_access_toggle_callback=*/base::DoNothing(),
          /*site_permissions_button_callback=*/base::RepeatingClosure());

  menu_entry->Update(GenerateState(initial_extension_name_, initial_tooltip_,
                                   /*is_enabled=*/true));

  menu_entry_ = menu_entry.get();
  widget_->SetContentsView(std::move(menu_entry));
}

void ExtensionsMenuEntryViewBrowserTest::TearDownOnMainThread() {
  menu_entry_ = nullptr;
  widget_.reset();
  action_model_holder_.reset();

  ExtensionsToolbarBrowserTest::TearDownOnMainThread();
}

ExtensionsMenuViewModel::MenuEntryState
ExtensionsMenuEntryViewBrowserTest::GenerateState(std::u16string name,
                                                  std::u16string tooltip,
                                                  bool is_enabled) {
  ExtensionsMenuViewModel::MenuEntryState state;

  state.action_button.text = name;
  state.action_button.tooltip_text = tooltip;
  state.action_button.status =
      is_enabled ? ExtensionsMenuViewModel::ControlState::Status::kEnabled
                 : ExtensionsMenuViewModel::ControlState::Status::kDisabled;

  state.context_menu_button.status =
      ExtensionsMenuViewModel::ControlState::Status::kEnabled;

  return state;
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuEntryViewBrowserTest,
                       UpdatesToDisplayCorrectActionText) {
  EXPECT_EQ(action_button()->GetText(), initial_extension_name_);

  std::u16string new_extension_name = u"Extension Name";
  std::u16string new_tooltip = u"New Tooltip";
  menu_entry_->Update(
      GenerateState(new_extension_name, new_tooltip, /*is_enabled=*/true));

  EXPECT_EQ(action_button()->GetText(), new_extension_name);
  EXPECT_EQ(action_button()->GetRenderedTooltipText(gfx::Point()), new_tooltip);
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuEntryViewBrowserTest,
                       AccessibilityStateForDisabledExtension) {
  menu_entry_->Update(GenerateState(initial_extension_name_, initial_tooltip_,
                                    /*is_enabled=*/false));
  EXPECT_TRUE(action_button()->GetEnabled());
  EXPECT_FALSE(action_button()->GetViewAccessibility().GetIsEnabled());

  menu_entry_->Update(GenerateState(initial_extension_name_, initial_tooltip_,
                                    /*is_enabled=*/true));
  EXPECT_TRUE(action_button()->GetEnabled());
  EXPECT_TRUE(action_button()->GetViewAccessibility().GetIsEnabled());
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuEntryViewBrowserTest,
                       ButtonStateMatchesEnabledStateOfExtension) {
  EXPECT_EQ(action_button()->GetState(), views::Button::STATE_NORMAL);

  menu_entry_->Update(GenerateState(initial_extension_name_, initial_tooltip_,
                                    /*is_enabled=*/false));
  EXPECT_EQ(action_button()->GetState(), views::Button::STATE_DISABLED);

  menu_entry_->Update(GenerateState(initial_extension_name_, initial_tooltip_,
                                    /*is_enabled=*/true));
  EXPECT_EQ(action_button()->GetState(), views::Button::STATE_NORMAL);
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuEntryViewBrowserTest,
                       NotifyClickExecutesAction) {
  EXPECT_EQ(0, action_callback_count_);

  action_button()->SetBounds(0, 0, 100, 100);
  ClickButton(action_button());

  EXPECT_EQ(1, action_callback_count_);
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuEntryViewBrowserTest,
                       ContextMenuButtonUserAction) {
  base::UserActionTester user_action_tester;
  constexpr char kContextMenuButtonUserAction[] =
      "Extensions.Toolbar.MoreActionsButtonPressedFromMenu";
  EXPECT_EQ(0, user_action_tester.GetActionCount(kContextMenuButtonUserAction));

  ClickButton(context_menu_button());

  EXPECT_EQ(1, user_action_tester.GetActionCount(kContextMenuButtonUserAction));
}
