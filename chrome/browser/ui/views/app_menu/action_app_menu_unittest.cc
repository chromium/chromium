// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/action_app_menu.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/i18n/number_formatting.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/actions/chrome_action_properties.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_block_button.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_block_view.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_footer_button.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_footer_view.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_manager.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_search_bar_view.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_test_base.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_zoom_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/saved_tab_groups/test_support/fake_tab_group_sync_service.h"
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
#include "ui/views/test/button_test_api.h"
#include "ui/views/view_class_properties.h"
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
#if !BUILDFLAG(IS_CHROMEOS)
  views::MenuItemView* profile_item =
      root->GetMenuItemByID(kActionProfileSubmenu);
  ASSERT_TRUE(profile_item);
#endif

  views::MenuItemView* password_item =
      root->GetMenuItemByID(kActionPasswordsAndAutofillSubmenu);
  ASSERT_TRUE(password_item);

  views::MenuItemView* print_item = root->GetMenuItemByID(kActionPrint);
  ASSERT_TRUE(print_item);

  views::MenuItemView* downloads_item =
      root->GetMenuItemByID(kActionShowDownloads);
  ASSERT_TRUE(downloads_item);

  views::MenuItemView* clear_browsing_item =
      root->GetMenuItemByID(kActionClearBrowsingData);
  ASSERT_TRUE(clear_browsing_item);

  views::MenuItemView* zoom_item = root->GetMenuItemByID(kActionZoomSubmenu);
  ASSERT_TRUE(zoom_item);

  // Check if the styling is applied to the menu items.
#if !BUILDFLAG(IS_CHROMEOS)
  ASSERT_TRUE(profile_item->GetMenuItemBackground().has_value());
#endif
  ASSERT_TRUE(password_item->GetMenuItemBackground().has_value());
  ASSERT_TRUE(print_item->GetMenuItemBackground().has_value());
  ASSERT_TRUE(downloads_item->GetMenuItemBackground().has_value());
  ASSERT_TRUE(clear_browsing_item->GetMenuItemBackground().has_value());
  ASSERT_TRUE(zoom_item->GetMenuItemBackground().has_value());

  // Check horizontal margins on card backgrounds are explicitly 0.
#if !BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(profile_item->GetMenuItemBackground()->horizontal_margin, 0);
#endif
  EXPECT_EQ(password_item->GetMenuItemBackground()->horizontal_margin, 0);
  EXPECT_EQ(print_item->GetMenuItemBackground()->horizontal_margin, 0);
  EXPECT_EQ(downloads_item->GetMenuItemBackground()->horizontal_margin, 0);
  EXPECT_EQ(clear_browsing_item->GetMenuItemBackground()->horizontal_margin, 0);
  EXPECT_EQ(zoom_item->GetMenuItemBackground()->horizontal_margin, 0);

  // Check corner radiuses for section cards (first item has top radius, last
  // has bottom radius, middle items have neither).
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(password_item->GetMenuItemBackground()->top_radius, 8);
  EXPECT_EQ(password_item->GetMenuItemBackground()->bottom_radius, 0);
#else
  EXPECT_EQ(profile_item->GetMenuItemBackground()->top_radius, 8);
  EXPECT_EQ(profile_item->GetMenuItemBackground()->bottom_radius, 0);
  EXPECT_EQ(password_item->GetMenuItemBackground()->top_radius, 0);
  EXPECT_EQ(password_item->GetMenuItemBackground()->bottom_radius, 0);
#endif
  EXPECT_EQ(downloads_item->GetMenuItemBackground()->top_radius, 0);
  EXPECT_EQ(downloads_item->GetMenuItemBackground()->bottom_radius, 0);
  EXPECT_EQ(clear_browsing_item->GetMenuItemBackground()->top_radius, 0);
  EXPECT_EQ(clear_browsing_item->GetMenuItemBackground()->bottom_radius, 8);

  // Check vertical padding:
  // Standard items (32dp row height): (32 - 16) / 2 = 8dp.
