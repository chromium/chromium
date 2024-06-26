// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/transformer/bookmarks_url_visit_aggregates_transformer.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/test/mock_callback.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/titled_url_match.h"
#include "components/bookmarks/browser/url_and_title.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/visited_url_ranking/internal/transformer/transformer_test_support.h"
#include "components/visited_url_ranking/public/test_support.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

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
    bookmark_model_ = bookmarks::TestBookmarkClient::CreateModel();
    bookmark_model_->AddURL(bookmark_model_->bookmark_bar_node(), 0,
                            std::u16string(), GURL(kSampleSearchUrl));

    transformer_ = std::make_unique<BookmarksURLVisitAggregatesTransformer>(
        bookmark_model_.get());
  }

 private:
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
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
