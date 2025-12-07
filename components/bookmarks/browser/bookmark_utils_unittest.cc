// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_utils.h"

#include <stddef.h>

#include <array>
#include <memory>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_client.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/signin/public/base/signin_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

namespace bookmarks {
namespace {

using base::ASCIIToUTF16;
using std::string;
using testing::ElementsAre;
using testing::UnorderedElementsAre;

class BookmarkUtilsTest : public testing::Test,
                          public BaseBookmarkModelObserver {
 public:
  BookmarkUtilsTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}

  BookmarkUtilsTest(const BookmarkUtilsTest&) = delete;
  BookmarkUtilsTest& operator=(const BookmarkUtilsTest&) = delete;

  ~BookmarkUtilsTest() override {}

// Copy and paste is not yet supported on iOS. http://crbug.com/228147
#if !BUILDFLAG(IS_IOS)
  void TearDown() override {
    ui::Clipboard::DestroyClipboardForCurrentThread();
  }
#endif  // !BUILDFLAG(IS_IOS)

  // Certain user actions require multiple changes to the bookmark model,
  // however these modifications need to be atomic for the undo framework. The
  // BaseBookmarkModelObserver is used to inform the boundaries of the user
  // action. For example, when multiple bookmarks are cut to the clipboard we
  // expect one call each to GroupedBookmarkChangesBeginning/Ended.
  void ExpectGroupedChangeCount(int expected_beginning_count,
                                int expected_ended_count) {
    // The undo framework is not used under Android.  Thus the group change
    // events will not be fired and so should not be tested for Android.
#if !BUILDFLAG(IS_ANDROID)
    EXPECT_EQ(grouped_changes_beginning_count_, expected_beginning_count);
    EXPECT_EQ(grouped_changes_ended_count_, expected_ended_count);
#endif
  }

  base::HistogramTester* histogram() { return &histogram_; }

 private:
  // BaseBookmarkModelObserver:
  void BookmarkModelChanged() override {}

  void GroupedBookmarkChangesBeginning() override {
    ++grouped_changes_beginning_count_;
  }

  void GroupedBookmarkChangesEnded() override {
    ++grouped_changes_ended_count_;
  }

  // Some of these tests exercise account bookmarks.
  base::test::ScopedFeatureList features_override_{
      switches::kSyncEnableBookmarksInTransportMode};

  // Clipboard requires a full TaskEnvironment.
  base::test::TaskEnvironment task_environment_;

  int grouped_changes_beginning_count_{0};
  int grouped_changes_ended_count_{0};
  base::HistogramTester histogram_;
};

// A bookmark client that suggests a save location for new nodes.
class SuggestFolderClient : public TestBookmarkClient {
 public:
  SuggestFolderClient() = default;
  SuggestFolderClient(const SuggestFolderClient&) = delete;
  SuggestFolderClient& operator=(const SuggestFolderClient&) = delete;
  ~SuggestFolderClient() override = default;

  const BookmarkNode* GetSuggestedSaveLocation(const GURL& url) override {
    return suggested_save_location_.get();
  }

  void SetSuggestedSaveLocation(const BookmarkNode* node) {
    suggested_save_location_ = node;
  }

 private:
  raw_ptr<const BookmarkNode> suggested_save_location_;
};

TEST_F(BookmarkUtilsTest, GetBookmarksMatchingPropertiesWordPhraseQuery) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  const BookmarkNode* node1 = model->AddURL(model->other_node(), 0, u"foo bar",
                                            GURL("http://www.google.com"));
  const BookmarkNode* node2 = model->AddURL(model->other_node(), 0, u"baz buz",
                                            GURL("http://www.cnn.com"));
  const BookmarkNode* folder1 =
      model->AddFolder(model->other_node(), 0, u"foo");
  QueryFields query;
  query.word_phrase_query = std::make_unique<std::u16string>();
  // No nodes are returned for empty string.
  *query.word_phrase_query = u"";
  EXPECT_TRUE(GetBookmarksMatchingProperties(model.get(), query, 100).empty());

  // No nodes are returned for space-only string.
  *query.word_phrase_query = u"   ";
  EXPECT_TRUE(GetBookmarksMatchingProperties(model.get(), query, 100).empty());

  // Node "foo bar" and folder "foo" are returned in search results.
  *query.word_phrase_query = u"foo";
  EXPECT_THAT(GetBookmarksMatchingProperties(model.get(), query, 100),
              UnorderedElementsAre(folder1, node1));

  // Ensure url matches return in search results.
  *query.word_phrase_query = u"cnn";
  EXPECT_THAT(GetBookmarksMatchingProperties(model.get(), query, 100),
              UnorderedElementsAre(node2));

  // Ensure folder "foo" is not returned in more specific search.
  *query.word_phrase_query = u"foo bar";
  EXPECT_THAT(GetBookmarksMatchingProperties(model.get(), query, 100),
              UnorderedElementsAre(node1));

  // Bookmark Bar and Other Bookmarks are not returned in search results.
  *query.word_phrase_query = u"Bookmark";
  EXPECT_TRUE(GetBookmarksMatchingProperties(model.get(), query, 100).empty());
}

// Check exact matching against a URL query.
TEST_F(BookmarkUtilsTest, GetBookmarksMatchingPropertiesUrl) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  const BookmarkNode* node1 = model->AddURL(model->other_node(), 0, u"Google",
                                            GURL("https://www.google.com/"));
  model->AddURL(model->other_node(), 0, u"Google Calendar",
                GURL("https://www.google.com/calendar"));

  model->AddFolder(model->other_node(), 0, u"Folder");

  QueryFields query;
  query.url = std::make_unique<std::u16string>();
  *query.url = u"https://www.google.com/";
  EXPECT_THAT(GetBookmarksMatchingProperties(model.get(), query, 100),
              UnorderedElementsAre(node1));

  *query.url = u"calendar";
  EXPECT_TRUE(GetBookmarksMatchingProperties(model.get(), query, 100).empty());

  // Empty URL should not match folders.
  *query.url = u"";
  EXPECT_TRUE(GetBookmarksMatchingProperties(model.get(), query, 100).empty());
}

