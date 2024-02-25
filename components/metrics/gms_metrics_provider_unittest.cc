// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/gms_metrics_provider.h"

#include "base/android/build_info.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {
namespace {

// Same as GmsMetricsProvider but Gms version is mocked for easy testing.
class MockedGmsMetricsProvider : public GmsMetricsProvider {
 public:
  void SetGmsVersionForTesting(const std::string& version) {
    version_ = version;
  }

 private:
  // GmsMetricsProvider.
  std::string GetGMSVersion() override { return version_; }

  std::string version_;
};

}  // namespace

class GmsMetricsProviderTest : public testing::Test {
 protected:
  GmsMetricsProviderTest() = default;

  base::HistogramTester& histogram_tester() { return histogram_tester_; }
  MockedGmsMetricsProvider& gms_metrics_provider() {
    return gms_metrics_provider_;
  }

 private:
  base::HistogramTester histogram_tester_;
  MockedGmsMetricsProvider gms_metrics_provider_;
};

TEST_F(GmsMetricsProviderTest, TestMetricsReportedCorrectly) {
  gms_metrics_provider().SetGmsVersionForTesting("234012000");
  gms_metrics_provider().ProvideHistograms();

  histogram_tester().ExpectUniqueSample("Android.PlayServices.ShortVersion",
                                        /*sample=*/23401,
                                        /*expected_bucket_count=*/1);

  gms_metrics_provider().SetGmsVersionForTesting("234016000");
  gms_metrics_provider().ProvideHistograms();

  histogram_tester().ExpectUniqueSample("Android.PlayServices.ShortVersion",
                                        /*sample=*/23401,
                                        /*expected_bucket_count=*/2);

  gms_metrics_provider().SetGmsVersionForTesting("234082000");
  gms_metrics_provider().ProvideHistograms();

  histogram_tester().ExpectUniqueSample("Android.PlayServices.ShortVersion",
                                        /*sample=*/23401,
                                        /*expected_bucket_count=*/3);
}

TEST_F(GmsMetricsProviderTest, TestMetricsReportedCorrectlyForBeta) {
  gms_metrics_provider().SetGmsVersionForTesting("234002000");
  gms_metrics_provider().ProvideHistograms();

  histogram_tester().ExpectUniqueSample("Android.PlayServices.ShortVersion",
                                        /*sample=*/23400,
                                        /*expected_bucket_count=*/1);

  gms_metrics_provider().SetGmsVersionForTesting("234008000");
  gms_metrics_provider().ProvideHistograms();

  histogram_tester().ExpectUniqueSample("Android.PlayServices.ShortVersion",
                                        /*sample=*/23400,
                                        /*expected_bucket_count=*/2);

  gms_metrics_provider().SetGmsVersionForTesting("234010000");
  gms_metrics_provider().ProvideHistograms();

  histogram_tester().ExpectUniqueSample("Android.PlayServices.ShortVersion",
                                        /*sample=*/23400,
                                        /*expected_bucket_count=*/3);
}

TEST_F(GmsMetricsProviderTest, TestGMSNotInstalled) {
  gms_metrics_provider().SetGmsVersionForTesting("0");
  gms_metrics_provider().ProvideHistograms();

  histogram_tester().ExpectUniqueSample("Android.PlayServices.ShortVersion",
                                        /*sample=*/0,
                                        /*expected_bucket_count=*/1);
}

TEST_F(GmsMetricsProviderTest, TestGMSVersionInvalid) {
  gms_metrics_provider().SetGmsVersionForTesting("aaaa");
  gms_metrics_provider().ProvideHistograms();

  histogram_tester().ExpectUniqueSample("Android.PlayServices.ShortVersion",
                                        /*sample=*/1,
                                        /*expected_bucket_count=*/1);
}

TEST_F(GmsMetricsProviderTest, TestGMSVersionOutOfRange) {
  gms_metrics_provider().SetGmsVersionForTesting("11111");
  gms_metrics_provider().ProvideHistograms();

  histogram_tester().ExpectUniqueSample("Android.PlayServices.ShortVersion",
                                        /*sample=*/2,
                                        /*expected_bucket_count=*/1);

  gms_metrics_provider().SetGmsVersionForTesting("999999999");
  gms_metrics_provider().ProvideHistograms();

  histogram_tester().ExpectUniqueSample("Android.PlayServices.ShortVersion",
                                        /*sample=*/2,
                                        /*expected_bucket_count=*/2);
}

}  // namespace metrics