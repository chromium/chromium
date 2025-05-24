// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_context_menu_controller.h"

#include <stddef.h>

#include <memory>
#include <numeric>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_test_helpers.h"
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
#include "components/signin/public/base/signin_switches.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "ui/gfx/native_widget_types.h"

using base::ASCIIToUTF16;
using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using content::BrowserThread;
using content::OpenURLParams;
using content::WebContents;

namespace {

std::u16string NodesToString(
    const std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>& nodes) {
  return std::accumulate(nodes.begin(), nodes.end(), std::u16string{},
                         [](const std::u16string& a, const BookmarkNode* node) {
                           return a + (a.empty() ? u"" : u", ") +
                                  node->GetTitle();
                         });
}

}  // namespace

class BookmarkContextMenuControllerTest : public testing::Test {
 public:
  BookmarkContextMenuControllerTest() : model_(nullptr) {
    feature_list_.InitWithFeatures(
        {switches::kSyncEnableBookmarksInTransportMode, features::kSideBySide},
        {});
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

    WaitForBookmarkMergedSurfaceServiceToLoad(
        BookmarkMergedSurfaceServiceFactory::GetForProfile(profile_.get()));

    chrome::BookmarkNavigationWrapper::SetInstanceForTesting(&wrapper_);

    // CutCopyPasteNode executes IDC_CUT and IDC_COPY commands.
    ui::TestClipboard::CreateForCurrentThread();
  }

  void TearDown() override {
    ui::Clipboard::DestroyClipboardForCurrentThread();
  }

  // Creates the following structure under the Local BookmarkBar node:
  // a
  // F1
  //  f1a
  //  F11
  //   f11a
  // F2
  // F3
  // F4
  //   f4a
  void AddTestData(BookmarkModel* model) {
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

  // Creates the following structure under the Account BookmarkBar node:
  // a
  // F1
  //  f1a
  //  F11
  //   f11a
  // F2
  // F3
  // F4
  //   f4a
  void AddAccountTestData(BookmarkModel* model) {
    const BookmarkNode* a_bb_node = model->account_bookmark_bar_node();
    CHECK(a_bb_node);
    std::string test_base = "file:///c:/tmp_account/";
    model->AddURL(a_bb_node, 0, u"a", GURL(test_base + "a"));
    const BookmarkNode* f1 = model->AddFolder(a_bb_node, 1, u"F1");
    model->AddURL(f1, 0, u"f1a", GURL(test_base + "f1a"));
    const BookmarkNode* f11 = model->AddFolder(f1, 1, u"F11");
    model->AddURL(f11, 0, u"f11a", GURL(test_base + "f11a"));
    model->AddFolder(a_bb_node, 2, u"F2");
    model->AddFolder(a_bb_node, 3, u"F3");
    const BookmarkNode* f4 = model->AddFolder(a_bb_node, 4, u"F4");
    model->AddURL(f4, 0, u"f4a", GURL(test_base + "f4a"));
  }

  BookmarkMergedSurfaceService* merged_surface_service() {
    return BookmarkMergedSurfaceServiceFactory::GetForProfile(profile_.get());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<BookmarkModel> model_ = nullptr;
  TestingBookmarkNavigationWrapper wrapper_;
};

// Tests Deleting from the menu.
TEST_F(BookmarkContextMenuControllerTest, DeleteURL) {
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes = {
      model_->bookmark_bar_node()->children().front().get(),
  };
  BookmarkContextMenuController controller(
      gfx::NativeWindow(), nullptr, nullptr, profile_.get(),
      BookmarkLaunchLocation::kNone, nodes);
  GURL url = model_->bookmark_bar_node()->children().front()->url();
  ASSERT_TRUE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_REMOVE));
  // Delete the URL.
  controller.ExecuteCommand(IDC_BOOKMARK_BAR_REMOVE, 0);
  // Model shouldn't have URL anymore.
  ASSERT_FALSE(model_->IsBookmarked(url));
}

// Tests the enabled state of the menus when supplied a vector with a single
// url.
TEST_F(BookmarkContextMenuControllerTest, SingleURL) {
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes = {
      model_->bookmark_bar_node()->children().front().get(),
  };
  BookmarkContextMenuController controller(
      gfx::NativeWindow(), nullptr, nullptr, profile_.get(),
      BookmarkLaunchLocation::kNone, nodes);
  EXPECT_TRUE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO));
  EXPECT_FALSE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_SPLIT_VIEW));
  EXPECT_TRUE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_REMOVE));
  EXPECT_TRUE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK));
  EXPECT_TRUE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_NEW_FOLDER));
}