// Check exact matching against a title query.
TEST_F(BookmarkUtilsTest, GetBookmarksMatchingPropertiesTitle) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  const BookmarkNode* node1 = model->AddURL(model->other_node(), 0, u"Google",
                                            GURL("https://www.google.com/"));
  model->AddURL(model->other_node(), 0, u"Google Calendar",
                GURL("https://www.google.com/calendar"));

  const BookmarkNode* folder1 =
      model->AddFolder(model->other_node(), 0, u"Folder");

  QueryFields query;
  query.title = std::make_unique<std::u16string>();
  *query.title = u"Google";
  EXPECT_THAT(GetBookmarksMatchingProperties(model.get(), query, 100),
              UnorderedElementsAre(node1));

  *query.title = u"Calendar";
  EXPECT_TRUE(GetBookmarksMatchingProperties(model.get(), query, 100).empty());

  // Title should match folders.
  *query.title = u"Folder";
  EXPECT_THAT(GetBookmarksMatchingProperties(model.get(), query, 100),
              UnorderedElementsAre(folder1));
}

// Check matching against a query with multiple predicates.
TEST_F(BookmarkUtilsTest, GetBookmarksMatchingPropertiesConjunction) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  const BookmarkNode* node1 = model->AddURL(model->other_node(), 0, u"Google",
                                            GURL("https://www.google.com/"));
  model->AddURL(model->other_node(), 0, u"Google Calendar",
                GURL("https://www.google.com/calendar"));

  model->AddFolder(model->other_node(), 0, u"Folder");

  QueryFields query;

  // Test all fields matching.
  query.word_phrase_query = std::make_unique<std::u16string>(u"www");
  query.url = std::make_unique<std::u16string>(u"https://www.google.com/");
  query.title = std::make_unique<std::u16string>(u"Google");
  EXPECT_THAT(GetBookmarksMatchingProperties(model.get(), query, 100),
              UnorderedElementsAre(node1));

  auto fields = std::to_array<std::unique_ptr<std::u16string>*>({
      &query.word_phrase_query,
      &query.url,
      &query.title,
  });

  // Test two fields matching.
  for (size_t i = 0; i < std::size(fields); i++) {
    std::unique_ptr<std::u16string> original_value(fields[i]->release());
    EXPECT_THAT(GetBookmarksMatchingProperties(model.get(), query, 100),
                UnorderedElementsAre(node1));
    *fields[i] = std::move(original_value);
  }

  // Test two fields matching with one non-matching field.
  for (size_t i = 0; i < std::size(fields); i++) {
    std::unique_ptr<std::u16string> original_value(fields[i]->release());
    *fields[i] = std::make_unique<std::u16string>(u"fjdkslafjkldsa");
    EXPECT_TRUE(
        GetBookmarksMatchingProperties(model.get(), query, 100).empty());
    *fields[i] = std::move(original_value);
  }
}

// Ensures the BookmarkClient has the power to suggest the parent for new nodes.
TEST_F(BookmarkUtilsTest, GetParentForNewNodes_ClientOverride) {
  std::unique_ptr<SuggestFolderClient> client =
      std::make_unique<SuggestFolderClient>();
  SuggestFolderClient* client_ptr = client.get();
  std::unique_ptr<BookmarkModel> model(
      TestBookmarkClient::CreateModelWithClient(std::move(client)));

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  EXPECT_EQ(model->mobile_node(), GetParentForNewNodes(model.get(), GURL()));
#else
  EXPECT_EQ(model->other_node(), GetParentForNewNodes(model.get(), GURL()));
  model->CreateAccountPermanentFolders();
  EXPECT_EQ(model->account_other_node(),
            GetParentForNewNodes(model.get(), GURL()));
#endif

  const BookmarkNode* folder_to_suggest =
      model->AddFolder(model->bookmark_bar_node(), 0, u"Suggested");
  const BookmarkNode* folder1 =
      model->AddFolder(model->bookmark_bar_node(), 1, u"Folder 1");
  EXPECT_EQ(folder1, GetParentForNewNodes(model.get(), GURL()));

  client_ptr->SetSuggestedSaveLocation(folder_to_suggest);
  EXPECT_EQ(folder_to_suggest, GetParentForNewNodes(model.get(), GURL()));
  client_ptr = nullptr;
}

// Verifies that meta info is copied when nodes are cloned.
TEST_F(BookmarkUtilsTest, CloneMetaInfo) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  // Add a node containing meta info.
  const BookmarkNode* node = model->AddURL(model->other_node(), 0, u"foo bar",
                                           GURL("http://www.google.com"));
  model->SetNodeMetaInfo(node, "somekey", "somevalue");
  model->SetNodeMetaInfo(node, "someotherkey", "someothervalue");

  // Clone node to a different folder.
  const BookmarkNode* folder =
      model->AddFolder(model->bookmark_bar_node(), 0, u"Folder");
  std::vector<BookmarkNodeData::Element> elements;
  BookmarkNodeData::Element node_data(node);
  elements.push_back(node_data);
  ASSERT_EQ(0u, folder->children().size());
  CloneBookmarkNode(model.get(), elements, folder, 0, false);
  ASSERT_EQ(1u, folder->children().size());

  // Verify that the cloned node contains the same meta info.
  const BookmarkNode* clone = folder->children().front().get();
  ASSERT_TRUE(clone->GetMetaInfoMap());
  EXPECT_EQ(2u, clone->GetMetaInfoMap()->size());
  std::string value;
  EXPECT_TRUE(clone->GetMetaInfo("somekey", &value));
  EXPECT_EQ("somevalue", value);
  EXPECT_TRUE(clone->GetMetaInfo("someotherkey", &value));
  EXPECT_EQ("someothervalue", value);
  histogram()->ExpectTotalCount("Bookmarks.Clone.NumCloned", 1);
  histogram()->ExpectBucketCount("Bookmarks.Clone.NumCloned", 1, 1);
}

