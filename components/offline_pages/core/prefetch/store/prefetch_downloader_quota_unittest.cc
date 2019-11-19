// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/store/prefetch_downloader_quota.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {
const char kTestConfig[] = "12345";
const char kTestZero[] = "0";
const char kInvalidNegative[] = "-12345";
const char kInvalidEmpty[] = "";
const char kInvalidText[] = "tacos";
}  // namespace

class PrefetchDownloaderQuotaTest : public testing::Test {
 public:
  PrefetchDownloaderQuotaTest() { store_test_util_.BuildStoreInMemory(); }
  ~PrefetchDownloaderQuotaTest() override { store_test_util_.DeleteStore(); }

  PrefetchStore* store() { return store_test_util_.store(); }

  PrefetchStoreTestUtil* store_util() { return &store_test_util_; }

  void SetTestingMaxDailyQuotaBytes(const std::string& max_config);

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  PrefetchStoreTestUtil store_test_util_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

void PrefetchDownloaderQuotaTest::SetTestingMaxDailyQuotaBytes(
    const std::string& max_config) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      kPrefetchingOfflinePagesFeature,
      {{"offline_pages_max_daily_quota_bytes", max_config}});
}

TEST_F(PrefetchDownloaderQuotaTest, GetMaxDailyQuotaBytes) {
  EXPECT_EQ(PrefetchDownloaderQuota::kDefaultMaxDailyQuotaBytes,
            PrefetchDownloaderQuota::GetMaxDailyQuotaBytes());

  SetTestingMaxDailyQuotaBytes(kTestConfig);
  EXPECT_EQ(12345LL, PrefetchDownloaderQuota::GetMaxDailyQuotaBytes());

  SetTestingMaxDailyQuotaBytes(kTestZero);
  EXPECT_EQ(0, PrefetchDownloaderQuota::GetMaxDailyQuotaBytes());

  SetTestingMaxDailyQuotaBytes(kInvalidNegative);
  EXPECT_EQ(PrefetchDownloaderQuota::kDefaultMaxDailyQuotaBytes,
            PrefetchDownloaderQuota::GetMaxDailyQuotaBytes());

  SetTestingMaxDailyQuotaBytes(kInvalidEmpty);
  EXPECT_EQ(PrefetchDownloaderQuota::kDefaultMaxDailyQuotaBytes,
            PrefetchDownloaderQuota::GetMaxDailyQuotaBytes());

  SetTestingMaxDailyQuotaBytes(kInvalidText);
  EXPECT_EQ(PrefetchDownloaderQuota::kDefaultMaxDailyQuotaBytes,
            PrefetchDownloaderQuota::GetMaxDailyQuotaBytes());
}

TEST_F(PrefetchDownloaderQuotaTest, GetQuotaWithNothingStored) {
  EXPECT_EQ(PrefetchDownloaderQuota::kDefaultMaxDailyQuotaBytes,
            store_util()->GetPrefetchQuota());
}

TEST_F(PrefetchDownloaderQuotaTest, SetQuotaToZeroAndRead) {
  EXPECT_TRUE(store_util()->SetPrefetchQuota(0));
  EXPECT_EQ(0, store_util()->GetPrefetchQuota());
}

TEST_F(PrefetchDownloaderQuotaTest, SetQuotaToLessThanZero) {
  EXPECT_TRUE(store_util()->SetPrefetchQuota(-10000LL));
  EXPECT_EQ(0, store_util()->GetPrefetchQuota());
}

TEST_F(PrefetchDownloaderQuotaTest, SetQuotaToMoreThanMaximum) {
  EXPECT_TRUE(store_util()->SetPrefetchQuota(
      2 * PrefetchDownloaderQuota::kDefaultMaxDailyQuotaBytes));
  EXPECT_EQ(PrefetchDownloaderQuota::kDefaultMaxDailyQuotaBytes,
            store_util()->GetPrefetchQuota());
}

// This test verifies that advancing time by 12 hours (half of quota period),
// recovers half of the quota, withing limits.
TEST_F(PrefetchDownloaderQuotaTest, CheckQuotaForHalfDay) {
  // Start with no quota, and wait for half a day, we should have half of quota.
  EXPECT_TRUE(store_util()->SetPrefetchQuota(0));
  store_util()->clock()->Advance(base::TimeDelta::FromHours(12));
  EXPECT_EQ(PrefetchDownloaderQuota::kDefaultMaxDailyQuotaBytes / 2,
            store_util()->GetPrefetchQuota());

  // Now start with quota set to 1/4. Expecting 3/4 of quota.
  EXPECT_TRUE(store_util()->SetPrefetchQuota(
      PrefetchDownloaderQuota::kDefaultMaxDailyQuotaBytes / 4));
  store_util()->clock()->Advance(base::TimeDelta::FromHours(12));
  EXPECT_EQ(3 * PrefetchDownloaderQuota::kDefaultMaxDailyQuotaBytes / 4,
            store_util()->GetPrefetchQuota());

  // Now start with quota set to 1/2. Expects full quota.
  EXPECT_TRUE(store_util()->SetPrefetchQuota(
      PrefetchDownloaderQuota::kDefaultMaxDailyQuotaBytes / 2));
  store_util()->clock()->Advance(base::TimeDelta::FromHours(12));
  EXPECT_EQ(PrefetchDownloaderQuota::kDefaultMaxDailyQuotaBytes,
            store_util()->GetPrefetchQuota());

  // Now start with quota set to 3/4. Expects a limit on quota of daily quota
  // maximum.
  EXPECT_TRUE(store_util()->SetPrefetchQuota(
      3 * PrefetchDownloaderQuota::kDefaultMaxDailyQuotaBytes / 4));
  store_util()->clock()->Advance(base::TimeDelta::FromHours(12));
  EXPECT_EQ(PrefetchDownloaderQuota::kDefaultMaxDailyQuotaBytes,
            store_util()->GetPrefetchQuota());
}

// This test deals with small and extreme time change scenarios.
TEST_F(PrefetchDownloaderQuotaTest, CheckQuotaAfterTimeChange) {
  EXPECT_TRUE(store_util()->SetPrefetchQuota(0));
  store_util()->clock()->Advance(base::TimeDelta::FromHours(-1));
  EXPECT_EQ(0, store_util()->GetPrefetchQuota());

  EXPECT_TRUE(store_util()->SetPrefetchQuota(
      PrefetchDownloaderQuota::kDefaultMaxDailyQuotaBytes));
  store_util()->clock()->Advance(base::TimeDelta::FromHours(1));
  EXPECT_EQ(PrefetchDownloaderQuota::kDefaultMaxDailyQuotaBytes,
            store_util()->GetPrefetchQuota());

  // When going back in time, we get a situation where quota would be negative,
  // we detect it and write back a value of 0 with that date.
  store_util()->clock()->Advance(base::TimeDelta::FromDays(-365));
  EXPECT_EQ(0, store_util()->GetPrefetchQuota());
  // Above implies that after another day our quota is back to full daily max.
  store_util()->clock()->Advance(base::TimeDelta::FromDays(1));
  EXPECT_EQ(PrefetchDownloaderQuota::kDefaultMaxDailyQuotaBytes,
            store_util()->GetPrefetchQuota());
}

}  // namespace offline_pages
