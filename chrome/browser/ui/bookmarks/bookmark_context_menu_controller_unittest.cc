// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_context_menu_controller.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/test/test_clipboard.h"

using base::ASCIIToUTF16;
using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using content::BrowserThread;
using content::OpenURLParams;
using content::PageNavigator;
using content::WebContents;

// PageNavigator implementation that records the URL.
class TestingPageNavigator : public PageNavigator {
 public:
  WebContents* OpenURL(const OpenURLParams& params) override {
    urls_.push_back(params.url);
    return NULL;
  }

  std::vector<GURL> urls_;
};

class BookmarkContextMenuControllerTest : public testing::Test {
 public:
  BookmarkContextMenuControllerTest() : model_(nullptr) {}

  void SetUp() override {
    TestingProfile::Builder builder;
    profile_ = builder.Build();
    profile_->CreateBookmarkModel(true);
    model_ = BookmarkModelFactory::GetForBrowserContext(profile_.get());
    bookmarks::test::WaitForBookmarkModelToLoad(model_);
    AddTestData(model_);
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
    model->AddURL(bb_node, 0, ASCIIToUTF16("a"), GURL(test_base + "a"));
    const BookmarkNode* f1 = model->AddFolder(bb_node, 1, ASCIIToUTF16("F1"));
    model->AddURL(f1, 0, ASCIIToUTF16("f1a"), GURL(test_base + "f1a"));
    const BookmarkNode* f11 = model->AddFolder(f1, 1, ASCIIToUTF16("F11"));
    model->AddURL(f11, 0, ASCIIToUTF16("f11a"), GURL(test_base + "f11a"));
    model->AddFolder(bb_node, 2, ASCIIToUTF16("F2"));
    model->AddFolder(bb_node, 3, ASCIIToUTF16("F3"));
    const BookmarkNode* f4 = model->AddFolder(bb_node, 4, ASCIIToUTF16("F4"));
    model->AddURL(f4, 0, ASCIIToUTF16("f4a"), GURL(test_base + "f4a"));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  BookmarkModel* model_;
  TestingPageNavigator navigator_;
};

// Tests Deleting from the menu.
TEST_F(BookmarkContextMenuControllerTest, DeleteURL) {
  std::vector<const BookmarkNode*> nodes = {
      model_->bookmark_bar_node()->children().front().get(),
  };
  BookmarkContextMenuController controller(NULL, NULL, NULL, profile_.get(),
                                           NULL, BOOKMARK_LAUNCH_LOCATION_NONE,
                                           nodes[0]->parent(), nodes);
  GURL url = model_->bookmark_bar_node()->children().front()->url();
  ASSERT_TRUE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_REMOVE));
  // Delete the URL.
  controller.ExecuteCommand(IDC_BOOKMARK_BAR_REMOVE, 0);
  // Model shouldn't have URL anymore.
  ASSERT_FALSE(model_->IsBookmarked(url));
}

// Tests open all on a folder with a couple of bookmarks.
TEST_F(BookmarkContextMenuControllerTest, OpenAll) {
  const BookmarkNode* folder = model_->bookmark_bar_node()->children()[1].get();
  chrome::OpenAll(NULL, &navigator_, folder,
                  WindowOpenDisposition::NEW_FOREGROUND_TAB, NULL);

  // Should have navigated to F1's child, but not F11's child.
  ASSERT_EQ(1u, navigator_.urls_.size());
  ASSERT_TRUE(folder->children()[0]->url() == navigator_.urls_[0]);
}

