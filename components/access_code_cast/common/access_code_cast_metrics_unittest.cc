// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/access_code_cast/common/access_code_cast_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(AccessCodeCastMetricsTest, RecordDialogOpenLocation) {
  base::HistogramTester histogram_tester;

  AccessCodeCastMetrics::RecordDialogOpenLocation(
      AccessCodeCastDialogOpenLocation::kBrowserCastMenu);
  histogram_tester.ExpectBucketCount("AccessCodeCast.Ui.DialogOpenLocation", 0,
                                     1);

  AccessCodeCastMetrics::RecordDialogOpenLocation(
      AccessCodeCastDialogOpenLocation::kSystemTrayCastFeaturePod);
  histogram_tester.ExpectBucketCount("AccessCodeCast.Ui.DialogOpenLocation", 1,
                                     1);
  histogram_tester.ExpectTotalCount("AccessCodeCast.Ui.DialogOpenLocation", 2);

  AccessCodeCastMetrics::RecordDialogOpenLocation(
      AccessCodeCastDialogOpenLocation::kSystemTrayCastMenu);
  histogram_tester.ExpectBucketCount("AccessCodeCast.Ui.DialogOpenLocation", 2,
                                     1);
  histogram_tester.ExpectTotalCount("AccessCodeCast.Ui.DialogOpenLocation", 3);
}
