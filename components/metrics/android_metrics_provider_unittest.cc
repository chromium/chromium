// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/android_metrics_provider.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/metrics/metrics_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace metrics {

class AndroidMetricsProviderTest : public testing::Test,
                                   public ::testing::WithParamInterface<bool> {
 public:
  AndroidMetricsProviderTest() = default;
  ~AndroidMetricsProviderTest() override = default;

  bool ShouldEmitHistogramsEarlier() { return GetParam(); }

  void SetUp() override {
    if (ShouldEmitHistogramsEarlier()) {
      feature_list_.InitWithFeatures({features::kEmitHistogramsEarlier}, {});
    } else {
      feature_list_.InitWithFeatures({}, {features::kEmitHistogramsEarlier});
    }
  }

 protected:
  base::HistogramTester histogram_tester_;
  AndroidMetricsProvider metrics_provider_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, AndroidMetricsProviderTest, testing::Bool());

TEST_P(AndroidMetricsProviderTest, ProvidePreviousSessionData_IsLowRamDevice) {
  ChromeUserMetricsExtension uma_proto;
  metrics_provider_.ProvidePreviousSessionData(&uma_proto);
  histogram_tester_.ExpectTotalCount("MemoryAndroid.LowRamDevice", 1);
}

TEST_P(AndroidMetricsProviderTest, ProvideCurrentSessionData_IsLowRamDevice) {
  if (!ShouldEmitHistogramsEarlier()) {
    ChromeUserMetricsExtension uma_proto;
    metrics_provider_.ProvideCurrentSessionData(&uma_proto);
  } else {
    metrics_provider_.OnDidCreateMetricsLog();
  }
  histogram_tester_.ExpectTotalCount("MemoryAndroid.LowRamDevice", 1);
}

}  // namespace metrics
