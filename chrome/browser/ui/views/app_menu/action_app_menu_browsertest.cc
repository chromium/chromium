// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/action_app_menu.h"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/recent_tabs_builder_test_helper.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/app_menu/app_menu_search_bar_view.h"
#include "chrome/browser/ui/views/app_menu/recent_tabs_dynamic_menu.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/toolbar/app_menu_control.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/sync/base/features.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/model/data_type_controller_delegate.h"
#include "components/sync/test/mock_commit_queue.h"
#include "components/sync_sessions/session_sync_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/actions/actions.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace {

views::MenuItemView* FindMenuItemByTitle(views::MenuItemView* parent,
                                         const std::u16string& title) {
  if (!parent || !parent->HasSubmenu()) {
    return nullptr;
  }
  for (views::MenuItemView* item : parent->GetSubmenu()->GetMenuItems()) {
    if (item->title() == title) {
      return item;
    }
  }
  return nullptr;
}

void SimulateBookmarkDragAndDrop(
    ActionAppMenu* menu,
    views::MenuItemView* source_item,
    views::MenuItemView* target_item,
    views::MenuDelegate::DropPosition drop_position) {
  ASSERT_TRUE(menu->CanDrag(source_item));
  EXPECT_NE(menu->GetDragOperations(source_item), ui::DragDropTypes::DRAG_NONE);

  ui::OSExchangeData drag_data;
  menu->WriteDragData(source_item, &drag_data);

  views::MenuItemView* target_parent = target_item->GetParentMenuItem();
  ASSERT_TRUE(target_parent);
  int formats = 0;
  std::set<ui::ClipboardFormatType> format_types;
  EXPECT_TRUE(menu->GetDropFormats(target_parent, &formats, &format_types));
  EXPECT_TRUE(menu->AreDropTypesRequired(target_parent));
  ASSERT_TRUE(menu->CanDrop(target_parent, drag_data));

  ui::DropTargetEvent target_event(drag_data, gfx::PointF(), gfx::PointF(),
                                   ui::DragDropTypes::DRAG_MOVE);
  EXPECT_EQ(menu->GetDropOperation(target_item, target_event, &drop_position),
            ui::mojom::DragOperation::kMove);

  views::View::DropCallback drop_cb =
      menu->GetDropCallback(target_item, drop_position, target_event);
  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  std::move(drop_cb).Run(target_event, output_drag_op,
                         /*drag_image_layer_owner=*/nullptr);
  EXPECT_EQ(output_drag_op, ui::mojom::DragOperation::kMove);
}

std::u16string GetSessionName(RecentTabsBuilderTestHelper& helper,
                              size_t session_index) {
  return u"session_name" +
         base::NumberToString16(helper.GetSessionID(session_index).id());
}

std::vector<std::u16string> GetItemsDepthFirst(actions::BaseAction* item) {
  std::vector<std::u16string> items;
  for (const auto& child : item->GetChildren().children()) {
    items.emplace_back(child->GetActionItem()->GetText());
    auto grandchildren = GetItemsDepthFirst(child.get());
    items.insert(items.end(), grandchildren.begin(), grandchildren.end());
  }
  return items;
}

}  // namespace

