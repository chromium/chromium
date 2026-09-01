// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/action_app_menu.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/i18n/number_formatting.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_footer_view.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_manager.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_search_bar_view.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_test_base.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_zoom_view.h"
#include "chrome/browser/ui/views/app_menu/app_menu_footer_button.h"
#include "chrome/browser/ui/views/app_menu/block_menu_entry_button.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace {

class ActionAppMenuTest : public ActionAppMenuTestBase {
 public:
  ActionAppMenuTest() = default;
  ~ActionAppMenuTest() override = default;

  void SetUp() override {
    ActionAppMenuTestBase::SetUp();
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    button_ = widget_->SetContentsView(std::make_unique<views::MenuButton>(
        views::Button::PressedCallback(), u"Menu"));
  }

  void TearDown() override {
    button_ = nullptr;
    widget_.reset();
    ActionAppMenuTestBase::TearDown();
  }

 protected:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::MenuButton> button_ = nullptr;
};

TEST_F(ActionAppMenuTest, RunAndCloseMenu) {
  base::MockCallback<base::RepeatingClosure> on_menu_closed;

  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());

  EXPECT_FALSE(menu.IsShowing());

  menu.RunMenu(button_->button_controller());
  EXPECT_TRUE(menu.IsShowing());

  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  menu.CloseMenu();
  EXPECT_FALSE(menu.IsShowing());
}

TEST_F(ActionAppMenuTest, PopulatesSectionCardsWithStyling) {
  base::MockCallback<base::RepeatingClosure> on_menu_closed;

  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());

  menu.RunMenu(button_->button_controller());
  EXPECT_TRUE(menu.IsShowing());

  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);

  // Check if the menu contains section menu items.
  views::MenuItemView* password_item =
      root->GetMenuItemByID(kActionPasswordsAndAutofillSubmenu);
  ASSERT_TRUE(password_item);

  views::MenuItemView* print_item = root->GetMenuItemByID(kActionPrint);
  ASSERT_TRUE(print_item);

  // Check if the styling is applied to the menu items.
  EXPECT_TRUE(password_item->GetMenuItemBackground().has_value());
  EXPECT_TRUE(print_item->GetMenuItemBackground().has_value());

  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  menu.CloseMenu();
}

TEST_F(ActionAppMenuTest, PopulatesRecentTabsSubmenu) {
  base::MockCallback<base::RepeatingClosure> on_menu_closed;

  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());

  menu.RunMenu(button_->button_controller());
  EXPECT_TRUE(menu.IsShowing());

  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);

  views::MenuItemView* recent_tabs_item =
      root->GetMenuItemByID(kActionRecentTabsSubmenu);
  ASSERT_TRUE(recent_tabs_item);
  EXPECT_TRUE(recent_tabs_item->HasSubmenu());

  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  menu.CloseMenu();
}

TEST_F(ActionAppMenuTest, PopulatesStaticSubmenus) {
  base::MockCallback<base::RepeatingClosure> on_menu_closed;

  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());

  menu.RunMenu(button_->button_controller());
  EXPECT_TRUE(menu.IsShowing());

  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);

  // Passwords and autofill submenu.
  views::MenuItemView* password_item =
      root->GetMenuItemByID(kActionPasswordsAndAutofillSubmenu);
  ASSERT_TRUE(password_item);
  EXPECT_TRUE(password_item->HasSubmenu());
  EXPECT_NE(root->GetMenuItemByID(kActionShowPasswordManager), nullptr);
  EXPECT_NE(root->GetMenuItemByID(kActionShowPaymentMethods), nullptr);

  // Extensions submenu.
  views::MenuItemView* extensions_item =
      root->GetMenuItemByID(kActionExtensionsSubmenu);
  ASSERT_TRUE(extensions_item);
  EXPECT_TRUE(extensions_item->HasSubmenu());
  EXPECT_NE(root->GetMenuItemByID(kActionExtensionsSubmenuManageExtensions),
            nullptr);
  EXPECT_NE(root->GetMenuItemByID(kActionExtensionsSubmenuVisitChromeWebStore),
            nullptr);

  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  menu.CloseMenu();
}

