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

TEST(AccessCodeCastMetricsTest, RecordAddSinkResult) {
  base::HistogramTester histogram_tester;

  AccessCodeCastMetrics::RecordAddSinkResult(
      false, AccessCodeCastAddSinkResult::kUnknownError);
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Discovery.AddSinkResult.New", 0, 1);
  AccessCodeCastMetrics::RecordAddSinkResult(false,
                                             AccessCodeCastAddSinkResult::kOk);
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Discovery.AddSinkResult.New", 1, 1);
  AccessCodeCastMetrics::RecordAddSinkResult(
      false, AccessCodeCastAddSinkResult::kAuthError);
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Discovery.AddSinkResult.New", 2, 1);
  histogram_tester.ExpectTotalCount(
      "AccessCodeCast.Discovery.AddSinkResult.New", 3);

  AccessCodeCastMetrics::RecordAddSinkResult(
      true, AccessCodeCastAddSinkResult::kUnknownError);
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Discovery.AddSinkResult.Remembered", 0, 1);
  AccessCodeCastMetrics::RecordAddSinkResult(true,
                                             AccessCodeCastAddSinkResult::kOk);
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Discovery.AddSinkResult.Remembered", 1, 1);
  AccessCodeCastMetrics::RecordAddSinkResult(
      true, AccessCodeCastAddSinkResult::kAuthError);
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Discovery.AddSinkResult.Remembered", 2, 1);
  histogram_tester.ExpectTotalCount(
      "AccessCodeCast.Discovery.AddSinkResult.New", 3);
  histogram_tester.ExpectTotalCount(
      "AccessCodeCast.Discovery.AddSinkResult.Remembered", 3);
}

TEST(AccessCodeCastMetricsTest, OnCastSessionResult) {
  base::HistogramTester histogram_tester;

  AccessCodeCastMetrics::OnCastSessionResult(
      0 /* ResultCode::UNKNOWN_ERROR */, AccessCodeCastCastMode::kPresentation);
  histogram_tester.ExpectTotalCount(
      "AccessCodeCast.Discovery.CastModeOnSuccess", 0);

  AccessCodeCastMetrics::OnCastSessionResult(
      1 /* RouteRequest::OK */, AccessCodeCastCastMode::kPresentation);
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Discovery.CastModeOnSuccess", 0, 1);
  AccessCodeCastMetrics::OnCastSessionResult(
      1 /* RouteRequest::OK */, AccessCodeCastCastMode::kTabMirror);
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Discovery.CastModeOnSuccess", 1, 1);
  AccessCodeCastMetrics::OnCastSessionResult(
      1 /* RouteRequest::OK */, AccessCodeCastCastMode::kDesktopMirror);
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Discovery.CastModeOnSuccess", 2, 1);
  histogram_tester.ExpectTotalCount(
      "AccessCodeCast.Discovery.CastModeOnSuccess", 3);
}