// With the side by side feature flag disabled, tests the enabled state of the
// split view command when supplied a vector with a single url.
TEST_F(BookmarkContextMenuControllerTest, SingleURLNoSplitView) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures(
      {switches::kSyncEnableBookmarksInTransportMode}, {features::kSideBySide});
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes = {
      model_->bookmark_bar_node()->children().front().get(),
  };
  BookmarkContextMenuController controller(
      gfx::NativeWindow(), nullptr, nullptr, profile_.get(),
      BookmarkLaunchLocation::kNone, nodes);
  EXPECT_FALSE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_SPLIT_VIEW));
}

// Tests the enabled state of the menus when supplied a vector with multiple
// urls.
TEST_F(BookmarkContextMenuControllerTest, MultipleURLs) {
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes = {
      model_->bookmark_bar_node()->children()[0].get(),
      model_->bookmark_bar_node()->children()[1]->children()[0].get(),
  };
  BookmarkContextMenuController controller(
      gfx::NativeWindow(), nullptr, nullptr, profile_.get(),
      BookmarkLaunchLocation::kNone, nodes);
  EXPECT_TRUE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO));
  EXPECT_TRUE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_REMOVE));
  EXPECT_TRUE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK));
  EXPECT_TRUE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_NEW_FOLDER));
}

// Tests the enabled state of the menus when supplied an vector with a single
// folder.
TEST_F(BookmarkContextMenuControllerTest, SingleFolder) {
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes = {
      model_->bookmark_bar_node()->children()[2].get(),
  };
  BookmarkContextMenuController controller(
      gfx::NativeWindow(), nullptr, nullptr, profile_.get(),
      BookmarkLaunchLocation::kNone, nodes);
  EXPECT_FALSE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL));
  EXPECT_FALSE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW));
  EXPECT_FALSE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO));
  EXPECT_FALSE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_SPLIT_VIEW));
  EXPECT_TRUE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_REMOVE));
  EXPECT_TRUE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK));
  EXPECT_TRUE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_NEW_FOLDER));
}

// Tests the enabled state of the menus when supplied a vector with multiple
// folders, all of which are empty.
TEST_F(BookmarkContextMenuControllerTest, MultipleEmptyFolders) {
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes = {
      model_->bookmark_bar_node()->children()[2].get(),
      model_->bookmark_bar_node()->children()[3].get(),
  };
  BookmarkContextMenuController controller(
      gfx::NativeWindow(), nullptr, nullptr, profile_.get(),
      BookmarkLaunchLocation::kNone, nodes);
  EXPECT_FALSE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL));
  EXPECT_FALSE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW));
  EXPECT_FALSE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO));
  EXPECT_TRUE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_REMOVE));
  EXPECT_TRUE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK));
  EXPECT_TRUE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_NEW_FOLDER));
}

// Tests the enabled state of the menus when supplied a vector with multiple
// folders, some of which contain URLs.
TEST_F(BookmarkContextMenuControllerTest, MultipleFoldersWithURLs) {
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes = {
      model_->bookmark_bar_node()->children()[3].get(),
      model_->bookmark_bar_node()->children()[4].get(),
  };
  BookmarkContextMenuController controller(
      gfx::NativeWindow(), nullptr, nullptr, profile_.get(),
      BookmarkLaunchLocation::kNone, nodes);
  EXPECT_TRUE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW));
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO));
  EXPECT_TRUE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_REMOVE));
  EXPECT_TRUE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK));
  EXPECT_TRUE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_NEW_FOLDER));
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
  BookmarkContextMenuController controller(
      gfx::NativeWindow(), nullptr, nullptr, incognito,
      BookmarkLaunchLocation::kNone, nodes);
  EXPECT_FALSE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_INCOGNITO));
  EXPECT_FALSE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO));
}

// Tests that you can't remove/edit when showing the other node.
TEST_F(BookmarkContextMenuControllerTest, DisabledItemsWithOtherNode) {
  {
    std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes{
        model_->other_node()};
    BookmarkContextMenuController controller(
        gfx::NativeWindow(), nullptr, nullptr, profile_.get(),
        BookmarkLaunchLocation::kNone, nodes);
    EXPECT_FALSE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_EDIT));
    EXPECT_FALSE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_REMOVE));
  }

  model_->CreateAccountPermanentFolders();
  ASSERT_TRUE(model_->account_other_node());
  {
    std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes{
        model_->account_other_node(), model_->other_node()};
    BookmarkContextMenuController controller(
        gfx::NativeWindow(), nullptr, nullptr, profile_.get(),
        BookmarkLaunchLocation::kNone, nodes);
    EXPECT_FALSE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_EDIT));
    EXPECT_FALSE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_REMOVE));
  }
}

