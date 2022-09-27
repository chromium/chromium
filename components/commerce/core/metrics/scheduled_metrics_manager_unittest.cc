// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/commerce/core/metrics/scheduled_metrics_manager.h"
#include "components/commerce/core/pref_names.h"
#include "components/commerce/core/test_utils.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "components/power_bookmarks/core/proto/shopping_specifics.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace commerce::metrics {

class ScheduledMetricsManagerTest : public testing::Test {
 public:
  ScheduledMetricsManagerTest()
      : bookmark_model_(bookmarks::TestBookmarkClient::CreateModel()),
        pref_service_(std::make_unique<TestingPrefServiceSimple>()) {}
  ScheduledMetricsManagerTest(const ScheduledMetricsManagerTest&) = delete;
  ScheduledMetricsManagerTest operator=(const ScheduledMetricsManagerTest&) =
      delete;
  ~ScheduledMetricsManagerTest() override = default;

  void TestBody() override {}

  void SetUp() override { RegisterPrefs(pref_service_->registry()); }

  void CreateUpdateManagerAndWait() {
    auto metrics_manager = std::make_unique<ScheduledMetricsManager>(
        pref_service_.get(), bookmark_model_.get());
    base::RunLoop().RunUntilIdle();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
};

TEST_F(ScheduledMetricsManagerTest, TrackedProductCountRecorded) {
  // Set the last updated time to "never" so the scheduled task runs
  // immediately.
  pref_service_->SetTime(kCommerceDailyMetricsLastUpdateTime, base::Time());
  base::HistogramTester histogram_tester;

  // Add two tracked products and one untracked product.
  AddProductBookmark(bookmark_model_.get(), u"product 1",
                     GURL("http://example.com"), 123L, true);
  AddProductBookmark(bookmark_model_.get(), u"product 2",
                     GURL("http://example.com"), 456L, false);
  AddProductBookmark(bookmark_model_.get(), u"product 3",
                     GURL("http://example.com"), 789L, true);

  CreateUpdateManagerAndWait();

  histogram_tester.ExpectTotalCount(kTrackedProductCountHistogramName, 1);
}

TEST_F(ScheduledMetricsManagerTest, TrackedProductCountNotRecordedEarly) {
  // Set the last updated time to now so the task doesn't immediately run.
  pref_service_->SetTime(kCommerceDailyMetricsLastUpdateTime,
                         base::Time::Now());
  base::HistogramTester histogram_tester;

  AddProductBookmark(bookmark_model_.get(), u"product 1",
                     GURL("http://example.com"), 123L, true);

  CreateUpdateManagerAndWait();

  histogram_tester.ExpectTotalCount(kTrackedProductCountHistogramName, 0);
}

TEST_F(ScheduledMetricsManagerTest, EmailNotification_NoTrackedProducts) {
  // Set the last updated time to "never" so the scheduled task runs
  // immediately.
  pref_service_->SetTime(kCommerceDailyMetricsLastUpdateTime, base::Time());
  base::HistogramTester histogram_tester;

  // Assume the user has enabled notifications but has no tracked products.
  pref_service_->SetBoolean(kPriceEmailNotificationsEnabled, true);

  CreateUpdateManagerAndWait();

  histogram_tester.ExpectUniqueSample(
      kPriceNotificationEmailHistogramName,
      PriceNotificationEmailState::kNotResponded, 1);
  histogram_tester.ExpectTotalCount(kPriceNotificationEmailHistogramName, 1);
}

TEST_F(ScheduledMetricsManagerTest, EmailNotification_TrackedProducts) {
  // Set the last updated time to "never" so the scheduled task runs
  // immediately.
  pref_service_->SetTime(kCommerceDailyMetricsLastUpdateTime, base::Time());
  base::HistogramTester histogram_tester;

  // Assume the user has enabled notifications.
  pref_service_->SetBoolean(kPriceEmailNotificationsEnabled, true);

  // Have at least one tracked product.
  AddProductBookmark(bookmark_model_.get(), u"product 1",
                     GURL("http://example.com"), 123L, true);

  CreateUpdateManagerAndWait();

  histogram_tester.ExpectUniqueSample(kPriceNotificationEmailHistogramName,
                                      PriceNotificationEmailState::kEnabled, 1);
  histogram_tester.ExpectTotalCount(kPriceNotificationEmailHistogramName, 1);
}

TEST_F(ScheduledMetricsManagerTest,
       EmailNotification_TrackedProducts_Disabled) {
  // Set the last updated time to "never" so the scheduled task runs
  // immediately.
  pref_service_->SetTime(kCommerceDailyMetricsLastUpdateTime, base::Time());
  base::HistogramTester histogram_tester;

  // Assume the user has disabled notifications.
  pref_service_->SetBoolean(kPriceEmailNotificationsEnabled, false);

  // Have at least one tracked product.
  AddProductBookmark(bookmark_model_.get(), u"product 1",
                     GURL("http://example.com"), 123L, true);

  CreateUpdateManagerAndWait();

  histogram_tester.ExpectUniqueSample(kPriceNotificationEmailHistogramName,
                                      PriceNotificationEmailState::kDisabled,
                                      1);
  histogram_tester.ExpectTotalCount(kPriceNotificationEmailHistogramName, 1);
}

}  // namespace commerce::metrics
