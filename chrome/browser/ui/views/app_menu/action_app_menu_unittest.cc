// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/action_app_menu.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/i18n/number_formatting.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/actions/chrome_action_properties.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/safety_hub/menu_notification_service.h"
#include "chrome/browser/ui/safety_hub/menu_notification_service_factory.h"
#include "chrome/browser/ui/safety_hub/safe_browsing_result.h"
#include "chrome/browser/ui/safety_hub/safety_hub_util.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_manager.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_test_base.h"
#include "chrome/browser/ui/views/app_menu/app_menu_action_item.h"
#include "chrome/browser/ui/views/app_menu/app_menu_block_button.h"
#include "chrome/browser/ui/views/app_menu/app_menu_block_view.h"
#include "chrome/browser/ui/views/app_menu/app_menu_chip_view.h"
#include "chrome/browser/ui/views/app_menu/app_menu_footer_button.h"
#include "chrome/browser/ui/views/app_menu/app_menu_footer_view.h"
#include "chrome/browser/ui/views/app_menu/app_menu_minor_text_view.h"
#include "chrome/browser/ui/views/app_menu/app_menu_search_bar_view.h"
#include "chrome/browser/ui/views/app_menu/app_menu_zoom_view.h"
#include "chrome/browser/ui/views/app_menu/recent_tabs_dynamic_menu.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/user_education/mock_browser_user_education_interface.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/common/bookmark_bar_visibility_state.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/prefs/pref_service.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/saved_tab_groups/test_support/fake_tab_group_sync_service.h"
#include "components/search/ntp_features.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/user_education/common/tutorial/tutorial_description.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/accessibility/ax_update_notifier.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_separator.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#endif

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
      root->GetMenuItemByID(kActionShowDownloadsPage);
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

  // Check horizontal margins on card backgrounds match container margin.
  const int container_margin = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_ACTION_APP_MENU_CONTAINER_MARGIN);
#if !BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(profile_item->GetMenuItemBackground()->horizontal_margin,
            container_margin);
#endif
  EXPECT_EQ(password_item->GetMenuItemBackground()->horizontal_margin,
            container_margin);
  EXPECT_EQ(print_item->GetMenuItemBackground()->horizontal_margin,
            container_margin);
  EXPECT_EQ(downloads_item->GetMenuItemBackground()->horizontal_margin,
            container_margin);
  EXPECT_EQ(clear_browsing_item->GetMenuItemBackground()->horizontal_margin,
            container_margin);
  EXPECT_EQ(zoom_item->GetMenuItemBackground()->horizontal_margin,
            container_margin);

  // Check corner radiuses for section cards (first item has top radius, last
  // has bottom radius, middle items have neither).
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(password_item->GetMenuItemBackground()->top_radius, 12);
  EXPECT_EQ(password_item->GetMenuItemBackground()->bottom_radius, 0);
#else
  EXPECT_EQ(profile_item->GetMenuItemBackground()->top_radius, 12);
  EXPECT_EQ(profile_item->GetMenuItemBackground()->bottom_radius, 0);
  EXPECT_EQ(password_item->GetMenuItemBackground()->top_radius, 0);
  EXPECT_EQ(password_item->GetMenuItemBackground()->bottom_radius, 0);
#endif
  EXPECT_EQ(downloads_item->GetMenuItemBackground()->top_radius, 0);
  EXPECT_EQ(downloads_item->GetMenuItemBackground()->bottom_radius, 0);
  EXPECT_EQ(clear_browsing_item->GetMenuItemBackground()->top_radius, 0);
  EXPECT_EQ(clear_browsing_item->GetMenuItemBackground()->bottom_radius, 12);

  // Standard items (32dp row height): (32 - 16) / 2 = 8dp.
#if !BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(profile_item->GetTopMargin(), 12);
#endif
  // Standard items (32dp row height): (32 - 16) / 2 = 8dp.
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
  EXPECT_EQ(menu.GetLabelFontList(profile_item->GetCommand()),
            &views::TypographyProvider::Get().GetFont(
                views::style::CONTEXT_MENU, views::style::STYLE_BODY_3_MEDIUM));
  const int expected_avatar_size =
      GetLayoutConstant(LayoutConstant::kAppMenuProfileRowAvatarIconSize);
  EXPECT_EQ(profile_item->GetIcon().Size(),
            gfx::Size(expected_avatar_size, expected_avatar_size));

  EXPECT_TRUE(std::ranges::any_of(profile_item->children(),
                                  &views::IsViewClass<AppMenuChipView>));

  views::SubmenuView* root_submenu = root->GetSubmenu();
  ASSERT_NE(root_submenu, nullptr);
  auto profile_it = std::ranges::find(root_submenu->children(), profile_item);
  ASSERT_NE(profile_it, root_submenu->children().end());
  ASSERT_NE(std::next(profile_it), root_submenu->children().end());
  views::MenuSeparator* item_separator =
      views::AsViewClass<views::MenuSeparator>(*std::next(profile_it));
  ASSERT_NE(item_separator, nullptr);
  EXPECT_EQ(item_separator->GetType(),
            ui::MenuSeparatorType::MENU_ITEM_SEPARATOR);
  EXPECT_EQ(item_separator->GetColorId(), ui::kColorMenuBackground);
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

  // Trigger lazy loading of the dynamic bookmarks submenu.
  menu.WillShowMenu(bookmarks_item);

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
    if (item->title() == l10n_util::GetStringUTF16(IDS_MENU_EMPTY_SUBMENU)) {
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

TEST_F(ActionAppMenuTest, PopulatesBookmarksSubmenuWithManagedFolder) {
  BookmarkModelFactory::GetInstance()->SetTestingFactory(
      profile_.get(), BookmarkModelFactory::GetDefaultFactory());
  ManagedBookmarkServiceFactory::GetInstance()->SetTestingFactory(
      profile_.get(), ManagedBookmarkServiceFactory::GetDefaultFactory());
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

  base::DictValue dict;
  dict.Set("name", "Managed Link");
  dict.Set("url", "https://managed.example.com");
  base::ListValue list;
  list.Append(std::move(dict));
  profile_->GetPrefs()->Set(bookmarks::prefs::kManagedBookmarks,
                            base::Value(std::move(list)));

  BookmarkParentFolder managed_folder = BookmarkParentFolder::ManagedFolder();
  ASSERT_GT(bookmark_service->GetChildrenCount(managed_folder), 0u);
  std::vector<const bookmarks::BookmarkNode*> managed_nodes =
      bookmark_service->GetUnderlyingNodes(managed_folder);
  ASSERT_FALSE(managed_nodes.empty());
  const std::u16string managed_title = managed_nodes[0]->GetTitle();

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

  // Trigger lazy loading of the dynamic bookmarks submenu.
  menu.WillShowMenu(bookmarks_item);

  views::SubmenuView* bookmarks_submenu = bookmarks_item->GetSubmenu();
  ASSERT_TRUE(bookmarks_submenu);

  views::MenuItemView* managed_item = nullptr;
  for (views::MenuItemView* item : bookmarks_submenu->GetMenuItems()) {
    if (item->title() == managed_title) {
      managed_item = item;
      break;
    }
  }

  ASSERT_NE(managed_item, nullptr);
  EXPECT_TRUE(managed_item->HasSubmenu());
  views::MenuItemView* managed_child_item = nullptr;
  for (views::MenuItemView* item : managed_item->GetSubmenu()->GetMenuItems()) {
    if (item->title() == u"Managed Link") {
      managed_child_item = item;
      break;
    }
  }
  ASSERT_NE(managed_child_item, nullptr);

  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  menu.CloseMenu();
}

TEST_F(ActionAppMenuTest, BookmarkBarSubmenuCheckItems) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      ntp_features::kNtpSimplificationBookmarkBar);

  actions::ActionItem* always_show_action =
      actions::ActionManager::Get().FindAction(
          kActionBookmarkBarSubmenuAlwaysShow,
          browser_actions_->root_action_item());
  ASSERT_NE(always_show_action, nullptr);
  always_show_action->SetChecked(true);

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

  // Trigger lazy loading of the dynamic bookmarks submenu.
  menu.WillShowMenu(bookmarks_item);

  views::MenuItemView* always_hide =
      root->GetMenuItemByID(kActionBookmarkBarSubmenuAlwaysHide);
  ASSERT_NE(always_hide, nullptr);
  EXPECT_EQ(always_hide->GetType(), views::MenuItemView::Type::kCheckbox);
  EXPECT_FALSE(menu.IsItemChecked(kActionBookmarkBarSubmenuAlwaysHide));

  views::MenuItemView* always_show =
      root->GetMenuItemByID(kActionBookmarkBarSubmenuAlwaysShow);
  ASSERT_NE(always_show, nullptr);
  EXPECT_EQ(always_show->GetType(), views::MenuItemView::Type::kCheckbox);
  EXPECT_TRUE(menu.IsItemChecked(kActionBookmarkBarSubmenuAlwaysShow));

  views::MenuItemView* only_on_ntp =
      root->GetMenuItemByID(kActionBookmarkBarSubmenuOnlyOnNtp);
  ASSERT_NE(only_on_ntp, nullptr);
  EXPECT_EQ(only_on_ntp->GetType(), views::MenuItemView::Type::kCheckbox);
  EXPECT_FALSE(menu.IsItemChecked(kActionBookmarkBarSubmenuOnlyOnNtp));

  // Dynamically update checked state and verify:
  always_show_action->SetChecked(false);
  EXPECT_FALSE(menu.IsItemChecked(kActionBookmarkBarSubmenuAlwaysShow));

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

  base::Uuid shared_group_guid = base::Uuid::GenerateRandomV4();
  tab_groups::SavedTabGroupTab shared_tab(GURL("https://example.com/3"),
                                          u"Shared Tab", shared_group_guid,
                                          /*position=*/0);
  tab_groups::SavedTabGroup shared_group(
      u"My Shared Tab Group", tab_groups::TabGroupColorId::kRed, {shared_tab},
      /*position=*/std::nullopt, shared_group_guid);
  shared_group.SetCollaborationId(syncer::CollaborationId("collab"));
  sync_service->AddGroup(std::move(shared_group));

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

  // Trigger lazy loading of the dynamic tab groups submenu.
  menu.WillShowMenu(tab_groups_item);

  // Verify the static action item for creating a new group exists.
  views::MenuItemView* new_group_item =
      root->GetMenuItemByID(kActionCreateNewTabGroup);
  ASSERT_TRUE(new_group_item);

  // Find the dynamically populated tab group item by title.
  views::SubmenuView* tab_groups_submenu = tab_groups_item->GetSubmenu();
  ASSERT_TRUE(tab_groups_submenu);

  views::MenuItemView* group_item = nullptr;
  views::MenuItemView* shared_group_item = nullptr;
  for (views::MenuItemView* item : tab_groups_submenu->GetMenuItems()) {
    if (item->title() == u"My Test Tab Group") {
      group_item = item;
    } else if (item->title() == u"My Shared Tab Group") {
      shared_group_item = item;
    }
  }
  ASSERT_NE(group_item, nullptr);
  EXPECT_TRUE(group_item->HasSubmenu());
  ASSERT_NE(shared_group_item, nullptr);
  EXPECT_TRUE(shared_group_item->HasSubmenu());

  // Verify minor icon on shared tab group vs regular tab group.
  EXPECT_TRUE(group_item->GetMinorIcon().IsEmpty());
  EXPECT_FALSE(shared_group_item->GetMinorIcon().IsEmpty());

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

