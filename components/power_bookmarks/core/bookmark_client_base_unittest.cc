// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>
#include <string>

#include "base/containers/contains.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/power_bookmarks/core/bookmark_client_base.h"
#include "components/power_bookmarks/core/suggested_save_location_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace power_bookmarks {

namespace {

const base::TimeDelta kBackoffTime = base::Hours(2);

class TestBookmarkClientImpl : public BookmarkClientBase {
 public:
  TestBookmarkClientImpl() = default;
  TestBookmarkClientImpl(const TestBookmarkClientImpl&) = delete;
  TestBookmarkClientImpl& operator=(const TestBookmarkClientImpl&) = delete;
  ~TestBookmarkClientImpl() override = default;

  bookmarks::LoadManagedNodeCallback GetLoadManagedNodeCallback() override {
    return bookmarks::LoadManagedNodeCallback();
  }

  bool IsSyncFeatureEnabledIncludingBookmarks() override { return false; }

  bool CanSetPermanentNodeTitle(
      const bookmarks::BookmarkNode* permanent_node) override {
    return false;
  }

  bool IsNodeManaged(const bookmarks::BookmarkNode* node) override {
    return false;
  }

  std::string EncodeLocalOrSyncableBookmarkSyncMetadata() override {
    return "";
  }

  std::string EncodeAccountBookmarkSyncMetadata() override { return ""; }

  void DecodeLocalOrSyncableBookmarkSyncMetadata(
      const std::string& metadata_str,
      const base::RepeatingClosure& schedule_save_closure) override {}

  void DecodeAccountBookmarkSyncMetadata(
      const std::string& metadata_str,
      const base::RepeatingClosure& schedule_save_closure) override {}

