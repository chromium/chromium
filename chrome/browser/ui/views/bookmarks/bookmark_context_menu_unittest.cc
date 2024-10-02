// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_context_menu.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/bookmarks/test_bookmark_navigation_wrapper.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/test/scoped_views_test_helper.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#endif

using base::ASCIIToUTF16;
using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using content::BrowserThread;
using content::OpenURLParams;
using content::PageNavigator;
using content::WebContents;

class BookmarkContextMenuTest : public testing::Test {
 public:
  BookmarkContextMenuTest() : model_(nullptr) {}

  void SetUp() override {
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        BookmarkModelFactory::GetInstance(),
        BookmarkModelFactory::GetDefaultFactory());
    profile_builder.AddTestingFactory(
        ManagedBookmarkServiceFactory::GetInstance(),
        ManagedBookmarkServiceFactory::GetDefaultFactory());
    profile_builder.AddTestingFactory(
        BookmarkMergedSurfaceServiceFactory::GetInstance(),
        BookmarkMergedSurfaceServiceFactory::GetDefaultFactory());
    profile_ = profile_builder.Build();

    model_ = BookmarkModelFactory::GetForBrowserContext(profile_.get());
    bookmarks::test::WaitForBookmarkModelToLoad(model_);
    AddTestData();

    chrome::BookmarkNavigationWrapper::SetInstanceForTesting(&wrapper_);

    // CutCopyPasteNode executes IDC_COPY and IDC_CUT commands.
    ui::TestClipboard::CreateForCurrentThread();
  }

  void TearDown() override {
    ui::Clipboard::DestroyClipboardForCurrentThread();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  views::ScopedViewsTestHelper views_test_helper_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<BookmarkModel> model_;
  TestingBookmarkNavigationWrapper wrapper_;

 private:
  // Creates the following structure:
  // a
  // F1
  //  f1a
  // -f1b as "chrome://settings"
  //  F11
  //   f11a
  // F2
  // F3
  // F4
  //   f4a
  void AddTestData() {
    const BookmarkNode* bb_node = model_->bookmark_bar_node();
    std::string test_base = "file:///c:/tmp/";
    model_->AddURL(bb_node, 0, u"a", GURL(test_base + "a"));
    const BookmarkNode* f1 = model_->AddFolder(bb_node, 1, u"F1");
    model_->AddURL(f1, 0, u"f1a", GURL(test_base + "f1a"));
    model_->AddURL(f1, 1, u"f1b", GURL(chrome::kChromeUISettingsURL));
    const BookmarkNode* f11 = model_->AddFolder(f1, 2, u"F11");
    model_->AddURL(f11, 0, u"f11a", GURL(test_base + "f11a"));
    model_->AddFolder(bb_node, 2, u"F2");
    model_->AddFolder(bb_node, 3, u"F3");
    const BookmarkNode* f4 = model_->AddFolder(bb_node, 4, u"F4");
    model_->AddURL(f4, 0, u"f4a", GURL(test_base + "f4a"));
  }
};

// Tests Deleting from the menu.
TEST_F(BookmarkContextMenuTest, DeleteURL) {
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes = {
      model_->bookmark_bar_node()->children().front().get(),
  };
  BookmarkContextMenu controller(nullptr, nullptr, profile_.get(),
                                 BookmarkLaunchLocation::kNone,
                                 nodes[0]->parent(), nodes, false);
  GURL url = model_->bookmark_bar_node()->children().front()->url();
  ASSERT_TRUE(controller.IsCommandEnabled(IDC_BOOKMARK_BAR_REMOVE));
  // Delete the URL.
  controller.ExecuteCommand(IDC_BOOKMARK_BAR_REMOVE, 0);
  // Model shouldn't have URL anymore.
  ASSERT_FALSE(model_->IsBookmarked(url));
}

// Tests counting tabs for 'open all' on a folder with a couple of bookmarks.
TEST_F(BookmarkContextMenuTest, OpenCount) {
  const BookmarkNode* folder = model_->bookmark_bar_node()->children()[1].get();

  // Should count F1's child but not F11's child, as that's what OpenAll would
  // open.
  EXPECT_EQ(2, chrome::OpenCount(nullptr, folder));
}

// Same as above, but for counting bookmarks that would be opened in an
// incognito window.
TEST_F(BookmarkContextMenuTest, OpenCountIncognito) {
  const BookmarkNode* folder = model_->bookmark_bar_node()->children()[1].get();

  // Should count f1a but not f2a, as that's what OpenAll would open.
  EXPECT_EQ(1, chrome::OpenCount(nullptr, folder, profile_.get()));
}

