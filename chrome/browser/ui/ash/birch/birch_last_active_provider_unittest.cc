// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/birch/birch_last_active_provider.h"

#include "ash/birch/birch_model.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/browser_with_test_window_test.h"
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
  BirchLastActiveProviderTest() = default;
  ~BirchLastActiveProviderTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_{features::kForestFeature};
};

TEST_F(BirchLastActiveProviderTest, RequestBirchDataFetch) {
  BirchLastActiveProvider provider(profile());

  TestHistoryService history_service;
  provider.set_history_service_for_test(&history_service);

  // Requesting a data fetch should query Mock history service.
  provider.RequestBirchDataFetch();
  EXPECT_TRUE(history_service.did_query_history_);

  provider.OnGotHistory(CreateHistoryQueryResults());

  auto* birch_model = Shell::Get()->birch_model();
  EXPECT_EQ(birch_model->GetLastActiveItemsForTest().size(), 1u);
}

}  // namespace
}  // namespace ash