TEST_F(BookmarkUtilsTest, RemoveAllBookmarks) {
  // Load a model with an managed node that is not editable.
  auto client = std::make_unique<TestBookmarkClient>();
  BookmarkNode* managed_node = client->EnableManagedNode();

  std::unique_ptr<BookmarkModel> model(
      TestBookmarkClient::CreateModelWithClient(std::move(client)));
  ASSERT_TRUE(model->bookmark_bar_node()->children().empty());
  ASSERT_TRUE(model->other_node()->children().empty());
  ASSERT_TRUE(model->mobile_node()->children().empty());
  ASSERT_TRUE(managed_node->children().empty());

  const std::u16string title = u"Title";
  const GURL url("http://google.com");
  model->AddURL(model->bookmark_bar_node(), 0, title, url);
  model->AddURL(model->other_node(), 0, title, url);
  model->AddURL(model->mobile_node(), 0, title, url);
  model->AddURL(managed_node, 0, title, url);

  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes =
      model->GetNodesByURL(url);
  ASSERT_EQ(4u, nodes.size());

  RemoveAllBookmarks(model.get(), url, FROM_HERE);

  nodes = model->GetNodesByURL(url);
  ASSERT_EQ(1u, nodes.size());
  EXPECT_TRUE(model->bookmark_bar_node()->children().empty());
  EXPECT_TRUE(model->other_node()->children().empty());
  EXPECT_TRUE(model->mobile_node()->children().empty());
  EXPECT_EQ(1u, managed_node->children().size());
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(
    BookmarkUtilsTest,
    GetRecentlyUsedFoldersWithOnlyLocalBookmarks_PermanentNodesOrderUnaffectedByDisplay) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());

  // Note that because `local_other_bookmark` is a child of a permanent node its
  // use in `GetMostRecentlyUsedFoldersForDisplay()` will not cause
  // `other_node()` to be displayed before more recently modified folders.
  const BookmarkNode* const local_other_bookmark = model->AddURL(
      model->other_node(), 0, u"Title", GURL("http://google.com"));

  model->SetDateFolderModified(model->other_node(),
                               base::Time::FromMillisecondsSinceUnixEpoch(1));
  model->SetDateFolderModified(model->bookmark_bar_node(),
                               base::Time::FromMillisecondsSinceUnixEpoch(2));

  const bookmarks::BookmarkNodesSplitByAccountAndLocal mru_bookmarks =
      bookmarks::GetMostRecentlyUsedFoldersForDisplay(model.get(),
                                                      local_other_bookmark);

  // Permanent nodes display in a fixed order even if a bookmark in
  // `other_node()` is currently displayed.
  EXPECT_TRUE(mru_bookmarks.account_nodes.empty());
  EXPECT_THAT(mru_bookmarks.local_nodes,
              ElementsAre(model->bookmark_bar_node(), model->other_node()));
}

TEST_F(
    BookmarkUtilsTest,
    GetRecentlyUsedFoldersWithOnlyLocalBookmarks_NonPermanentNodesOrderAffectedByDisplay) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());

  // Add two folders.
  const BookmarkNode* const folder1 =
      model->AddFolder(model->other_node(), 0, u"Folder1");
  const BookmarkNode* const folder2 =
      model->AddFolder(model->other_node(), 0, u"Folder2");

  // Add a new bookmark to `folder1`.
  const BookmarkNode* const bookmark =
      model->AddURL(folder1, 0, u"Title", GURL("http://google.com"));

  // Set `folder2` to most recent.
  model->SetDateFolderModified(folder1,
                               base::Time::FromMillisecondsSinceUnixEpoch(1));
  model->SetDateFolderModified(folder2,
                               base::Time::FromMillisecondsSinceUnixEpoch(2));

  const bookmarks::BookmarkNodesSplitByAccountAndLocal mru_bookmarks =
      bookmarks::GetMostRecentlyUsedFoldersForDisplay(model.get(), bookmark);

  // `folder1` as a parent to `bookmark` displays first even though not most
  // recent.
  EXPECT_THAT(mru_bookmarks.local_nodes,
              ElementsAre(folder1, folder2, model->bookmark_bar_node(),
                          model->other_node()));
}

TEST_F(
    BookmarkUtilsTest,
    GetRecentlyUsedFoldersWithOnlyLocalBookmarks_DateFolderModifiedChangesOrder) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  const BookmarkNode* const local_other_bookmark = model->AddURL(
      model->other_node(), 0, u"Title", GURL("http://google.com"));

  // Add two folders.
  const BookmarkNode* const folder1 =
      model->AddFolder(model->other_node(), 0, u"Folder1");
  const BookmarkNode* const folder2 =
      model->AddFolder(model->other_node(), 0, u"Folder2");

  // `folder2` is set to most recent.
  model->SetDateFolderModified(folder1,
                               base::Time::FromMillisecondsSinceUnixEpoch(1));
  model->SetDateFolderModified(folder2,
                               base::Time::FromMillisecondsSinceUnixEpoch(2));

  bookmarks::BookmarkNodesSplitByAccountAndLocal mru_bookmarks =
      bookmarks::GetMostRecentlyUsedFoldersForDisplay(model.get(),
                                                      local_other_bookmark);
  EXPECT_THAT(mru_bookmarks.local_nodes,
              ElementsAre(folder2, folder1, model->bookmark_bar_node(),
                          model->other_node()));

  // `folder1` is set to most recent.
  model->SetDateFolderModified(folder1,
                               base::Time::FromMillisecondsSinceUnixEpoch(3));

  mru_bookmarks = bookmarks::GetMostRecentlyUsedFoldersForDisplay(
      model.get(), local_other_bookmark);
  EXPECT_THAT(mru_bookmarks.local_nodes,
              ElementsAre(folder1, folder2, model->bookmark_bar_node(),
                          model->other_node()));
}