TEST_F(ActionAppMenuTest, RecentTabsMinorIcon) {
  RecentTabsDynamicMenu dynamic_menu(&mock_window_interface_);
  auto parent = actions::ActionItem::Builder().Build();

  RecentTabItem tab_with_group(RecentTabItem::Type::kTab, u"Grouped Tab");
  tab_with_group.set_minor_icon(ui::ImageModel::FromVectorIcon(
      kCircleFilledIcon, ui::kColorMenuIcon, 12));

  RecentTabItem tab_without_group(RecentTabItem::Type::kTab, u"Regular Tab");

  dynamic_menu.CreateRecentTabsActionForTesting(
      parent.get(), {tab_with_group, tab_without_group});

  ASSERT_EQ(parent->GetChildren().children().size(), 2u);

  actions::ActionItem* grouped_action =
      parent->GetChildren().children()[0]->GetActionItem();
  ASSERT_TRUE(grouped_action);
  EXPECT_NE(grouped_action->GetProperty(AppMenuActionItem::kMinorIconKey),
            nullptr);

  actions::ActionItem* regular_action =
      parent->GetChildren().children()[1]->GetActionItem();
  ASSERT_TRUE(regular_action);
  EXPECT_EQ(regular_action->GetProperty(AppMenuActionItem::kMinorIconKey),
            nullptr);
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
      views::AsViewClass<AppMenuBlockView>(block_item->children()[0]);
  ASSERT_TRUE(block_section_view);
  ASSERT_EQ(block_section_view->children().size(), 3u);

  // Verify short title is applied for incognito button in general mode.
  auto* incognito_button =
      views::AsViewClass<AppMenuBlockButton>(block_section_view->children()[2]);
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
      views::AsViewClass<AppMenuBlockButton>(block_section_view->children()[0]);
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

TEST_F(ActionAppMenuTest, InflatesTopBlockRowButtonsIncognito) {
  Profile* otr_profile =
      profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  ON_CALL(mock_window_interface_, GetProfile())
      .WillByDefault(testing::Return(otr_profile));

  base::MockCallback<base::RepeatingClosure> on_menu_closed;
  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());

  menu.RunMenu(button_->button_controller());
  EXPECT_TRUE(menu.IsShowing());

  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);

  views::MenuItemView* block_item = root->GetSubmenu()->GetMenuItemAt(0);
  ASSERT_NE(block_item, nullptr);
  ASSERT_EQ(block_item->children().size(), 1u);
  auto* block_section_view =
      views::AsViewClass<AppMenuBlockView>(block_item->children()[0]);
  ASSERT_TRUE(block_section_view);
  ASSERT_EQ(block_section_view->children().size(), 3u);

  // Verify text override is applied for incognito button in Incognito mode.
  auto* incognito_button =
      views::AsViewClass<AppMenuBlockButton>(block_section_view->children()[2]);
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
            BrowserActions::GetCleanTitleAndTooltipText(
                l10n_util::GetStringUTF16(IDS_NEW_INCOGNITO_WINDOW)));

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
      views::AsViewClass<AppMenuBlockView>(block_item->children()[0]);
  ASSERT_TRUE(block_section_view);

  // The first child button in the block view is kActionNewTab.
  auto* new_tab_button =
      views::AsViewClass<AppMenuBlockButton>(block_section_view->children()[0]);
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
      views::AsViewClass<AppMenuFooterView>(footer_item->children()[0]);
  ASSERT_TRUE(footer_view);

#if BUILDFLAG(IS_MAC)
  // On Mac, right container child 0 is kActionOptions (Settings).
  views::View* settings_container = footer_view->right_container_for_testing();
#else
  // Left container child 0 is kActionOptions (Settings).
  views::View* settings_container = footer_view->left_container_for_testing();
#endif
  ASSERT_TRUE(settings_container);
  ASSERT_FALSE(settings_container->children().empty());
  auto* settings_button = views::AsViewClass<AppMenuFooterButton>(
      settings_container->children()[0]);
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
// dynamically synchronizes and updates the AppMenuBlockButton view
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
      views::AsViewClass<AppMenuBlockView>(block_item->children()[0]);
  ASSERT_TRUE(block_section_view);

  auto* new_tab_button =
      views::AsViewClass<AppMenuBlockButton>(block_section_view->children()[0]);
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
  // AppMenuZoomView.
  ASSERT_EQ(zoom_item->children().size(), 1u);
  auto* zoom_view =
      views::AsViewClass<AppMenuZoomView>(zoom_item->children()[0]);
  ASSERT_TRUE(zoom_view);

  // AppMenuZoomView should contain the zoom minus button, zoom label,
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

  // Check that the footer container is an AppMenuFooterView containing
  // only the top container when unmanaged.
  ASSERT_EQ(footer_item->children().size(), 1u);
  auto* footer_view =
      views::AsViewClass<AppMenuFooterView>(footer_item->children()[0]);
  ASSERT_TRUE(footer_view);
  ASSERT_EQ(footer_view->children().size(), 1u);

  views::View* top_container = footer_view->top_container_for_testing();
  ASSERT_TRUE(top_container);
  ASSERT_EQ(top_container->children().size(), 3u);

  views::View* left_container = footer_view->left_container_for_testing();
  ASSERT_TRUE(left_container);
  views::View* right_container = footer_view->right_container_for_testing();
  ASSERT_TRUE(right_container);
#if BUILDFLAG(IS_MAC)
  ASSERT_EQ(left_container->children().size(), 1u);  // Help
  EXPECT_TRUE(
      views::IsViewClass<AppMenuFooterButton>(left_container->children()[0]));
  ASSERT_EQ(right_container->children().size(), 1u);  // Settings
  EXPECT_TRUE(
      views::IsViewClass<AppMenuFooterButton>(right_container->children()[0]));