// Tests the enabled state of the menus when supplied a vector containing just
// the top-level permanent nodes.
TEST_F(BookmarkContextMenuControllerTest,
       CommandIdEnabledSelectionPermanentNodes) {
  model_->CreateAccountPermanentFolders();
  // Test only local or syncable bookmark bar, then both account and local
  // bookmark bar.
  std::vector<std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>>
      nodes_selections{
          {model_->bookmark_bar_node()},
          {model_->account_bookmark_bar_node(), model_->bookmark_bar_node()},
          {model_->other_node()},
          {model_->account_other_node(), model_->other_node()}};

  for (const auto& nodes : nodes_selections) {
    SCOPED_TRACE(NodesToString(nodes));
    BookmarkContextMenuController controller(
        gfx::NativeWindow(), nullptr, nullptr, profile_.get(),
        BookmarkLaunchLocation::kNone, nodes);
    const bool has_urls = chrome::HasBookmarkURLs(nodes);
    EXPECT_EQ(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL),
              has_urls);
    EXPECT_EQ(
        controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW),
        has_urls);
    EXPECT_EQ(
        controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_OPEN_ALL_INCOGNITO),
        has_urls);
    EXPECT_FALSE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_REMOVE));
    EXPECT_TRUE(
        controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK));
    EXPECT_TRUE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_NEW_FOLDER));
    EXPECT_TRUE(
        controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK));
    EXPECT_FALSE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_RENAME_FOLDER));
    EXPECT_FALSE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_EDIT));
    EXPECT_FALSE(
        controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_ADD_TO_BOOKMARKS_BAR));
    EXPECT_FALSE(controller.IsCommandIdEnabled(
        IDC_BOOKMARK_BAR_REMOVE_FROM_BOOKMARKS_BAR));
    EXPECT_FALSE(controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_REMOVE));
    EXPECT_FALSE(controller.IsCommandIdEnabled(IDC_CUT));
    EXPECT_FALSE(controller.IsCommandIdEnabled(IDC_COPY));
  }
}

// Tests the enabled state of direct children of the bookmark bar.
TEST_F(BookmarkContextMenuControllerTest,
       CommandIdEnabledSelectionBookmarkBarDirectChildren) {
  model_->CreateAccountPermanentFolders();
  ASSERT_TRUE(model_->account_bookmark_bar_node());
  const BookmarkNode* account_node =
      model_->AddURL(model_->account_bookmark_bar_node(), 0, u"Google",
                     GURL("http://google.com"));
  const BookmarkNode* local_node = model_->AddURL(
      model_->bookmark_bar_node(), 0, u"Google", GURL("http://google.com"));
  std::vector<std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>>
      nodes_selections{{account_node}, {local_node}};

  for (const auto& nodes : nodes_selections) {
    SCOPED_TRACE(NodesToString(nodes));
    BookmarkContextMenuController controller(
        gfx::NativeWindow(), nullptr, nullptr, profile_.get(),
        BookmarkLaunchLocation::kNone, nodes);
    EXPECT_FALSE(
        controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_ADD_TO_BOOKMARKS_BAR));
    EXPECT_TRUE(controller.IsCommandIdEnabled(
        IDC_BOOKMARK_BAR_REMOVE_FROM_BOOKMARKS_BAR));
  }
}

TEST_F(BookmarkContextMenuControllerTest,
       CommandIdEnabledSelectionNotBookmarkBarDirectChild) {
  const BookmarkNode* folder_node =
      model_->AddFolder(model_->bookmark_bar_node(), 1, u"Folder 1");
  const BookmarkNode* node =
      model_->AddURL(folder_node, 0, u"Google", GURL("http://google.com"));
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes{node};
  ASSERT_NE(node->parent()->type(), BookmarkNode::Type::BOOKMARK_BAR);

  BookmarkContextMenuController controller(
      gfx::NativeWindow(), nullptr, nullptr, profile_.get(),
      BookmarkLaunchLocation::kNone, nodes);
  EXPECT_TRUE(
      controller.IsCommandIdEnabled(IDC_BOOKMARK_BAR_ADD_TO_BOOKMARKS_BAR));
  EXPECT_FALSE(controller.IsCommandIdEnabled(
      IDC_BOOKMARK_BAR_REMOVE_FROM_BOOKMARKS_BAR));
}