TEST_F(ActionAppMenuTest, InflatesTopBlockRowButtons) {
  base::MockCallback<base::RepeatingClosure> on_menu_closed;

  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());

  menu.RunMenu(button_->button_controller());
  EXPECT_TRUE(menu.IsShowing());

  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);

  // The top MenuItemView in the submenu corresponds to the block container row.
  views::MenuItemView* block_item = root->GetSubmenu()->GetMenuItemAt(0);
  ASSERT_NE(block_item, nullptr);

  // Check that the container contains child block buttons.
  ASSERT_EQ(block_item->children().size(), 1u);
  views::View* row_view = block_item->children()[0];
  ASSERT_EQ(row_view->children().size(), 3u);

  // Verify text override is applied for incognito button.
  auto* incognito_button =
      views::AsViewClass<BlockMenuEntryButton>(row_view->children()[2]);
  ASSERT_TRUE(incognito_button);
  views::Label* incognito_label = nullptr;
  for (views::View* child : incognito_button->children()) {
    if (auto* label = views::AsViewClass<views::Label>(child)) {
      incognito_label = label;
      break;
    }
  }
  ASSERT_TRUE(incognito_label);
  EXPECT_EQ(incognito_label->GetText(),
            l10n_util::GetStringUTF16(IDS_INCOGNITO));

  // Verify icon override is applied for new tab button.
  auto* new_tab_button =
      views::AsViewClass<BlockMenuEntryButton>(row_view->children()[0]);
  ASSERT_TRUE(new_tab_button);
  views::ImageView* new_tab_icon = nullptr;
  for (views::View* child : new_tab_button->children()) {
    if (auto* icon = views::AsViewClass<views::ImageView>(child)) {
      new_tab_icon = icon;
      break;
    }
  }
  ASSERT_TRUE(new_tab_icon);
  EXPECT_FALSE(new_tab_icon->GetImageModel().IsEmpty());

  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  menu.CloseMenu();
}

TEST_F(ActionAppMenuTest, BlockActionsInvocation) {
  base::MockCallback<base::RepeatingClosure> on_menu_closed;
  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());

  // Find and invoke the New Tab action item. Verify its callback is triggered.
  actions::ActionItem* new_tab_action =
      actions::ActionManager::Get().FindAction(kActionNewTab);
  ASSERT_TRUE(new_tab_action);

  EXPECT_CALL(mock_action_invoked_, Call(kActionNewTab, testing::_, testing::_))
      .Times(1);
  new_tab_action->InvokeAction();
  testing::Mock::VerifyAndClearExpectations(&mock_action_invoked_);

  // Find and invoke the New Window action item. Verify its callback is
  // triggered.
  actions::ActionItem* new_window_action =
      actions::ActionManager::Get().FindAction(kActionNewWindow);
  ASSERT_TRUE(new_window_action);

  EXPECT_CALL(mock_action_invoked_,
              Call(kActionNewWindow, testing::_, testing::_))
      .Times(1);
  new_window_action->InvokeAction();
  testing::Mock::VerifyAndClearExpectations(&mock_action_invoked_);

  // Find and invoke the New Incognito Window action item. Verify its callback
  // is triggered.
  actions::ActionItem* incognito_action =
      actions::ActionManager::Get().FindAction(kActionNewIncognitoWindow);
  ASSERT_TRUE(incognito_action);

  EXPECT_CALL(mock_action_invoked_,
              Call(kActionNewIncognitoWindow, testing::_, testing::_))
      .Times(1);
  incognito_action->InvokeAction();
  testing::Mock::VerifyAndClearExpectations(&mock_action_invoked_);
}

