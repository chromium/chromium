// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/birch/birch_last_active_provider.h"

#include "ash/birch/birch_model.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
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
  base::CancelableTaskTracker::TaskId QueryHistory(
      const std::u16string& text_query,
      const history::QueryOptions& options,
      QueryHistoryCallback callback,
      base::CancelableTaskTracker* tracker) override {
    did_query_history_ = true;
    return base::CancelableTaskTracker::TaskId();
  }

  void Reset() { did_query_history_ = false; }

  bool did_query_history_ = false;
};

class TestFaviconService : public favicon::MockFaviconService {
 public:
  TestFaviconService() = default;
  TestFaviconService(const TestFaviconService&) = delete;
  TestFaviconService& operator=(const TestFaviconService&) = delete;
  ~TestFaviconService() override = default;

  // favicon::FaviconService:
  base::CancelableTaskTracker::TaskId GetFaviconImageForPageURL(
      const GURL& page_url,
      favicon_base::FaviconImageCallback callback,
      base::CancelableTaskTracker* tracker) override {
    did_get_favicon_image_for_page_url_ = true;
    page_url_ = page_url;
    return base::CancelableTaskTracker::TaskId();
  }

  bool did_get_favicon_image_for_page_url_ = false;
  GURL page_url_;
};

// Creates history query results with the URL http://example.com/.
history::QueryResults CreateHistoryQueryResults() {
  history::QueryResults results;
  std::vector<history::URLResult> url_results;
  url_results.emplace_back(GURL("http://example.com/"), base::Time());
  results.SetURLResults(std::move(url_results));
  return results;
}

// BrowserWithTestWindowTest provides a Profile and ash::Shell (which provides
// a BirchModel) needed by the test.
class BirchLastActiveProviderTest : public BrowserWithTestWindowTest {
 public:
  BirchLastActiveProviderTest() {
    switches::SetIgnoreForestSecretKeyForTest(true);
    feature_list_.InitAndEnableFeature(features::kForestFeature);
  }
  ~BirchLastActiveProviderTest() override {
    switches::SetIgnoreForestSecretKeyForTest(false);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(BirchLastActiveProviderTest, RequestBirchDataFetch) {
  BirchLastActiveProvider provider(profile());

  TestHistoryService history_service;
  provider.set_history_service_for_test(&history_service);
  TestFaviconService favicon_service;
  provider.set_favicon_service_for_test(&favicon_service);

  // Requesting a data fetch should query history.
  provider.RequestBirchDataFetch();
  EXPECT_TRUE(history_service.did_query_history_);

  // Once the last active URL is fetched the favicon database is queried.
  provider.OnGotHistory(CreateHistoryQueryResults());
  EXPECT_TRUE(favicon_service.did_get_favicon_image_for_page_url_);
  EXPECT_EQ(favicon_service.page_url_, GURL("http://example.com/"));

  // Simulate a favicon image.
  favicon_base::FaviconImageResult image_result;
  image_result.image = gfx::test::CreateImage(16);

  // Once the favicon is fetched the birch model is populated.
  provider.OnGotFaviconImage(u"title", GURL("http://example.com/"),
                             base::Time(), image_result);
  auto* birch_model = Shell::Get()->birch_model();
  EXPECT_EQ(birch_model->GetLastActiveItemsForTest().size(), 1u);

  // Reset the birch model last active items and the mock history service.
  birch_model->SetLastActiveItems({});
  history_service.Reset();

  // Simulate a fetch for the same last-active URL. The data should come out of
  // cache and the favicon load is not required.
  provider.RequestBirchDataFetch();
  EXPECT_TRUE(history_service.did_query_history_);

  // Simulate the last active URL with the same URL as before.
  provider.OnGotHistory(CreateHistoryQueryResults());

  // The birch model is populated without a favicon load.
  EXPECT_EQ(birch_model->GetLastActiveItemsForTest().size(), 1u);
}

}  // namespace
}  // namespace ash
