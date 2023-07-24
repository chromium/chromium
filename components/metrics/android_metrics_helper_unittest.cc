// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/android_metrics_helper.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

TEST(AndroidMetricsHelperTest, CpuAbiBitnessSupport) {
  std::unique_ptr<AndroidMetricsHelper> helper(
      AndroidMetricsHelper::CreateInstanceForTest("", false, false));
  EXPECT_EQ(helper->cpu_abi_bitness_support(), CpuAbiBitnessSupport::kNeither);

  helper.reset(AndroidMetricsHelper::CreateInstanceForTest("", true, false));
  EXPECT_EQ(helper->cpu_abi_bitness_support(),
            CpuAbiBitnessSupport::k32bitOnly);

  helper.reset(AndroidMetricsHelper::CreateInstanceForTest("", false, true));
  EXPECT_EQ(helper->cpu_abi_bitness_support(),
            CpuAbiBitnessSupport::k64bitOnly);

  helper.reset(AndroidMetricsHelper::CreateInstanceForTest("", true, true));
  EXPECT_EQ(helper->cpu_abi_bitness_support(),
            CpuAbiBitnessSupport::k32And64bit);
}

TEST(AndroidMetricsHelperTest, VersionCode) {
  std::unique_ptr<AndroidMetricsHelper> helper(
      AndroidMetricsHelper::CreateInstanceForTest("588700000", true, true));
  EXPECT_EQ(helper->version_code_int(), 588700000);
}

TEST(AndroidMetricsHelperTest, VersionCode_BadData) {
  std::unique_ptr<AndroidMetricsHelper> helper(
      AndroidMetricsHelper::CreateInstanceForTest("", true, true));
  EXPECT_EQ(helper->version_code_int(), 0);

  helper.reset(
      AndroidMetricsHelper::CreateInstanceForTest("5887_000_0_9", true, true));
  EXPECT_EQ(helper->version_code_int(), 0);
}

class AndroidMetricsHelperHistTest : public testing::Test {
 public:
  AndroidMetricsHelperHistTest() {
    AndroidMetricsHelper::RegisterPrefs(pref_service.registry());
  }
  ~AndroidMetricsHelperHistTest() override {
    AndroidMetricsHelper::ResetGlobalStateForTesting();
  }

 protected:
  TestingPrefServiceSimple pref_service;
  base::HistogramTester histogram_tester;
};

TEST_F(AndroidMetricsHelperHistTest, EmitHistograms_CurrentSession) {
  std::unique_ptr<AndroidMetricsHelper> helper(
      AndroidMetricsHelper::CreateInstanceForTest("588700002", true, true));
  helper->EmitHistograms(&pref_service, /*current_session=*/true);
  histogram_tester.ExpectTotalCount("Android.VersionCode", 1);
  histogram_tester.ExpectUniqueSample("Android.VersionCode", 588700002, 1);
  histogram_tester.ExpectTotalCount("Android.CpuAbiBitnessSupport", 1);
  histogram_tester.ExpectUniqueSample("Android.CpuAbiBitnessSupport",
                                      CpuAbiBitnessSupport::k32And64bit, 1);
}

TEST_F(AndroidMetricsHelperHistTest, EmitHistograms_LogPreviousSession) {
  std::unique_ptr<AndroidMetricsHelper> helper(
      AndroidMetricsHelper::CreateInstanceForTest("588700002", true, true));
  helper->EmitHistograms(&pref_service, /*current_session=*/false);

  histogram_tester.ExpectTotalCount("Android.VersionCode", 0);
  histogram_tester.ExpectTotalCount("Android.CpuAbiBitnessSupport", 1);
  histogram_tester.ExpectUniqueSample("Android.CpuAbiBitnessSupport",
                                      CpuAbiBitnessSupport::k32And64bit, 1);
}

TEST_F(AndroidMetricsHelperHistTest,
       EmitHistograms_LogPreviousSessionWithSavedLocalState) {
  AndroidMetricsHelper::SaveLocalState(&pref_service, 588700002);
  std::unique_ptr<AndroidMetricsHelper> helper(
      AndroidMetricsHelper::CreateInstanceForTest("588700006", false, false));
  helper->EmitHistograms(&pref_service, /*current_session=*/false);
  // The previous value was used.
  histogram_tester.ExpectTotalCount("Android.VersionCode", 1);
  histogram_tester.ExpectUniqueSample("Android.VersionCode", 588700002, 1);
  // We don't bother to save/restore CpuAbiBitnessSupport, as we assume that the
  // value doesn't change across sessions. The test uses a different value
  // artificially, solely for the purpose of testing this behavior.
  histogram_tester.ExpectTotalCount("Android.CpuAbiBitnessSupport", 1);
  histogram_tester.ExpectUniqueSample("Android.CpuAbiBitnessSupport",
                                      CpuAbiBitnessSupport::kNeither, 1);
}

TEST_F(AndroidMetricsHelperHistTest, EmitHistograms_BadData) {
  std::unique_ptr<AndroidMetricsHelper> helper(
      AndroidMetricsHelper::CreateInstanceForTest("5887_000_0_2", true, false));
  helper->EmitHistograms(&pref_service, /*current_session=*/true);

  histogram_tester.ExpectTotalCount("Android.VersionCode", 0);
  histogram_tester.ExpectTotalCount("Android.CpuAbiBitnessSupport", 1);
  histogram_tester.ExpectUniqueSample("Android.CpuAbiBitnessSupport",
                                      CpuAbiBitnessSupport::k32bitOnly, 1);
}

}  // namespace metrics