// Tests the enabled state of the menus when supplied an empty vector.
TEST_F(BookmarkContextMenuTest, EmptyNodes) {
  BookmarkContextMenu controller(
      nullptr, nullptr, profile_.get(), BookmarkLaunchLocation::kNone,
      model_->other_node(),
      std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>(), false);
  EXPECT_FALSE(controller.IsCommandEnabled(IDC_BOOKMARK_BAR_OPEN_ALL));
  EXPECT_FALSE(
      controller.IsCommandEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW));
  EXPECT_FALSE(
      controller.IsCommandEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO));
  EXPECT_FALSE(controller.IsCommandEnabled(IDC_BOOKMARK_BAR_REMOVE));
  EXPECT_TRUE(
      controller.IsCommandEnabled(IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK));
  EXPECT_TRUE(
      controller.IsCommandEnabled(IDC_BOOKMARK_BAR_NEW_FOLDER));
}

// Tests the enabled state of the menus when supplied a vector with a single
// url.
TEST_F(BookmarkContextMenuTest, SingleURL) {
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes = {
      model_->bookmark_bar_node()->children().front().get(),
  };
  BookmarkContextMenu controller(nullptr, nullptr, profile_.get(),
                                 BookmarkLaunchLocation::kNone,
                                 nodes[0]->parent(), nodes, false);
  EXPECT_TRUE(controller.IsCommandEnabled(IDC_BOOKMARK_BAR_OPEN_ALL));
  EXPECT_TRUE(
      controller.IsCommandEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW));
  EXPECT_TRUE(controller.IsCommandEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO));
  EXPECT_TRUE(controller.IsCommandEnabled(IDC_BOOKMARK_BAR_REMOVE));
  EXPECT_TRUE(
      controller.IsCommandEnabled(IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK));
  EXPECT_TRUE(
      controller.IsCommandEnabled(IDC_BOOKMARK_BAR_NEW_FOLDER));
}

// Tests the enabled state of the menus when supplied a vector with multiple
// urls.
TEST_F(BookmarkContextMenuTest, MultipleURLs) {
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes = {
      model_->bookmark_bar_node()->children()[0].get(),
      model_->bookmark_bar_node()->children()[1]->children()[0].get(),
  };
  BookmarkContextMenu controller(nullptr, nullptr, profile_.get(),
                                 BookmarkLaunchLocation::kNone,
                                 nodes[0]->parent(), nodes, false);
  EXPECT_TRUE(controller.IsCommandEnabled(IDC_BOOKMARK_BAR_OPEN_ALL));
  EXPECT_TRUE(
      controller.IsCommandEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW));
  EXPECT_TRUE(controller.IsCommandEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO));
  EXPECT_TRUE(controller.IsCommandEnabled(IDC_BOOKMARK_BAR_REMOVE));
  EXPECT_TRUE(
      controller.IsCommandEnabled(IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK));
  EXPECT_TRUE(
      controller.IsCommandEnabled(IDC_BOOKMARK_BAR_NEW_FOLDER));
}

// Tests the enabled state of the menus when supplied an vector with a single
// folder.
TEST_F(BookmarkContextMenuTest, SingleFolder) {
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes = {
      model_->bookmark_bar_node()->children()[2].get(),
  };
  BookmarkContextMenu controller(nullptr, nullptr, profile_.get(),
                                 BookmarkLaunchLocation::kNone,
                                 nodes[0]->parent(), nodes, false);
  EXPECT_FALSE(controller.IsCommandEnabled(IDC_BOOKMARK_BAR_OPEN_ALL));
  EXPECT_FALSE(
      controller.IsCommandEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW));
  EXPECT_FALSE(
      controller.IsCommandEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO));
  EXPECT_TRUE(controller.IsCommandEnabled(IDC_BOOKMARK_BAR_REMOVE));
  EXPECT_TRUE(
      controller.IsCommandEnabled(IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK));
  EXPECT_TRUE(
      controller.IsCommandEnabled(IDC_BOOKMARK_BAR_NEW_FOLDER));
}

