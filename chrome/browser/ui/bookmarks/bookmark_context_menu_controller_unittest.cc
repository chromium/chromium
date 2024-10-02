// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_context_menu_controller.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/bookmarks/test_bookmark_navigation_wrapper.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/test/test_clipboard.h"

using base::ASCIIToUTF16;
using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using content::BrowserThread;
using content::OpenURLParams;
using content::WebContents;

class BookmarkContextMenuControllerTest : public testing::Test {
 public:
  BookmarkContextMenuControllerTest() : model_(nullptr) {
    feature_list_.InitWithFeatures({tab_groups::kTabGroupsSaveUIUpdate}, {});
  }

  void SetUp() override {
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        BookmarkModelFactory::GetInstance(),
        BookmarkModelFactory::GetDefaultFactory());
    profile_builder.AddTestingFactory(
        BookmarkMergedSurfaceServiceFactory::GetInstance(),
        BookmarkMergedSurfaceServiceFactory::GetDefaultFactory());
    profile_ = profile_builder.Build();
    model_ = BookmarkModelFactory::GetForBrowserContext(profile_.get());
    bookmarks::test::WaitForBookmarkModelToLoad(model_);
    AddTestData(model_);

    chrome::BookmarkNavigationWrapper::SetInstanceForTesting(&wrapper_);

    // CutCopyPasteNode executes IDC_CUT and IDC_COPY commands.
    ui::TestClipboard::CreateForCurrentThread();
  }

  void TearDown() override {
    ui::Clipboard::DestroyClipboardForCurrentThread();
  }

  // Creates the following structure:
  // a
  // F1
  //  f1a
  //  F11
  //   f11a
  // F2
  // F3
  // F4
  //   f4a
  static void AddTestData(BookmarkModel* model) {
    const BookmarkNode* bb_node = model->bookmark_bar_node();
    std::string test_base = "file:///c:/tmp/";
    model->AddURL(bb_node, 0, u"a", GURL(test_base + "a"));
    const BookmarkNode* f1 = model->AddFolder(bb_node, 1, u"F1");
    model->AddURL(f1, 0, u"f1a", GURL(test_base + "f1a"));
    const BookmarkNode* f11 = model->AddFolder(f1, 1, u"F11");
    model->AddURL(f11, 0, u"f11a", GURL(test_base + "f11a"));
    model->AddFolder(bb_node, 2, u"F2");
    model->AddFolder(bb_node, 3, u"F3");
    const BookmarkNode* f4 = model->AddFolder(bb_node, 4, u"F4");
    model->AddURL(f4, 0, u"f4a", GURL(test_base + "f4a"));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<BookmarkModel> model_;
  TestingBookmarkNavigationWrapper wrapper_;
  base::test::ScopedFeatureList feature_list_;
};

// Tests Deleting from the menu.
TEST_F(BookmarkContextMenuControllerTest, DeleteURL) {
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes = {
      model_->bookmark_bar_node()->children().front().get(),
  };
  BookmarkContextMenuController controller(
      nullptr, nullptr, nullptr, profile_.get(), BookmarkLaunchLocation::kNone,
      nodes[0]->parent(), nodes);
  GURL url = model_->bookmark_bar_node()->children().front()->url();
  ASSERT_TRUE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_REMOVE));
  // Delete the URL.
  controller.ExecuteCommand(IDC_BOOKMARK_BAR_REMOVE, 0);
  // Model shouldn't have URL anymore.
  ASSERT_FALSE(model_->IsBookmarked(url));
}

// Tests the enabled state of the menus when supplied an empty vector.
TEST_F(BookmarkContextMenuControllerTest, EmptyNodes) {
  BookmarkContextMenuController controller(
      nullptr, nullptr, nullptr, profile_.get(), BookmarkLaunchLocation::kNone,
      model_->other_node(),
      std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>());
  EXPECT_FALSE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL));
  EXPECT_FALSE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW));
  EXPECT_FALSE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO));
  EXPECT_FALSE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_REMOVE));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_NEW_FOLDER));
}

// Tests the enabled state of the menus when supplied a vector with a single
// url.
TEST_F(BookmarkContextMenuControllerTest, SingleURL) {
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes = {
      model_->bookmark_bar_node()->children().front().get(),
  };
  BookmarkContextMenuController controller(
      nullptr, nullptr, nullptr, profile_.get(), BookmarkLaunchLocation::kNone,
      nodes[0]->parent(), nodes);
  EXPECT_TRUE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO));
  EXPECT_TRUE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_REMOVE));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_NEW_FOLDER));
}