// Tests that changing the enabled state of a delegate action item
// dynamically synchronizes and updates the BlockMenuEntryButton view state.
TEST_F(ActionAppMenuTest, BlockButtonSyncsEnabledStateWithActionItem) {
  base::MockCallback<base::RepeatingClosure> on_menu_closed;
  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());

  menu.RunMenu(button_->button_controller());
  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);

  views::MenuItemView* block_item = root->GetSubmenu()->GetMenuItemAt(0);
  ASSERT_NE(block_item, nullptr);
  views::View* row_view = block_item->children()[0];
  ASSERT_TRUE(row_view);

  auto* new_tab_button =
      views::AsViewClass<BlockMenuEntryButton>(row_view->children()[0]);
  ASSERT_TRUE(new_tab_button);
  EXPECT_TRUE(new_tab_button->GetEnabled());

  // Disable the delegate ActionItem.
  actions::ActionItem* new_tab_action =
      actions::ActionManager::Get().FindAction(kActionNewTab);
  ASSERT_TRUE(new_tab_action);
  new_tab_action->SetEnabled(false);

  EXPECT_FALSE(new_tab_button->GetEnabled());

  // Re-enable the delegate ActionItem.
  new_tab_action->SetEnabled(true);
  EXPECT_TRUE(new_tab_button->GetEnabled());

  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  menu.CloseMenu();
}

TEST_F(ActionAppMenuTest, ZoomMenuRowCreationAndChildren) {
  base::MockCallback<base::RepeatingClosure> on_menu_closed;

  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());
  menu.RunMenu(button_->button_controller());
  EXPECT_TRUE(menu.IsShowing());

  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);

  views::MenuItemView* zoom_item = root->GetMenuItemByID(kActionZoomSubmenu);
  ASSERT_TRUE(zoom_item);

  // The custom menu item should have a single child view which is
  // ActionAppMenuZoomView.
  ASSERT_EQ(zoom_item->children().size(), 1u);
  auto* zoom_view =
      views::AsViewClass<ActionAppMenuZoomView>(zoom_item->children()[0]);
  ASSERT_TRUE(zoom_view);

  // ActionAppMenuZoomView should contain the zoom minus button, zoom label,
  // zoom plus button, separator, and fullscreen button.
  EXPECT_GE(zoom_view->children().size(), 4u);

  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  menu.CloseMenu();
}

TEST_F(ActionAppMenuTest, ZoomChildActionsInvocation) {
  base::MockCallback<base::RepeatingClosure> on_menu_closed;

  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());
  menu.RunMenu(button_->button_controller());
  EXPECT_TRUE(menu.IsShowing());

  actions::ActionItem* minus_action =
      actions::ActionManager::Get().FindAction(kActionZoomMinus);
  ASSERT_TRUE(minus_action);

  EXPECT_CALL(mock_action_invoked_,
              Call(kActionZoomMinus, testing::_, testing::_))
      .Times(1);
  minus_action->InvokeAction();
  testing::Mock::VerifyAndClearExpectations(&mock_action_invoked_);

  actions::ActionItem* plus_action =
      actions::ActionManager::Get().FindAction(kActionZoomPlus);
  ASSERT_TRUE(plus_action);

  EXPECT_CALL(mock_action_invoked_,
              Call(kActionZoomPlus, testing::_, testing::_))
      .Times(1);
  plus_action->InvokeAction();
  testing::Mock::VerifyAndClearExpectations(&mock_action_invoked_);
}

TEST_F(ActionAppMenuTest, ColorTokensValidInDarkMode) {
  widget_->SetColorModeOverride(ui::ColorProviderKey::ColorMode::kDark);

  base::MockCallback<base::RepeatingClosure> on_menu_closed;
  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());

  menu.RunMenu(button_->button_controller());
  ASSERT_TRUE(menu.IsShowing());

  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->HasSubmenu());

  const ui::ColorProvider* color_provider =
      root->GetSubmenu()->GetColorProvider();
  ASSERT_TRUE(color_provider);

  views::MenuItemView* password_item =
      root->GetMenuItemByID(kActionPasswordsAndAutofillSubmenu);
  ASSERT_TRUE(password_item);
  ASSERT_TRUE(password_item->GetMenuItemBackground().has_value());
  EXPECT_NE(color_provider->GetColor(
                password_item->GetMenuItemBackground()->background_color_id),
            gfx::kPlaceholderColor);

  views::MenuItemView* print_item = root->GetMenuItemByID(kActionPrint);
  ASSERT_TRUE(print_item);
  ASSERT_TRUE(print_item->GetMenuItemBackground().has_value());
  EXPECT_NE(color_provider->GetColor(
                print_item->GetMenuItemBackground()->background_color_id),
            gfx::kPlaceholderColor);

  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  menu.CloseMenu();
}