#if !BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(profile_item->GetTopMargin(), 8);
#endif
  EXPECT_EQ(password_item->GetTopMargin(), 8);
  EXPECT_EQ(print_item->GetTopMargin(), 8);
  EXPECT_EQ(downloads_item->GetTopMargin(), 8);
  EXPECT_EQ(clear_browsing_item->GetTopMargin(), 8);

  // Expanded items (Zoom item, 48dp row height): (48 - 16) / 2 = 16dp.
  EXPECT_EQ(zoom_item->GetTopMargin(), 16);

  // Check hover selection color matching subtle state.
#if !BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(profile_item->GetSelectedColorId(),
            ui::kColorSysStateHoverOnSubtle);
#endif
  EXPECT_EQ(password_item->GetSelectedColorId(),
            ui::kColorSysStateHoverOnSubtle);
  EXPECT_EQ(print_item->GetSelectedColorId(), ui::kColorSysStateHoverOnSubtle);
  EXPECT_EQ(zoom_item->GetSelectedColorId(), ui::kColorSysStateHoverOnSubtle);

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

TEST_F(ActionAppMenuTest, PassesClickDisposition) {
  base::MockCallback<base::RepeatingClosure> on_menu_closed;
  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());

  menu.RunMenu(button_->button_controller());
  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);

  views::MenuItemView* print_item = root->GetMenuItemByID(kActionPrint);
  ASSERT_TRUE(print_item);

  EXPECT_CALL(mock_action_invoked_, Call(kActionPrint, testing::_, testing::_))
      .WillOnce([](actions::ActionId, actions::ActionItem*,
                   actions::ActionInvocationContext context) {
        EXPECT_EQ(context.GetProperty(chrome::kDispositionKey),
                  WindowOpenDisposition::CURRENT_TAB);
      });
  menu.ExecuteCommand(print_item->GetCommand(), ui::EF_NONE);

  EXPECT_CALL(mock_action_invoked_, Call(kActionPrint, testing::_, testing::_))
      .WillOnce([](actions::ActionId, actions::ActionItem*,
                   actions::ActionInvocationContext context) {
        EXPECT_EQ(context.GetProperty(chrome::kDispositionKey),
                  WindowOpenDisposition::NEW_BACKGROUND_TAB);
      });
  menu.ExecuteCommand(print_item->GetCommand(), ui::EF_MIDDLE_MOUSE_BUTTON);

  EXPECT_CALL(mock_action_invoked_, Call(kActionPrint, testing::_, testing::_))
      .WillOnce([](actions::ActionId, actions::ActionItem*,
                   actions::ActionInvocationContext context) {
        EXPECT_EQ(context.GetProperty(chrome::kDispositionKey),
                  WindowOpenDisposition::NEW_WINDOW);
      });
  menu.ExecuteCommand(print_item->GetCommand(), ui::EF_SHIFT_DOWN);

  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  menu.CloseMenu();
}