// Tests the enabled state of the menus when supplied a vector with multiple
// urls.
TEST_F(BookmarkContextMenuControllerTest, MultipleURLs) {
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes = {
      model_->bookmark_bar_node()->children()[0].get(),
      model_->bookmark_bar_node()->children()[1]->children()[0].get(),
  };
  BookmarkContextMenuController controller(
      nullptr, nullptr, nullptr, profile_.get(), BookmarkLaunchLocation::kNone,
      nodes[0]->parent(), nodes);
  EXPECT_TRUE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO));
  EXPECT_TRUE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_REMOVE));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_NEW_FOLDER));
}

// Tests the enabled state of the menus when supplied an vector with a single
// folder.
TEST_F(BookmarkContextMenuControllerTest, SingleFolder) {
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes = {
      model_->bookmark_bar_node()->children()[2].get(),
  };
  BookmarkContextMenuController controller(
      nullptr, nullptr, nullptr, profile_.get(), BookmarkLaunchLocation::kNone,
      nodes[0]->parent(), nodes);
  EXPECT_FALSE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL));
  EXPECT_FALSE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW));
  EXPECT_FALSE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO));
  EXPECT_TRUE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_REMOVE));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_NEW_FOLDER));
}

// Tests the enabled state of the menus when supplied a vector with multiple
// folders, all of which are empty.
TEST_F(BookmarkContextMenuControllerTest, MultipleEmptyFolders) {
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes = {
      model_->bookmark_bar_node()->children()[2].get(),
      model_->bookmark_bar_node()->children()[3].get(),
  };
  BookmarkContextMenuController controller(
      nullptr, nullptr, nullptr, profile_.get(), BookmarkLaunchLocation::kNone,
      nodes[0]->parent(), nodes);
  EXPECT_FALSE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL));
  EXPECT_FALSE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW));
  EXPECT_FALSE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO));
  EXPECT_TRUE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_REMOVE));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_NEW_FOLDER));
}

// Tests the enabled state of the menus when supplied a vector with multiple
// folders, some of which contain URLs.
TEST_F(BookmarkContextMenuControllerTest, MultipleFoldersWithURLs) {
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes = {
      model_->bookmark_bar_node()->children()[3].get(),
      model_->bookmark_bar_node()->children()[4].get(),
  };
  BookmarkContextMenuController controller(
      nullptr, nullptr, nullptr, profile_.get(), BookmarkLaunchLocation::kNone,
      nodes[0]->parent(), nodes);
  EXPECT_TRUE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO));
  EXPECT_TRUE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_REMOVE));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_NEW_FOLDER));
}

// Tests the enabled state of open incognito.
TEST_F(BookmarkContextMenuControllerTest, DisableIncognito) {
  TestingProfile::Builder profile_builder;
  profile_builder.AddTestingFactory(BookmarkModelFactory::GetInstance(),
                                    BookmarkModelFactory::GetDefaultFactory());
  TestingProfile* incognito = profile_builder.BuildIncognito(profile_.get());

  BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(incognito);
  bookmarks::test::WaitForBookmarkModelToLoad(model);
  AddTestData(model);

  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes = {
      model_->bookmark_bar_node()->children().front().get(),
  };
  BookmarkContextMenuController controller(nullptr, nullptr, nullptr, incognito,
                                           BookmarkLaunchLocation::kNone,
                                           nodes[0]->parent(), nodes);
  EXPECT_FALSE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_INCOGNITO));
  EXPECT_FALSE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO));
}

// Tests that you can't remove/edit when showing the other node.
TEST_F(BookmarkContextMenuControllerTest, DisabledItemsWithOtherNode) {
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes;
  nodes.push_back(model_->other_node());
  BookmarkContextMenuController controller(
      nullptr, nullptr, nullptr, profile_.get(), BookmarkLaunchLocation::kNone,
      nodes[0], nodes);
  EXPECT_FALSE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_EDIT));
  EXPECT_FALSE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_REMOVE));
}

// Tests the enabled state of the menus when supplied an empty vector and null
// parent.
TEST_F(BookmarkContextMenuControllerTest, EmptyNodesNullParent) {
  BookmarkContextMenuController controller(
      nullptr, nullptr, nullptr, profile_.get(), BookmarkLaunchLocation::kNone,
      nullptr, std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>());
  EXPECT_FALSE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL));
  EXPECT_FALSE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW));
  EXPECT_FALSE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO));
  EXPECT_FALSE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_REMOVE));
  EXPECT_FALSE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK));
  EXPECT_FALSE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_NEW_FOLDER));
}

// Tests the enabled state of the menus when supplied a vector containing just
// the top-level bookmark bar node.
TEST_F(BookmarkContextMenuControllerTest, BookmarkBar) {
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes;
  nodes.push_back(model_->bookmark_bar_node());
  BookmarkContextMenuController controller(
      nullptr, nullptr, nullptr, profile_.get(), BookmarkLaunchLocation::kNone,
      nodes[0]->parent(), nodes);
  EXPECT_TRUE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO));
  EXPECT_FALSE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_REMOVE));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_NEW_FOLDER));
}