TEST_F(ActionAppMenuTest, PopulatesFooterElements) {
  base::MockCallback<base::RepeatingClosure> on_menu_closed;

  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());
  menu.RunMenu(button_->button_controller());
  EXPECT_TRUE(menu.IsShowing());

  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);

  // The last MenuItemView in the submenu corresponds to the footer row.
  views::SubmenuView* submenu = root->GetSubmenu();
  ASSERT_TRUE(submenu);
  views::MenuItemView* footer_item =
      submenu->GetMenuItemAt(submenu->GetMenuItems().size() - 1);
  ASSERT_NE(footer_item, nullptr);

  // Check that the footer container is an ActionAppMenuFooterView containing
  // left container, spacer, and right container.
  ASSERT_EQ(footer_item->children().size(), 1u);
  views::View* footer_container = footer_item->children()[0];
  EXPECT_TRUE(views::IsViewClass<ActionAppMenuFooterView>(footer_container));
  ASSERT_EQ(footer_container->children().size(), 3u);

  views::View* left_container = footer_container->children()[0];
  ASSERT_EQ(left_container->children().size(), 2u);  // Settings, Help
  EXPECT_TRUE(
      views::IsViewClass<AppMenuFooterButton>(left_container->children()[0]));
  EXPECT_TRUE(
      views::IsViewClass<AppMenuFooterButton>(left_container->children()[1]));

  views::View* right_container = footer_container->children()[2];
  if (browser_defaults::kShowExitMenuItem) {
    ASSERT_EQ(right_container->children().size(), 1u);  // Exit
    EXPECT_TRUE(views::IsViewClass<AppMenuFooterButton>(
        right_container->children()[0]));
  } else {
    EXPECT_EQ(right_container->children().size(), 0u);
  }

  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  menu.CloseMenu();
}

TEST_F(ActionAppMenuTest, ZoomLabelUpdatesOnZoomChange) {
  content::RenderViewHostTestEnabler rvh_test_enabler;
  tabs::MockTabInterface mock_tab;
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile_.get(),
                                                        nullptr);
  zoom::ZoomController::CreateForWebContents(web_contents.get());
  auto* zoom_controller =
      zoom::ZoomController::FromWebContents(web_contents.get());
  ASSERT_TRUE(zoom_controller);

  EXPECT_CALL(mock_window_interface_, GetActiveTabInterface())
      .WillRepeatedly(testing::Return(&mock_tab));
  EXPECT_CALL(mock_tab, GetContents())
      .WillRepeatedly(testing::Return(web_contents.get()));

  ActionAppMenu menu(&mock_window_interface_, base::DoNothing());
  menu.RunMenu(button_->button_controller());
  EXPECT_TRUE(menu.IsShowing());

  views::MenuItemView* const root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);

  views::MenuItemView* zoom_item = root->GetMenuItemByID(kActionZoomSubmenu);
  ASSERT_TRUE(zoom_item);

  auto* zoom_view =
      views::AsViewClass<ActionAppMenuZoomView>(zoom_item->children()[0]);
  ASSERT_TRUE(zoom_view);

  views::Label* const zoom_label = zoom_view->zoom_label_for_testing();
  ASSERT_NE(zoom_label, nullptr);
  EXPECT_TRUE(zoom_label->GetVisible());
  EXPECT_EQ(zoom_label->GetText(), base::FormatPercent(100));

  // Change zoom level and verify that the label updates to print the new zoom
  // percent.
  zoom_controller->SetZoomLevel(2.0);
  EXPECT_EQ(zoom_label->GetText(),
            base::FormatPercent(zoom_controller->GetZoomPercent()));

  menu.CloseMenu();
}

TEST_F(ActionAppMenuTest, SearchBarDisabledByDefault) {
  base::MockCallback<base::RepeatingClosure> on_menu_closed;
  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());

  menu.RunMenu(button_->button_controller());
  ASSERT_TRUE(menu.IsShowing());

  EXPECT_EQ(menu.search_bar_for_testing(), nullptr);

  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->HasSubmenu());
  EXPECT_EQ(root->GetSubmenu()->GetInsets().top(), 16);

  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  menu.CloseMenu();
}