class ActionAppMenuBrowserTest : public InProcessBrowserTest {
 public:
  ActionAppMenuBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kAppMenuGlowUp);
  }
  ~ActionAppMenuBrowserTest() override = default;

  AppMenuControl* GetMenuControl(
      BrowserWindowInterface* target_browser = nullptr) {
    return BrowserView::GetBrowserViewForBrowser(target_browser ? target_browser
                                                                : browser())
        ->toolbar_button_provider()
        ->GetAppMenuControl();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ActionAppMenuBrowserTest, ShowActionAppMenu) {
  AppMenuControl* menu_control = GetMenuControl();
  ASSERT_TRUE(menu_control);

  EXPECT_FALSE(menu_control->IsMenuShowing());
  EXPECT_FALSE(menu_control->GetActionAppMenuForTesting());

  menu_control->ShowMenuWithFlags(views::MenuRunner::NO_FLAGS);

  EXPECT_TRUE(menu_control->IsMenuShowing());
  ActionAppMenu* action_menu = menu_control->GetActionAppMenuForTesting();
  ASSERT_TRUE(action_menu);
  if (auto* menu_button = views::AsViewClass<BrowserAppMenuButton>(
          menu_control->GetFocusablePaneView())) {
    EXPECT_FALSE(menu_button->app_menu());
  }

  views::MenuItemView* root = action_menu->root_menu_item_for_testing();
  ASSERT_TRUE(root);

  // Verify that the Action items have been converted into visual menu items.
  views::MenuItemView* password_item =
      root->GetMenuItemByID(kActionPasswordsAndAutofillSubmenu);
  ASSERT_TRUE(password_item);

  views::MenuItemView* print_item = root->GetMenuItemByID(kActionPrint);
  ASSERT_TRUE(print_item);

  views::MenuItemView* block_item = root->GetSubmenu()->GetMenuItemAt(0);
  ASSERT_TRUE(block_item);

  // Check if the menu items have background styling
  EXPECT_TRUE(password_item->GetMenuItemBackground().has_value());
  EXPECT_TRUE(print_item->GetMenuItemBackground().has_value());

  menu_control->CloseMenu();
  EXPECT_FALSE(menu_control->IsMenuShowing());
}

IN_PROC_BROWSER_TEST_F(ActionAppMenuBrowserTest, ShowActionAppMenuDarkMode) {
  ThemeServiceFactory::GetForProfile(browser()->GetProfile())
      ->SetBrowserColorScheme(ThemeService::BrowserColorScheme::kDark);

  AppMenuControl* menu_control = GetMenuControl();
  ASSERT_TRUE(menu_control);

  menu_control->ShowMenuWithFlags(views::MenuRunner::NO_FLAGS);
  EXPECT_TRUE(menu_control->IsMenuShowing());
  ActionAppMenu* action_menu = menu_control->GetActionAppMenuForTesting();
  ASSERT_TRUE(action_menu);

  views::MenuItemView* root = action_menu->root_menu_item_for_testing();
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->HasSubmenu());
  views::SubmenuView* submenu = root->GetSubmenu();
  const ui::ColorProvider* color_provider = submenu->GetColorProvider();
  ASSERT_TRUE(color_provider);

  for (views::MenuItemView* child : submenu->GetMenuItems()) {
    if (child->GetMenuItemBackground().has_value()) {
      ui::ColorId bg_id = child->GetMenuItemBackground()->background_color_id;
      SkColor bg_color = color_provider->GetColor(bg_id);
      EXPECT_NE(bg_color, gfx::kPlaceholderColor)
          << "Item with id " << child->GetCommand()
          << " has placeholder color red background!";
    }
  }

  menu_control->CloseMenu();
  EXPECT_FALSE(menu_control->IsMenuShowing());
}

