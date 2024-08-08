// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/birch/birch_most_visited_provider.h"

#include "ash/birch/birch_model.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {
namespace {

class TestHistoryService : public history::HistoryService {
 public:
  TestHistoryService() = default;
  TestHistoryService(const TestHistoryService&) = delete;
  TestHistoryService& operator=(const TestHistoryService&) = delete;
  ~TestHistoryService() override = default;

  // history::HistoryService:
  base::CancelableTaskTracker::TaskId QueryMostVisitedURLs(
      int result_count,
      QueryMostVisitedURLsCallback callback,
      base::CancelableTaskTracker* tracker) override {
    did_query_most_visited_urls_ = true;
    return base::CancelableTaskTracker::TaskId();
  }

  bool did_query_most_visited_urls_ = false;
};

// BrowserWithTestWindowTest provides a Profile and ash::Shell (which provides
// a BirchModel) needed by the test.
class BirchMostVisitedProviderTest : public BrowserWithTestWindowTest {
 public:
  BirchMostVisitedProviderTest() = default;
  ~BirchMostVisitedProviderTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_{features::kForestFeature};
};

TEST_F(BirchMostVisitedProviderTest, RequestBirchDataFetch) {
  BirchMostVisitedProvider provider(profile());

  TestHistoryService history_service;
  provider.set_history_service_for_test(&history_service);

  // Requesting a data fetch should query most visited URLs.
  provider.RequestBirchDataFetch();
  EXPECT_TRUE(history_service.did_query_most_visited_urls_);

  // Simulate a most-visited URL.
  history::MostVisitedURLList urls;
  history::MostVisitedURL url;
  url.title = u"title";
  url.url = GURL("http://example.com/");
  urls.push_back(url);

  // Query for most visited urls.
  provider.OnGotMostVisitedURLs(urls);

  auto* birch_model = Shell::Get()->birch_model();
  EXPECT_EQ(birch_model->GetMostVisitedItemsForTest().size(), 1u);
}

}  // namespace
}  // namespace ash