// Tests the enabled state of the menus when supplied an empty vector.
TEST_F(BookmarkContextMenuControllerTest, EmptyNodes) {
  BookmarkContextMenuController controller(
      NULL, NULL, NULL, profile_.get(), NULL, BOOKMARK_LAUNCH_LOCATION_NONE,
      model_->other_node(), std::vector<const BookmarkNode*>());
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
  std::vector<const BookmarkNode*> nodes = {
      model_->bookmark_bar_node()->children().front().get(),
  };
  BookmarkContextMenuController controller(NULL, NULL, NULL, profile_.get(),
                                           NULL, BOOKMARK_LAUNCH_LOCATION_NONE,
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
  std::vector<const BookmarkNode*> nodes = {
      model_->bookmark_bar_node()->children()[0].get(),
      model_->bookmark_bar_node()->children()[1]->children()[0].get(),
  };
  BookmarkContextMenuController controller(NULL, NULL, NULL, profile_.get(),
                                           NULL, BOOKMARK_LAUNCH_LOCATION_NONE,
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
  std::vector<const BookmarkNode*> nodes = {
      model_->bookmark_bar_node()->children()[2].get(),
  };
  BookmarkContextMenuController controller(NULL, NULL, NULL, profile_.get(),
                                           NULL, BOOKMARK_LAUNCH_LOCATION_NONE,
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
  std::vector<const BookmarkNode*> nodes = {
      model_->bookmark_bar_node()->children()[2].get(),
      model_->bookmark_bar_node()->children()[3].get(),
  };
  BookmarkContextMenuController controller(NULL, NULL, NULL, profile_.get(),
                                           NULL, BOOKMARK_LAUNCH_LOCATION_NONE,
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
  std::vector<const BookmarkNode*> nodes = {
      model_->bookmark_bar_node()->children()[3].get(),
      model_->bookmark_bar_node()->children()[4].get(),
  };
  BookmarkContextMenuController controller(NULL, NULL, NULL, profile_.get(),
                                           NULL, BOOKMARK_LAUNCH_LOCATION_NONE,
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
  TestingProfile* incognito =
      TestingProfile::Builder().BuildIncognito(profile_.get());

  incognito->CreateBookmarkModel(true);
  BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(incognito);
  bookmarks::test::WaitForBookmarkModelToLoad(model);
  AddTestData(model);

  std::vector<const BookmarkNode*> nodes = {
      model_->bookmark_bar_node()->children().front().get(),
  };
  BookmarkContextMenuController controller(NULL, NULL, NULL, incognito, NULL,
                                           BOOKMARK_LAUNCH_LOCATION_NONE,
                                           nodes[0]->parent(), nodes);
  EXPECT_FALSE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_INCOGNITO));
  EXPECT_FALSE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO));
}

// Tests that you can't remove/edit when showing the other node.
TEST_F(BookmarkContextMenuControllerTest, DisabledItemsWithOtherNode) {
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(model_->other_node());
  BookmarkContextMenuController controller(NULL, NULL, NULL, profile_.get(),
                                           NULL, BOOKMARK_LAUNCH_LOCATION_NONE,
                                           nodes[0], nodes);
  EXPECT_FALSE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_EDIT));
  EXPECT_FALSE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_REMOVE));
}

// Tests the enabled state of the menus when supplied an empty vector and null
// parent.
TEST_F(BookmarkContextMenuControllerTest, EmptyNodesNullParent) {
  BookmarkContextMenuController controller(
      NULL, NULL, NULL, profile_.get(), NULL, BOOKMARK_LAUNCH_LOCATION_NONE,
      NULL, std::vector<const BookmarkNode*>());
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
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(model_->bookmark_bar_node());
  BookmarkContextMenuController controller(NULL, NULL, NULL, profile_.get(),
                                           NULL, BOOKMARK_LAUNCH_LOCATION_NONE,
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
  std::vector<const BookmarkNode*> nodes = {
      model_->bookmark_bar_node()->children()[0].get(),
  };
  std::unique_ptr<BookmarkContextMenuController> controller(
      new BookmarkContextMenuController(NULL, NULL, NULL, profile_.get(), NULL,
                                        BOOKMARK_LAUNCH_LOCATION_NONE,
                                        nodes[0]->parent(), nodes));
  EXPECT_TRUE(controller->IsCommandIdEnabled(IDC_COPY));
  EXPECT_TRUE(controller->IsCommandIdEnabled(IDC_CUT));

  // Copy the URL.
  controller->ExecuteCommand(IDC_COPY, 0);

  controller.reset(new BookmarkContextMenuController(
      NULL, NULL, NULL, profile_.get(), NULL, BOOKMARK_LAUNCH_LOCATION_NONE,
      nodes[0]->parent(), nodes));
  size_t old_count = bb_node->children().size();
  controller->ExecuteCommand(IDC_PASTE, 0);

  ASSERT_TRUE(bb_node->children()[1]->is_url());
  ASSERT_EQ(old_count + 1, bb_node->children().size());
  ASSERT_EQ(bb_node->children()[0]->url(), bb_node->children()[1]->url());

  controller.reset(new BookmarkContextMenuController(
      NULL, NULL, NULL, profile_.get(), NULL, BOOKMARK_LAUNCH_LOCATION_NONE,
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
      NULL, NULL, NULL, profile_.get(), NULL, BOOKMARK_LAUNCH_LOCATION_NONE,
      model_->bookmark_bar_node(), std::vector<const BookmarkNode*>());

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