TEST_F(ActionAppMenuTest, PopulatesBookmarksSubmenu) {
  BookmarkModelFactory::GetInstance()->SetTestingFactory(
      profile_.get(), BookmarkModelFactory::GetDefaultFactory());
  BookmarkMergedSurfaceServiceFactory::GetInstance()->SetTestingFactory(
      profile_.get(), BookmarkMergedSurfaceServiceFactory::GetDefaultFactory());

  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile_.get());
  ASSERT_TRUE(bookmark_model);
  bookmark_model->LoadEmptyForTest();

  BookmarkMergedSurfaceService* bookmark_service =
      BookmarkMergedSurfaceServiceFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(bookmark_service);
  bookmark_service->LoadForTesting({});

  const bookmarks::BookmarkNode* bar_node = bookmark_model->bookmark_bar_node();
  bookmark_model->AddURL(bar_node, 0, u"Google",
                         GURL("https://www.google.com"));
  const bookmarks::BookmarkNode* folder =
      bookmark_model->AddFolder(bar_node, 1, u"Test Folder");
  bookmark_model->AddURL(folder, 0, u"Child Bookmark",
                         GURL("https://example.com"));
  bookmark_model->AddFolder(bar_node, 2, u"Empty Folder");

  const bookmarks::BookmarkNode* other_node = bookmark_model->other_node();
  bookmark_model->AddURL(other_node, 0, u"Other Bookmark",
                         GURL("https://other.com"));

  base::MockCallback<base::RepeatingClosure> on_menu_closed;
  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());

  menu.RunMenu(button_->button_controller());
  EXPECT_TRUE(menu.IsShowing());

  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);

  views::MenuItemView* bookmarks_item =
      root->GetMenuItemByID(kActionBookmarksSubmenu);
  ASSERT_TRUE(bookmarks_item);
  EXPECT_TRUE(bookmarks_item->HasSubmenu());

  // Verify static items like Bookmark This Tab and Bookmark Manager exist.
  EXPECT_NE(root->GetMenuItemByID(kActionBookmarkThisTab), nullptr);
  EXPECT_NE(root->GetMenuItemByID(kActionBookmarkAllTabs), nullptr);
  EXPECT_NE(root->GetMenuItemByID(kActionShowBookmarkManager), nullptr);

  // Verify dynamic bookmark items under the bookmarks submenu.
  views::SubmenuView* bookmarks_submenu = bookmarks_item->GetSubmenu();
  ASSERT_TRUE(bookmarks_submenu);

  views::MenuItemView* google_item = nullptr;
  views::MenuItemView* folder_item = nullptr;
  views::MenuItemView* empty_folder_item = nullptr;
  views::MenuItemView* other_folder_item = nullptr;
  for (views::MenuItemView* item : bookmarks_submenu->GetMenuItems()) {
    if (item->title() == u"Google") {
      google_item = item;
    } else if (item->title() == u"Test Folder") {
      folder_item = item;
    } else if (item->title() == u"Empty Folder") {
      empty_folder_item = item;
    } else if (item->title() == bookmark_model->other_node()->GetTitle()) {
      other_folder_item = item;
    }
  }

  // Top-level URL bookmark in bookmark bar.
  ASSERT_NE(google_item, nullptr);
  EXPECT_FALSE(google_item->HasSubmenu());

  // Folder containing a child bookmark.
  ASSERT_NE(folder_item, nullptr);
  EXPECT_TRUE(folder_item->HasSubmenu());
  views::MenuItemView* child_item = nullptr;
  for (views::MenuItemView* item : folder_item->GetSubmenu()->GetMenuItems()) {
    if (item->title() == u"Child Bookmark") {
      child_item = item;
      break;
    }
  }
  ASSERT_NE(child_item, nullptr);

  // Empty folder containing disabled "(empty)" placeholder item.
  ASSERT_NE(empty_folder_item, nullptr);
  EXPECT_TRUE(empty_folder_item->HasSubmenu());
  views::MenuItemView* empty_item = nullptr;
  for (views::MenuItemView* item :
       empty_folder_item->GetSubmenu()->GetMenuItems()) {
    if (item->title() == u"(empty)") {
      empty_item = item;
      break;
    }
  }
  ASSERT_NE(empty_item, nullptr);
  EXPECT_FALSE(empty_item->GetEnabled());

  // Other bookmarks folder containing child bookmark.
  ASSERT_NE(other_folder_item, nullptr);
  EXPECT_TRUE(other_folder_item->HasSubmenu());
  views::MenuItemView* other_child_item = nullptr;
  for (views::MenuItemView* item :
       other_folder_item->GetSubmenu()->GetMenuItems()) {
    if (item->title() == u"Other Bookmark") {
      other_child_item = item;
      break;
    }
  }
  ASSERT_NE(other_child_item, nullptr);

  // Verify click disposition for bookmarks.
  EXPECT_CALL(mock_window_interface_,
              OpenGURL(GURL("https://www.google.com"),
                       WindowOpenDisposition::CURRENT_TAB));
  menu.ExecuteCommand(google_item->GetCommand(), ui::EF_NONE);

  EXPECT_CALL(mock_window_interface_,
              OpenGURL(GURL("https://www.google.com"),
                       WindowOpenDisposition::NEW_BACKGROUND_TAB));
  menu.ExecuteCommand(google_item->GetCommand(), ui::EF_MIDDLE_MOUSE_BUTTON);

  EXPECT_CALL(mock_window_interface_,
              OpenGURL(GURL("https://www.google.com"),
                       WindowOpenDisposition::NEW_WINDOW));
  menu.ExecuteCommand(google_item->GetCommand(), ui::EF_SHIFT_DOWN);

  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  menu.CloseMenu();
}