// Tests the enabled state of the menus when supplied a vector with multiple
// folders, all of which are empty.
TEST_F(BookmarkContextMenuTest, MultipleEmptyFolders) {
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes = {
      model_->bookmark_bar_node()->children()[2].get(),
      model_->bookmark_bar_node()->children()[3].get(),
  };
  BookmarkContextMenu controller(nullptr, nullptr, profile_.get(),
                                 BookmarkLaunchLocation::kNone,
                                 nodes[0]->parent(), nodes, false);
  EXPECT_FALSE(controller.IsCommandEnabled(IDC_BOOKMARK_BAR_OPEN_ALL));
  EXPECT_FALSE(
      controller.IsCommandEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW));
  EXPECT_FALSE(
      controller.IsCommandEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO));
  EXPECT_TRUE(controller.IsCommandEnabled(IDC_BOOKMARK_BAR_REMOVE));
  EXPECT_TRUE(
      controller.IsCommandEnabled(IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK));
  EXPECT_TRUE(
      controller.IsCommandEnabled(IDC_BOOKMARK_BAR_NEW_FOLDER));
}

// Tests the enabled state of the menus when supplied a vector with multiple
// folders, some of which contain URLs.
TEST_F(BookmarkContextMenuTest, MultipleFoldersWithURLs) {
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes = {
      model_->bookmark_bar_node()->children()[3].get(),
      model_->bookmark_bar_node()->children()[4].get(),
  };
  BookmarkContextMenu controller(nullptr, nullptr, profile_.get(),
                                 BookmarkLaunchLocation::kNone,
                                 nodes[0]->parent(), nodes, false);
  EXPECT_TRUE(controller.IsCommandEnabled(IDC_BOOKMARK_BAR_OPEN_ALL));
  EXPECT_TRUE(
      controller.IsCommandEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW));
  EXPECT_TRUE(controller.IsCommandEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO));
  EXPECT_TRUE(controller.IsCommandEnabled(IDC_BOOKMARK_BAR_REMOVE));
  EXPECT_TRUE(
      controller.IsCommandEnabled(IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK));
  EXPECT_TRUE(
      controller.IsCommandEnabled(IDC_BOOKMARK_BAR_NEW_FOLDER));
}

// Tests the enabled state of open incognito.
TEST_F(BookmarkContextMenuTest, DisableIncognito) {
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes = {
      model_->bookmark_bar_node()->children().front().get(),
  };
  Profile* incognito =
      profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  BookmarkContextMenu controller(nullptr, nullptr, incognito,
                                 BookmarkLaunchLocation::kNone,
                                 nodes[0]->parent(), nodes, false);
  EXPECT_FALSE(controller.IsCommandEnabled(IDC_BOOKMARK_BAR_OPEN_INCOGNITO));
  EXPECT_FALSE(
      controller.IsCommandEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO));
}

// Tests that you can't remove/edit when showing the other node.
TEST_F(BookmarkContextMenuTest, DisabledItemsWithOtherNode) {
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes = {
      model_->other_node()};
  BookmarkContextMenu controller(nullptr, nullptr, profile_.get(),
                                 BookmarkLaunchLocation::kNone, nodes[0], nodes,
                                 false);
  EXPECT_FALSE(controller.IsCommandEnabled(IDC_BOOKMARK_BAR_EDIT));
  EXPECT_FALSE(controller.IsCommandEnabled(IDC_BOOKMARK_BAR_REMOVE));
}

// Tests the enabled state of the menus when supplied an empty vector and null
// parent.
TEST_F(BookmarkContextMenuTest, EmptyNodesNullParent) {
  BookmarkContextMenu controller(
      nullptr, nullptr, profile_.get(), BookmarkLaunchLocation::kNone, nullptr,
      std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>(), false);
  EXPECT_FALSE(controller.IsCommandEnabled(IDC_BOOKMARK_BAR_OPEN_ALL));
  EXPECT_FALSE(
      controller.IsCommandEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW));
  EXPECT_FALSE(
      controller.IsCommandEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO));
  EXPECT_FALSE(controller.IsCommandEnabled(IDC_BOOKMARK_BAR_REMOVE));
  EXPECT_FALSE(
      controller.IsCommandEnabled(IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK));
  EXPECT_FALSE(
      controller.IsCommandEnabled(IDC_BOOKMARK_BAR_NEW_FOLDER));
}