IN_PROC_BROWSER_TEST_F(ActionAppMenuBrowserTest, BookmarksDragAndDropReorder) {
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser()->GetProfile());
  bookmarks::test::WaitForBookmarkModelToLoad(model);
  const bookmarks::BookmarkNode* bb_node = model->bookmark_bar_node();
  const bookmarks::BookmarkNode* node_a =
      model->AddURL(bb_node, 0, u"Bookmark A", GURL("https://a.example.com"));
  const bookmarks::BookmarkNode* node_b =
      model->AddURL(bb_node, 1, u"Bookmark B", GURL("https://b.example.com"));

  AppMenuControl* menu_control = GetMenuControl();
  ASSERT_TRUE(menu_control);
  menu_control->ShowMenuWithFlags(views::MenuRunner::NO_FLAGS);
  ActionAppMenu* action_menu = menu_control->GetActionAppMenuForTesting();
  ASSERT_TRUE(action_menu);

  views::MenuItemView* root = action_menu->root_menu_item_for_testing();
  views::MenuItemView* bookmarks_item =
      root->GetMenuItemByID(kActionBookmarksSubmenu);
  ASSERT_TRUE(bookmarks_item);
  action_menu->WillShowMenu(bookmarks_item);

  // Non-bookmark items cannot be dragged.
  views::MenuItemView* bookmark_this_tab =
      root->GetMenuItemByID(kActionBookmarkThisTab);
  ASSERT_TRUE(bookmark_this_tab);
  EXPECT_FALSE(action_menu->CanDrag(bookmark_this_tab));

  views::MenuItemView* item_a =
      FindMenuItemByTitle(bookmarks_item, u"Bookmark A");
  views::MenuItemView* item_b =
      FindMenuItemByTitle(bookmarks_item, u"Bookmark B");
  ASSERT_TRUE(item_a);
  ASSERT_TRUE(item_b);

  // Drag Bookmark B before Bookmark A.
  SimulateBookmarkDragAndDrop(action_menu, item_b, item_a,
                              views::MenuDelegate::DropPosition::kBefore);

  EXPECT_EQ(bb_node->children()[0].get(), node_b);
  EXPECT_EQ(bb_node->children()[1].get(), node_a);

  // Verify the live menu updated in place with Bookmark B before Bookmark A.
  views::MenuItemView* updated_b =
      FindMenuItemByTitle(bookmarks_item, u"Bookmark B");
  views::MenuItemView* updated_a =
      FindMenuItemByTitle(bookmarks_item, u"Bookmark A");
  ASSERT_TRUE(updated_b);
  ASSERT_TRUE(updated_a);
  EXPECT_LT(bookmarks_item->GetSubmenu()->GetIndexOf(updated_b),
            bookmarks_item->GetSubmenu()->GetIndexOf(updated_a));

  menu_control->CloseMenu();
}

IN_PROC_BROWSER_TEST_F(ActionAppMenuBrowserTest,
                       BookmarksDragAndDropToAndFromNestedFolder) {
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser()->GetProfile());
  bookmarks::test::WaitForBookmarkModelToLoad(model);
  const bookmarks::BookmarkNode* bb_node = model->bookmark_bar_node();
  const bookmarks::BookmarkNode* node_a =
      model->AddURL(bb_node, 0, u"Bookmark A", GURL("https://a.example.com"));
  const bookmarks::BookmarkNode* folder_b =
      model->AddFolder(bb_node, 1, u"Folder B");

  AppMenuControl* menu_control = GetMenuControl();
  ASSERT_TRUE(menu_control);
  menu_control->ShowMenuWithFlags(views::MenuRunner::NO_FLAGS);
  ActionAppMenu* action_menu = menu_control->GetActionAppMenuForTesting();
  ASSERT_TRUE(action_menu);

  views::MenuItemView* root = action_menu->root_menu_item_for_testing();
  views::MenuItemView* bookmarks_item =
      root->GetMenuItemByID(kActionBookmarksSubmenu);
  ASSERT_TRUE(bookmarks_item);
  action_menu->WillShowMenu(bookmarks_item);

  views::MenuItemView* item_a =
      FindMenuItemByTitle(bookmarks_item, u"Bookmark A");
  views::MenuItemView* item_b =
      FindMenuItemByTitle(bookmarks_item, u"Folder B");
  ASSERT_TRUE(item_a);
  ASSERT_TRUE(item_b);

  // Drop Bookmark A onto empty Folder B.
  SimulateBookmarkDragAndDrop(action_menu, item_a, item_b,
                              views::MenuDelegate::DropPosition::kOn);
  ASSERT_EQ(folder_b->children().size(), 1u);
  EXPECT_EQ(folder_b->children()[0].get(), node_a);

  // Verify Bookmark A now appears inside Folder B's submenu in the live menu.
  views::MenuItemView* updated_b =
      FindMenuItemByTitle(bookmarks_item, u"Folder B");
  ASSERT_TRUE(updated_b);
  views::MenuItemView* a_in_b = FindMenuItemByTitle(updated_b, u"Bookmark A");
  ASSERT_TRUE(a_in_b);

  // Drag Bookmark A back out of Folder B before Folder B.
  SimulateBookmarkDragAndDrop(action_menu, a_in_b, updated_b,
                              views::MenuDelegate::DropPosition::kBefore);
  EXPECT_TRUE(folder_b->children().empty());
  ASSERT_EQ(bb_node->children().size(), 2u);
  EXPECT_EQ(bb_node->children()[0].get(), node_a);
  EXPECT_EQ(bb_node->children()[1].get(), folder_b);

  menu_control->CloseMenu();
}