TEST_F(ActionAppMenuTest, PopulatesTabGroupsSubmenu) {
  FaviconServiceFactory::GetInstance()->SetTestingFactory(
      profile_.get(), base::BindRepeating([](content::BrowserContext* context)
                                              -> std::unique_ptr<KeyedService> {
        return std::make_unique<
            testing::NiceMock<favicon::MockFaviconService>>();
      }));

  auto* sync_service = static_cast<tab_groups::FakeTabGroupSyncService*>(
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile_.get()));
  ASSERT_TRUE(sync_service);

  base::Uuid group_guid = base::Uuid::GenerateRandomV4();
  tab_groups::SavedTabGroupTab tab1(GURL("https://example.com/1"),
                                    u"Test Tab 1", group_guid,
                                    /*position=*/0);
  tab_groups::SavedTabGroupTab tab2(GURL("https://example.com/2"),
                                    u"Test Tab 2", group_guid,
                                    /*position=*/1);
  tab_groups::SavedTabGroup group(
      u"My Test Tab Group", tab_groups::TabGroupColorId::kBlue, {tab1, tab2},
      /*position=*/std::nullopt, group_guid);
  sync_service->AddGroup(std::move(group));

  base::MockCallback<base::RepeatingClosure> on_menu_closed;
  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());

  menu.RunMenu(button_->button_controller());
  EXPECT_TRUE(menu.IsShowing());

  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);

  views::MenuItemView* tab_groups_item =
      root->GetMenuItemByID(kActionSavedTabGroupsSubmenu);
  ASSERT_TRUE(tab_groups_item);
  EXPECT_TRUE(tab_groups_item->HasSubmenu());

  // Verify the static action item for creating a new group exists.
  views::MenuItemView* new_group_item =
      root->GetMenuItemByID(kActionCreateNewTabGroup);
  ASSERT_TRUE(new_group_item);

  // Find the dynamically populated tab group item by title.
  views::SubmenuView* tab_groups_submenu = tab_groups_item->GetSubmenu();
  ASSERT_TRUE(tab_groups_submenu);

  views::MenuItemView* group_item = nullptr;
  for (views::MenuItemView* item : tab_groups_submenu->GetMenuItems()) {
    if (item->title() == u"My Test Tab Group") {
      group_item = item;
      break;
    }
  }
  ASSERT_NE(group_item, nullptr);
  EXPECT_TRUE(group_item->HasSubmenu());

  // Verify group commands inside the group submenu.
  views::SubmenuView* group_submenu = group_item->GetSubmenu();
  ASSERT_TRUE(group_submenu);

  EXPECT_NE(root->GetMenuItemByID(kActionTabGroupOpenInBrowser), nullptr);
  EXPECT_NE(root->GetMenuItemByID(kActionTabGroupOpenInNewWindow), nullptr);
  EXPECT_NE(root->GetMenuItemByID(kActionTabGroupPin), nullptr);
  EXPECT_NE(root->GetMenuItemByID(kActionTabGroupDelete), nullptr);

  // Verify the saved tabs under the group item.
  views::MenuItemView* tab1_item = nullptr;
  views::MenuItemView* tab2_item = nullptr;
  for (views::MenuItemView* item : group_submenu->GetMenuItems()) {
    if (item->title() == u"Test Tab 1") {
      tab1_item = item;
    } else if (item->title() == u"Test Tab 2") {
      tab2_item = item;
    }
  }
  ASSERT_NE(tab1_item, nullptr);
  ASSERT_NE(tab2_item, nullptr);

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
  auto* block_section_view =
      views::AsViewClass<ActionAppMenuBlockView>(block_item->children()[0]);
  ASSERT_TRUE(block_section_view);
  ASSERT_EQ(block_section_view->children().size(), 3u);

  // Verify text override is applied for incognito button.
  auto* incognito_button = views::AsViewClass<ActionAppMenuBlockButton>(
      block_section_view->children()[2]);
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
  auto* new_tab_button = views::AsViewClass<ActionAppMenuBlockButton>(
      block_section_view->children()[0]);
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