  void OnBookmarkNodeRemovedUndoable(
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
  MOCK_METHOD(base::TimeDelta, GetBackoffTime, (), (override));
  MOCK_METHOD(std::string, GetFeatureNameForMetrics, (), (override));
  MOCK_METHOD(void, OnSuggestionRejected, (), (override));
};

}  // namespace

// Tests for the bookmark client base.
class BookmarkClientBaseTest : public testing::Test {
 protected:
  void SetUp() override {
    auto client = std::make_unique<TestBookmarkClientImpl>();
    client_ = client.get();
    model_ = std::make_unique<bookmarks::BookmarkModel>(std::move(client));
    model_->LoadEmptyForTest();
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
      .WillByDefault(testing::Return(kBackoffTime));
  ON_CALL(provider, GetFeatureNameForMetrics)
      .WillByDefault(testing::Return("feature"));

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
  const std::set<GURL> url_set = {url_for_suggestion, url_for_suggestion2};
  const bookmarks::BookmarkNode* suggested_folder =
      model()->AddFolder(model()->other_node(), 0, u"suggested folder");

  MockSuggestionProvider provider;
  ON_CALL(provider, GetSuggestion)
      .WillByDefault([suggested_folder, url_set](const GURL& url) {
        // Suggest for multiple URLs.
        return base::Contains(url_set, url) ? suggested_folder : nullptr;
      });
  ON_CALL(provider, GetBackoffTime)
      .WillByDefault(testing::Return(kBackoffTime));
  ON_CALL(provider, GetFeatureNameForMetrics)
      .WillByDefault(testing::Return("feature"));

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

  task_environment_.FastForwardBy(kBackoffTime + base::Minutes(1));

  // Remove and re-bookmark the second URL. The suggested folder should be
  // allowed again.
  model()->Remove(node, bookmarks::metrics::BookmarkEditSource::kUser,
                  FROM_HERE);

  bookmarks::AddIfNotBookmarked(model(), url_for_suggestion2, u"bookmark 2");
  node = model()->GetMostRecentlyAddedUserNodeForURL(url_for_suggestion2);
  ASSERT_EQ(node->parent(), suggested_folder);

  client()->RemoveSuggestedSaveLocationProvider(&provider);
}

// Make sure the rejection logic expires after a certain amount of time. E.g.
// moving a bookmark out of the suggested folder shouldn't be considered a
// rejection after some time passes, even if no other bookmarks are added.
TEST_F(BookmarkClientBaseTest, SuggestedFolder_RejectionCoolOff) {
  const GURL url_for_suggestion("http://example.com");
  const GURL url_for_suggestion2("http://example.com/other");
  const std::set<GURL> url_set = {url_for_suggestion, url_for_suggestion2};
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
  ON_CALL(provider, GetFeatureNameForMetrics)
      .WillByDefault(testing::Return("feature_name"));

  client()->AddSuggestedSaveLocationProvider(&provider);

  bookmarks::AddIfNotBookmarked(model(), url_for_suggestion, u"bookmark");

  // The bookmark should have been added to the suggested location.
  const bookmarks::BookmarkNode* node =
      model()->GetMostRecentlyAddedUserNodeForURL(url_for_suggestion);
  ASSERT_EQ(node->parent(), suggested_folder);

  task_environment_.FastForwardBy(kRejectionCoolOffTime + base::Seconds(1));

  // Move the new bookmark. This indicates the user did not like the suggested
  // location and changed its location in the hierarchy.
  model()->Move(node, model()->other_node(),
                model()->other_node()->children().size());

  // Save another bookmark to ensure the suggested location is allowed to be
  // used for the next save.
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
      .WillByDefault(testing::Return(kBackoffTime));
  ON_CALL(provider, GetFeatureNameForMetrics)
      .WillByDefault(testing::Return("feature"));

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

TEST_F(BookmarkClientBaseTest, SaveLocationMetrics) {
  base::HistogramTester histogram_tester;

  const GURL feature_url("http://example.com/1");

  const bookmarks::BookmarkNode* feature1_suggested_folder =
      model()->AddFolder(model()->other_node(), 0, u"suggested folder 1");
  MockSuggestionProvider provider1;
  std::string feature1_name = "feature1";
  ON_CALL(provider1, GetSuggestion)
      .WillByDefault([feature1_suggested_folder, feature_url](const GURL& url) {
        return feature_url == url ? feature1_suggested_folder : nullptr;
      });
  ON_CALL(provider1, GetBackoffTime)
      .WillByDefault(testing::Return(kBackoffTime));
  ON_CALL(provider1, GetFeatureNameForMetrics)
      .WillByDefault(testing::Return(feature1_name));
  client()->AddSuggestedSaveLocationProvider(&provider1);

  // Set up another provider to compete with.
  const bookmarks::BookmarkNode* feature2_suggested_folder =
      model()->AddFolder(model()->other_node(), 0, u"suggested folder 2");
  MockSuggestionProvider provider2;
  std::string feature2_name = "feature2";
  ON_CALL(provider2, GetSuggestion)
      .WillByDefault([feature2_suggested_folder, feature_url](const GURL& url) {
        return feature_url == url ? feature2_suggested_folder : nullptr;
      });
  ON_CALL(provider2, GetBackoffTime)
      .WillByDefault(testing::Return(kBackoffTime));
  ON_CALL(provider2, GetFeatureNameForMetrics)
      .WillByDefault(testing::Return(feature2_name));
  client()->AddSuggestedSaveLocationProvider(&provider2);

  // Since both providers want to suggest for the same URL, make sure we report
  // that one was superseded.
  const bookmarks::BookmarkNode* node =
      bookmarks::AddIfNotBookmarked(model(), feature_url, u"bookmark");

  const std::string feature1_histogram =
      kSaveLocationStateHistogramBase + feature1_name;
  const std::string feature2_histogram =
      kSaveLocationStateHistogramBase + feature2_name;

  histogram_tester.ExpectTotalCount(feature1_histogram, 1);
  histogram_tester.ExpectBucketCount(feature1_histogram,
                                     SuggestedSaveLocationState::kUsed, 1);

  histogram_tester.ExpectTotalCount(feature2_histogram, 1);
  histogram_tester.ExpectBucketCount(
      feature2_histogram, SuggestedSaveLocationState::kSuperseded, 1);

  // Move the new bookmark so that the folder for "feature 1" is in a "rejected"
  // state.
  model()->Move(node, model()->other_node(),
                model()->other_node()->children().size());
  model()->Remove(node, bookmarks::metrics::BookmarkEditSource::kUser,
                  FROM_HERE);

  // Adding the bookmark again should use "feature 2" since "feature 1" was
  // previously rejected.
  bookmarks::AddIfNotBookmarked(model(), feature_url, u"bookmark");

  histogram_tester.ExpectTotalCount(feature1_histogram, 2);
  histogram_tester.ExpectBucketCount(feature1_histogram,
                                     SuggestedSaveLocationState::kBlocked, 1);

  histogram_tester.ExpectTotalCount(feature2_histogram, 2);
  histogram_tester.ExpectBucketCount(feature2_histogram,
                                     SuggestedSaveLocationState::kUsed, 1);

  // Adding a URL that the providers ignore should add "no suggestion" to both.
  bookmarks::AddIfNotBookmarked(model(), GURL("http://example.com/ignored"),
                                u"bookmark2");

  histogram_tester.ExpectTotalCount(feature1_histogram, 3);
  histogram_tester.ExpectBucketCount(
      feature1_histogram, SuggestedSaveLocationState::kNoSuggestion, 1);

  histogram_tester.ExpectTotalCount(feature2_histogram, 3);
  histogram_tester.ExpectBucketCount(
      feature2_histogram, SuggestedSaveLocationState::kNoSuggestion, 1);

  client()->RemoveSuggestedSaveLocationProvider(&provider1);
  client()->RemoveSuggestedSaveLocationProvider(&provider2);
}

}  // namespace power_bookmarks