class ActionAppMenuWithSearchBrowserTest : public ActionAppMenuBrowserTest {
 public:
  ActionAppMenuWithSearchBrowserTest() {
    search_feature_list_.InitAndEnableFeature(features::kChroMenuSearch);
  }
  ~ActionAppMenuWithSearchBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList search_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ActionAppMenuWithSearchBrowserTest,
                       ShowActionAppMenuWithSearch) {
  AppMenuControl* menu_control = GetMenuControl();
  ASSERT_TRUE(menu_control);

  EXPECT_FALSE(menu_control->IsMenuShowing());
  EXPECT_FALSE(menu_control->GetActionAppMenuForTesting());

  menu_control->ShowMenuWithFlags(views::MenuRunner::NO_FLAGS);

  EXPECT_TRUE(menu_control->IsMenuShowing());
  ActionAppMenu* action_menu = menu_control->GetActionAppMenuForTesting();
  ASSERT_TRUE(action_menu);

  AppMenuSearchBarView* search_bar = action_menu->search_bar_for_testing();
  ASSERT_TRUE(search_bar);
  views::View* search_icon =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          AppMenuSearchBarView::kSearchIconElementId,
          BrowserView::GetBrowserViewForBrowser(browser())
              ->GetElementContext());
  ASSERT_TRUE(search_icon);
  EXPECT_TRUE(search_icon->GetVisible());

  menu_control->CloseMenu();
  EXPECT_FALSE(menu_control->IsMenuShowing());
}

class ActionAppMenuWithSearchQueryBrowserTest
    : public ActionAppMenuBrowserTest {
 public:
  ActionAppMenuWithSearchQueryBrowserTest() {
    search_feature_list_.InitAndEnableFeatureWithParameters(
        features::kChroMenuSearch, {{"query", "extensions"}});
  }
  ~ActionAppMenuWithSearchQueryBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList search_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ActionAppMenuWithSearchQueryBrowserTest,
                       ShowActionAppMenuWithExtensionsSearchTwice) {
  AppMenuControl* menu_control = GetMenuControl();
  ASSERT_TRUE(menu_control);

  EXPECT_FALSE(menu_control->IsMenuShowing());
  EXPECT_FALSE(menu_control->GetActionAppMenuForTesting());

  menu_control->ShowMenuWithFlags(views::MenuRunner::NO_FLAGS);
  EXPECT_TRUE(menu_control->IsMenuShowing());
  ActionAppMenu* action_menu = menu_control->GetActionAppMenuForTesting();
  ASSERT_TRUE(action_menu);

  AppMenuSearchBarView* search_bar = action_menu->search_bar_for_testing();
  ASSERT_TRUE(search_bar);
  EXPECT_EQ(search_bar->GetText(), u"extensions");

  menu_control->CloseMenu();
  EXPECT_FALSE(menu_control->IsMenuShowing());

  menu_control->ShowMenuWithFlags(views::MenuRunner::NO_FLAGS);
  EXPECT_TRUE(menu_control->IsMenuShowing());
  action_menu = menu_control->GetActionAppMenuForTesting();
  ASSERT_TRUE(action_menu);

  search_bar = action_menu->search_bar_for_testing();
  ASSERT_TRUE(search_bar);
  EXPECT_EQ(search_bar->GetText(), u"extensions");

  menu_control->CloseMenu();
  EXPECT_FALSE(menu_control->IsMenuShowing());
}