TEST_F(
    BookmarkUtilsTest,
    GetRecentlyUsedFoldersWithOnlyLocalBookmarks_PermanentFoldersAlwaysShownWithCustomFolders) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  const BookmarkNode* const local_other_bookmark = model->AddURL(
      model->other_node(), 0, u"Title", GURL("http://google.com"));

  // Add more than 5 custom folders. The first one will not be displayed, as
  // only the 5 most recent ones are chosen for display.
  std::vector<const BookmarkNode*> custom_nodes;
  for (int i = 0; i < 6; i++) {
    custom_nodes.push_back(
        model->AddFolder(model->other_node(), 0, u"CustomFolder"));
  }
  CHECK_EQ(6u, custom_nodes.size());

  const bookmarks::BookmarkNodesSplitByAccountAndLocal mru_bookmarks =
      bookmarks::GetMostRecentlyUsedFoldersForDisplay(model.get(),
                                                      local_other_bookmark);

  // Only 5 custom nodes should be displayed. The permanent ones come last.
  EXPECT_TRUE(mru_bookmarks.account_nodes.empty());
  EXPECT_THAT(mru_bookmarks.local_nodes,
              ElementsAre(custom_nodes[5], custom_nodes[4], custom_nodes[3],
                          custom_nodes[2], custom_nodes[1],
                          model->bookmark_bar_node(), model->other_node()));
}

TEST_F(
    BookmarkUtilsTest,
    GetRecentlyUsedFoldersWithOnlyLocalBookmarks_CustomFolderShownWhenChildNodeDisplayed) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  std::vector<const BookmarkNode*> custom_nodes;

  // Add a first custom folder node and a new bookmark to it.
  custom_nodes.push_back(
      model->AddFolder(model->other_node(), 0, u"CustomFolder"));
  const BookmarkNode* const bookmark =
      model->AddURL(custom_nodes[0], 0, u"Title", GURL("http://google.com"));

  // Add 5 more custom folders now to make sure the first custom folder will
  // not be the most recently modified one.
  for (int i = 0; i < 5; i++) {
    custom_nodes.push_back(
        model->AddFolder(model->other_node(), 0, u"CustomFolder"));
  }
  CHECK_EQ(6u, custom_nodes.size());

  const bookmarks::BookmarkNodesSplitByAccountAndLocal mru_bookmarks =
      bookmarks::GetMostRecentlyUsedFoldersForDisplay(model.get(), bookmark);

  // Only 5 custom nodes should be displayed. The parent node of the currently
  // displayed bookmark comes first. The permanent nodes come last.
  EXPECT_TRUE(mru_bookmarks.account_nodes.empty());
  EXPECT_THAT(mru_bookmarks.local_nodes,
              ElementsAre(custom_nodes[0], custom_nodes[5], custom_nodes[4],
                          custom_nodes[3], custom_nodes[2],
                          model->bookmark_bar_node(), model->other_node()));
}

TEST_F(
    BookmarkUtilsTest,
    GetRecentlyUsedFoldersWithOnlyLocalBookmarks_CustomFolderShownWhenRecentlyModified) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  const BookmarkNode* const local_other_bookmark = model->AddURL(
      model->other_node(), 0, u"Title", GURL("http://google.com"));

  // Add more than 5 custom folders.
  std::vector<const BookmarkNode*> custom_nodes;
  for (int i = 0; i < 6; i++) {
    custom_nodes.push_back(
        model->AddFolder(model->other_node(), 0, u"CustomFolder"));
  }
  CHECK_EQ(6u, custom_nodes.size());

  // The first folder is set to most recently modified.
  model->SetDateFolderModified(custom_nodes[0], base::Time::Now());
  const bookmarks::BookmarkNodesSplitByAccountAndLocal mru_bookmarks =
      bookmarks::GetMostRecentlyUsedFoldersForDisplay(model.get(),
                                                      local_other_bookmark);

  // Only 5 custom nodes should be displayed. The recently modified node comes
  // first. The permanent nodes come last.
  EXPECT_TRUE(mru_bookmarks.account_nodes.empty());
  EXPECT_THAT(mru_bookmarks.local_nodes,
              ElementsAre(custom_nodes[0], custom_nodes[5], custom_nodes[4],
                          custom_nodes[3], custom_nodes[2],
                          model->bookmark_bar_node(), model->other_node()));
}

TEST_F(
    BookmarkUtilsTest,
    GetRecentlyUsedFoldersWithAccountBookmarks_PermanentNodesOrderUnaffectedByDisplay) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  model->CreateAccountPermanentFolders();

  // Note that because `bookmark_in_account_other_node` is a child of a
  // permanent node its use in `GetMostRecentlyUsedFoldersForDisplay()` will not
  // cause `account_other_node()` to be displayed before more recently modified
  // folders.
  const BookmarkNode* const bookmark_in_account_other_node = model->AddURL(
      model->account_other_node(), 0, u"Title", GURL("http://google.com"));

  const bookmarks::BookmarkNodesSplitByAccountAndLocal mru_bookmarks =
      bookmarks::GetMostRecentlyUsedFoldersForDisplay(
          model.get(), bookmark_in_account_other_node);

  // No local nodes display by default.
  EXPECT_TRUE(mru_bookmarks.local_nodes.empty());

  // Permanent nodes are only added to account nodes by default. They display in
  // a fixed order even if a bookmark in `account_other_node()` is currently
  // displayed.
  EXPECT_THAT(mru_bookmarks.account_nodes,
              ElementsAre(model->account_bookmark_bar_node(),
                          model->account_other_node()));
}