#else
  ASSERT_EQ(left_container->children().size(), 2u);  // Settings, Help
  EXPECT_TRUE(
      views::IsViewClass<AppMenuFooterButton>(left_container->children()[0]));
  EXPECT_TRUE(
      views::IsViewClass<AppMenuFooterButton>(left_container->children()[1]));

  if (browser_defaults::kShowExitMenuItem) {
    ASSERT_EQ(right_container->children().size(), 1u);  // Exit
    EXPECT_TRUE(views::IsViewClass<AppMenuFooterButton>(
        right_container->children()[0]));
  } else {
    EXPECT_EQ(right_container->children().size(), 0u);
  }
#endif

  // Without managed UI, separator and bottom container should not be added.
  EXPECT_EQ(footer_view->separator_for_testing(), nullptr);
  EXPECT_EQ(footer_view->bottom_container_for_testing(), nullptr);

  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  menu.CloseMenu();
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(ActionAppMenuTest, PopulatesFooterElementsWithManagedAction) {
  policy::ScopedManagementServiceOverrideForTesting profile_management(
      policy::ManagementServiceFactory::GetForProfile(profile_.get()),
      policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);

  base::MockCallback<base::RepeatingClosure> on_menu_closed;

  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());
  menu.RunMenu(button_->button_controller());
  EXPECT_TRUE(menu.IsShowing());

  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);

  views::SubmenuView* submenu = root->GetSubmenu();
  ASSERT_TRUE(submenu);
  views::MenuItemView* footer_item =
      submenu->GetMenuItemAt(submenu->GetMenuItems().size() - 1);
  ASSERT_NE(footer_item, nullptr);
  EXPECT_EQ(footer_item->GetType(), views::MenuItemView::Type::kHighlighted);

  auto* footer_view =
      views::AsViewClass<AppMenuFooterView>(footer_item->children()[0]);
  ASSERT_TRUE(footer_view);
  ASSERT_EQ(footer_view->children().size(), 3u);

  const auto* provider = ChromeLayoutProvider::Get();

  // When managed, separator and bottom container are added inside the footer.
  ASSERT_NE(footer_view->separator_for_testing(), nullptr);
  EXPECT_TRUE(footer_view->separator_for_testing()->GetVisible());
  EXPECT_EQ(footer_view->separator_for_testing()->GetColorId(),
            ui::kColorMenuSeparator);
  const gfx::Insets* separator_margins =
      footer_view->separator_for_testing()->GetProperty(views::kMarginsKey);
  ASSERT_TRUE(separator_margins);
  EXPECT_EQ(*separator_margins,
            gfx::Insets::TLBR(
                0, 0,
                provider->GetDistanceMetric(
                    DISTANCE_ACTION_APP_MENU_FOOTER_SEPARATOR_BOTTOM_MARGIN),
                0));
  ASSERT_NE(footer_view->bottom_container_for_testing(), nullptr);
  EXPECT_TRUE(footer_view->bottom_container_for_testing()->GetVisible());
  ASSERT_EQ(footer_view->bottom_container_for_testing()->children().size(), 1u);
  EXPECT_EQ(
      footer_view->bottom_container_for_testing()->GetInsideBorderInsets(),
      provider->GetInsetsMetric(
          INSETS_ACTION_APP_MENU_FOOTER_BOTTOM_CONTAINER));
  EXPECT_EQ(footer_view->bottom_container_for_testing()->GetProperty(
                views::kMarginsKey),
            nullptr);
  EXPECT_TRUE(views::IsViewClass<AppMenuFooterButton>(
      footer_view->bottom_container_for_testing()->children()[0]));

  auto* button = views::AsViewClass<AppMenuFooterButton>(
      footer_view->bottom_container_for_testing()->children()[0]);
  const int vertical_padding = provider->GetDistanceMetric(
      DISTANCE_ACTION_APP_MENU_FOOTER_BOTTOM_CONTAINER_SPACING);
  const int horizontal_padding =
      provider->GetInsetsMetric(INSETS_ACTION_APP_MENU_FOOTER_MARGIN).left() +
      provider->GetInsetsMetric(INSETS_ACTION_APP_MENU_FOOTER_BUTTON).left();
  auto* box_layout = static_cast<views::BoxLayout*>(button->GetLayoutManager());
  ASSERT_NE(box_layout, nullptr);
  EXPECT_EQ(box_layout->inside_border_insets(),
            gfx::Insets::VH(vertical_padding, horizontal_padding));

  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  menu.CloseMenu();
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

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
      views::AsViewClass<AppMenuZoomView>(zoom_item->children()[0]);
  ASSERT_TRUE(zoom_view);

  views::Label* const zoom_label = zoom_view->zoom_label_for_testing();
  ASSERT_NE(zoom_label, nullptr);
  EXPECT_TRUE(zoom_label->GetVisible());
  EXPECT_EQ(zoom_label->GetText(), base::FormatPercent(100));

  // Verify that the label updates when the action item's text changes.
  actions::ActionItem* normal_action =
      actions::ActionManager::Get().FindAction(kActionZoomNormal);
  ASSERT_TRUE(normal_action);
  normal_action->SetText(base::FormatPercent(200));
  EXPECT_EQ(zoom_label->GetText(), base::FormatPercent(200));

  menu.CloseMenu();
}

// On macOS, accessibility announcements are dispatched through native Cocoa
// VoiceOver APIs (NSAccessibilityAnnouncementRequestedNotification) rather than
// Views' AXUpdateNotifier / ax::mojom::Event::kAlert event pipeline.
#if !BUILDFLAG(IS_MAC)
TEST_F(ActionAppMenuTest, ZoomLabelAccessibilityAnnouncementOnZoomChange) {
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
  ASSERT_TRUE(menu.IsShowing());

  views::MenuItemView* const root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);

  views::MenuItemView* zoom_item = root->GetMenuItemByID(kActionZoomSubmenu);
  ASSERT_TRUE(zoom_item);

  auto* zoom_view =
      views::AsViewClass<AppMenuZoomView>(zoom_item->children()[0]);
  ASSERT_TRUE(zoom_view);

  views::Label* const zoom_label = zoom_view->zoom_label_for_testing();
  ASSERT_NE(zoom_label, nullptr);
  EXPECT_EQ(zoom_label->GetText(), base::FormatPercent(100));

  actions::ActionItem* normal_action =
      actions::ActionManager::Get().FindAction(kActionZoomNormal);
  ASSERT_TRUE(normal_action);

  views::test::AXEventCounter counter(views::AXUpdateNotifier::Get());
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kAlert));

  normal_action->SetText(base::FormatPercent(200));
  EXPECT_EQ(zoom_label->GetText(), base::FormatPercent(200));
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kAlert));

  // Setting the same zoom level should not trigger a duplicate announcement.
  normal_action->SetText(base::FormatPercent(200));
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kAlert));

  menu.CloseMenu();
}
#endif  // !BUILDFLAG(IS_MAC)

TEST_F(ActionAppMenuTest, FullscreenActionUpdatesAccessibilityName) {
  base::MockCallback<base::RepeatingClosure> on_menu_closed;
  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());
  menu.RunMenu(button_->button_controller());
  EXPECT_TRUE(menu.IsShowing());

  views::MenuItemView* const root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);

  views::MenuItemView* zoom_item = root->GetMenuItemByID(kActionZoomSubmenu);
  ASSERT_TRUE(zoom_item);

  auto* zoom_view =
      views::AsViewClass<AppMenuZoomView>(zoom_item->children()[0]);
  ASSERT_TRUE(zoom_view);

  auto* const fullscreen_button =
      views::AsViewClass<views::ImageButton>(zoom_view->children().back());
  ASSERT_NE(fullscreen_button, nullptr);
  EXPECT_EQ(fullscreen_button->GetViewAccessibility().GetCachedName(),
            u"Fullscreen");

  // Updating the action's tooltip should propagate to the button's accessible
  // name.
  actions::ActionItem* fullscreen_action =
      actions::ActionManager::Get().FindAction(kActionFullscreen);
  ASSERT_TRUE(fullscreen_action);
  fullscreen_action->SetTooltipText(u"Exit full screen");
  EXPECT_EQ(fullscreen_button->GetViewAccessibility().GetCachedName(),
            u"Exit full screen");

  menu.CloseMenu();
}