TEST_F(ActionAppMenuTest, BlockButtonClickExecutesActionAfterMenuClosed) {
  testing::InSequence s;
  base::MockCallback<base::RepeatingClosure> on_menu_closed;

  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());
  menu.RunMenu(button_->button_controller());
  ASSERT_TRUE(menu.IsShowing());

  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);

  // The block section is in the top MenuItemView.
  views::MenuItemView* block_item = root->GetSubmenu()->GetMenuItemAt(0);
  ASSERT_NE(block_item, nullptr);
  auto* block_section_view =
      views::AsViewClass<ActionAppMenuBlockView>(block_item->children()[0]);
  ASSERT_TRUE(block_section_view);

  // The first child button in the block view is kActionNewTab.
  auto* new_tab_button = views::AsViewClass<ActionAppMenuBlockButton>(
      block_section_view->children()[0]);
  ASSERT_TRUE(new_tab_button);

  // Verify strict ordering:
  // 1. Menu must close and run on_menu_closed callback FIRST.
  // 2. Action must be executed SECOND.
  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  EXPECT_CALL(mock_action_invoked_, Call(kActionNewTab, testing::_, testing::_))
      .Times(1);

  // Simulate button click on the block button.
  views::test::ButtonTestApi(new_tab_button).NotifyDefaultMouseClick();

  EXPECT_FALSE(menu.IsShowing());
}

TEST_F(ActionAppMenuTest, FooterButtonClickExecutesActionAfterMenuClosed) {
  testing::InSequence s;
  base::MockCallback<base::RepeatingClosure> on_menu_closed;

  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());
  menu.RunMenu(button_->button_controller());
  ASSERT_TRUE(menu.IsShowing());

  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);

  // The footer row is the last item in the submenu.
  views::SubmenuView* submenu = root->GetSubmenu();
  ASSERT_TRUE(submenu);
  views::MenuItemView* footer_item =
      submenu->GetMenuItemAt(submenu->GetMenuItems().size() - 1);
  ASSERT_NE(footer_item, nullptr);

  auto* footer_view =
      views::AsViewClass<ActionAppMenuFooterView>(footer_item->children()[0]);
  ASSERT_TRUE(footer_view);

  // Left container child 0 is kActionOptions (Settings).
  views::View* left_container = footer_view->children()[0];
  auto* settings_button = views::AsViewClass<ActionAppMenuFooterButton>(
      left_container->children()[0]);
  ASSERT_TRUE(settings_button);

  // Verify strict ordering:
  // 1. Menu must close and run on_menu_closed callback FIRST.
  // 2. Action must be executed SECOND.
  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  EXPECT_CALL(mock_action_invoked_,
              Call(kActionOptions, testing::_, testing::_))
      .Times(1);

  // Simulate button click on the footer button.
  views::test::ButtonTestApi(settings_button).NotifyDefaultMouseClick();

  EXPECT_FALSE(menu.IsShowing());
}

