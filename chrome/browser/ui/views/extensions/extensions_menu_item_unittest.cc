// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/toolbar/test_toolbar_action_view_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_button.h"
#include "chrome/browser/ui/views/hover_button_controller.h"
#include "chrome/browser/ui/views/native_widget_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "ui/events/event.h"
#include "ui/views/controls/styled_label.h"

class ExtensionsMenuItemViewTest : public BrowserWithTestWindowTest {
 protected:
  ExtensionsMenuItemViewTest()
      : initial_extension_name_(base::ASCIIToUTF16("Initial Extension Name")),
        initial_tooltip_(base::ASCIIToUTF16("Initial tooltip")) {}
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kExtensionsToolbarMenu);
    BrowserWithTestWindowTest::SetUp();

    widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams init_params(
        views::Widget::InitParams::TYPE_POPUP);
    init_params.ownership =
        views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
#if !defined(OS_CHROMEOS) && !defined(OS_MACOSX)
    // This was copied from BookmarkBarViewTest:
    // On Chrome OS, this always creates a NativeWidgetAura, but it should
    // create a DesktopNativeWidgetAura for Mash. We can get by without manually
    // creating it because AshTestViewsDelegate and MusClient will do the right
    // thing automatically.
    init_params.native_widget =
        CreateNativeWidget(NativeWidgetType::DESKTOP_NATIVE_WIDGET_AURA,
                           &init_params, widget_.get());
#endif
    widget_->Init(std::move(init_params));

    auto controller =
        std::make_unique<TestToolbarActionViewController>("hello");
    controller_ = controller.get();
    controller_->SetActionName(initial_extension_name_);
    controller_->SetTooltip(initial_tooltip_);
    menu_item_ = std::make_unique<ExtensionsMenuItemView>(
        browser(), std::move(controller));
    menu_item_->set_owned_by_client();

    widget_->SetContentsView(menu_item_.get());
  }

  void TearDown() override {
    // All windows need to be closed before tear down.
    widget_.reset();

    BrowserWithTestWindowTest::TearDown();
  }

  ExtensionsMenuButton* primary_button() {
    return menu_item_->primary_action_button_for_testing();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  const base::string16 initial_extension_name_;
  const base::string16 initial_tooltip_;
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<ExtensionsMenuItemView> menu_item_;
  TestToolbarActionViewController* controller_ = nullptr;
};

TEST_F(ExtensionsMenuItemViewTest, UpdatesToDisplayCorrectActionTitle) {
  EXPECT_EQ(primary_button()->label_text_for_testing(),
            initial_extension_name_);

  base::string16 extension_name = base::ASCIIToUTF16("Extension Name");
  controller_->SetActionName(extension_name);

  EXPECT_EQ(primary_button()->label_text_for_testing(), extension_name);
}

TEST_F(ExtensionsMenuItemViewTest, NotifyClickExecutesAction) {
  EXPECT_EQ(0, controller_->execute_action_count());

  primary_button()->SetBounds(0, 0, 100, 100);
  ui::MouseEvent click_event(ui::ET_MOUSE_RELEASED,
                             primary_button()->GetLocalBounds().CenterPoint(),
                             primary_button()->GetLocalBounds().CenterPoint(),
                             base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, 0);
  primary_button()->button_controller()->OnMouseReleased(click_event);

  EXPECT_EQ(1, controller_->execute_action_count());
}

TEST_F(ExtensionsMenuItemViewTest, UpdatesToDisplayTooltip) {
  EXPECT_EQ(primary_button()->GetTooltipText(gfx::Point()), initial_tooltip_);

  base::string16 tooltip = base::ASCIIToUTF16("New Tooltip");
  controller_->SetTooltip(tooltip);

  EXPECT_EQ(primary_button()->GetTooltipText(gfx::Point()), tooltip);
}

TEST_F(ExtensionsMenuItemViewTest, ButtonMatchesEnabledStateOfExtension) {
  EXPECT_TRUE(primary_button()->GetEnabled());
  controller_->SetEnabled(false);
  EXPECT_FALSE(primary_button()->GetEnabled());
  controller_->SetEnabled(true);
  EXPECT_TRUE(primary_button()->GetEnabled());
}