TEST_F(ActionAppMenuTest, ZoomButtonsInkDropAtLimits) {
  content::RenderViewHostTestEnabler rvh_test_enabler;
  tabs::MockTabInterface mock_tab;
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile_.get(),
                                                        nullptr);
  zoom::ZoomController::CreateForWebContents(web_contents.get());
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(GURL("https://example.com"));

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
      views::AsViewClass<AppMenuZoomView>(zoom_item->children()[0]);
  ASSERT_TRUE(zoom_view);

  auto* minus_button = zoom_view->zoom_minus_button_for_testing();
  auto* plus_button = zoom_view->zoom_plus_button_for_testing();
  ASSERT_TRUE(minus_button);
  ASSERT_TRUE(plus_button);

  actions::ActionItem* minus_action =
      actions::ActionManager::Get().FindAction(kActionZoomMinus);
  ASSERT_TRUE(minus_action);
  actions::ActionItem* plus_action =
      actions::ActionManager::Get().FindAction(kActionZoomPlus);
  ASSERT_TRUE(plus_action);

  // At 100% zoom (default), both buttons are enabled with ink drop ON.
  EXPECT_TRUE(minus_button->GetEnabled());
  EXPECT_TRUE(plus_button->GetEnabled());
  EXPECT_EQ(views::InkDrop::Get(minus_button)->GetMode(),
            views::InkDropHost::InkDropMode::ON);
  EXPECT_EQ(views::InkDrop::Get(plus_button)->GetMode(),
            views::InkDropHost::InkDropMode::ON);

  // Zoom to minimum: minus button should be disabled and its ink drop OFF.
  minus_action->SetEnabled(false);
  EXPECT_FALSE(minus_button->GetEnabled());
  EXPECT_TRUE(plus_button->GetEnabled());
  EXPECT_EQ(views::InkDrop::Get(minus_button)->GetMode(),
            views::InkDropHost::InkDropMode::OFF);
  EXPECT_EQ(views::InkDrop::Get(plus_button)->GetMode(),
            views::InkDropHost::InkDropMode::ON);

  // Zoom to maximum: plus button should be disabled and its ink drop OFF.
  minus_action->SetEnabled(true);
  plus_action->SetEnabled(false);
  EXPECT_TRUE(minus_button->GetEnabled());
  EXPECT_FALSE(plus_button->GetEnabled());
  EXPECT_EQ(views::InkDrop::Get(minus_button)->GetMode(),
            views::InkDropHost::InkDropMode::ON);
  EXPECT_EQ(views::InkDrop::Get(plus_button)->GetMode(),
            views::InkDropHost::InkDropMode::OFF);

  menu.CloseMenu();
}

TEST_F(ActionAppMenuTest, SearchBarDisabledByDefault) {
  base::MockCallback<base::RepeatingClosure> on_menu_closed;
  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());

  menu.RunMenu(button_->button_controller());
  ASSERT_TRUE(menu.IsShowing());

  EXPECT_EQ(menu.search_bar_for_testing(), nullptr);

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

  AppMenuSearchBarView* search_bar = menu.search_bar_for_testing();
  ASSERT_NE(search_bar, nullptr);

  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->HasSubmenu());

  const auto* provider = ChromeLayoutProvider::Get();

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
  EXPECT_EQ(*search_bar_margins, gfx::Insets::TLBR(0, 12, 12, 12));

  // The block section item follows the search bar and has 0dp top margin.
  views::MenuItemView* block_item = root->GetSubmenu()->GetMenuItemAt(1);
  ASSERT_NE(block_item, nullptr);
  EXPECT_EQ(block_item->GetInsets(), gfx::Insets());
  EXPECT_EQ(block_item->GetTopMargin(), 0);
  ASSERT_EQ(block_item->children().size(), 1u);
  auto* block_view =
      views::AsViewClass<AppMenuBlockView>(block_item->children()[0]);
  ASSERT_TRUE(block_view);
  const gfx::Insets* block_margins =
      block_view->GetProperty(views::kMarginsKey);
  ASSERT_TRUE(block_margins);
  EXPECT_EQ(*block_margins,
            provider->GetInsetsMetric(INSETS_ACTION_APP_MENU_BLOCK_MARGIN));
  EXPECT_EQ(*block_margins, gfx::Insets::TLBR(0, 12, 8, 12));

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
  EXPECT_EQ(submenu->GetInsets(), gfx::Insets());

  // The block section item has no border. Margins are applied to the child
  // block view (0px top margin and 12px horizontal insets).
  views::MenuItemView* block_item = submenu->GetMenuItemAt(0);
  ASSERT_NE(block_item, nullptr);
  EXPECT_EQ(block_item->GetInsets(), gfx::Insets());
  EXPECT_EQ(block_item->GetTopMargin(), 0);
  ASSERT_EQ(block_item->children().size(), 1u);
  auto* block_view =
      views::AsViewClass<AppMenuBlockView>(block_item->children()[0]);
  ASSERT_TRUE(block_view);
  const gfx::Insets* block_margins =
      block_view->GetProperty(views::kMarginsKey);
  ASSERT_TRUE(block_margins);
  EXPECT_EQ(*block_margins,
            provider->GetInsetsMetric(INSETS_ACTION_APP_MENU_BLOCK_MARGIN));
  EXPECT_EQ(*block_margins, gfx::Insets::TLBR(0, 12, 8, 12));
  EXPECT_EQ(block_view->GetInsideBorderInsets(), gfx::Insets());

  // The footer item has 0 vertical margin, 0 insets (border removed).
  views::MenuItemView* footer_item =
      submenu->GetMenuItemAt(submenu->GetMenuItems().size() - 1);
  ASSERT_NE(footer_item, nullptr);
  EXPECT_EQ(footer_item->GetInsets(), gfx::Insets());
  EXPECT_EQ(footer_item->GetTopMargin(), 0);
  ASSERT_EQ(footer_item->children().size(), 1u);
  auto* footer_view =
      views::AsViewClass<AppMenuFooterView>(footer_item->children()[0]);
  ASSERT_TRUE(footer_view);
  EXPECT_EQ(footer_view->GetInsets(), gfx::Insets());
  EXPECT_EQ(footer_view->GetProperty(views::kMarginsKey), nullptr);
  views::BoxLayoutView* top_container =
      footer_view->top_container_for_testing();
  ASSERT_TRUE(top_container);
  EXPECT_EQ(top_container->GetInsideBorderInsets(),
            provider->GetInsetsMetric(INSETS_ACTION_APP_MENU_FOOTER_MARGIN));
  EXPECT_EQ(top_container->GetInsideBorderInsets(),
            gfx::Insets::TLBR(8, 12, 8, 12));

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
  EXPECT_EQ(submenu->content_start(), root->GetInsets().left());
  EXPECT_EQ(submenu->trailing_padding(), root->GetInsets().right());

  // 2. Regular card menu items do not set an individual border, avoiding
  // leaking a custom content start to child submenus. Instead, trailing
  // padding and icon/label positioning are inherited from the root menu.
  views::MenuItemView* password_item =
      root->GetMenuItemByID(kActionPasswordsAndAutofillSubmenu);
  ASSERT_TRUE(password_item);
  EXPECT_FALSE(password_item->GetBorder());
  EXPECT_EQ(password_item->GetContentStart(), 20);

  views::MenuItemView* downloads_item =
      root->GetMenuItemByID(kActionShowDownloadsPage);
  ASSERT_TRUE(downloads_item);
  EXPECT_FALSE(downloads_item->GetBorder());
  EXPECT_EQ(downloads_item->GetContentStart(), 20);

  // 3. Headers directly under root.
  std::vector<views::MenuItemView*> root_titles;
  for (views::MenuItemView* item : submenu->GetMenuItems()) {
    if (item->GetType() == views::MenuItemView::Type::kTitle) {
      root_titles.push_back(item);
    }
  }
  ASSERT_GE(root_titles.size(), 2u);

  // First header ("Your Chrome"):
  // - Starts flush with the card (12dp horizontal insets, content start 12)
  // - Standard top margin (8dp).
  EXPECT_EQ(root_titles[0]->GetParentMenuItem(), root);
  ASSERT_TRUE(root_titles[0]->GetBorder());
  EXPECT_EQ(root_titles[0]->GetInsets(),
            provider->GetInsetsMetric(INSETS_ACTION_APP_MENU_HEADER));
  EXPECT_EQ(root_titles[0]->GetInsets(), gfx::Insets::VH(0, 12));
  EXPECT_EQ(root_titles[0]->GetContentStart(), 12);
  EXPECT_EQ(root_titles[0]->GetTopMargin(), 8);

  // Second header ("Tools and Actions"):
  // - Starts flush with the card (12dp horizontal insets, content start 12)
  // - Doubled top margin (16dp).
  EXPECT_EQ(root_titles[1]->GetParentMenuItem(), root);
  ASSERT_TRUE(root_titles[1]->GetBorder());
  EXPECT_EQ(root_titles[1]->GetInsets(),
            provider->GetInsetsMetric(INSETS_ACTION_APP_MENU_HEADER));
  EXPECT_EQ(root_titles[1]->GetInsets(), gfx::Insets::VH(0, 12));
  EXPECT_EQ(root_titles[1]->GetContentStart(), 12);
  EXPECT_EQ(root_titles[1]->GetTopMargin(), 16);

  // 4. Headers in submenus (not under root).
  views::MenuItemView* tab_groups_item =
      root->GetMenuItemByID(kActionSavedTabGroupsSubmenu);
  ASSERT_TRUE(tab_groups_item);
  menu.WillShowMenu(tab_groups_item);
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

