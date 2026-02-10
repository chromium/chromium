// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/user_action_tester.h"
#include "build/build_config.h"
#include "chrome/browser/ui/toolbar/test_toolbar_action_view_model.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_entry_view.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_unittest.h"
#include "chrome/browser/ui/views/native_widget_factory.h"
#include "extensions/common/extension_features.h"

class ExtensionsMenuEntryViewTest : public ExtensionsToolbarUnitTest {
 public:
  ExtensionsMenuEntryViewTest()
      : initial_extension_name_(u"Initial Extension Name"),
        initial_tooltip_(u"Initial tooltip") {
    feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionsMenuAccessControl);
  }
  ExtensionsMenuEntryViewTest(const ExtensionsMenuEntryViewTest&) = delete;
  ExtensionsMenuEntryViewTest& operator=(const ExtensionsMenuEntryViewTest&) =
      delete;
  ~ExtensionsMenuEntryViewTest() override = default;

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

  // ExtensionsToolbarUnitTest:
  void SetUp() override;
  void TearDown() override;

  const std::u16string initial_extension_name_;
  const std::u16string initial_tooltip_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<ExtensionsMenuEntryView> menu_entry_ = nullptr;
  std::unique_ptr<TestToolbarActionViewModel> action_model_holder_;
  int action_callback_count_ = 0;

 private:
  base::test::ScopedFeatureList feature_list_;
};

void ExtensionsMenuEntryViewTest::SetUp() {
  ExtensionsToolbarUnitTest::SetUp();

  widget_ = std::make_unique<views::Widget>();
  views::Widget::InitParams init_params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_MAC)
  // This was copied from BookmarkBarViewTest:
  // On Chrome OS, this always creates a NativeWidgetAura, but it should
  // create a DesktopNativeWidgetAura for Mash. We can get by without manually
  // creating it because AshTestViewsDelegate and MusClient will do the right
  // thing automatically.
  init_params.native_widget = CreateNativeWidget(
      NativeWidgetType::kDesktopNativeWidgetAura, &init_params, widget_.get());
#endif
  widget_->Init(std::move(init_params));

  // The view still needs a valid pointer to a model during construction,
  // even if we update it later via MenuEntryState.
  action_model_holder_ = std::make_unique<TestToolbarActionViewModel>("hello");

  std::unique_ptr<ExtensionsMenuEntryView> menu_entry =
      std::make_unique<ExtensionsMenuEntryView>(
          browser(), /*is_enterprise=*/false, action_model_holder_.get(),
          /*action_toggle_callback=*/
          base::BindRepeating(
              [](ExtensionsMenuEntryViewTest* test) {
                test->action_callback_count_++;
              },
              base::Unretained(this)),
          /*site_access_toggle_callback*/ base::DoNothing(),
          /*site_permissions_button_callback=*/base::RepeatingClosure());

  // Initialize entry with initial state
  menu_entry->Update(GenerateState(initial_extension_name_, initial_tooltip_,
                                   /*is_enabled=*/true));

  menu_entry_ = menu_entry.get();

  widget_->SetContentsView(std::move(menu_entry));
}

void ExtensionsMenuEntryViewTest::TearDown() {
  menu_entry_ = nullptr;
  // All windows need to be closed before tear down.
  widget_.reset();

  ExtensionsToolbarUnitTest::TearDown();
}

ExtensionsMenuViewModel::MenuEntryState
ExtensionsMenuEntryViewTest::GenerateState(std::u16string name,
                                           std::u16string tooltip,
                                           bool is_enabled) {
  ExtensionsMenuViewModel::MenuEntryState state;

  // Set Action Button State
  state.action_button.text = name;
  state.action_button.tooltip_text = tooltip;
  state.action_button.status =
      is_enabled ? ExtensionsMenuViewModel::ControlState::Status::kEnabled
                 : ExtensionsMenuViewModel::ControlState::Status::kDisabled;

  // Set defaults for other controls so the view doesn't crash/DCHECK.
  state.context_menu_button.status =
      ExtensionsMenuViewModel::ControlState::Status::kEnabled;

  return state;
}

TEST_F(ExtensionsMenuEntryViewTest, UpdatesToDisplayCorrectActionText) {
  EXPECT_EQ(action_button()->GetText(), initial_extension_name_);

  std::u16string new_extension_name = u"Extension Name";
  std::u16string new_tooltip = u"New Tooltip";
  menu_entry_->Update(
      GenerateState(new_extension_name, new_tooltip, /*is_enabled=*/true));

  EXPECT_EQ(action_button()->GetText(), new_extension_name);
  EXPECT_EQ(action_button()->GetRenderedTooltipText(gfx::Point()), new_tooltip);
}

TEST_F(ExtensionsMenuEntryViewTest, ButtonMatchesEnabledStateOfExtension) {
  EXPECT_TRUE(action_button()->GetEnabled());

  menu_entry_->Update(GenerateState(initial_extension_name_, initial_tooltip_,
                                    /*is_enabled=*/false));
  EXPECT_FALSE(action_button()->GetEnabled());

  menu_entry_->Update(GenerateState(initial_extension_name_, initial_tooltip_,
                                    /*is_enabled=*/true));
  EXPECT_TRUE(action_button()->GetEnabled());
}

TEST_F(ExtensionsMenuEntryViewTest, NotifyClickExecutesAction) {
  base::UserActionTester user_action_tester;
  // We don't check the UserActionTester for "ExtensionActivatedFromMenu" here
  // because that metric is recorded by the ExtensionsMenuViewModel
  // when handling the callback, not by the View or the ActionViewModel (as
  // previously done).
  EXPECT_EQ(0, action_callback_count_);

  action_button()->SetBounds(0, 0, 100, 100);
  ClickButton(action_button());

  EXPECT_EQ(1, action_callback_count_);
}

TEST_F(ExtensionsMenuEntryViewTest, ContextMenuButtonUserAction) {
  base::UserActionTester user_action_tester;
  constexpr char kContextMenuButtonUserAction[] =
      "Extensions.Toolbar.MoreActionsButtonPressedFromMenu";
  EXPECT_EQ(0, user_action_tester.GetActionCount(kContextMenuButtonUserAction));

  ClickButton(context_menu_button());

  EXPECT_EQ(1, user_action_tester.GetActionCount(kContextMenuButtonUserAction));
}