TEST_F(BookmarkUtilsTest,
       GetRecentlyUsedFoldersWithAccountBookmarks_LocalPermanentNodesNotShown) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  model->CreateAccountPermanentFolders();
  const BookmarkNode* const bookmark_in_account_other_node = model->AddURL(
      model->account_other_node(), 0, u"Title", GURL("http://google.com"));

  // The most recent folders under account and local are split up as the
  // topmost entries.
  const BookmarkNode* const account_folder =
      model->AddFolder(model->account_other_node(), 0, u"Folder");
  const BookmarkNode* const local_folder =
      model->AddFolder(model->other_node(), 0, u"Folder2");

  model->SetDateFolderModified(account_folder,
                               base::Time::FromMillisecondsSinceUnixEpoch(20));
  // Older than `account_folder` but not filtered out (not permanent node).
  model->SetDateFolderModified(local_folder,
                               base::Time::FromMillisecondsSinceUnixEpoch(10));

  const bookmarks::BookmarkNodesSplitByAccountAndLocal mru_bookmarks =
      bookmarks::GetMostRecentlyUsedFoldersForDisplay(
          model.get(), bookmark_in_account_other_node);

  // Permanent account folders are included, permanent local folders are not.
  EXPECT_THAT(mru_bookmarks.account_nodes,
              ElementsAre(account_folder, model->account_bookmark_bar_node(),
                          model->account_other_node()));
  EXPECT_THAT(mru_bookmarks.local_nodes, ElementsAre(local_folder));
}

TEST_F(
    BookmarkUtilsTest,
    GetRecentlyUsedFoldersWithAccountBookmarks_LocalPermanentNodeShownWhenChildDisplayed) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  model->CreateAccountPermanentFolders();
  const BookmarkNode* const local_other_bookmark = model->AddURL(
      model->other_node(), 0, u"Title", GURL("http://google.com"));

  // Make sure local permanent nodes are not the most recently modified ones.
  model->SetDateFolderModified(model->other_node(),
                               base::Time::FromMillisecondsSinceUnixEpoch(1));
  model->SetDateFolderModified(model->account_other_node(),
                               base::Time::FromMillisecondsSinceUnixEpoch(2));

  const bookmarks::BookmarkNodesSplitByAccountAndLocal mru_bookmarks =
      bookmarks::GetMostRecentlyUsedFoldersForDisplay(model.get(),
                                                      local_other_bookmark);

  // Local permanent node included when its child is being displayed.
  EXPECT_THAT(mru_bookmarks.local_nodes, ElementsAre(model->other_node()));
}

TEST_F(
    BookmarkUtilsTest,
    GetRecentlyUsedFoldersWithAccountBookmarks_LocalPermanentNodeShownWhenRecentlyModified) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  model->CreateAccountPermanentFolders();
  const base::Time account_folders_created_time = base::Time::Now();

  // Add a local node as well so that local permanent nodes are not filtered
  // out.
  model->AddURL(model->bookmark_bar_node(), 0, u"Title",
                GURL("http://google.com"));

  const BookmarkNode* const bookmark_in_account_other_node = model->AddURL(
      model->account_other_node(), 0, u"Title", GURL("http://google.com"));

  // Make sure a local permanent node is the most recently modified one.
  model->SetDateFolderModified(model->account_other_node(),
                               account_folders_created_time + base::Seconds(1));
  model->SetDateFolderModified(model->bookmark_bar_node(),
                               account_folders_created_time + base::Seconds(2));

  const bookmarks::BookmarkNodesSplitByAccountAndLocal mru_bookmarks =
      bookmarks::GetMostRecentlyUsedFoldersForDisplay(
          model.get(), bookmark_in_account_other_node);

  // Local permanent node included when most recently modified.
  EXPECT_THAT(mru_bookmarks.local_nodes,
              ElementsAre(model->bookmark_bar_node()));
}

TEST_F(
    BookmarkUtilsTest,
    GetRecentlyUsedFoldersWithAccountBookmarks_LocalPermanentNodeDisplayedOnlyOnce) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  model->CreateAccountPermanentFolders();
  const BookmarkNode* const local_bookmark = model->AddURL(
      model->bookmark_bar_node(), 0, u"Title", GURL("http://google.com"));

  // Make sure a local permanent node is the most recently modified one.
  model->SetDateFolderModified(model->account_other_node(),
                               base::Time::FromMillisecondsSinceUnixEpoch(1));
  model->SetDateFolderModified(model->bookmark_bar_node(),
                               base::Time::FromMillisecondsSinceUnixEpoch(2));

  // Display the bookmark saved to the local permanent node.
  const bookmarks::BookmarkNodesSplitByAccountAndLocal mru_bookmarks =
      bookmarks::GetMostRecentlyUsedFoldersForDisplay(model.get(),
                                                      local_bookmark);

  // The local permanent node is only included once, even though there are two
  // conditions fulfilled which would add it to the most recently used folders.
  EXPECT_THAT(mru_bookmarks.local_nodes,
              ElementsAre(model->bookmark_bar_node()));
}

TEST_F(
    BookmarkUtilsTest,
    GetRecentlyUsedFoldersWithAccountBookmarks_LocalPermanentNodesDisplayedLast) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  model->CreateAccountPermanentFolders();
  const base::Time account_folders_created_time = base::Time::Now();

  const BookmarkNode* const bookmark_in_account_other_node = model->AddURL(
      model->account_other_node(), 0, u"Title", GURL("http://google.com"));
  const BookmarkNode* const local_bookmark = model->AddURL(
      model->bookmark_bar_node(), 0, u"Title", GURL("http://google.com"));

  // Add two local folders.
  const BookmarkNode* const folder1 =
      model->AddFolder(model->bookmark_bar_node(), 0, u"Folder");
  const BookmarkNode* const folder2 =
      model->AddFolder(model->bookmark_bar_node(), 0, u"Folder2");

  // Set the account other node as most recently modified.
  model->SetDateFolderModified(folder1, account_folders_created_time);
  model->SetDateFolderModified(model->bookmark_bar_node(),
                               account_folders_created_time + base::Seconds(1));
  model->SetDateFolderModified(folder2,
                               account_folders_created_time + base::Seconds(2));
  model->SetDateFolderModified(model->account_other_node(),
                               account_folders_created_time + base::Seconds(3));

  bookmarks::BookmarkNodesSplitByAccountAndLocal mru_bookmarks =
      bookmarks::GetMostRecentlyUsedFoldersForDisplay(
          model.get(), bookmark_in_account_other_node);

  // When the permanent node is not the one most recently modified or the parent
  // of a currently displayed bookmark, it is not included in the list at all.
  EXPECT_THAT(mru_bookmarks.local_nodes, ElementsAre(folder2, folder1));

  mru_bookmarks = bookmarks::GetMostRecentlyUsedFoldersForDisplay(
      model.get(), local_bookmark);

  // When the permanent node is the parent of the displayed node, it is added at
  // the end.
  mru_bookmarks = bookmarks::GetMostRecentlyUsedFoldersForDisplay(
      model.get(), local_bookmark);
  EXPECT_THAT(mru_bookmarks.local_nodes,
              ElementsAre(folder2, folder1, model->bookmark_bar_node()));

  // When the permanent node is the most recently modified node, it is added at
  // the end.
  model->SetDateFolderModified(
      model->bookmark_bar_node(),
      account_folders_created_time + base::Seconds(10));
  mru_bookmarks = bookmarks::GetMostRecentlyUsedFoldersForDisplay(
      model.get(), bookmark_in_account_other_node);

  EXPECT_THAT(mru_bookmarks.local_nodes,
              ElementsAre(folder2, folder1, model->bookmark_bar_node()));
}