TEST_F(ActionAppMenuTest, BlockSectionAndMenuHostWidth) {
  base::MockCallback<base::RepeatingClosure> on_menu_closed;

  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());

  menu.RunMenu(button_->button_controller());
  EXPECT_TRUE(menu.IsShowing());

  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);
  views::SubmenuView* submenu = root->GetSubmenu();
  ASSERT_TRUE(submenu);

  // 1. Verify SubmenuView (the menu host content) preferred width and laid-out
  // width is 322dp.
  EXPECT_EQ(submenu->GetPreferredSize({}).width(), 322);
  EXPECT_EQ(submenu->width(), 322);

  // 2. Verify block container row MenuItemView width is 322dp.
  views::MenuItemView* block_item = submenu->GetMenuItemAt(0);
  ASSERT_NE(block_item, nullptr);
  EXPECT_EQ(block_item->GetDimensions().children_width, 322);
  EXPECT_EQ(block_item->width(), 322);

  // 3. Verify AppMenuBlockView preferred width (298dp = 3 * 94dp + 2 *
  // 8dp) and laid-out width (322dp - 24dp margins = 298dp).
  ASSERT_EQ(block_item->children().size(), 1u);
  auto* block_view =
      views::AsViewClass<AppMenuBlockView>(block_item->children()[0]);
  ASSERT_TRUE(block_view);
  EXPECT_EQ(block_view->GetPreferredSize({}).width(), 298);
  EXPECT_EQ(block_view->width(), 298);

  // 4. Verify each of the 3 block buttons has preferred width and laid-out
  // width of 94dp.
  ASSERT_EQ(block_view->children().size(), 3u);
  for (views::View* child : block_view->children()) {
    auto* button = views::AsViewClass<AppMenuBlockButton>(child);
    ASSERT_TRUE(button);
    EXPECT_EQ(button->GetPreferredSize({}).width(), 94);
    EXPECT_EQ(button->width(), 94);
  }

  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  menu.CloseMenu();
}

TEST_F(ActionAppMenuTest, MaxWidthForMenu) {
  base::MockCallback<base::RepeatingClosure> on_menu_closed;
  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());
  menu.RunMenu(button_->button_controller());
  EXPECT_TRUE(menu.IsShowing());

  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);
  EXPECT_EQ(menu.GetMaxWidthForMenu(root),
            ChromeLayoutProvider::Get()->GetDistanceMetric(
                DISTANCE_ACTION_APP_MENU_MAX_WIDTH));
  EXPECT_EQ(menu.GetMaxWidthForMenu(root), 800);

  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  menu.CloseMenu();
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(ActionAppMenuTest, UpgradeNotificationRowStyling) {
  if (!browser_defaults::kShowUpgradeMenuItem) {
    GTEST_SKIP() << "Upgrade menu item is not supported on this platform.";
  }

  actions::ActionItem* upgrade_action =
      actions::ActionManager::Get().FindAction(
          kActionUpgradeDialog, browser_actions_->root_action_item());
  ASSERT_NE(upgrade_action, nullptr);
  upgrade_action->SetVisible(true);

  base::MockCallback<base::RepeatingClosure> on_menu_closed;
  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());

  actions::ActionItem* app_menu_root = actions::ActionManager::Get().FindAction(
      kActionAppMenuRoot, browser_actions_->root_action_item());
  ASSERT_NE(app_menu_root, nullptr);
  actions::BaseAction* upgrade_indirect_item = app_menu_root->GetChildren()
                                                   .children()[0]
                                                   ->GetChildren()
                                                   .children()[0]
                                                   .get();
  upgrade_indirect_item->SetProperty(
      AppMenuActionItem::kMinorTextKey,
      std::make_unique<std::u16string>(u"Restart to update"));

  menu.RunMenu(button_->button_controller());
  EXPECT_TRUE(menu.IsShowing());

  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);

  views::MenuItemView* upgrade_item =
      root->GetMenuItemByID(kActionUpgradeDialog);
  ASSERT_TRUE(upgrade_item);
  EXPECT_TRUE(upgrade_item->GetVisible());
  EXPECT_EQ(upgrade_item->title(), u"Update Chrome");
  AppMenuMinorTextView* minor_text_view = nullptr;
  for (views::View* child : upgrade_item->children()) {
    if (auto* candidate = views::AsViewClass<AppMenuMinorTextView>(child)) {
      minor_text_view = candidate;
      break;
    }
  }
  ASSERT_NE(minor_text_view, nullptr);
  EXPECT_EQ(minor_text_view->label_for_testing()->GetText(),
            u"Restart to update");
  EXPECT_EQ(upgrade_item->GetViewAccessibility().GetCachedName(),
            views::MenuItemView::GetAccessibleNameForMenuItem(
                u"Update Chrome", u"Restart to update", std::nullopt));

  // Upgrade row should have container background with rounded top and bottom
  // corners.
  ASSERT_TRUE(upgrade_item->GetMenuItemBackground().has_value());
  EXPECT_EQ(upgrade_item->GetMenuItemBackground()->background_color_id,
            ui::kColorAppMenuUpgradeRowBackground);
  EXPECT_EQ(upgrade_item->GetMenuItemBackground()->top_radius, 12);
  EXPECT_EQ(upgrade_item->GetMenuItemBackground()->bottom_radius, 12);

  // Verify the following block section item has
  // DISTANCE_ACTION_APP_MENU_NOTIFICATION_MARGIN added to its top margin,
  // creating 12px of whitespace below the notification banner.
  views::MenuItemView* block_item = root->GetSubmenu()->GetMenuItemAt(1);
  ASSERT_NE(block_item, nullptr);
  ASSERT_EQ(block_item->children().size(), 1u);
  auto* block_view =
      views::AsViewClass<AppMenuBlockView>(block_item->children()[0]);
  ASSERT_TRUE(block_view);
  const gfx::Insets* block_margins =
      block_view->GetProperty(views::kMarginsKey);
  ASSERT_TRUE(block_margins);
  const auto* provider = ChromeLayoutProvider::Get();
  gfx::Insets expected_margins =
      provider->GetInsetsMetric(INSETS_ACTION_APP_MENU_BLOCK_MARGIN);
  expected_margins.set_top(expected_margins.top() +
                           provider->GetDistanceMetric(
                               DISTANCE_ACTION_APP_MENU_NOTIFICATION_MARGIN));
  EXPECT_EQ(*block_margins, expected_margins);

  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  menu.CloseMenu();
}

TEST_F(ActionAppMenuTest, UpgradeNotificationWithoutMinorText) {
  if (!browser_defaults::kShowUpgradeMenuItem) {
    GTEST_SKIP() << "Upgrade menu item is not supported on this platform.";
  }

  actions::ActionItem* upgrade_action =
      actions::ActionManager::Get().FindAction(
          kActionUpgradeDialog, browser_actions_->root_action_item());
  ASSERT_NE(upgrade_action, nullptr);
  upgrade_action->SetVisible(true);

  base::MockCallback<base::RepeatingClosure> on_menu_closed;
  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());

  actions::ActionItem* app_menu_root = actions::ActionManager::Get().FindAction(
      kActionAppMenuRoot, browser_actions_->root_action_item());
  ASSERT_NE(app_menu_root, nullptr);
  actions::BaseAction* upgrade_indirect_item = app_menu_root->GetChildren()
                                                   .children()[0]
                                                   ->GetChildren()
                                                   .children()[0]
                                                   .get();
  upgrade_indirect_item->SetProperty(AppMenuActionItem::kMinorTextKey,
                                     std::make_unique<std::u16string>());

  menu.RunMenu(button_->button_controller());
  EXPECT_TRUE(menu.IsShowing());

  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);

  views::MenuItemView* upgrade_item =
      root->GetMenuItemByID(kActionUpgradeDialog);
  ASSERT_TRUE(upgrade_item);
  EXPECT_TRUE(upgrade_item->GetVisible());
  EXPECT_EQ(upgrade_item->GetViewAccessibility().GetCachedName(),
            u"Update Chrome");

  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  menu.CloseMenu();
}

