// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/android_metrics_provider.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace metrics {

class AndroidMetricsProviderTest : public testing::Test {
 public:
  AndroidMetricsProviderTest() = default;
  ~AndroidMetricsProviderTest() override = default;

 protected:
  base::HistogramTester histogram_tester_;
  AndroidMetricsProvider metrics_provider_;
};

TEST_F(AndroidMetricsProviderTest, ProvidePreviousSessionData_IsLowRamDevice) {
  ChromeUserMetricsExtension uma_proto;
  metrics_provider_.ProvidePreviousSessionData(&uma_proto);
  histogram_tester_.ExpectTotalCount("MemoryAndroid.LowRamDevice", 1);
}

TEST_F(AndroidMetricsProviderTest, OnDidCreateMetricsLog_IsLowRamDevice) {
  metrics_provider_.OnDidCreateMetricsLog();
  histogram_tester_.ExpectTotalCount("MemoryAndroid.LowRamDevice", 1);
}

}  // namespace metrics