TEST_F(
    BookmarkUtilsTest,
    GetRecentlyUsedFoldersWithAccountBookmarks_MultipleLocalPermanentNodesDisplayed) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  model->CreateAccountPermanentFolders();
  const base::Time account_folders_created_time = base::Time::Now();

  const BookmarkNode* const local_bookmark = model->AddURL(
      model->bookmark_bar_node(), 0, u"Title", GURL("http://google.com"));

  // Set the local other node as most recently modified.
  model->SetDateFolderModified(model->bookmark_bar_node(),
                               account_folders_created_time + base::Seconds(1));
  model->SetDateFolderModified(model->account_bookmark_bar_node(),
                               account_folders_created_time + base::Seconds(1));
  model->SetDateFolderModified(model->account_other_node(),
                               account_folders_created_time + base::Seconds(1));
  model->SetDateFolderModified(model->other_node(),
                               account_folders_created_time + base::Seconds(2));

  const bookmarks::BookmarkNodesSplitByAccountAndLocal mru_bookmarks =
      bookmarks::GetMostRecentlyUsedFoldersForDisplay(model.get(),
                                                      local_bookmark);

  // The local permanent nodes are both shown as one of them is the parent of
  // the displayed node, and one is the most recently modified.
  EXPECT_THAT(mru_bookmarks.local_nodes,
              ElementsAre(model->other_node(), model->bookmark_bar_node()));
}

TEST_F(
    BookmarkUtilsTest,
    GetRecentlyUsedFoldersWithAccountBookmarks_AccountPermanentFoldersAlwaysShownWithCustomFolders) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  model->CreateAccountPermanentFolders();
  const BookmarkNode* const bookmark_in_account_other_node = model->AddURL(
      model->account_other_node(), 0, u"Title", GURL("http://google.com"));

  // Add more than 5 custom folders. The first one will not be displayed, as
  // only the 5 most recent ones are chosen for display.
  std::vector<const BookmarkNode*> custom_nodes;
  for (int i = 0; i < 6; i++) {
    custom_nodes.push_back(model->AddFolder(model->account_other_node(), 0,
                                            u"CustomAccountFolder"));
  }
  CHECK_EQ(6u, custom_nodes.size());

  const bookmarks::BookmarkNodesSplitByAccountAndLocal mru_bookmarks =
      bookmarks::GetMostRecentlyUsedFoldersForDisplay(
          model.get(), bookmark_in_account_other_node);

  // Only 5 custom nodes should be displayed. The account permanent ones come
  // last. Local permanent nodes should not be included.
  EXPECT_TRUE(mru_bookmarks.local_nodes.empty());
  EXPECT_THAT(mru_bookmarks.account_nodes,
              ElementsAre(custom_nodes[5], custom_nodes[4], custom_nodes[3],
                          custom_nodes[2], custom_nodes[1],
                          model->account_bookmark_bar_node(),
                          model->account_other_node()));
}

TEST_F(
    BookmarkUtilsTest,
    GetRecentlyUsedFoldersWithAccountBookmarks_CustomFolderShownWhenChildNodeDisplayed) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  model->CreateAccountPermanentFolders();
  std::vector<const BookmarkNode*> custom_nodes;

  // Add a first custom folder node and a new bookmark to it.
  custom_nodes.push_back(
      model->AddFolder(model->account_other_node(), 0, u"CustomAccountFolder"));
  const BookmarkNode* const bookmark =
      model->AddURL(custom_nodes[0], 0, u"Title", GURL("http://google.com"));

  // Add 5 more custom folders now to make sure the first custom folder will
  // not be the most recently modified one.
  for (int i = 0; i < 5; i++) {
    custom_nodes.push_back(model->AddFolder(model->account_other_node(), 0,
                                            u"CustomAccountFolder"));
  }
  CHECK_EQ(6u, custom_nodes.size());

  const bookmarks::BookmarkNodesSplitByAccountAndLocal mru_bookmarks =
      bookmarks::GetMostRecentlyUsedFoldersForDisplay(model.get(), bookmark);

  // Only 5 custom nodes should be displayed. The parent node of the currently
  // displayed bookmark comes first. The account permanent nodes come last.
  // Local permanent nodes should not be included.
  EXPECT_TRUE(mru_bookmarks.local_nodes.empty());
  EXPECT_THAT(mru_bookmarks.account_nodes,
              ElementsAre(custom_nodes[0], custom_nodes[5], custom_nodes[4],
                          custom_nodes[3], custom_nodes[2],
                          model->account_bookmark_bar_node(),
                          model->account_other_node()));
}

