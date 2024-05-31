// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/transformer/bookmarks_url_visit_aggregates_transformer.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/test/mock_callback.h"
#include "components/bookmarks/browser/core_bookmark_model.h"
#include "components/bookmarks/browser/titled_url_match.h"
#include "components/bookmarks/browser/url_and_title.h"
#include "components/visited_url_ranking/internal/transformer/transformer_test_support.h"
#include "components/visited_url_ranking/public/test_support.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace bookmarks {

class MockCoreBookmarkModel : public CoreBookmarkModel {
 public:
  MockCoreBookmarkModel() = default;
  ~MockCoreBookmarkModel() override = default;

  // Disallow copy/assign.
  MockCoreBookmarkModel(const MockCoreBookmarkModel&) = delete;
  MockCoreBookmarkModel& operator=(const MockCoreBookmarkModel&) = delete;

  MOCK_CONST_METHOD0(loaded, bool());

  MOCK_CONST_METHOD1(IsBookmarked, bool(const GURL& url));

  MOCK_CONST_METHOD1(GetNodeCountByURL, size_t(const GURL& url));

  MOCK_CONST_METHOD1(GetNodeTitlesByURL,
                     std::vector<std::u16string_view>(const GURL& url));

  MOCK_CONST_METHOD0(GetUniqueUrls, std::vector<UrlAndTitle>());

  MOCK_CONST_METHOD3(GetBookmarksMatching,
                     std::vector<TitledUrlMatch>(
                         const std::u16string& query,
                         size_t max_count_hint,
                         query_parser::MatchingAlgorithm matching_algorithm));

  MOCK_METHOD1(RemoveAllUserBookmarks, void(const base::Location& location));
};

}  // namespace bookmarks

namespace visited_url_ranking {

class BookmarksURLVisitAggregatesTransformerTest
    : public URLVisitAggregatesTransformerTest {
 public:
  BookmarksURLVisitAggregatesTransformerTest() = default;

  ~BookmarksURLVisitAggregatesTransformerTest() override {
    transformer_ = nullptr;
  }

  // Disallow copy/assign.
  BookmarksURLVisitAggregatesTransformerTest(
      const BookmarksURLVisitAggregatesTransformerTest&) = delete;
  BookmarksURLVisitAggregatesTransformerTest& operator=(
      const BookmarksURLVisitAggregatesTransformerTest&) = delete;

  void SetUp() override {
    bookmark_model_ = std::make_unique<bookmarks::MockCoreBookmarkModel>();
    EXPECT_CALL(*bookmark_model_, IsBookmarked(testing::_))
        .Times(1)
        .WillOnce(testing::Return(true));

    transformer_ = std::make_unique<BookmarksURLVisitAggregatesTransformer>(
        bookmark_model_.get());
  }

 private:
  std::unique_ptr<bookmarks::MockCoreBookmarkModel> bookmark_model_;
};

TEST_F(BookmarksURLVisitAggregatesTransformerTest, Transform) {
  URLVisitAggregate visit_aggregate(kSampleSearchUrl);
  visit_aggregate.fetcher_data_map.emplace(
      Fetcher::kSession,
      URLVisitAggregate::TabData(URLVisitAggregate::Tab(
          1,
          URLVisit(GURL(kSampleSearchUrl), u"sample_title", base::Time::Now(),
                   syncer::DeviceInfo::FormFactor::kUnknown,
                   URLVisit::Source::kLocal),
          "sample_tag", "sample_session_name")));
  std::vector<URLVisitAggregate> input_sample_aggregates = {};
  input_sample_aggregates.push_back(std::move(visit_aggregate));

  BookmarksURLVisitAggregatesTransformerTest::Result result =
      TransformAndGetResult(std::move(input_sample_aggregates));
  ASSERT_EQ(result.first, URLVisitAggregatesTransformer::Status::kSuccess);
  ASSERT_EQ(result.second.front().bookmarked, true);
}

}  // namespace visited_url_ranking