TEST_F(BookmarkContextMenuTest, CutCopyPasteNode) {
  const BookmarkNode* bb_node = model_->bookmark_bar_node();
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes = {
      bb_node->children()[0].get()};
  std::unique_ptr<BookmarkContextMenu> controller(new BookmarkContextMenu(
      nullptr, nullptr, profile_.get(), BookmarkLaunchLocation::kNone,
      nodes[0]->parent(), nodes, false));
  EXPECT_TRUE(controller->IsCommandEnabled(IDC_COPY));
  EXPECT_TRUE(controller->IsCommandEnabled(IDC_CUT));

  // Copy the URL.
  controller->ExecuteCommand(IDC_COPY, 0);

  controller = std::make_unique<BookmarkContextMenu>(
      nullptr, nullptr, profile_.get(), BookmarkLaunchLocation::kNone,
      nodes[0]->parent(), nodes, false);
  size_t old_count = bb_node->children().size();
  controller->ExecuteCommand(IDC_PASTE, 0);

  ASSERT_TRUE(bb_node->children()[1]->is_url());
  ASSERT_EQ(old_count + 1, bb_node->children().size());
  ASSERT_EQ(bb_node->children()[0]->url(), bb_node->children()[1]->url());

  controller = std::make_unique<BookmarkContextMenu>(
      nullptr, nullptr, profile_.get(), BookmarkLaunchLocation::kNone,
      nodes[0]->parent(), nodes, false);
  // Cut the URL.
  controller->ExecuteCommand(IDC_CUT, 0);
  ASSERT_TRUE(bb_node->children()[0]->is_url());
  ASSERT_TRUE(bb_node->children()[1]->is_folder());
  ASSERT_EQ(old_count, bb_node->children().size());
}

// Tests that the "Show managed bookmarks" option in the context menu is only
// visible if the policy is set.
TEST_F(BookmarkContextMenuTest, ShowManagedBookmarks) {
  // Create a BookmarkContextMenu for the bookmarks bar.
  const BookmarkNode* bb_node = model_->bookmark_bar_node();
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes = {
      bb_node->children().front().get(),
  };
  std::unique_ptr<BookmarkContextMenu> controller(new BookmarkContextMenu(
      nullptr, nullptr, profile_.get(), BookmarkLaunchLocation::kNone,
      nodes[0]->parent(), nodes, false));

  // Verify that there are no managed nodes yet.
  bookmarks::ManagedBookmarkService* managed =
      ManagedBookmarkServiceFactory::GetForProfile(profile_.get());
  EXPECT_TRUE(managed->managed_node()->children().empty());

  // The context menu should not show the option to "Show managed bookmarks".
  EXPECT_FALSE(
      controller->IsCommandVisible(IDC_BOOKMARK_BAR_SHOW_MANAGED_BOOKMARKS));
  views::MenuItemView* menu = controller->menu();
  EXPECT_FALSE(menu->GetMenuItemByID(IDC_BOOKMARK_BAR_SHOW_MANAGED_BOOKMARKS)
                   ->GetVisible());

  // Other options are not affected.
  EXPECT_TRUE(controller->IsCommandVisible(IDC_BOOKMARK_BAR_NEW_FOLDER));
  EXPECT_TRUE(menu->GetMenuItemByID(IDC_BOOKMARK_BAR_NEW_FOLDER)->GetVisible());

  // Now set the managed bookmarks policy.
  base::Value::Dict dict;
  dict.Set("name", "Google");
  dict.Set("url", "http://google.com");
  base::Value::List list;
  list.Append(std::move(dict));
  EXPECT_TRUE(managed->managed_node()->children().empty());
  profile_->GetPrefs()->Set(bookmarks::prefs::kManagedBookmarks,
                            base::Value(std::move(list)));
  EXPECT_FALSE(managed->managed_node()->children().empty());

  // New context menus now show the "Show managed bookmarks" option.
  controller = std::make_unique<BookmarkContextMenu>(
      nullptr, nullptr, profile_.get(), BookmarkLaunchLocation::kNone,
      nodes[0]->parent(), nodes, false);
  EXPECT_TRUE(controller->IsCommandVisible(IDC_BOOKMARK_BAR_NEW_FOLDER));
  EXPECT_TRUE(
      controller->IsCommandVisible(IDC_BOOKMARK_BAR_SHOW_MANAGED_BOOKMARKS));
  menu = controller->menu();
  EXPECT_TRUE(menu->GetMenuItemByID(IDC_BOOKMARK_BAR_NEW_FOLDER)->GetVisible());
  EXPECT_TRUE(menu->GetMenuItemByID(IDC_BOOKMARK_BAR_SHOW_MANAGED_BOOKMARKS)
                  ->GetVisible());
}