TEST_F(
    BookmarkUtilsTest,
    GetRecentlyUsedFoldersWithAccountBookmarks_CustomFolderShownWhenRecentlyModified) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  model->CreateAccountPermanentFolders();
  const BookmarkNode* const bookmark_in_account_other_node = model->AddURL(
      model->account_other_node(), 0, u"Title", GURL("http://google.com"));

  // Add more than 5 custom folders.
  std::vector<const BookmarkNode*> custom_nodes;
  for (int i = 0; i < 6; i++) {
    custom_nodes.push_back(model->AddFolder(model->account_other_node(), 0,
                                            u"CustomAccountFolder"));
  }
  CHECK_EQ(6u, custom_nodes.size());

  // The first folder is set to most recently modified.
  model->SetDateFolderModified(custom_nodes[0], base::Time::Now());
  const bookmarks::BookmarkNodesSplitByAccountAndLocal mru_bookmarks =
      bookmarks::GetMostRecentlyUsedFoldersForDisplay(
          model.get(), bookmark_in_account_other_node);

  // Only 5 custom nodes should be displayed. The recently modified node comes
  // first. The account permanent nodes come last. Local permanent nodes should
  // not be included.
  EXPECT_TRUE(mru_bookmarks.local_nodes.empty());
  EXPECT_THAT(mru_bookmarks.account_nodes,
              ElementsAre(custom_nodes[0], custom_nodes[5], custom_nodes[4],
                          custom_nodes[3], custom_nodes[2],
                          model->account_bookmark_bar_node(),
                          model->account_other_node()));
}

TEST_F(
    BookmarkUtilsTest,
    GetRecentlyUsedFoldersWithAccountBookmarks_CustomFolderMaximumDisplayedIndependentOfStorage) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  model->CreateAccountPermanentFolders();
  const BookmarkNode* const bookmark_in_account_other_node = model->AddURL(
      model->account_other_node(), 0, u"Title", GURL("http://google.com"));

  // Add 3 custom folders to each account and local permanent nodes.
  std::vector<const BookmarkNode*> custom_nodes;
  for (int i = 0; i < 3; i++) {
    custom_nodes.push_back(
        model->AddFolder(model->other_node(), 0, u"CustomLocalFolder"));
    custom_nodes.push_back(model->AddFolder(model->account_other_node(), 0,
                                            u"CustomAccountFolder"));
  }
  CHECK_EQ(6u, custom_nodes.size());

  const bookmarks::BookmarkNodesSplitByAccountAndLocal mru_bookmarks =
      bookmarks::GetMostRecentlyUsedFoldersForDisplay(
          model.get(), bookmark_in_account_other_node);

  // Only 5 custom nodes should be displayed. These are the most recently added
  // ones. The account permanent nodes come last. Local permanent nodes should
  // not be included.
  EXPECT_THAT(mru_bookmarks.account_nodes,
              ElementsAre(custom_nodes[5], custom_nodes[3], custom_nodes[1],
                          model->account_bookmark_bar_node(),
                          model->account_other_node()));
  EXPECT_THAT(mru_bookmarks.local_nodes,
              ElementsAre(custom_nodes[4], custom_nodes[2]));
}

TEST_F(BookmarkUtilsTest, GetPermanentNodesForDisplayWithOnlyLocalBookmarks) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  const BookmarkNodesSplitByAccountAndLocal permanent_display_nodes =
      GetPermanentNodesForDisplay(model.get());

  EXPECT_TRUE(permanent_display_nodes.account_nodes.empty());
  EXPECT_THAT(permanent_display_nodes.local_nodes,
              ElementsAre(model->bookmark_bar_node(), model->other_node()));
}

TEST_F(BookmarkUtilsTest, GetPermanentNodesForDisplayWithAccountBookmarks) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  model->CreateAccountPermanentFolders();
  BookmarkNodesSplitByAccountAndLocal permanent_display_nodes =
      GetPermanentNodesForDisplay(model.get());

  ASSERT_FALSE(HasLocalOrSyncableBookmarks(model.get()));
  ASSERT_TRUE(permanent_display_nodes.local_nodes.empty());
  ASSERT_THAT(permanent_display_nodes.account_nodes,
              ElementsAre(model->account_bookmark_bar_node(),
                          model->account_other_node()));

  // With visible local/syncable bookmarks we should display visible local
  // permanent nodes too.
  model->AddURL(model->other_node(), 0, u"Title", GURL("http://google.com"));
  ASSERT_TRUE(HasLocalOrSyncableBookmarks(model.get()));

  permanent_display_nodes = GetPermanentNodesForDisplay(model.get());
  // Account nodes still showing.
  EXPECT_THAT(permanent_display_nodes.account_nodes,
              ElementsAre(model->account_bookmark_bar_node(),
                          model->account_other_node()));
  // Local nodes too.
  EXPECT_THAT(permanent_display_nodes.local_nodes,
              ElementsAre(model->bookmark_bar_node(), model->other_node()));
}

TEST_F(BookmarkUtilsTest, GetPermanentNodesForDisplayWithSyncEnabled) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  static_cast<TestBookmarkClient*>(model->client())
      ->SetIsSyncFeatureEnabledIncludingBookmarks(true);

  const BookmarkNodesSplitByAccountAndLocal permanent_display_nodes =
      GetPermanentNodesForDisplay(model.get());

  EXPECT_TRUE(permanent_display_nodes.account_nodes.empty());
  EXPECT_THAT(permanent_display_nodes.local_nodes,
              ElementsAre(model->bookmark_bar_node(), model->other_node()));
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

TEST_F(BookmarkUtilsTest,
       GetMostRecentlyModifiedUserFolders_DefaultOrderWithoutAccountBookmarks) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  ASSERT_FALSE(model->account_other_node());

  // The local other node should come first in order.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  EXPECT_THAT(GetMostRecentlyModifiedUserFolders(model.get()),
              ElementsAre(model->mobile_node()));
#else   // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  EXPECT_THAT(GetMostRecentlyModifiedUserFolders(model.get()),
              ElementsAre(model->other_node(), model->bookmark_bar_node()));
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
}