TEST_F(BookmarkContextMenuControllerTest, CutCopyPasteNode) {
  const BookmarkNode* bb_node = model_->bookmark_bar_node();
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes = {
      model_->bookmark_bar_node()->children()[0].get(),
  };
  std::unique_ptr<BookmarkContextMenuController> controller(
      new BookmarkContextMenuController(gfx::NativeWindow(), nullptr, nullptr,
                                        profile_.get(),
                                        BookmarkLaunchLocation::kNone, nodes));
  EXPECT_TRUE(controller->IsCommandIdEnabled(IDC_COPY));
  EXPECT_TRUE(controller->IsCommandIdEnabled(IDC_CUT));

  // Copy the URL.
  controller->ExecuteCommand(IDC_COPY, 0);

  controller = base::WrapUnique(new BookmarkContextMenuController(
      gfx::NativeWindow(), nullptr, nullptr, profile_.get(),
      BookmarkLaunchLocation::kNone, nodes));
  size_t old_count = bb_node->children().size();
  controller->ExecuteCommand(IDC_PASTE, 0);

  ASSERT_TRUE(bb_node->children()[1]->is_url());
  ASSERT_EQ(old_count + 1, bb_node->children().size());
  ASSERT_EQ(bb_node->children()[0]->url(), bb_node->children()[1]->url());

  controller = base::WrapUnique(new BookmarkContextMenuController(
      gfx::NativeWindow(), nullptr, nullptr, profile_.get(),
      BookmarkLaunchLocation::kNone, nodes));
  // Cut the URL.
  controller->ExecuteCommand(IDC_CUT, 0);
  ASSERT_TRUE(bb_node->children()[0]->is_url());
  ASSERT_TRUE(bb_node->children()[1]->is_folder());
  ASSERT_EQ(old_count, bb_node->children().size());
}

TEST_F(BookmarkContextMenuControllerTest,
       ManagedShowAppsShortcutInBookmarksBar) {
  BookmarkContextMenuController controller(
      gfx::NativeWindow(), nullptr, nullptr, profile_.get(),
      BookmarkLaunchLocation::kNone, {model_->other_node()});

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
      gfx::NativeWindow(), nullptr, nullptr, profile_.get(),
      BookmarkLaunchLocation::kNone, {model_->bookmark_bar_node()});
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

TEST_F(BookmarkContextMenuControllerTest, GetParentForNewNodesSelectionURL) {
  // This tests the case where selection contains one item and that item is an
  // url.
  model_->CreateAccountPermanentFolders();
  ASSERT_TRUE(model_->account_bookmark_bar_node());
  const BookmarkNode* page = model_->AddURL(
      model_->bookmark_bar_node(), 0, u"Google", GURL("http://google.com"));
  model_->AddURL(model_->account_bookmark_bar_node(), 0, u"Google",
                 GURL("http://google.com"));
  const size_t page_index = 1u;
  // Note: Account bookmarks show-up first.
  ASSERT_EQ(merged_surface_service()->GetIndexOf(page), page_index);
  {
    std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes{page};
    std::unique_ptr<BookmarkParentFolder> parent =
        BookmarkContextMenuController::GetParentForNewNodes(nodes);
    ASSERT_TRUE(parent);
    EXPECT_EQ(*parent.get(), BookmarkParentFolder::BookmarkBarFolder());
    BookmarkContextMenuController controller(
        gfx::NativeWindow(), nullptr, nullptr, profile_.get(),
        BookmarkLaunchLocation::kNone, nodes);
    // New nodes added just after page.
    EXPECT_EQ(controller.GetIndexForNewNodes(), page_index + 1u);
  }
}

