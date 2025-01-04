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
#include "components/sync/base/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

namespace bookmarks {
namespace {

using base::ASCIIToUTF16;
using std::string;
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
      syncer::kSyncEnableBookmarksInTransportMode};

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

  const BookmarkNode* folder_to_suggest =
      model->AddFolder(model->bookmark_bar_node(), 0, u"Suggested");
  const BookmarkNode* folder1 =
      model->AddFolder(model->bookmark_bar_node(), 1, u"Folder 1");

  ASSERT_EQ(folder1, GetParentForNewNodes(model.get(), GURL()));

  client_ptr->SetSuggestedSaveLocation(folder_to_suggest);

  ASSERT_EQ(folder_to_suggest, GetParentForNewNodes(model.get(), GURL()));

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
  EXPECT_EQ(0u, folder->children().size());
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
  EXPECT_TRUE(model->bookmark_bar_node()->children().empty());
  EXPECT_TRUE(model->other_node()->children().empty());
  EXPECT_TRUE(model->mobile_node()->children().empty());
  EXPECT_TRUE(managed_node->children().empty());

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

TEST_F(BookmarkUtilsTest, CleanUpUrlForMatching) {
  EXPECT_EQ(u"http://foo.com/", CleanUpUrlForMatching(GURL("http://foo.com"),
                                                      /*adjustments=*/nullptr));
  EXPECT_EQ(u"http://foo.com/", CleanUpUrlForMatching(GURL("http://Foo.com"),
                                                      /*adjustments=*/nullptr));
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// TODO(crbug.com/380820764): Break this up into smaller parts that:
// * Add more than 5 custom folders and make sure that permanent nodes still
//   show.
// * Add 6 custom folders, make sure that only 5 show and that the folder past
//   the cutoff starts showing if it's the parent of the currently showing
//   bookmark.
// * Permanent nodes don't show up first even if they are the parents of the
//   currently displaying bookmarks.
TEST_F(BookmarkUtilsTest, GetRecentlyUsedFoldersWithOnlyLocalBookmarks) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());

  const std::u16string title = u"Title";
  const GURL url("http://google.com");
  // Note that because `other_bookmark` is a child of a permanent node its use
  // in GetMostRecentlyUsedFoldersForDisplay will not cause other_node() to be
  // displayed before more recently modified folders.
  const BookmarkNode* const other_bookmark =
      model->AddURL(model->other_node(), 0, title, url);

  const bookmarks::RecentlyUsedFolders mru_bookmarks =
      bookmarks::GetMostRecentlyUsedFoldersForDisplay(model.get(),
                                                      other_bookmark);

  EXPECT_TRUE(mru_bookmarks.account_nodes.empty());
  ASSERT_EQ(mru_bookmarks.local_nodes.size(), 2u);
  // Permanent nodes display in a fixed order even if a bookmark in other_node()
  // is currently displayed.
  EXPECT_EQ(mru_bookmarks.local_nodes[0], model->bookmark_bar_node());
  EXPECT_EQ(mru_bookmarks.local_nodes[1], model->other_node());

  const BookmarkNode* const folder1 =
      model->AddFolder(model->other_node(), 0, u"Folder");
  const BookmarkNode* const folder2 =
      model->AddFolder(model->other_node(), 0, u"Folder2");

  model->SetDateFolderModified(folder1,
                               base::Time::FromMillisecondsSinceUnixEpoch(1));
  model->SetDateFolderModified(folder2,
                               base::Time::FromMillisecondsSinceUnixEpoch(2));

  // folder2 is most recent
  EXPECT_EQ(folder2, bookmarks::GetMostRecentlyUsedFoldersForDisplay(
                         model.get(), other_bookmark)
                         .local_nodes[0]);

  model->SetDateFolderModified(folder1,
                               base::Time::FromMillisecondsSinceUnixEpoch(3));
  // folder1 is most recent
  EXPECT_EQ(folder1, bookmarks::GetMostRecentlyUsedFoldersForDisplay(
                         model.get(), other_bookmark)
                         .local_nodes[0]);

  const BookmarkNode* const bookmark = model->AddURL(folder2, 0, title, url);

  // folder2 as a parent to `bookmark` displays first even though not most
  // recent.
  EXPECT_EQ(folder2, bookmarks::GetMostRecentlyUsedFoldersForDisplay(
                         model.get(), bookmark)
                         .local_nodes[0]);
}