TEST_F(BookmarkUtilsTest,
       GetMostRecentlyModifiedUserFolders_DefaultOrderWithAccountBookmarks) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  model->CreateAccountPermanentFolders();

  // The account other node should come first in order. Local nodes are not
  // included if empty.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  EXPECT_THAT(GetMostRecentlyModifiedUserFolders(model.get()),
              ElementsAre(model->account_mobile_node(), model->mobile_node()));
#else   // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  EXPECT_THAT(GetMostRecentlyModifiedUserFolders(model.get()),
              ElementsAre(model->account_other_node(),
                          model->account_bookmark_bar_node()));
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
}
TEST_F(
    BookmarkUtilsTest,
    GetMostRecentlyModifiedUserFolders_LocalFolderModifiedBeforeAccountFoldersAdded) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());

  // Add a bookmark to the local bookmark bar node, so it should be visible and
  // first in the list.
  model->AddURL(model->bookmark_bar_node(), 0, u"Title",
                GURL("http://google.com"));

  std::vector<const BookmarkNode*> recently_modified =
      GetMostRecentlyModifiedUserFolders(model.get());
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  ASSERT_THAT(recently_modified,
              ElementsAre(model->bookmark_bar_node(), model->mobile_node()));
#else   // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  ASSERT_THAT(recently_modified,
              ElementsAre(model->bookmark_bar_node(), model->other_node()));
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

  // Creating the account permanent folders should push the account other node
  // to the front of the list.
  model->CreateAccountPermanentFolders();
  recently_modified = GetMostRecentlyModifiedUserFolders(model.get());

  // The permanent account nodes should come first in order, then the local
  // ones. The account other node comes first.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  EXPECT_THAT(recently_modified,
              ElementsAre(model->account_mobile_node(),
                          model->bookmark_bar_node(), model->mobile_node()));
#else   // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  EXPECT_THAT(recently_modified,
              ElementsAre(model->account_other_node(),
                          model->account_bookmark_bar_node(),
                          model->bookmark_bar_node(), model->other_node()));
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
}

TEST_F(
    BookmarkUtilsTest,
    GetMostRecentlyModifiedUserFolders_LocalFolderModifiedAfterAccountFoldersAdded) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  model->CreateAccountPermanentFolders();

  // Add a bookmark to the local bookmark bar node after the account nodes were
  // added, so it should be visible and first in the list.
  model->AddURL(model->bookmark_bar_node(), 0, u"Title",
                GURL("http://google.com"));

  // The local bookmark bar should come first in order.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  EXPECT_THAT(GetMostRecentlyModifiedUserFolders(model.get()),
              ElementsAre(model->bookmark_bar_node(),
                          model->account_mobile_node(), model->mobile_node()));
#else   // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  EXPECT_THAT(
      GetMostRecentlyModifiedUserFolders(model.get()),
      ElementsAre(model->bookmark_bar_node(), model->account_other_node(),
                  model->account_bookmark_bar_node(), model->other_node()));
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
}

TEST_F(
    BookmarkUtilsTest,
    GetMostRecentlyModifiedUserFolders_AccountFolderModifiedAfterAccountFoldersAdded) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  model->CreateAccountPermanentFolders();

  // Add a bookmark to the account bookmark bar node after the account nodes
  // were added, so it should be first in the list.
  model->AddURL(model->account_bookmark_bar_node(), 0, u"Title",
                GURL("http://google.com"));

  // The account bookmark bar should come first in order.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  EXPECT_THAT(GetMostRecentlyModifiedUserFolders(model.get()),
              ElementsAre(model->account_bookmark_bar_node(),
                          model->account_mobile_node(), model->mobile_node()));
#else   // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  EXPECT_THAT(GetMostRecentlyModifiedUserFolders(model.get()),
              ElementsAre(model->account_bookmark_bar_node(),
                          model->account_other_node()));
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
}

TEST_F(
    BookmarkUtilsTest,
    GetMostRecentlyModifiedUserFolders_DefinedModifiedOrderWithCustomLocalFolders) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  model->CreateAccountPermanentFolders();

  const BookmarkNode* const local_folder1 =
      model->AddFolder(model->other_node(), 0, u"Folder");
  const BookmarkNode* const local_folder2 =
      model->AddFolder(model->bookmark_bar_node(), 0, u"Folder2");

  model->SetDateFolderModified(model->account_other_node(),
                               base::Time::FromMillisecondsSinceUnixEpoch(0));
  model->SetDateFolderModified(model->account_bookmark_bar_node(),
                               base::Time::FromMillisecondsSinceUnixEpoch(1));

  model->SetDateFolderModified(model->bookmark_bar_node(),
                               base::Time::FromMillisecondsSinceUnixEpoch(2));
  model->SetDateFolderModified(local_folder2,
                               base::Time::FromMillisecondsSinceUnixEpoch(3));
  model->SetDateFolderModified(model->other_node(),
                               base::Time::FromMillisecondsSinceUnixEpoch(4));
  model->SetDateFolderModified(local_folder1,
                               base::Time::FromMillisecondsSinceUnixEpoch(5));

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  model->SetDateFolderModified(model->mobile_node(),
                               base::Time::FromMillisecondsSinceUnixEpoch(6));
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

  // This simulates signing out and in again, or turning account storage for
  // bookmarks off and on through the settings. Doing this should row the
  // account permanent nodes first, and then follow the order of the last
  // recently modified folders.
  model->RemoveAccountPermanentFolders();
  model->CreateAccountPermanentFolders();

  // The permanent account nodes should come first in order, then the local
  // permanent folders and non-permanent nodes. The account other node comes
  // first.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  EXPECT_THAT(GetMostRecentlyModifiedUserFolders(model.get()),
              ElementsAre(model->account_mobile_node(), model->mobile_node(),
                          local_folder1, model->other_node(), local_folder2,
                          model->bookmark_bar_node()));
#else   // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  EXPECT_THAT(GetMostRecentlyModifiedUserFolders(model.get()),
              ElementsAre(model->account_other_node(),
                          model->account_bookmark_bar_node(), local_folder1,
                          model->other_node(), local_folder2,
                          model->bookmark_bar_node()));
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
}

}  // namespace
}  // namespace bookmarks