TEST_F(ActionAppMenuTest, UpgradeNotificationHiddenWhenInvisible) {
  base::MockCallback<base::RepeatingClosure> on_menu_closed;
  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());

  menu.RunMenu(button_->button_controller());
  EXPECT_TRUE(menu.IsShowing());

  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);

  views::MenuItemView* upgrade_item =
      root->GetMenuItemByID(kActionUpgradeDialog);
  EXPECT_FALSE(upgrade_item);

  // Block section should be at index 0 and use standard block margins when the
  // notification is hidden.
  views::MenuItemView* block_item = root->GetSubmenu()->GetMenuItemAt(0);
  ASSERT_NE(block_item, nullptr);
  ASSERT_EQ(block_item->children().size(), 1u);
  auto* block_view =
      views::AsViewClass<AppMenuBlockView>(block_item->children()[0]);
  ASSERT_TRUE(block_view);
  const gfx::Insets* block_margins =
      block_view->GetProperty(views::kMarginsKey);
  ASSERT_TRUE(block_margins);
  EXPECT_EQ(*block_margins, ChromeLayoutProvider::Get()->GetInsetsMetric(
                                INSETS_ACTION_APP_MENU_BLOCK_MARGIN));

  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  menu.CloseMenu();
}
#endif

TEST_F(ActionAppMenuTest, MenuItemVerticalMarginExpandedHeight) {
  base::MockCallback<base::RepeatingClosure> on_menu_closed;
  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());

  menu.RunMenu(button_->button_controller());
  EXPECT_TRUE(menu.IsShowing());

  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);

  const auto* provider = ChromeLayoutProvider::Get();
  const int expected_normal_margin =
      (provider->GetDistanceMetric(DISTANCE_ACTION_APP_MENU_FULL_ITEM_HEIGHT) -
       provider->GetDistanceMetric(DISTANCE_ACTION_APP_MENU_ICON_SIZE)) /
      2;
  const int expected_expanded_margin =
      (provider->GetDistanceMetric(
           DISTANCE_ACTION_APP_MENU_EXPANDED_ITEM_HEIGHT) -
       provider->GetDistanceMetric(DISTANCE_ACTION_APP_MENU_ICON_SIZE)) /
      2;

  views::MenuItemView* zoom_item = root->GetMenuItemByID(kActionZoomSubmenu);
  ASSERT_TRUE(zoom_item);
  EXPECT_EQ(zoom_item->GetTopMargin(), expected_expanded_margin);
  EXPECT_EQ(zoom_item->GetBottomMargin(), expected_expanded_margin);

  views::MenuItemView* print_item = root->GetMenuItemByID(kActionPrint);
  ASSERT_TRUE(print_item);
  EXPECT_EQ(print_item->GetTopMargin(), expected_normal_margin);
  EXPECT_EQ(print_item->GetBottomMargin(), expected_normal_margin);
  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  menu.CloseMenu();
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
TEST_F(ActionAppMenuTest, DefaultBrowserNotificationRowStyling) {
  actions::ActionItem* default_browser_action =
      actions::ActionManager::Get().FindAction(
          kActionSetBrowserAsDefault, browser_actions_->root_action_item());
  ASSERT_NE(default_browser_action, nullptr);
  default_browser_action->SetVisible(true);

  base::MockCallback<base::RepeatingClosure> on_menu_closed;
  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());

  menu.RunMenu(button_->button_controller());
  EXPECT_TRUE(menu.IsShowing());

  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);

  views::MenuItemView* default_browser_item =
      root->GetMenuItemByID(kActionSetBrowserAsDefault);
  ASSERT_TRUE(default_browser_item);
  EXPECT_TRUE(default_browser_item->GetVisible());
  EXPECT_EQ(default_browser_item->title(), u"Set Chrome as default browser");

  // Default browser row should have container background with rounded top and
  // bottom corners.
  ASSERT_TRUE(default_browser_item->GetMenuItemBackground().has_value());
  EXPECT_EQ(default_browser_item->GetMenuItemBackground()->background_color_id,
            ui::kColorAppMenuUpgradeRowBackground);
  EXPECT_EQ(default_browser_item->GetMenuItemBackground()->top_radius, 12);
  EXPECT_EQ(default_browser_item->GetMenuItemBackground()->bottom_radius, 12);

  // Verify the following block section item has
  // DISTANCE_ACTION_APP_MENU_NOTIFICATION_MARGIN added to its top margin,
  // creating 12px of whitespace below the notification banner.
  views::MenuItemView* block_item = root->GetSubmenu()->GetMenuItemAt(1);
  ASSERT_NE(block_item, nullptr);
  ASSERT_EQ(block_item->children().size(), 1u);
  auto* block_view =
      views::AsViewClass<AppMenuBlockView>(block_item->children()[0]);
  ASSERT_TRUE(block_view);
  const gfx::Insets* block_margins =
      block_view->GetProperty(views::kMarginsKey);
  ASSERT_TRUE(block_margins);
  const auto* provider = ChromeLayoutProvider::Get();
  gfx::Insets expected_margins =
      provider->GetInsetsMetric(INSETS_ACTION_APP_MENU_BLOCK_MARGIN);
  expected_margins.set_top(expected_margins.top() +
                           provider->GetDistanceMetric(
                               DISTANCE_ACTION_APP_MENU_NOTIFICATION_MARGIN));
  EXPECT_EQ(*block_margins, expected_margins);

  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  menu.CloseMenu();
}

TEST_F(ActionAppMenuTest, DefaultBrowserNotificationHiddenWhenInvisible) {
  base::MockCallback<base::RepeatingClosure> on_menu_closed;
  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());

  menu.RunMenu(button_->button_controller());
  EXPECT_TRUE(menu.IsShowing());

  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);

  views::MenuItemView* default_browser_item =
      root->GetMenuItemByID(kActionSetBrowserAsDefault);
  EXPECT_FALSE(default_browser_item);

  // Block section should be at index 0 and use standard block margins when the
  // notification is hidden.
  views::MenuItemView* block_item = root->GetSubmenu()->GetMenuItemAt(0);
  ASSERT_NE(block_item, nullptr);
  ASSERT_EQ(block_item->children().size(), 1u);
  auto* block_view =
      views::AsViewClass<AppMenuBlockView>(block_item->children()[0]);
  ASSERT_TRUE(block_view);
  const gfx::Insets* block_margins =
      block_view->GetProperty(views::kMarginsKey);
  ASSERT_TRUE(block_margins);
  EXPECT_EQ(*block_margins, ChromeLayoutProvider::Get()->GetInsetsMetric(
                                INSETS_ACTION_APP_MENU_BLOCK_MARGIN));
}
#endif

TEST_F(ActionAppMenuTest, MenuItemNewBadgeProperty) {
  MockBrowserUserEducationInterface user_education(&mock_window_interface_);
  EXPECT_CALL(user_education,
              MaybeShowNewBadgeFor(testing::Ref(tabs::kVerticalTabsNewBadge)))
      .WillOnce(testing::Return(
          user_education::DisplayNewBadge::create_for_test(true)));

  base::MockCallback<base::RepeatingClosure> on_menu_closed;
  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());

  actions::ActionItem* print_action = actions::ActionManager::Get().FindAction(
      kActionPrint, browser_actions_->root_action_item());
  ASSERT_NE(print_action, nullptr);
  print_action->SetProperty(AppMenuActionItem::kNewBadgeFeatureKey,
                            &tabs::kVerticalTabsNewBadge);

  menu.RunMenu(button_->button_controller());
  EXPECT_TRUE(menu.IsShowing());

  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);

  views::MenuItemView* print_item = root->GetMenuItemByID(kActionPrint);
  ASSERT_TRUE(print_item);
  EXPECT_EQ(print_item->new_badge_type(), ui::NewBadgeType::kNew);

  views::MenuItemView* downloads_item =
      root->GetMenuItemByID(kActionShowDownloadsPage);
  ASSERT_TRUE(downloads_item);
  EXPECT_EQ(downloads_item->new_badge_type(), std::nullopt);

  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  menu.CloseMenu();
}

TEST_F(ActionAppMenuTest, MenuItemIsAlertedProperty) {
  UserEducationServiceFactory::GetInstance()->SetTestingFactory(
      profile_.get(), base::BindRepeating([](content::BrowserContext* context)
                                              -> std::unique_ptr<KeyedService> {
        return std::make_unique<UserEducationService>(
            Profile::FromBrowserContext(context), /*allows_promos=*/true);
      }));
  auto* const user_ed_service =
      UserEducationServiceFactory::GetForBrowserContext(profile_.get());
  user_education::TutorialDescription desc;
  desc.steps.push_back(user_education::TutorialDescription::BubbleStep(
                           AppMenuModel::kDownloadsMenuItem)
                           .SetBubbleBodyText(IDS_OK));
  user_ed_service->tutorial_registry().AddTutorial("TestTutorial",
                                                   std::move(desc));
  user_ed_service->tutorial_service()->StartTutorial(
      "TestTutorial", ui::ElementContext::CreateFakeContextForTesting(1));

  base::MockCallback<base::RepeatingClosure> on_menu_closed;
  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());

  menu.RunMenu(button_->button_controller());
  EXPECT_TRUE(menu.IsShowing());

  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);

  views::MenuItemView* downloads_item =
      root->GetMenuItemByID(kActionShowDownloadsPage);
  ASSERT_TRUE(downloads_item);
  EXPECT_TRUE(downloads_item->is_alerted());

  views::MenuItemView* print_item = root->GetMenuItemByID(kActionPrint);
  ASSERT_TRUE(print_item);
  EXPECT_FALSE(print_item->is_alerted());

  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  menu.CloseMenu();
}

