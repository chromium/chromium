// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>

#include "base/containers/contains.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/power_bookmarks/core/bookmark_client_base.h"
#include "components/power_bookmarks/core/suggested_save_location_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace power_bookmarks {

namespace {

class TestBookmarkClientImpl : public BookmarkClientBase {
 public:
  TestBookmarkClientImpl() = default;
  TestBookmarkClientImpl(const TestBookmarkClientImpl&) = delete;
  TestBookmarkClientImpl& operator=(const TestBookmarkClientImpl&) = delete;
  ~TestBookmarkClientImpl() override = default;

  bool IsPermanentNodeVisibleWhenEmpty(
      bookmarks::BookmarkNode::Type type) override {
    return true;
  }

  bookmarks::LoadManagedNodeCallback GetLoadManagedNodeCallback() override {
    return bookmarks::LoadManagedNodeCallback();
  }

  bookmarks::metrics::StorageStateForUma GetStorageStateForUma() override {
    return bookmarks::metrics::StorageStateForUma::kLocalOnly;
  }

  bool CanSetPermanentNodeTitle(
      const bookmarks::BookmarkNode* permanent_node) override {
    return false;
  }

  bool IsNodeManaged(const bookmarks::BookmarkNode* node) override {
    return false;
  }

  std::string EncodeBookmarkSyncMetadata() override { return ""; }

  void DecodeBookmarkSyncMetadata(
      const std::string& metadata_str,
      const base::RepeatingClosure& schedule_save_closure) override {}

  void OnBookmarkNodeRemovedUndoable(
      bookmarks::BookmarkModel* model,
      const bookmarks::BookmarkNode* parent,
      size_t index,
      std::unique_ptr<bookmarks::BookmarkNode> node) override {}
};

class MockSuggestionProvider : public SuggestedSaveLocationProvider {
 public:
  MockSuggestionProvider() = default;
  MockSuggestionProvider(const MockSuggestionProvider&) = delete;
  MockSuggestionProvider& operator=(const MockSuggestionProvider&) = delete;
  ~MockSuggestionProvider() override = default;

  MOCK_METHOD(const bookmarks::BookmarkNode*,
              GetSuggestion,
              (const GURL& url),
              (override));
  MOCK_METHOD(const base::TimeDelta, GetBackoffTime, (), (override));
};

}  // namespace

// Tests for the bookmark client base.
class BookmarkClientBaseTest : public testing::Test {
 protected:
  void SetUp() override {
    std::unique_ptr<TestBookmarkClientImpl> client =
        std::make_unique<TestBookmarkClientImpl>();
    client_ = client.get();

    model_ =
        bookmarks::TestBookmarkClient::CreateModelWithClient(std::move(client));
  }

  void TearDown() override { client_ = nullptr; }

  bookmarks::BookmarkModel* model() { return model_.get(); }
  BookmarkClientBase* client() { return client_; }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  std::unique_ptr<bookmarks::BookmarkModel> model_;
  raw_ptr<TestBookmarkClientImpl> client_;
};

TEST_F(BookmarkClientBaseTest, SuggestedFolder) {
  const GURL url_for_suggestion("http://example.com");
  const bookmarks::BookmarkNode* suggested_folder =
      model()->AddFolder(model()->other_node(), 0, u"suggested folder");

  MockSuggestionProvider provider;
  ON_CALL(provider, GetSuggestion)
      .WillByDefault([suggested_folder, url_for_suggestion](const GURL& url) {
        // Provide a suggested save location for a very specific URL.
        return url == url_for_suggestion ? suggested_folder : nullptr;
      });
  ON_CALL(provider, GetBackoffTime)
      .WillByDefault(testing::Return(base::Hours(2)));

  client()->AddSuggestedSaveLocationProvider(&provider);

  bookmarks::AddIfNotBookmarked(model(), url_for_suggestion, u"bookmark");

  // The bookmark should have been added to the suggested location.
  const bookmarks::BookmarkNode* node =
      model()->GetMostRecentlyAddedUserNodeForURL(url_for_suggestion);
  ASSERT_EQ(node->parent(), suggested_folder);

  // Save another bookmark to ensure the suggested location is not used for the
  // next save.
  const GURL normal_bookmark_url = GURL("http://example.com/normal");
  bookmarks::AddIfNotBookmarked(model(), normal_bookmark_url, u"bookmark 2");
  node = model()->GetMostRecentlyAddedUserNodeForURL(normal_bookmark_url);
  ASSERT_NE(node->parent(), suggested_folder);

  client()->RemoveSuggestedSaveLocationProvider(&provider);
}