TEST_F(ActionAppMenuTest, SearchBarEnabledWithFeatureFlag) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kChroMenuSearch);

  base::MockCallback<base::RepeatingClosure> on_menu_closed;
  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());

  menu.RunMenu(button_->button_controller());
  ASSERT_TRUE(menu.IsShowing());

  ActionAppMenuSearchBarView* search_bar = menu.search_bar_for_testing();
  ASSERT_NE(search_bar, nullptr);

  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->HasSubmenu());

  // Check padding: 4px on top, 16px left and right on submenu.
  gfx::Insets insets = root->GetSubmenu()->GetInsets();
  EXPECT_EQ(insets.top(), 4);
  EXPECT_EQ(insets.left(), 16);
  EXPECT_EQ(insets.right(), 16);

  // Search bar is at index 0 of submenu.
  EXPECT_EQ(root->GetSubmenu()->children()[0], search_bar);

  // Check initial empty state.
  views::ImageView* icon = search_bar->search_icon_for_testing();
  ASSERT_NE(icon, nullptr);

  EXPECT_TRUE(search_bar->GetText().empty());
  EXPECT_EQ(search_bar->GetPlaceholderText(),
            l10n_util::GetStringUTF16(IDS_APP_MENU_SEARCH_PLACEHOLDER));
  EXPECT_EQ(search_bar->GetPlaceholderText(),
            u"Search menu, or type an action");
  EXPECT_EQ(search_bar->placeholder_text_color_id(),
            ui::kColorTextfieldForegroundPlaceholder);
  EXPECT_TRUE(icon->GetVisible());

  // Verify empty border with insets and background is transparent.
  ASSERT_NE(search_bar->GetBorder(), nullptr);
  const auto* provider = ChromeLayoutProvider::Get();
  int icon_size =
      provider->GetDistanceMetric(DISTANCE_ACTION_APP_MENU_ICON_SIZE);
  int icon_padding = 12;
  int icon_text_spacing =
      provider->GetDistanceMetric(DISTANCE_RELATED_CONTROL_HORIZONTAL_SMALL);
  int left_inset = icon_padding + icon_size + icon_text_spacing;
  EXPECT_EQ(search_bar->GetBorder()->GetInsets(),
            gfx::Insets::TLBR(6, left_inset, 6, 12));
  EXPECT_EQ(search_bar->GetBackgroundColor(), SK_ColorTRANSPARENT);

  // Verify inkdrop is enabled on the search bar.
  EXPECT_EQ(views::InkDrop::Get(search_bar)->GetMode(),
            views::InkDropHost::InkDropMode::ON);

  // Mouse press should focus the textfield and enable the cursor.
  EXPECT_TRUE(search_bar->OnMousePressed(ui::MouseEvent(
      ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
      base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON)));
  EXPECT_TRUE(search_bar->is_active_for_testing());
  EXPECT_TRUE(search_bar->GetCursorEnabled());

  // Type text: icon remains visible and text appears.
  ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_A, ui::EF_NONE);
  key_event.set_character('a');
  search_bar->HandleKeyEvent(&key_event);
  EXPECT_EQ(search_bar->GetText(), u"a");
  EXPECT_TRUE(icon->GetVisible());

  // Hitting Enter should do nothing and not trigger any action.
  ui::KeyEvent enter_event(ui::EventType::kKeyPressed, ui::VKEY_RETURN,
                           ui::EF_NONE);
  search_bar->HandleKeyEvent(&enter_event);
  EXPECT_EQ(search_bar->GetText(), u"a");

  // Hitting Backspace deletes the text.
  ui::KeyEvent backspace_event(ui::EventType::kKeyPressed, ui::VKEY_BACK,
                               ui::EF_NONE);
  search_bar->HandleKeyEvent(&backspace_event);
  EXPECT_EQ(search_bar->GetText(), u"");
  EXPECT_TRUE(icon->GetVisible());

  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  menu.CloseMenu();
}
}  // namespace