TEST_F(ActionAppMenuTest, GlobalErrorNotificationRowStyling) {
  actions::ActionItem* global_error_action =
      actions::ActionManager::Get().FindAction(
          kActionGlobalError, browser_actions_->root_action_item());
  ASSERT_NE(global_error_action, nullptr);
  global_error_action->SetVisible(true);
  global_error_action->SetText(u"Extension error");

  base::MockCallback<base::RepeatingClosure> on_menu_closed;
  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());

  menu.RunMenu(button_->button_controller());
  EXPECT_TRUE(menu.IsShowing());

  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);

  views::MenuItemView* global_error_item =
      root->GetMenuItemByID(kActionGlobalError);
  ASSERT_TRUE(global_error_item);
  EXPECT_TRUE(global_error_item->GetVisible());
  EXPECT_EQ(global_error_item->title(), u"Extension error");

  ASSERT_TRUE(global_error_item->GetMenuItemBackground().has_value());
  EXPECT_EQ(global_error_item->GetMenuItemBackground()->background_color_id,
            ui::kColorAppMenuUpgradeRowBackground);
  EXPECT_EQ(global_error_item->GetMenuItemBackground()->top_radius, 12);
  EXPECT_EQ(global_error_item->GetMenuItemBackground()->bottom_radius, 12);

  views::MenuItemView* block_item = root->GetSubmenu()->GetMenuItemAt(1);
  ASSERT_NE(block_item, nullptr);
  ASSERT_EQ(block_item->children().size(), 1u);
  auto* block_view =
      views::AsViewClass<AppMenuBlockView>(block_item->children()[0]);
  ASSERT_TRUE(block_view);
  const gfx::Insets* block_margins =
      block_view->GetProperty(views::kMarginsKey);
  ASSERT_TRUE(block_margins);
  const auto* provider = ChromeLayoutProvider::Get();
  gfx::Insets expected_margins =
      provider->GetInsetsMetric(INSETS_ACTION_APP_MENU_BLOCK_MARGIN);
  expected_margins.set_top(expected_margins.top() +
                           provider->GetDistanceMetric(
                               DISTANCE_ACTION_APP_MENU_NOTIFICATION_MARGIN));
  EXPECT_EQ(*block_margins, expected_margins);

  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  menu.CloseMenu();
}

TEST_F(ActionAppMenuTest,
       SafetyHubPrioritizedOverGlobalErrorAndDefaultBrowser) {
  SafetyHubMenuNotificationServiceFactory::GetInstance()->SetTestingFactory(
      profile_.get(), base::BindRepeating([](content::BrowserContext* context)
                                              -> std::unique_ptr<KeyedService> {
        Profile* profile = Profile::FromBrowserContext(context);
        auto service = std::make_unique<SafetyHubMenuNotificationService>(
            profile->GetPrefs(), nullptr, nullptr,
#if !BUILDFLAG(IS_ANDROID)
            nullptr,
#endif
            profile);
        auto getter = base::BindRepeating(
            []() -> std::optional<std::unique_ptr<SafetyHubResult>> {
              return std::make_unique<SafetyHubSafeBrowsingResult>(
                  SafeBrowsingState::kDisabledByUser);
            });
        service->UpdateResultGetterForTesting(
            safety_hub::SafetyHubModuleType::UNUSED_SITE_PERMISSIONS, getter);
        return service;
      }));

  actions::ActionItem* global_error_action =
      actions::ActionManager::Get().FindAction(
          kActionGlobalError, browser_actions_->root_action_item());
  ASSERT_NE(global_error_action, nullptr);
  global_error_action->SetVisible(true);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  actions::ActionItem* default_browser_action =
      actions::ActionManager::Get().FindAction(
          kActionSetBrowserAsDefault, browser_actions_->root_action_item());
  ASSERT_NE(default_browser_action, nullptr);
  default_browser_action->SetVisible(true);
#endif

  base::MockCallback<base::RepeatingClosure> on_menu_closed;
  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());
  menu.RunMenu(button_->button_controller());

  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);

  // Only the Safety Hub notification should be populated in the menu, and it
  // should still round its bottom corners.
  views::MenuItemView* safety_hub_item =
      root->GetMenuItemByID(kActionOpenSafetyHub);
  ASSERT_TRUE(safety_hub_item);
  ASSERT_TRUE(safety_hub_item->GetMenuItemBackground().has_value());
  EXPECT_EQ(safety_hub_item->GetMenuItemBackground()->top_radius, 12);
  EXPECT_EQ(safety_hub_item->GetMenuItemBackground()->bottom_radius, 12);

  EXPECT_FALSE(root->GetMenuItemByID(kActionGlobalError));
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  EXPECT_FALSE(root->GetMenuItemByID(kActionSetBrowserAsDefault));
#endif

  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  menu.CloseMenu();
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
TEST_F(ActionAppMenuTest, GlobalErrorPrioritizedOverDefaultBrowser) {
  actions::ActionItem* global_error_action =
      actions::ActionManager::Get().FindAction(
          kActionGlobalError, browser_actions_->root_action_item());
  ASSERT_NE(global_error_action, nullptr);
  global_error_action->SetVisible(true);

  actions::ActionItem* default_browser_action =
      actions::ActionManager::Get().FindAction(
          kActionSetBrowserAsDefault, browser_actions_->root_action_item());
  ASSERT_NE(default_browser_action, nullptr);
  default_browser_action->SetVisible(true);

  base::MockCallback<base::RepeatingClosure> on_menu_closed;
  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());
  menu.RunMenu(button_->button_controller());

  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);

  // Only the Global Error notification should be populated in the menu, and it
  // should still round its bottom corners.
  views::MenuItemView* global_error_item =
      root->GetMenuItemByID(kActionGlobalError);
  ASSERT_TRUE(global_error_item);
  ASSERT_TRUE(global_error_item->GetMenuItemBackground().has_value());
  EXPECT_EQ(global_error_item->GetMenuItemBackground()->top_radius, 12);
  EXPECT_EQ(global_error_item->GetMenuItemBackground()->bottom_radius, 12);

  EXPECT_FALSE(root->GetMenuItemByID(kActionSetBrowserAsDefault));

  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  menu.CloseMenu();
}
#endif

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(ActionAppMenuTest, MultipleNotificationsSeparatedBySpacingSeparator) {
  if (!browser_defaults::kShowUpgradeMenuItem) {
    GTEST_SKIP() << "Upgrade menu item is not supported on this platform.";
  }

  actions::ActionItem* upgrade_action =
      actions::ActionManager::Get().FindAction(
          kActionUpgradeDialog, browser_actions_->root_action_item());
  ASSERT_NE(upgrade_action, nullptr);
  upgrade_action->SetVisible(true);

  actions::ActionItem* global_error_action =
      actions::ActionManager::Get().FindAction(
          kActionGlobalError, browser_actions_->root_action_item());
  ASSERT_NE(global_error_action, nullptr);
  global_error_action->SetVisible(true);

  base::MockCallback<base::RepeatingClosure> on_menu_closed;
  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());
  menu.RunMenu(button_->button_controller());

  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);
  views::SubmenuView* submenu = root->GetSubmenu();
  ASSERT_TRUE(submenu);

  views::MenuItemView* upgrade_item =
      root->GetMenuItemByID(kActionUpgradeDialog);
  ASSERT_TRUE(upgrade_item);
  ASSERT_TRUE(upgrade_item->GetMenuItemBackground().has_value());
  EXPECT_EQ(upgrade_item->GetMenuItemBackground()->top_radius, 12);
  EXPECT_EQ(upgrade_item->GetMenuItemBackground()->bottom_radius, 12);

  views::MenuItemView* global_error_item =
      root->GetMenuItemByID(kActionGlobalError);
  ASSERT_TRUE(global_error_item);
  ASSERT_TRUE(global_error_item->GetMenuItemBackground().has_value());
  EXPECT_EQ(global_error_item->GetMenuItemBackground()->top_radius, 12);
  EXPECT_EQ(global_error_item->GetMenuItemBackground()->bottom_radius, 12);

  ASSERT_GE(submenu->children().size(), 3u);
  EXPECT_EQ(submenu->children()[0], upgrade_item);
  auto* separator =
      views::AsViewClass<views::MenuSeparator>(submenu->children()[1]);
  ASSERT_NE(separator, nullptr);
  EXPECT_EQ(separator->GetType(), ui::MenuSeparatorType::SPACING_SEPARATOR);
  EXPECT_EQ(submenu->children()[2], global_error_item);

  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  menu.CloseMenu();
}
#endif

