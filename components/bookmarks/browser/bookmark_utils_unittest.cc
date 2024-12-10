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
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_client.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/bookmarks/test/test_bookmark_client.h"
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

}  // namespace
}  // namespace bookmarks