TEST_F(BookmarkClientBaseTest, SuggestedFolder_Rejected) {
  const GURL url_for_suggestion("http://example.com");
  const GURL url_for_suggestion2("http://example.com/other");
  std::set<const GURL> url_set = {url_for_suggestion, url_for_suggestion2};
  const bookmarks::BookmarkNode* suggested_folder =
      model()->AddFolder(model()->other_node(), 0, u"suggested folder");

  MockSuggestionProvider provider;
  ON_CALL(provider, GetSuggestion)
      .WillByDefault([suggested_folder, url_set](const GURL& url) {
        // Suggest for multiple URLs.
        return base::Contains(url_set, url) ? suggested_folder : nullptr;
      });
  ON_CALL(provider, GetBackoffTime)
      .WillByDefault(testing::Return(base::Hours(2)));

  client()->AddSuggestedSaveLocationProvider(&provider);

  bookmarks::AddIfNotBookmarked(model(), url_for_suggestion, u"bookmark");

  // The bookmark should have been added to the suggested location.
  const bookmarks::BookmarkNode* node =
      model()->GetMostRecentlyAddedUserNodeForURL(url_for_suggestion);
  ASSERT_EQ(node->parent(), suggested_folder);

  // Move the new bookmark. This indicates the user did not like the suggested
  // location and changed its location in the hierarchy.
  model()->Move(node, model()->other_node(),
                model()->other_node()->children().size());

  // Save another bookmark to ensure the suggested location is not used for the
  // next save.
  bookmarks::AddIfNotBookmarked(model(), url_for_suggestion2, u"bookmark 2");
  node = model()->GetMostRecentlyAddedUserNodeForURL(url_for_suggestion2);
  ASSERT_NE(node->parent(), suggested_folder);

  task_environment_.FastForwardBy(base::Hours(3));

  // Remove and re-bookmark the second URL. The suggested folder should be
  // allowed again.
  model()->Remove(node, bookmarks::metrics::BookmarkEditSource::kUser);

  bookmarks::AddIfNotBookmarked(model(), url_for_suggestion2, u"bookmark 2");
  node = model()->GetMostRecentlyAddedUserNodeForURL(url_for_suggestion2);
  ASSERT_EQ(node->parent(), suggested_folder);

  client()->RemoveSuggestedSaveLocationProvider(&provider);
}

// The suggested folder should be allowed for "normal" saves if it was
// explicitly saved to in the past.
TEST_F(BookmarkClientBaseTest, SuggestedFolder_ExplicitSave) {
  const GURL url_for_suggestion("http://example.com");
  const bookmarks::BookmarkNode* suggested_folder =
      model()->AddFolder(model()->other_node(), 0, u"suggested folder");

  MockSuggestionProvider provider;
  ON_CALL(provider, GetSuggestion)
      .WillByDefault([suggested_folder, url_for_suggestion](const GURL& url) {
        // Provide a suggested save location for a very specific URL.
        return url == url_for_suggestion ? suggested_folder : nullptr;
      });
  ON_CALL(provider, GetBackoffTime)
      .WillByDefault(testing::Return(base::Hours(2)));

  client()->AddSuggestedSaveLocationProvider(&provider);

  bookmarks::AddIfNotBookmarked(model(), url_for_suggestion, u"bookmark 0");

  // The bookmark should have been added to the suggested location.
  const bookmarks::BookmarkNode* node =
      model()->GetMostRecentlyAddedUserNodeForURL(url_for_suggestion);
  ASSERT_EQ(node->parent(), suggested_folder);

  // Save another bookmark to the suggested folder explicitly, even though the
  // system wouldn't normally suggest it.
  const GURL normal_bookmark_url1 = GURL("http://example.com/normal_1");
  bookmarks::AddIfNotBookmarked(model(), normal_bookmark_url1, u"bookmark 1",
                                suggested_folder);
  node = model()->GetMostRecentlyAddedUserNodeForURL(normal_bookmark_url1);
  ASSERT_EQ(node->parent(), suggested_folder);

  // Save another bookmark. Even though the folder is suggested by a feature,
  // the user previously saved to it explicitly. In this case we're allowed to
  // suggest it again.
  const GURL normal_bookmark_url2 = GURL("http://example.com/normal_2");
  bookmarks::AddIfNotBookmarked(model(), normal_bookmark_url2, u"bookmark 2");
  node = model()->GetMostRecentlyAddedUserNodeForURL(normal_bookmark_url2);
  ASSERT_EQ(node->parent(), suggested_folder);

  client()->RemoveSuggestedSaveLocationProvider(&provider);
}

}  // namespace power_bookmarks