// Tests that changing the enabled state of a delegate action item
// dynamically synchronizes and updates the ActionAppMenuBlockButton view
// state.
TEST_F(ActionAppMenuTest, BlockButtonSyncsEnabledStateWithActionItem) {
  base::MockCallback<base::RepeatingClosure> on_menu_closed;
  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());

  menu.RunMenu(button_->button_controller());
  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);

  views::MenuItemView* block_item = root->GetSubmenu()->GetMenuItemAt(0);
  ASSERT_NE(block_item, nullptr);
  auto* block_section_view =
      views::AsViewClass<ActionAppMenuBlockView>(block_item->children()[0]);
  ASSERT_TRUE(block_section_view);

  auto* new_tab_button = views::AsViewClass<ActionAppMenuBlockButton>(
      block_section_view->children()[0]);
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
  EXPECT_TRUE(views::IsViewClass<ActionAppMenuFooterButton>(
      left_container->children()[0]));
  EXPECT_TRUE(views::IsViewClass<ActionAppMenuFooterButton>(
      left_container->children()[1]));

  views::View* right_container = footer_container->children()[2];
  if (browser_defaults::kShowExitMenuItem) {
    ASSERT_EQ(right_container->children().size(), 1u);  // Exit
    EXPECT_TRUE(views::IsViewClass<ActionAppMenuFooterButton>(
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
  EXPECT_EQ(root->GetSubmenu()->GetInsets(),
            ChromeLayoutProvider::Get()->GetInsetsMetric(
                INSETS_ACTION_APP_MENU_POPUP));

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

  const auto* provider = ChromeLayoutProvider::Get();

  // Check padding on submenu matches popup insets metric.
  EXPECT_EQ(root->GetSubmenu()->GetInsets(),
            provider->GetInsetsMetric(INSETS_ACTION_APP_MENU_POPUP));
  EXPECT_EQ(root->GetSubmenu()->GetInsets(), gfx::Insets::VH(4, 16));

  // Search bar is hosted inside the first MenuItemView of submenu.
  views::MenuItemView* search_item = root->GetSubmenu()->GetMenuItemAt(0);
  ASSERT_NE(search_item, nullptr);
  ASSERT_EQ(search_item->children().size(), 1u);
  EXPECT_EQ(search_item->children()[0], search_bar);
  EXPECT_EQ(search_item->GetTopMargin(), 0);

  const gfx::Insets* search_bar_margins =
      search_bar->GetProperty(views::kMarginsKey);
  ASSERT_TRUE(search_bar_margins);
  EXPECT_EQ(*search_bar_margins, provider->GetInsetsMetric(
                                     INSETS_ACTION_APP_MENU_SEARCH_BAR_MARGIN));
  EXPECT_EQ(*search_bar_margins, gfx::Insets::TLBR(0, 0, 16, 0));

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

  // The search bar is active and focused initially when added to the menu.
  EXPECT_TRUE(search_bar->is_active_for_testing());
  EXPECT_TRUE(search_bar->GetCursorEnabled());

  // Pressing Down arrow deactivates search bar focus for menu navigation.
  ui::KeyEvent down_key(ui::EventType::kKeyPressed, ui::VKEY_DOWN, ui::EF_NONE);
  search_bar->HandleKeyEvent(&down_key);
  EXPECT_FALSE(search_bar->is_active_for_testing());

  // Mouse press re-focuses the textfield and enables the cursor.
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

TEST_F(ActionAppMenuTest, PopupAndComponentLayoutInsets) {
  base::MockCallback<base::RepeatingClosure> on_menu_closed;

  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());

  menu.RunMenu(button_->button_controller());
  EXPECT_TRUE(menu.IsShowing());

  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);
  views::SubmenuView* submenu = root->GetSubmenu();
  ASSERT_TRUE(submenu);

  const auto* provider = ChromeLayoutProvider::Get();

  // Outer popup insets.
  EXPECT_EQ(submenu->GetInsets(),
            provider->GetInsetsMetric(INSETS_ACTION_APP_MENU_POPUP));
  EXPECT_EQ(submenu->GetInsets(), gfx::Insets::VH(4, 16));

  // The block section item has 0 vertical margin, and the block view has 0
  // horizontal insets.
  views::MenuItemView* block_item = submenu->GetMenuItemAt(0);
  ASSERT_NE(block_item, nullptr);
  EXPECT_EQ(block_item->GetTopMargin(), 0);
  ASSERT_EQ(block_item->children().size(), 1u);
  auto* block_view =
      views::AsViewClass<ActionAppMenuBlockView>(block_item->children()[0]);
  ASSERT_TRUE(block_view);
  EXPECT_EQ(block_view->GetInsideBorderInsets(),
            provider->GetInsetsMetric(INSETS_ACTION_APP_MENU_BLOCK_ROW));
  EXPECT_EQ(block_view->GetInsideBorderInsets(), gfx::Insets::TLBR(0, 0, 8, 0));

  // The footer item has 0 vertical margin, and the footer view has 0 horizontal
  // insets.
  views::MenuItemView* footer_item =
      submenu->GetMenuItemAt(submenu->GetMenuItems().size() - 1);
  ASSERT_NE(footer_item, nullptr);
  EXPECT_EQ(footer_item->GetTopMargin(), 0);
  ASSERT_EQ(footer_item->children().size(), 1u);
  auto* footer_view =
      views::AsViewClass<ActionAppMenuFooterView>(footer_item->children()[0]);
  ASSERT_TRUE(footer_view);
  EXPECT_EQ(footer_view->GetInsets(),
            provider->GetInsetsMetric(INSETS_ACTION_APP_MENU_FOOTER));
  EXPECT_EQ(footer_view->GetInsets(), gfx::Insets::VH(0, 0));
  const gfx::Insets* footer_margins =
      footer_view->GetProperty(views::kMarginsKey);
  ASSERT_TRUE(footer_margins);
  EXPECT_EQ(*footer_margins,
            provider->GetInsetsMetric(INSETS_ACTION_APP_MENU_FOOTER_MARGIN));
  EXPECT_EQ(*footer_margins, gfx::Insets::TLBR(8, 0, 0, 0));

  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  menu.CloseMenu();
}

TEST_F(ActionAppMenuTest, HeaderAndMenuItemBorderLayout) {
  FaviconServiceFactory::GetInstance()->SetTestingFactory(
      profile_.get(), base::BindRepeating([](content::BrowserContext* context)
                                              -> std::unique_ptr<KeyedService> {
        return std::make_unique<
            testing::NiceMock<favicon::MockFaviconService>>();
      }));

  auto* sync_service = static_cast<tab_groups::FakeTabGroupSyncService*>(
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile_.get()));
  ASSERT_TRUE(sync_service);

  base::Uuid group_guid = base::Uuid::GenerateRandomV4();
  tab_groups::SavedTabGroupTab tab(GURL("https://example.com/1"), u"Test Tab 1",
                                   group_guid,
                                   /*position=*/0);
  tab_groups::SavedTabGroup group(u"My Test Tab Group",
                                  tab_groups::TabGroupColorId::kBlue, {tab},
                                  /*position=*/std::nullopt, group_guid);
  sync_service->AddGroup(std::move(group));

  base::MockCallback<base::RepeatingClosure> on_menu_closed;
  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());

  menu.RunMenu(button_->button_controller());
  EXPECT_TRUE(menu.IsShowing());

  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);
  views::SubmenuView* submenu = root->GetSubmenu();
  ASSERT_TRUE(submenu);

  const auto* provider = ChromeLayoutProvider::Get();

  // 1. Root menu item border and content start.
  ASSERT_TRUE(root->GetBorder());
  EXPECT_EQ(root->GetInsets(),
            provider->GetInsetsMetric(INSETS_ACTION_APP_MENU_ITEM));
  EXPECT_EQ(root->GetInsets(), gfx::Insets::TLBR(0, 16, 0, 12));
  EXPECT_EQ(root->GetContentStart(), 16);

  // 2. Regular card menu items border and content start.
  views::MenuItemView* password_item =
      root->GetMenuItemByID(kActionPasswordsAndAutofillSubmenu);
  ASSERT_TRUE(password_item);
  ASSERT_TRUE(password_item->GetBorder());
  EXPECT_EQ(password_item->GetInsets(),
            provider->GetInsetsMetric(INSETS_ACTION_APP_MENU_ITEM));
  EXPECT_EQ(password_item->GetContentStart(), 16);

  views::MenuItemView* downloads_item =
      root->GetMenuItemByID(kActionShowDownloads);
  ASSERT_TRUE(downloads_item);
  ASSERT_TRUE(downloads_item->GetBorder());
  EXPECT_EQ(downloads_item->GetInsets(),
            provider->GetInsetsMetric(INSETS_ACTION_APP_MENU_ITEM));
  EXPECT_EQ(downloads_item->GetContentStart(), 16);

  // 3. Headers directly under root.
  std::vector<views::MenuItemView*> root_titles;
  for (views::MenuItemView* item : submenu->GetMenuItems()) {
    if (item->GetType() == views::MenuItemView::Type::kTitle) {
      root_titles.push_back(item);
    }
  }
  ASSERT_GE(root_titles.size(), 2u);

  // First header ("Your Chrome"):
  // - Starts flush with the card (0dp insets, content start 0)
  // - Standard top margin (8dp).
  EXPECT_EQ(root_titles[0]->GetParentMenuItem(), root);
  ASSERT_TRUE(root_titles[0]->GetBorder());
  EXPECT_EQ(root_titles[0]->GetInsets(), gfx::Insets());
  EXPECT_EQ(root_titles[0]->GetContentStart(), 0);
  EXPECT_EQ(root_titles[0]->GetTopMargin(), 8);

  // Second header ("Tools and Actions"):
  // - Starts flush with the card (0dp insets, content start 0)
  // - Doubled top margin (16dp).
  EXPECT_EQ(root_titles[1]->GetParentMenuItem(), root);
  ASSERT_TRUE(root_titles[1]->GetBorder());
  EXPECT_EQ(root_titles[1]->GetInsets(), gfx::Insets());
  EXPECT_EQ(root_titles[1]->GetContentStart(), 0);
  EXPECT_EQ(root_titles[1]->GetTopMargin(), 16);

  // 4. Headers in submenus (not under root).
  views::MenuItemView* tab_groups_item =
      root->GetMenuItemByID(kActionSavedTabGroupsSubmenu);
  ASSERT_TRUE(tab_groups_item);
  views::SubmenuView* tab_groups_submenu = tab_groups_item->GetSubmenu();
  ASSERT_TRUE(tab_groups_submenu);

  views::MenuItemView* group_item = nullptr;
  for (views::MenuItemView* item : tab_groups_submenu->GetMenuItems()) {
    if (item->title() == u"My Test Tab Group") {
      group_item = item;
      break;
    }
  }
  ASSERT_NE(group_item, nullptr);
  views::SubmenuView* group_submenu = group_item->GetSubmenu();
  ASSERT_TRUE(group_submenu);

  views::MenuItemView* submenu_header = nullptr;
  for (views::MenuItemView* item : group_submenu->GetMenuItems()) {
    if (item->GetType() == views::MenuItemView::Type::kTitle) {
      submenu_header = item;
      break;
    }
  }
  ASSERT_NE(submenu_header, nullptr);
  EXPECT_NE(submenu_header->GetParentMenuItem(), root);
  EXPECT_EQ(submenu_header->GetParentMenuItem(), group_item);
  // Submenu headers do NOT receive the empty border or doubled top margin.
  EXPECT_EQ(submenu_header->GetBorder(), nullptr);
  EXPECT_NE(submenu_header->GetContentStart(), 0);
  EXPECT_EQ(submenu_header->GetTopMargin(), 8);

  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  menu.CloseMenu();
}
}  // namespace