class ActionAppMenuWithOtherDeviceTabsBrowserTest
    : public ActionAppMenuBrowserTest {
 public:
  ActionAppMenuWithOtherDeviceTabsBrowserTest() {
    feature_list_.InitAndEnableFeature(syncer::kCrossDeviceTabsInAppMenu);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class ActionAppMenuWithoutOtherDeviceTabsBrowserTest
    : public ActionAppMenuBrowserTest {
 public:
  ActionAppMenuWithoutOtherDeviceTabsBrowserTest() {
    feature_list_.InitAndDisableFeature(syncer::kCrossDeviceTabsInAppMenu);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ActionAppMenuWithOtherDeviceTabsBrowserTest,
                       BuildOtherDeviceTabsActions) {
  sync_sessions::SessionSyncService* session_sync_service =
      SessionSyncServiceFactory::GetForProfile(browser()->GetProfile());
  syncer::DataTypeActivationRequest activation_request;
  activation_request.cache_guid = "test_cache_guid";
  activation_request.error_handler = base::DoNothing();

  std::unique_ptr<syncer::DataTypeActivationResponse> activation_response;
  base::RunLoop loop;
  session_sync_service->GetControllerDelegate()->OnSyncStarting(
      activation_request,
      base::BindLambdaForTesting(
          [&](std::unique_ptr<syncer::DataTypeActivationResponse> response) {
            activation_response = std::move(response);
            loop.Quit();
          }));
  loop.Run();
  ASSERT_NE(nullptr, activation_response);
  ASSERT_NE(nullptr, activation_response->type_processor);
  std::unique_ptr<syncer::DataTypeProcessor> sync_processor =
      std::move(activation_response->type_processor);
  sync_processor->ConnectSync(
      std::make_unique<testing::NiceMock<syncer::MockCommitQueue>>());

  const base::Time now = base::Time::Now();
  RecentTabsBuilderTestHelper helper;
  helper.AddSession();
  helper.AddWindow(0);
  helper.AddTabWithInfo(0, 0, now - base::Minutes(10), u"Older");
  helper.AddTabWithInfo(0, 0, now - base::Minutes(5), u"Newest");
  helper.AddSession();
  helper.AddWindow(1);
  helper.AddTabWithInfo(1, 0, now - base::Minutes(20), u"Oldest");
  helper.ExportToSessionSync(sync_processor.get());

  RecentTabsDynamicMenu dynamic_menu(browser());
  std::unique_ptr<actions::ActionItem> root =
      actions::ActionItem::Builder().Build();
  dynamic_menu.BuildOtherDeviceTabsActions(root.get());

  EXPECT_THAT(
      GetItemsDepthFirst(root.get()),
      testing::ElementsAre(GetSessionName(helper, 0), u"Newest", u"Older", u"",
                           GetSessionName(helper, 1), u"Oldest"));
}

IN_PROC_BROWSER_TEST_F(ActionAppMenuWithOtherDeviceTabsBrowserTest,
                       OtherDeviceTabsSubmenu) {
  AppMenuControl* menu_control = GetMenuControl();
  ASSERT_TRUE(menu_control);

  menu_control->ShowMenuWithFlags(views::MenuRunner::NO_FLAGS);

  ActionAppMenu* action_menu = menu_control->GetActionAppMenuForTesting();
  ASSERT_TRUE(action_menu);

  views::MenuItemView* root = action_menu->root_menu_item_for_testing();
  ASSERT_TRUE(root);

  views::MenuItemView* other_device_tabs_item =
      root->GetMenuItemByID(kActionOtherDeviceTabsSubmenu);
  ASSERT_TRUE(other_device_tabs_item);
  EXPECT_TRUE(other_device_tabs_item->GetVisible());
  EXPECT_TRUE(other_device_tabs_item->GetEnabled());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_OTHER_DEVICE_TABS_MENU),
            other_device_tabs_item->title());
  EXPECT_FALSE(other_device_tabs_item->GetIcon().IsEmpty());
  EXPECT_TRUE(other_device_tabs_item->HasSubmenu());
  EXPECT_EQ(AppMenuModel::kOtherDeviceTabsMenuItem,
            other_device_tabs_item->GetProperty(views::kElementIdentifierKey));

  // Exercise dynamic submenu population via WillShowMenu.
  action_menu->WillShowMenu(other_device_tabs_item);
  EXPECT_TRUE(other_device_tabs_item->HasSubmenu());

  views::MenuItemView* recent_tabs_item =
      root->GetMenuItemByID(kActionRecentTabsSubmenu);
  ASSERT_TRUE(recent_tabs_item);

  views::MenuItemView* downloads_item =
      root->GetMenuItemByID(kActionShowDownloadsPage);
  ASSERT_TRUE(downloads_item);

  views::SubmenuView* submenu = root->GetSubmenu();
  std::optional<size_t> recent_tabs_index =
      submenu->GetIndexOf(recent_tabs_item);
  std::optional<size_t> other_device_tabs_index =
      submenu->GetIndexOf(other_device_tabs_item);
  std::optional<size_t> downloads_index = submenu->GetIndexOf(downloads_item);

  ASSERT_TRUE(recent_tabs_index.has_value());
  ASSERT_TRUE(other_device_tabs_index.has_value());
  ASSERT_TRUE(downloads_index.has_value());

  EXPECT_EQ(recent_tabs_index.value() + 1, other_device_tabs_index.value());
  EXPECT_EQ(other_device_tabs_index.value() + 1, downloads_index.value());

  menu_control->CloseMenu();
}

IN_PROC_BROWSER_TEST_F(ActionAppMenuWithOtherDeviceTabsBrowserTest,
                       OtherDeviceTabsSubmenuIncognito) {
  AppMenuControl* menu_control = GetMenuControl(CreateIncognitoBrowser());
  ASSERT_TRUE(menu_control);

  menu_control->ShowMenuWithFlags(views::MenuRunner::NO_FLAGS);
  ActionAppMenu* action_menu = menu_control->GetActionAppMenuForTesting();
  ASSERT_TRUE(action_menu);

  views::MenuItemView* root = action_menu->root_menu_item_for_testing();
  ASSERT_TRUE(root);
  EXPECT_EQ(nullptr, root->GetMenuItemByID(kActionOtherDeviceTabsSubmenu));

  menu_control->CloseMenu();
}

IN_PROC_BROWSER_TEST_F(ActionAppMenuWithoutOtherDeviceTabsBrowserTest,
                       OtherDeviceTabsSubmenuFlagOff) {
  AppMenuControl* menu_control = GetMenuControl();
  ASSERT_TRUE(menu_control);

  menu_control->ShowMenuWithFlags(views::MenuRunner::NO_FLAGS);

  ActionAppMenu* action_menu = menu_control->GetActionAppMenuForTesting();
  ASSERT_TRUE(action_menu);

  views::MenuItemView* root = action_menu->root_menu_item_for_testing();
  ASSERT_TRUE(root);

  views::MenuItemView* other_device_tabs_item =
      root->GetMenuItemByID(kActionOtherDeviceTabsSubmenu);
  EXPECT_EQ(nullptr, other_device_tabs_item);

  views::MenuItemView* recent_tabs_item =
      root->GetMenuItemByID(kActionRecentTabsSubmenu);
  ASSERT_TRUE(recent_tabs_item);
  views::MenuItemView* downloads_item =
      root->GetMenuItemByID(kActionShowDownloadsPage);
  ASSERT_TRUE(downloads_item);

  views::SubmenuView* submenu = root->GetSubmenu();
  std::optional<size_t> recent_tabs_index =
      submenu->GetIndexOf(recent_tabs_item);
  std::optional<size_t> downloads_index = submenu->GetIndexOf(downloads_item);
  ASSERT_TRUE(recent_tabs_index.has_value());
  ASSERT_TRUE(downloads_index.has_value());
  EXPECT_EQ(recent_tabs_index.value() + 1, downloads_index.value());

  menu_control->CloseMenu();
}
