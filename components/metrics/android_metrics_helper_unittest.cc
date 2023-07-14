// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/android_metrics_helper.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

TEST(AndroidMetricsHelperTest, AbiBitnessSupport) {
  std::unique_ptr<AndroidMetricsHelper> helper(
      AndroidMetricsHelper::CreateInstanceForTest("", false, false));
  EXPECT_EQ(helper->abi_bitness_support(), AbiBitnessSupport::kNeither);

  helper.reset(AndroidMetricsHelper::CreateInstanceForTest("", true, false));
  EXPECT_EQ(helper->abi_bitness_support(), AbiBitnessSupport::k32bitOnly);

  helper.reset(AndroidMetricsHelper::CreateInstanceForTest("", false, true));
  EXPECT_EQ(helper->abi_bitness_support(), AbiBitnessSupport::k64bitOnly);

  helper.reset(AndroidMetricsHelper::CreateInstanceForTest("", true, true));
  EXPECT_EQ(helper->abi_bitness_support(), AbiBitnessSupport::k32And64bit);
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

TEST(AndroidMetricsHelperTest, EmitHistograms_CurrentSession) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<AndroidMetricsHelper> helper(
      AndroidMetricsHelper::CreateInstanceForTest("588700002", true, true));
  helper->EmitHistograms(/*current_session=*/true);

  histogram_tester.ExpectTotalCount("Android.VersionCode", 1);
  histogram_tester.ExpectUniqueSample("Android.VersionCode", 588700002, 1);
  histogram_tester.ExpectTotalCount("Android.AbiBitnessSupport", 1);
  histogram_tester.ExpectUniqueSample("Android.AbiBitnessSupport",
                                      AbiBitnessSupport::k32And64bit, 1);
}

TEST(AndroidMetricsHelperTest, EmitHistograms_PreviousSession) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<AndroidMetricsHelper> helper(
      AndroidMetricsHelper::CreateInstanceForTest("588700002", true, true));
  helper->EmitHistograms(/*current_session=*/false);

  histogram_tester.ExpectTotalCount("Android.VersionCode", 0);
  histogram_tester.ExpectTotalCount("Android.AbiBitnessSupport", 1);
  histogram_tester.ExpectUniqueSample("Android.AbiBitnessSupport",
                                      AbiBitnessSupport::k32And64bit, 1);
}

TEST(AndroidMetricsHelperTest, EmitHistograms_BadData) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<AndroidMetricsHelper> helper(
      AndroidMetricsHelper::CreateInstanceForTest("5887_000_0_2", true, false));
  helper->EmitHistograms(/*current_session=*/true);

  histogram_tester.ExpectTotalCount("Android.VersionCode", 0);
  histogram_tester.ExpectTotalCount("Android.AbiBitnessSupport", 1);
  histogram_tester.ExpectUniqueSample("Android.AbiBitnessSupport",
                                      AbiBitnessSupport::k32bitOnly, 1);
}

}  // namespace metrics