TEST_F(BookmarkContextMenuControllerTest,
       GetParentForNewNodesSelectionPermanentNodes) {
  // This tests the case where selection contains permanent local and/or account
  // node(s).
  model_->CreateAccountPermanentFolders();
  ASSERT_TRUE(model_->account_other_node());
  model_->AddURL(model_->account_other_node(), 0, u"Google",
                 GURL("http://google.com"));
  model_->AddURL(model_->other_node(), 0, u"Google", GURL("http://google.com"));
  const size_t other_folder_children_count =
      merged_surface_service()->GetChildrenCount(
          BookmarkParentFolder::OtherFolder());

  std::vector<std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>>
      nodes_selections{{model_->other_node()},
                       {model_->account_other_node(), model_->other_node()}};
  for (const auto& nodes : nodes_selections) {
    SCOPED_TRACE(NodesToString(nodes));
    std::unique_ptr<BookmarkParentFolder> parent =
        BookmarkContextMenuController::GetParentForNewNodes(nodes);
    ASSERT_TRUE(parent);
    EXPECT_EQ(*parent.get(), BookmarkParentFolder::OtherFolder());
    BookmarkContextMenuController controller(
        gfx::NativeWindow(), nullptr, nullptr, profile_.get(),
        BookmarkLaunchLocation::kNone, nodes);
    // New nodes added just after page.
    EXPECT_EQ(controller.GetIndexForNewNodes(), other_folder_children_count);
  }
}

TEST_F(BookmarkContextMenuControllerTest,
       GetParentForNewNodesSelectionSingleFolder) {
  const BookmarkNode* folder_node =
      model_->AddFolder(model_->bookmark_bar_node(), 0, u"Folder 1");
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes{
      folder_node};
  std::unique_ptr<BookmarkParentFolder> parent =
      BookmarkContextMenuController::GetParentForNewNodes(nodes);
  ASSERT_TRUE(parent);
  EXPECT_EQ(*parent.get(), BookmarkParentFolder::FromFolderNode(folder_node));
  BookmarkContextMenuController controller(
      gfx::NativeWindow(), nullptr, nullptr, profile_.get(),
      BookmarkLaunchLocation::kNone, nodes);
  EXPECT_EQ(controller.GetIndexForNewNodes(), 0u);
}

TEST_F(BookmarkContextMenuControllerTest,
       ComputeNodeToFocusForBookmarkManagerReturnsNoNode) {
  model_->CreateAccountPermanentFolders();
  AddAccountTestData(model_);
  const BookmarkNode* a_bb_node = model_->account_bookmark_bar_node();
  ASSERT_GE(a_bb_node->children().size(), 2u);
  const BookmarkNode* child1_node = a_bb_node->children()[0].get();
  const BookmarkNode* child2_node = a_bb_node->children()[1].get();

  // Selecting two nodes that are not permanent nodes should not return any node to focus.
  BookmarkContextMenuController controller(
      gfx::NativeWindow(), nullptr, nullptr, profile_.get(),
      BookmarkLaunchLocation::kNone, {child1_node, child2_node});
  EXPECT_EQ(nullptr, controller.ComputeNodeToFocusForBookmarkManager());
}

TEST_F(BookmarkContextMenuControllerTest,
       ComputeNodeToFocusForBookmarkManagerForPermanentNodesSelection) {
  const BookmarkNode* l_bb_node = model_->bookmark_bar_node();

  // Selecting local bookmark bar permanent node when not having account data.
  {
    ASSERT_FALSE(model_->account_bookmark_bar_node());
    BookmarkContextMenuController controller(
        gfx::NativeWindow(), nullptr, nullptr, profile_.get(),
        BookmarkLaunchLocation::kNone, {l_bb_node});
    EXPECT_EQ(l_bb_node, controller.ComputeNodeToFocusForBookmarkManager());
  }

  model_->CreateAccountPermanentFolders();
  AddAccountTestData(model_);
  const BookmarkNode* a_bb_node = model_->account_bookmark_bar_node();

  // Selecting both bookmark bar permanent nodes should default to the account.
  {
    BookmarkContextMenuController controller(
        gfx::NativeWindow(), nullptr, nullptr, profile_.get(),
        BookmarkLaunchLocation::kNone, {a_bb_node, l_bb_node});
    EXPECT_EQ(a_bb_node, controller.ComputeNodeToFocusForBookmarkManager());
  }

  // Selecting account bookmark bar permanent node directly.
  {
    BookmarkContextMenuController controller(
        gfx::NativeWindow(), nullptr, nullptr, profile_.get(),
        BookmarkLaunchLocation::kNone, {a_bb_node});
    EXPECT_EQ(a_bb_node, controller.ComputeNodeToFocusForBookmarkManager());
  }

  // Selecting local bookmark bar permanent node directly will return the
  // account one (assuming it exists).
  {
    ASSERT_TRUE(a_bb_node);
    BookmarkContextMenuController controller(
        gfx::NativeWindow(), nullptr, nullptr, profile_.get(),
        BookmarkLaunchLocation::kNone, {l_bb_node});
    EXPECT_EQ(a_bb_node, controller.ComputeNodeToFocusForBookmarkManager());
  }
}