TEST_F(BookmarkContextMenuControllerTest, CutCopyPasteNode) {
  const BookmarkNode* bb_node = model_->bookmark_bar_node();
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes = {
      model_->bookmark_bar_node()->children()[0].get(),
  };
  std::unique_ptr<BookmarkContextMenuController> controller(
      new BookmarkContextMenuController(
          nullptr, nullptr, nullptr, profile_.get(),
          BookmarkLaunchLocation::kNone, nodes[0]->parent(), nodes));
  EXPECT_TRUE(controller->IsCommandIdEnabled(IDC_COPY));
  EXPECT_TRUE(controller->IsCommandIdEnabled(IDC_CUT));

  // Copy the URL.
  controller->ExecuteCommand(IDC_COPY, 0);

  controller = base::WrapUnique(new BookmarkContextMenuController(
      nullptr, nullptr, nullptr, profile_.get(), BookmarkLaunchLocation::kNone,
      nodes[0]->parent(), nodes));
  size_t old_count = bb_node->children().size();
  controller->ExecuteCommand(IDC_PASTE, 0);

  ASSERT_TRUE(bb_node->children()[1]->is_url());
  ASSERT_EQ(old_count + 1, bb_node->children().size());
  ASSERT_EQ(bb_node->children()[0]->url(), bb_node->children()[1]->url());

  controller = base::WrapUnique(new BookmarkContextMenuController(
      nullptr, nullptr, nullptr, profile_.get(), BookmarkLaunchLocation::kNone,
      nodes[0]->parent(), nodes));
  // Cut the URL.
  controller->ExecuteCommand(IDC_CUT, 0);
  ASSERT_TRUE(bb_node->children()[0]->is_url());
  ASSERT_TRUE(bb_node->children()[1]->is_folder());
  ASSERT_EQ(old_count, bb_node->children().size());
}

TEST_F(BookmarkContextMenuControllerTest,
       ManagedShowAppsShortcutInBookmarksBar) {
  BookmarkContextMenuController controller(
      nullptr, nullptr, nullptr, profile_.get(), BookmarkLaunchLocation::kNone,
      model_->bookmark_bar_node(),
      std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>());

  // By default, the pref is not managed and the command is enabled.
  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile_->GetTestingPrefService();
  EXPECT_FALSE(prefs->IsManagedPreference(
      bookmarks::prefs::kShowAppsShortcutInBookmarkBar));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_SHOW_APPS_SHORTCUT));

  // Disabling the shorcut by policy disables the command.
  prefs->SetManagedPref(bookmarks::prefs::kShowAppsShortcutInBookmarkBar,
                        std::make_unique<base::Value>(false));
  EXPECT_FALSE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_SHOW_APPS_SHORTCUT));

  // And enabling the shortcut by policy disables the command too.
  prefs->SetManagedPref(bookmarks::prefs::kShowAppsShortcutInBookmarkBar,
                        std::make_unique<base::Value>(true));
  EXPECT_FALSE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_SHOW_APPS_SHORTCUT));
}

TEST_F(BookmarkContextMenuControllerTest, ShowTabGroupsPref) {
  BookmarkContextMenuController controller(
      nullptr, nullptr, nullptr, profile_.get(), BookmarkLaunchLocation::kNone,
      model_->bookmark_bar_node(),
      std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>());
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_TOGGLE_SHOW_TAB_GROUPS));

  // The pref is to show by default.
  EXPECT_TRUE(
      controller.IsCommandIdChecked(IDC_BOOKMARK_BAR_TOGGLE_SHOW_TAB_GROUPS));
  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(
      bookmarks::prefs::kShowTabGroupsInBookmarkBar));

  // Toggle to not show.
  controller.ExecuteCommand(IDC_BOOKMARK_BAR_TOGGLE_SHOW_TAB_GROUPS, 0);
  EXPECT_FALSE(
      controller.IsCommandIdChecked(IDC_BOOKMARK_BAR_TOGGLE_SHOW_TAB_GROUPS));
  EXPECT_FALSE(profile_->GetPrefs()->GetBoolean(
      bookmarks::prefs::kShowTabGroupsInBookmarkBar));

  // Toggle to show.
  controller.ExecuteCommand(IDC_BOOKMARK_BAR_TOGGLE_SHOW_TAB_GROUPS, 0);
  EXPECT_TRUE(
      controller.IsCommandIdChecked(IDC_BOOKMARK_BAR_TOGGLE_SHOW_TAB_GROUPS));
  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(
      bookmarks::prefs::kShowTabGroupsInBookmarkBar));
}