TEST_F(ActionAppMenuTest, MenuOpenAndCommandExecutionMetrics) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  base::MockCallback<base::RepeatingClosure> on_menu_closed;

  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());
  menu.RunMenu(button_->button_controller());
  ASSERT_TRUE(menu.IsShowing());

  EXPECT_EQ(user_action_tester.GetActionCount("ShowAppMenu"), 1);
  histogram_tester.ExpectBucketCount("WrenchMenu.MenuAction",
                                     MENU_ACTION_MENU_OPENED, 1);

  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);

  views::MenuItemView* tab_groups_item =
      root->GetMenuItemByID(kActionSavedTabGroupsSubmenu);
  ASSERT_TRUE(tab_groups_item);
  menu.WillShowMenu(tab_groups_item);

  histogram_tester.ExpectBucketCount("WrenchMenu.MenuAction",
                                     MENU_ACTION_SHOW_SAVED_TAB_GROUPS, 1);
  histogram_tester.ExpectTotalCount(
      "WrenchMenu.TimeToAction.ShowSavedTabGroups", 1);

  EXPECT_CALL(mock_action_invoked_, Call(kActionPrint, testing::_, testing::_))
      .Times(1);
  menu.ExecuteCommand(kActionPrint, /*mouse_event_flags=*/0);

  histogram_tester.ExpectBucketCount("WrenchMenu.MenuAction", MENU_ACTION_PRINT,
                                     1);
  histogram_tester.ExpectTotalCount("WrenchMenu.TimeToAction.Print", 1);
  histogram_tester.ExpectTotalCount("WrenchMenu.TimeToAction", 1);

  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  menu.CloseMenu();
}

TEST_F(ActionAppMenuTest, ZoomAndBlockButtonMetrics) {
  base::HistogramTester histogram_tester;
  base::MockCallback<base::RepeatingClosure> on_menu_closed;

  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());
  menu.RunMenu(button_->button_controller());
  ASSERT_TRUE(menu.IsShowing());

  views::MenuItemView* root = menu.root_menu_item_for_testing();
  ASSERT_TRUE(root);

  views::MenuItemView* zoom_item = root->GetMenuItemByID(kActionZoomSubmenu);
  ASSERT_TRUE(zoom_item);
  auto* zoom_view =
      views::AsViewClass<AppMenuZoomView>(zoom_item->children()[0]);
  ASSERT_TRUE(zoom_view);

  EXPECT_CALL(mock_action_invoked_,
              Call(kActionZoomPlus, testing::_, testing::_))
      .Times(2);
  views::test::ButtonTestApi(zoom_view->zoom_plus_button_for_testing())
      .NotifyDefaultMouseClick();
  // Second click while menu stays open should not record a duplicate
  // TimeToAction or ZoomPlus action.
  views::test::ButtonTestApi(zoom_view->zoom_plus_button_for_testing())
      .NotifyDefaultMouseClick();

  histogram_tester.ExpectBucketCount("WrenchMenu.MenuAction",
                                     MENU_ACTION_ZOOM_PLUS, 1);
  histogram_tester.ExpectTotalCount("WrenchMenu.TimeToAction.ZoomPlus", 1);
  histogram_tester.ExpectTotalCount("WrenchMenu.TimeToAction", 1);

  // Clicking a block button closes the menu and records its MenuAction,
  // without recording a second TimeToAction.
  views::MenuItemView* block_item = root->GetSubmenu()->GetMenuItemAt(0);
  ASSERT_NE(block_item, nullptr);
  auto* block_section_view =
      views::AsViewClass<AppMenuBlockView>(block_item->children()[0]);
  ASSERT_TRUE(block_section_view);
  auto* new_tab_button =
      views::AsViewClass<AppMenuBlockButton>(block_section_view->children()[0]);
  ASSERT_TRUE(new_tab_button);

  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  EXPECT_CALL(mock_action_invoked_, Call(kActionNewTab, testing::_, testing::_))
      .Times(1);
  views::test::ButtonTestApi(new_tab_button).NotifyDefaultMouseClick();

  histogram_tester.ExpectBucketCount("WrenchMenu.MenuAction",
                                     MENU_ACTION_NEW_TAB, 1);
  histogram_tester.ExpectTotalCount("WrenchMenu.TimeToAction.NewTab", 0);
  histogram_tester.ExpectTotalCount("WrenchMenu.TimeToAction", 1);
}

TEST_F(ActionAppMenuTest, SafetyHubNotificationMetrics) {
  SafetyHubMenuNotificationServiceFactory::GetInstance()->SetTestingFactory(
      profile_.get(), base::BindRepeating([](content::BrowserContext* context)
                                              -> std::unique_ptr<KeyedService> {
        Profile* profile = Profile::FromBrowserContext(context);
        auto service = std::make_unique<SafetyHubMenuNotificationService>(
            profile->GetPrefs(), nullptr, nullptr,
#if !BUILDFLAG(IS_ANDROID)
            nullptr,
#endif
            profile);
        auto getter = base::BindRepeating(
            []() -> std::optional<std::unique_ptr<SafetyHubResult>> {
              return std::make_unique<SafetyHubSafeBrowsingResult>(
                  SafeBrowsingState::kDisabledByUser);
            });
        service->UpdateResultGetterForTesting(
            safety_hub::SafetyHubModuleType::UNUSED_SITE_PERMISSIONS, getter);
        return service;
      }));

  base::HistogramTester histogram_tester;
  base::MockCallback<base::RepeatingClosure> on_menu_closed;

  ActionAppMenu menu(&mock_window_interface_, on_menu_closed.Get());
  menu.RunMenu(button_->button_controller());
  ASSERT_TRUE(menu.IsShowing());

  histogram_tester.ExpectUniqueSample(
      "Settings.SafetyHub.Impression",
      safety_hub::SafetyHubSurfaces::kThreeDotMenu, 1);
  histogram_tester.ExpectUniqueSample(
      "Settings.SafetyHub.EntryPointImpression",
      safety_hub::SafetyHubEntryPoint::kMenuNotifications, 1);
  histogram_tester.ExpectUniqueSample(
      "Settings.SafetyHub.MenuNotificationImpression",
      safety_hub::SafetyHubModuleType::UNUSED_SITE_PERMISSIONS, 1);

  EXPECT_CALL(mock_action_invoked_,
              Call(kActionOpenSafetyHub, testing::_, testing::_))
      .WillOnce([this](actions::ActionId id, actions::ActionItem* item,
                       actions::ActionInvocationContext context) {
        EXPECT_EQ(
            context.GetProperty(AppMenuActionItem::kActionParamKey),
            static_cast<int>(
                safety_hub::SafetyHubModuleType::UNUSED_SITE_PERMISSIONS));
        safety_hub_util::LogMenuNotificationClicked(
            profile_.get(),
            static_cast<safety_hub::SafetyHubModuleType>(
                context.GetProperty(AppMenuActionItem::kActionParamKey)));
      });
  menu.ExecuteCommand(kActionOpenSafetyHub, /*mouse_event_flags=*/0);

  histogram_tester.ExpectUniqueSample(
      "Settings.SafetyHub.Interaction",
      safety_hub::SafetyHubSurfaces::kThreeDotMenu, 1);
  histogram_tester.ExpectUniqueSample(
      "Settings.SafetyHub.EntryPointInteraction",
      safety_hub::SafetyHubEntryPoint::kMenuNotifications, 1);
  histogram_tester.ExpectUniqueSample(
      "Settings.SafetyHub.MenuNotificationClicked",
      safety_hub::SafetyHubModuleType::UNUSED_SITE_PERMISSIONS, 1);
  histogram_tester.ExpectBucketCount("WrenchMenu.MenuAction",
                                     MENU_ACTION_SHOW_SAFETY_HUB, 1);
  histogram_tester.ExpectTotalCount(
      "WrenchMenu.TimeToAction.SafetyHubNotificationOpenSafetyHub", 1);

  EXPECT_CALL(on_menu_closed, Run()).Times(1);
  menu.CloseMenu();
}

}  // namespace