TEST_F(BookmarkContextMenuControllerTest,
       ComputeNodeToFocusForBookmarkManagerForDirectChildrenOfPermanentNodes) {
  model_->CreateAccountPermanentFolders();
  AddAccountTestData(model_);

  const BookmarkNode* l_bb_node = model_->bookmark_bar_node();
  ASSERT_GE(l_bb_node->children().size(), 2u);
  const BookmarkNode* a_bb_node = model_->account_bookmark_bar_node();
  ASSERT_GE(a_bb_node->children().size(), 2u);

  // Selecting a Url child of Local BookmarkBar. Should return Local
  // BookmarkBar.
  {
    const BookmarkNode* local_url = l_bb_node->children()[0].get();
    ASSERT_TRUE(local_url->is_url());
    ASSERT_TRUE(local_url->parent()->is_permanent_node());
    BookmarkContextMenuController controller(
        gfx::NativeWindow(), nullptr, nullptr, profile_.get(),
        BookmarkLaunchLocation::kNone, {local_url});
    EXPECT_EQ(l_bb_node, controller.ComputeNodeToFocusForBookmarkManager());
  }

  // Selecting a Url child of Account BookmarkBar. Should return Account
  // BookmarkBar.
  {
    const BookmarkNode* account_url = a_bb_node->children()[0].get();
    ASSERT_TRUE(account_url->is_url());
    ASSERT_TRUE(account_url->parent()->is_permanent_node());
    BookmarkContextMenuController controller(
        gfx::NativeWindow(), nullptr, nullptr, profile_.get(),
        BookmarkLaunchLocation::kNone, {account_url});
    EXPECT_EQ(a_bb_node, controller.ComputeNodeToFocusForBookmarkManager());
  }

  // Selecting a folder child of Local BookmarkBar. Should return the folder.
  {
    const BookmarkNode* local_folder = l_bb_node->children()[1].get();
    ASSERT_TRUE(local_folder->is_folder());
    ASSERT_TRUE(local_folder->parent()->is_permanent_node());
    BookmarkContextMenuController controller(
        gfx::NativeWindow(), nullptr, nullptr, profile_.get(),
        BookmarkLaunchLocation::kNone, {local_folder});
    EXPECT_EQ(local_folder, controller.ComputeNodeToFocusForBookmarkManager());
  }

  // Selecting a folder child of Account BookmarkBar. Should return the folder.
  {
    const BookmarkNode* account_folder = a_bb_node->children()[1].get();
    ASSERT_TRUE(account_folder->is_folder());
    ASSERT_TRUE(account_folder->parent()->is_permanent_node());
    BookmarkContextMenuController controller(
        gfx::NativeWindow(), nullptr, nullptr, profile_.get(),
        BookmarkLaunchLocation::kNone, {account_folder});
    EXPECT_EQ(account_folder,
              controller.ComputeNodeToFocusForBookmarkManager());
  }
}

TEST_F(
    BookmarkContextMenuControllerTest,
    ComputeNodeToFocusForBookmarkManagerForNonDirectChildrenOfPermanentNodes) {
  const BookmarkNode* l_bb_node = model_->bookmark_bar_node();
  ASSERT_GE(l_bb_node->children().size(), 2u);
  const BookmarkNode* F1 = l_bb_node->children()[1].get();
  ASSERT_GE(F1->children().size(), 2u);

  // Selecting a url bookmark should focus on its parent.
  {
    const BookmarkNode* f1 = F1->children()[0].get();
    ASSERT_TRUE(f1->is_url());
    ASSERT_FALSE(f1->parent()->is_permanent_node());
    BookmarkContextMenuController controller(
        gfx::NativeWindow(), nullptr, nullptr, profile_.get(),
        BookmarkLaunchLocation::kNone, {f1});
    EXPECT_EQ(F1, controller.ComputeNodeToFocusForBookmarkManager());
  }

  // Selecting a folder should focus on itself.
  {
    const BookmarkNode* F11 = F1->children()[1].get();
    ASSERT_TRUE(F11->is_folder());
    ASSERT_FALSE(F11->parent()->is_permanent_node());
    BookmarkContextMenuController controller(
        gfx::NativeWindow(), nullptr, nullptr, profile_.get(),
        BookmarkLaunchLocation::kNone, {F11});
    EXPECT_EQ(F11, controller.ComputeNodeToFocusForBookmarkManager());
  }
}
