// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/user_action_tester.h"
#include "build/build_config.h"
#include "chrome/browser/ui/toolbar/test_toolbar_action_view_model.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_button.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_entry_view.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_unittest.h"
#include "chrome/browser/ui/views/hover_button_controller.h"
#include "chrome/browser/ui/views/native_widget_factory.h"
#include "extensions/common/extension_features.h"
#include "ui/views/controls/styled_label.h"

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
  ExtensionsMenuButton* primary_button() { return primary_button_; }
  HoverButton* context_menu_button() { return context_menu_button_; }

  // ExtensionsToolbarUnitTest:
  void SetUp() override;
  void TearDown() override;

  const std::u16string initial_extension_name_;
  const std::u16string initial_tooltip_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<ExtensionsMenuButton, DanglingUntriaged> primary_button_ = nullptr;
  raw_ptr<HoverButton, DanglingUntriaged> pin_button_ = nullptr;
  raw_ptr<HoverButton, DanglingUntriaged> context_menu_button_ = nullptr;
  raw_ptr<TestToolbarActionViewModel, DanglingUntriaged> model_ = nullptr;

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestToolbarActionViewModel> action_model_holder_;
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

  auto model = std::make_unique<TestToolbarActionViewModel>("hello");
  model_ = model.get();
  model_->SetActionName(initial_extension_name_);
  model_->SetTooltip(initial_tooltip_);

  action_model_holder_ = std::move(model);

  std::unique_ptr<ExtensionsMenuEntryView> menu_item =
      std::make_unique<ExtensionsMenuEntryView>(
          browser(), /*is_enterprise=*/false, action_model_holder_.get(),
          /*site_access_toggle_callback*/ base::DoNothing(),
          /*site_permissions_button_callback=*/base::RepeatingClosure());
  primary_button_ = menu_item->primary_action_button_for_testing();
  context_menu_button_ = menu_item->context_menu_button_for_testing();

  widget_->SetContentsView(std::move(menu_item));
}

void ExtensionsMenuEntryViewTest::TearDown() {
  // All windows need to be closed before tear down.
  widget_.reset();

  ExtensionsToolbarUnitTest::TearDown();
}

TEST_F(ExtensionsMenuEntryViewTest, UpdatesToDisplayCorrectActionTitle) {
  EXPECT_EQ(primary_button()->label_text_for_testing(),
            initial_extension_name_);

  std::u16string extension_name = u"Extension Name";
  model_->SetActionName(extension_name);

  EXPECT_EQ(primary_button()->label_text_for_testing(), extension_name);
}

TEST_F(ExtensionsMenuEntryViewTest, UpdatesToDisplayTooltip) {
  EXPECT_EQ(primary_button()->GetRenderedTooltipText(gfx::Point()),
            initial_tooltip_);

  std::u16string tooltip = u"New Tooltip";
  model_->SetTooltip(tooltip);

  EXPECT_EQ(primary_button()->GetRenderedTooltipText(gfx::Point()), tooltip);
}

TEST_F(ExtensionsMenuEntryViewTest, ButtonMatchesEnabledStateOfExtension) {
  EXPECT_TRUE(primary_button()->GetEnabled());
  model_->SetEnabled(false);
  EXPECT_FALSE(primary_button()->GetEnabled());
  model_->SetEnabled(true);
  EXPECT_TRUE(primary_button()->GetEnabled());
}

TEST_F(ExtensionsMenuEntryViewTest, NotifyClickExecutesAction) {
  base::UserActionTester user_action_tester;
  constexpr char kActivatedUserAction[] =
      "Extensions.Toolbar.ExtensionActivatedFromMenu";
  EXPECT_EQ(0, model_->execute_action_count());
  EXPECT_EQ(0, user_action_tester.GetActionCount(kActivatedUserAction));

  primary_button()->SetBounds(0, 0, 100, 100);
  ClickButton(primary_button());

  EXPECT_EQ(1, model_->execute_action_count());
  EXPECT_EQ(1, user_action_tester.GetActionCount(kActivatedUserAction));
}

TEST_F(ExtensionsMenuEntryViewTest, ContextMenuButtonUserAction) {
  base::UserActionTester user_action_tester;
  constexpr char kContextMenuButtonUserAction[] =
      "Extensions.Toolbar.MoreActionsButtonPressedFromMenu";
  EXPECT_EQ(0, user_action_tester.GetActionCount(kContextMenuButtonUserAction));

  ClickButton(context_menu_button());

  EXPECT_EQ(1, user_action_tester.GetActionCount(kContextMenuButtonUserAction));
}
