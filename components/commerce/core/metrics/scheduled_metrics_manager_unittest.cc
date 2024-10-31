// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/metrics/scheduled_metrics_manager.h"

#include <map>
#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/mock_account_checker.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/pref_names.h"
#include "components/commerce/core/price_tracking_utils.h"
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
      : account_checker_(std::make_unique<MockAccountChecker>()),
        pref_service_(std::make_unique<TestingPrefServiceSimple>()),
        shopping_service_(std::make_unique<MockShoppingService>()) {}
  ScheduledMetricsManagerTest(const ScheduledMetricsManagerTest&) = delete;
  ScheduledMetricsManagerTest operator=(const ScheduledMetricsManagerTest&) =
      delete;
  ~ScheduledMetricsManagerTest() override = default;

  void TestBody() override {}

  void SetUp() override {
    test_features_.InitWithFeatures({kSubscriptionsApi, kShoppingList}, {});
    RegisterCommercePrefs(pref_service_->registry());
    SetShoppingListEnterprisePolicyPref(pref_service_.get(), true);
    account_checker_->SetPrefs(pref_service_.get());
    shopping_service_->SetAccountChecker(account_checker_.get());
  }

  void CreateUpdateManagerAndWait() {
    auto metrics_manager = std::make_unique<ScheduledMetricsManager>(
        pref_service_.get(), shopping_service_.get());
    base::RunLoop().RunUntilIdle();
  }

 protected:
  base::test::ScopedFeatureList test_features_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MockAccountChecker> account_checker_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<MockShoppingService> shopping_service_;
};

TEST_F(ScheduledMetricsManagerTest, TrackedProductCountRecorded) {
  // Set the last updated time to "never" so the scheduled task runs
  // immediately.
  pref_service_->SetTime(kCommerceDailyMetricsLastUpdateTime, base::Time());
  base::HistogramTester histogram_tester;

  // Add two tracked products.
  shopping_service_->SetGetAllSubscriptionsCallbackValue(
      {BuildUserSubscriptionForClusterId(123L),
       BuildUserSubscriptionForClusterId(456L)});

  CreateUpdateManagerAndWait();

  histogram_tester.ExpectTotalCount(kTrackedProductCountHistogramName, 1);
}

TEST_F(ScheduledMetricsManagerTest, TrackedProductCountNotRecordedEarly) {
  // Set the last updated time to now so the task doesn't immediately run.
  pref_service_->SetTime(kCommerceDailyMetricsLastUpdateTime,
                         base::Time::Now());
  base::HistogramTester histogram_tester;

  shopping_service_->SetGetAllSubscriptionsCallbackValue(
      {BuildUserSubscriptionForClusterId(123L)});

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

  shopping_service_->SetGetAllSubscriptionsCallbackValue(
      std::vector<CommerceSubscription>());
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
  shopping_service_->SetGetAllSubscriptionsCallbackValue(
      {BuildUserSubscriptionForClusterId(123L)});

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
  shopping_service_->SetGetAllSubscriptionsCallbackValue(
      {BuildUserSubscriptionForClusterId(123L)});

  CreateUpdateManagerAndWait();

  histogram_tester.ExpectUniqueSample(kPriceNotificationEmailHistogramName,
                                      PriceNotificationEmailState::kDisabled,
                                      1);
  histogram_tester.ExpectTotalCount(kPriceNotificationEmailHistogramName, 1);
}

}  // namespace commerce::metrics
