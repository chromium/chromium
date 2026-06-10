// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/signin_deep_link_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/signin/public/base/signin_deep_link_payload.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin_metrics {

TEST(SigninDeepLinkMetricsTest, RecordUrlDetected) {
  base::HistogramTester histogram_tester;
  RecordUrlDetected(1000);
  histogram_tester.ExpectUniqueSample("Signin.CrossDevice.UrlDetected", 1000,
                                      1);
}

TEST(SigninDeepLinkMetricsTest, RecordInitialAccountsNumber) {
  base::HistogramTester histogram_tester;
  RecordInitialAccountsNumber(signin::ExternalEntryPoint::kDesktopDefault, 3);
  histogram_tester.ExpectUniqueSample(
      "Signin.CrossDevice.InitialAccountsNumber", 3, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.CrossDevice.InitialAccountsNumber.DesktopDefault", 3, 1);
}

}  // namespace signin_metrics