// TODO(crbug.com/380820764): Break this up into smaller parts and make sure we
// have full coverage for:
// * Add more than 5 custom folders and make sure that account permanent nodes
//   still show.
// * Add 6 custom folders, make sure that the oldest one is cut off (regardless
//   of local or account). Make sure that the oldest one starts showing if it's
//   the parent of a currently-showing bookmarl (and the second-oldest one is
//   gone instead).
// * Make sure that local permanent nodes show up if most recently used and if
//   the displayed bookmark is a child of that local node it's only added once
//   (no duplicates).
// * Make sure local permanent nodes show up even if the displayed bookmark is
//   another permanent node (both local permanent nodes can show).
// * Make sure that local permanent nodes show up last among local nodes even if
//   they are the most recent ones or parents of the current currently-displayed
//   bookmark.
// * Make sure local nodes are empty if there's >5 more recently used folders
//   under account bookmarks.
TEST_F(BookmarkUtilsTest, GetRecentlyUsedFoldersWithAccountBookmarks) {
  std::unique_ptr<BookmarkModel> model(TestBookmarkClient::CreateModel());
  model->CreateAccountPermanentFolders();

  const std::u16string title = u"Title";
  const GURL url("http://google.com");
  // Note that because `bookmark_in_account_other_node` is a child of a
  // permanent node its use in GetMostRecentlyUsedFoldersForDisplay will not
  // cause account_other_node() to be displayed before more recently modified
  // folders.
  const BookmarkNode* const bookmark_in_account_other_node =
      model->AddURL(model->account_other_node(), 0, title, url);

  bookmarks::RecentlyUsedFolders mru_bookmarks =
      bookmarks::GetMostRecentlyUsedFoldersForDisplay(
          model.get(), bookmark_in_account_other_node);

  // Permanent nodes are only added to account nodes by default.
  ASSERT_EQ(mru_bookmarks.account_nodes.size(), 2u);
  // No local nodes display by default.
  EXPECT_TRUE(mru_bookmarks.local_nodes.empty());

  // Permanent nodes display in a fixed order even if a bookmark in
  // account_other_node() is currently displayed.
  EXPECT_EQ(mru_bookmarks.account_nodes[0], model->account_bookmark_bar_node());
  EXPECT_EQ(mru_bookmarks.account_nodes[1], model->account_other_node());

  const BookmarkNode* const local_other_bookmark =
      model->AddURL(model->other_node(), 0, title, url);

  // Make sure local permanent nodes are not the most recently modified ones.
  model->SetDateFolderModified(model->other_node(),
                               base::Time::FromMillisecondsSinceUnixEpoch(1));
  model->SetDateFolderModified(model->account_other_node(),
                               base::Time::FromMillisecondsSinceUnixEpoch(2));

  mru_bookmarks = bookmarks::GetMostRecentlyUsedFoldersForDisplay(
      model.get(), local_other_bookmark);

  // Local permanent node included when its children are being displayed.
  ASSERT_EQ(mru_bookmarks.local_nodes.size(), 1u);
  EXPECT_EQ(model->other_node(), mru_bookmarks.local_nodes[0]);

  // But not otherwise.
  EXPECT_TRUE(bookmarks::GetMostRecentlyUsedFoldersForDisplay(
                  model.get(), bookmark_in_account_other_node)
                  .local_nodes.empty());

  // The most recent folders under account and local are split up as the topmost
  // entries.
  const BookmarkNode* const account_folder =
      model->AddFolder(model->account_other_node(), 0, u"Folder");
  const BookmarkNode* const local_folder =
      model->AddFolder(model->other_node(), 0, u"Folder2");

  model->SetDateFolderModified(account_folder,
                               base::Time::FromMillisecondsSinceUnixEpoch(20));
  // Older than `account_folder` but not filtered out (not permanent node).
  model->SetDateFolderModified(local_folder,
                               base::Time::FromMillisecondsSinceUnixEpoch(10));

  mru_bookmarks = bookmarks::GetMostRecentlyUsedFoldersForDisplay(
      model.get(), bookmark_in_account_other_node);
  EXPECT_EQ(account_folder, mru_bookmarks.account_nodes[0]);
  EXPECT_EQ(local_folder, mru_bookmarks.local_nodes[0]);
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace
}  // namespace bookmarks
