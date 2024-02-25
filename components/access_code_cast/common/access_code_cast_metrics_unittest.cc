// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/access_code_cast/common/access_code_cast_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/test/metrics/histogram_enum_reader.h"
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
  AccessCodeCastMetrics::OnCastSessionResult(
      1 /* RouteRequest::OK */, AccessCodeCastCastMode::kRemotePlayback);
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Discovery.CastModeOnSuccess", 3, 1);
  histogram_tester.ExpectTotalCount(
      "AccessCodeCast.Discovery.CastModeOnSuccess", 4);
}

TEST(AccessCodeCastMetricsTest, RecordAccessCodeNotFoundCount) {
  base::HistogramTester histogram_tester;

  AccessCodeCastMetrics::RecordAccessCodeNotFoundCount(0);
  histogram_tester.ExpectTotalCount("AccessCodeCast.Ui.AccessCodeNotFoundCount",
                                    0);

  AccessCodeCastMetrics::RecordAccessCodeNotFoundCount(1);
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Ui.AccessCodeNotFoundCount", 1, 1);

  AccessCodeCastMetrics::RecordAccessCodeNotFoundCount(100);
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Ui.AccessCodeNotFoundCount", 100, 1);

  // Over 100 should be reported as 100.
  AccessCodeCastMetrics::RecordAccessCodeNotFoundCount(500);
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Ui.AccessCodeNotFoundCount", 100, 2);

  histogram_tester.ExpectTotalCount("AccessCodeCast.Ui.AccessCodeNotFoundCount",
                                    3);
}

TEST(AccessCodeCastMetricsTest, RecordAccessCodeRouteStarted) {
  base::HistogramTester histogram_tester;

  AccessCodeCastCastMode cast_mode = AccessCodeCastCastMode::kPresentation;

  AccessCodeCastMetrics::RecordAccessCodeRouteStarted(base::Seconds(0), false,
                                                      cast_mode);
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Discovery.DeviceDurationOnRoute", 0, 1);

  // Ensure the functions properly converts duration to seconds
  AccessCodeCastMetrics::RecordAccessCodeRouteStarted(base::Milliseconds(10000),
                                                      false, cast_mode);
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Discovery.DeviceDurationOnRoute", 10, 1);
  AccessCodeCastMetrics::RecordAccessCodeRouteStarted(base::Milliseconds(20000),
                                                      false, cast_mode);
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Discovery.DeviceDurationOnRoute", 20, 1);

  histogram_tester.ExpectTotalCount(
      "AccessCodeCast.Discovery.DeviceDurationOnRoute", 3);
}

TEST(AccessCodeCastMetricsTest, RecordAccessCodeRouteStartedRouteInfo) {
  base::HistogramTester histogram_tester;

  AccessCodeCastMetrics::RecordAccessCodeRouteStarted(
      base::Seconds(0), true, AccessCodeCastCastMode::kPresentation);
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Session.RouteDiscoveryTypeAndSource", 1, 1);

  AccessCodeCastMetrics::RecordAccessCodeRouteStarted(
      base::Seconds(0), true, AccessCodeCastCastMode::kTabMirror);
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Session.RouteDiscoveryTypeAndSource", 2, 1);

  AccessCodeCastMetrics::RecordAccessCodeRouteStarted(
      base::Seconds(0), true, AccessCodeCastCastMode::kDesktopMirror);
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Session.RouteDiscoveryTypeAndSource", 3, 1);

  AccessCodeCastMetrics::RecordAccessCodeRouteStarted(
      base::Seconds(0), true, AccessCodeCastCastMode::kRemotePlayback);
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Session.RouteDiscoveryTypeAndSource", 4, 1);

  AccessCodeCastMetrics::RecordAccessCodeRouteStarted(
      base::Seconds(0), false, AccessCodeCastCastMode::kPresentation);
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Session.RouteDiscoveryTypeAndSource", 5, 1);

  AccessCodeCastMetrics::RecordAccessCodeRouteStarted(
      base::Seconds(0), false, AccessCodeCastCastMode::kTabMirror);
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Session.RouteDiscoveryTypeAndSource", 6, 1);

  AccessCodeCastMetrics::RecordAccessCodeRouteStarted(
      base::Seconds(0), false, AccessCodeCastCastMode::kDesktopMirror);
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Session.RouteDiscoveryTypeAndSource", 7, 1);

  AccessCodeCastMetrics::RecordAccessCodeRouteStarted(
      base::Seconds(0), false, AccessCodeCastCastMode::kRemotePlayback);
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Session.RouteDiscoveryTypeAndSource", 8, 1);

  histogram_tester.ExpectTotalCount(
      "AccessCodeCast.Session.RouteDiscoveryTypeAndSource", 8);
}

TEST(AccessCodeCastMetricsTest, RecordDialogLoadTime) {
  base::HistogramTester histogram_tester;

  AccessCodeCastMetrics::RecordDialogLoadTime(base::Milliseconds(10));
  histogram_tester.ExpectBucketCount("AccessCodeCast.Ui.DialogLoadTime", 10, 1);

  // Ten seconds (10,000 ms) is the max for UmaHistogramTimes.
  AccessCodeCastMetrics::RecordDialogLoadTime(base::Seconds(10));
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Ui.DialogLoadTime", 10000, 1);
  AccessCodeCastMetrics::RecordDialogLoadTime(base::Seconds(20));
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Ui.DialogLoadTime", 10000, 2);

  histogram_tester.ExpectTotalCount("AccessCodeCast.Ui.DialogLoadTime", 3);
}

TEST(AccessCodeCastMetricsTest, RecordDialogCloseReason) {
  base::HistogramTester histogram_tester;

  AccessCodeCastMetrics::RecordDialogCloseReason(
      AccessCodeCastDialogCloseReason::kFocus);
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Ui.DialogCloseReason", 0, 1);

  AccessCodeCastMetrics::RecordDialogCloseReason(
      AccessCodeCastDialogCloseReason::kCancel);
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Ui.DialogCloseReason", 1, 1);

  AccessCodeCastMetrics::RecordDialogCloseReason(
      AccessCodeCastDialogCloseReason::kCastSuccess);
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Ui.DialogCloseReason", 2, 1);

  histogram_tester.ExpectTotalCount("AccessCodeCast.Ui.DialogCloseReason", 3);
}

TEST(AccessCodeCastMetricsTest, RecordRememberedDevicesCount) {
  base::HistogramTester histogram_tester;

  AccessCodeCastMetrics::RecordRememberedDevicesCount(0);
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Discovery.RememberedDevicesCount", 0, 1);

  AccessCodeCastMetrics::RecordRememberedDevicesCount(1);
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Discovery.RememberedDevicesCount", 1, 1);

  AccessCodeCastMetrics::RecordRememberedDevicesCount(100);
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Discovery.RememberedDevicesCount", 100, 1);

  // Over 100 should be reported as 100.
  AccessCodeCastMetrics::RecordRememberedDevicesCount(500);
  histogram_tester.ExpectBucketCount(
      "AccessCodeCast.Discovery.RememberedDevicesCount", 100, 2);
}

TEST(AccessCodeCastMetricsTest, RecordRouteDuration) {
  base::HistogramTester histogram_tester;
  char histogram[] = "AccessCodeCast.Session.RouteDuration";

  AccessCodeCastMetrics::RecordRouteDuration(base::Milliseconds(1));
  // The custom times histogram has a minimum value of 1 second.
  histogram_tester.ExpectTimeBucketCount(histogram, base::Seconds(1), 1);

  AccessCodeCastMetrics::RecordRouteDuration(base::Minutes(5));
  histogram_tester.ExpectTimeBucketCount(histogram, base::Minutes(5), 1);

  AccessCodeCastMetrics::RecordRouteDuration(base::Hours(10));
  // The custom times histogram has a maximum value of 8 hours.
  histogram_tester.ExpectTimeBucketCount(histogram, base::Hours(8), 1);

  histogram_tester.ExpectTotalCount(histogram, 3);
}

TEST(AccessCodeCastMetricsTest, RecordMirroringPauseCount) {
  base::HistogramTester histogram_tester;
  char histogram[] = "AccessCodeCast.Session.FreezeCount";

  AccessCodeCastMetrics::RecordMirroringPauseCount(0);
  histogram_tester.ExpectBucketCount(histogram, 0, 1);

  AccessCodeCastMetrics::RecordMirroringPauseCount(1);
  histogram_tester.ExpectBucketCount(histogram, 1, 1);

  AccessCodeCastMetrics::RecordMirroringPauseCount(100);
  histogram_tester.ExpectBucketCount(histogram, 100, 1);

  // Over 100 should be reported as 100.
  AccessCodeCastMetrics::RecordMirroringPauseCount(500);
  histogram_tester.ExpectBucketCount(histogram, 100, 2);

  histogram_tester.ExpectTotalCount(histogram, 4);
}

TEST(AccessCodeCastMetricsTest, RecordMirroringPauseDuration) {
  base::HistogramTester histogram_tester;
  char histogram[] = "AccessCodeCast.Session.FreezeDuration";

  AccessCodeCastMetrics::RecordMirroringPauseDuration(base::Milliseconds(1));
  histogram_tester.ExpectTimeBucketCount(histogram, base::Milliseconds(1), 1);

  AccessCodeCastMetrics::RecordMirroringPauseDuration(base::Minutes(5));
  histogram_tester.ExpectTimeBucketCount(histogram, base::Minutes(5), 1);

  AccessCodeCastMetrics::RecordMirroringPauseDuration(base::Hours(2));
  // The long times histogram has a maximum value of 1 hours.
  histogram_tester.ExpectTimeBucketCount(histogram, base::Hours(1), 1);

  histogram_tester.ExpectTotalCount(histogram, 3);
}

TEST(AccessCodeCastMetricsTest, CheckMetricsEnums) {
  base::HistogramTester histogram_tester;

  // AddSinkResult
  std::optional<base::HistogramEnumEntryMap> add_sink_results =
      base::ReadEnumFromEnumsXml("AccessCodeCastAddSinkResult");
  EXPECT_TRUE(add_sink_results->size() ==
      static_cast<int>(AccessCodeCastAddSinkResult::kMaxValue) + 1)
      << "'AccessCodeCastAddSinkResult' enum was changed in "
         "access_code_cast_metrics.h. Please update the entry in "
         "enums.xml to match.";

  // CastMode
  std::optional<base::HistogramEnumEntryMap> cast_modes =
      base::ReadEnumFromEnumsXml("AccessCodeCastCastMode");
  EXPECT_TRUE(cast_modes->size() ==
      static_cast<int>(AccessCodeCastCastMode::kMaxValue) + 1)
      << "'AccessCodeCastCastMode' enum was changed in "
         "access_code_cast_metrics.h. Please update the entry in "
         "enums.xml to match.";

  // DialogCloseReason
  std::optional<base::HistogramEnumEntryMap> dialog_close_reasons =
      base::ReadEnumFromEnumsXml("AccessCodeCastDialogCloseReason");
  EXPECT_TRUE(dialog_close_reasons->size() ==
      static_cast<int>(AccessCodeCastDialogCloseReason::kMaxValue) + 1)
      << "'AccessCodeCastDialogCloseReason' enum was changed in "
         "access_code_cast_metrics.h. Please update the entry in "
         "enums.xml to match.";

  // DialogOpenLocation
  std::optional<base::HistogramEnumEntryMap> dialog_open_locations =
      base::ReadEnumFromEnumsXml("AccessCodeCastDialogOpenLocation");
  EXPECT_TRUE(dialog_open_locations->size() ==
      static_cast<int>(AccessCodeCastDialogOpenLocation::kMaxValue) + 1)
      << "'AccessCodeCastDialogOpenLocation' enum was changed in "
         "access_code_cast_metrics.h. Please update the entry in "
         "enums.xml to match.";

  // DiscoveryTypeAndSource
  std::optional<base::HistogramEnumEntryMap> discovery_types_and_sources =
      base::ReadEnumFromEnumsXml("AccessCodeCastDiscoveryTypeAndSource");
  EXPECT_TRUE(
      discovery_types_and_sources->size() ==
      static_cast<int>(AccessCodeCastDiscoveryTypeAndSource::kMaxValue) + 1)
      << "'AccessCodeCastDicoveryTypeAndSource' enum was changed in "
         "access_code_cast_metrics.h. Please update the entry in "
         "enums.xml to match.";
}

TEST(AccessCodeCastMetricsTest, RecordSavedDeviceConnectDuration) {
  base::HistogramTester histogram_tester;
  char histogram[] = "AccessCodeCast.Session.SavedDeviceRouteCreationDuration";

  AccessCodeCastMetrics::RecordSavedDeviceConnectDuration(
      base::Milliseconds(1));
  histogram_tester.ExpectTimeBucketCount(histogram, base::Milliseconds(1), 1);

  AccessCodeCastMetrics::RecordSavedDeviceConnectDuration(
      base::Milliseconds(500));
  histogram_tester.ExpectTimeBucketCount(histogram, base::Milliseconds(500), 1);

  AccessCodeCastMetrics::RecordSavedDeviceConnectDuration(base::Hours(10));
  histogram_tester.ExpectTimeBucketCount(histogram, base::Seconds(180), 1);

  histogram_tester.ExpectTotalCount(histogram, 3);
}

TEST(AccessCodeCastMetricsTest, RecordNewDeviceConnectDuration) {
  base::HistogramTester histogram_tester;
  char histogram[] = "AccessCodeCast.Session.NewDeviceRouteCreationDuration";

  AccessCodeCastMetrics::RecordNewDeviceConnectDuration(base::Milliseconds(1));
  histogram_tester.ExpectTimeBucketCount(histogram, base::Milliseconds(1), 1);

  AccessCodeCastMetrics::RecordNewDeviceConnectDuration(
      base::Milliseconds(500));
  histogram_tester.ExpectTimeBucketCount(histogram, base::Milliseconds(500), 1);

  AccessCodeCastMetrics::RecordNewDeviceConnectDuration(base::Hours(10));
  histogram_tester.ExpectTimeBucketCount(histogram, base::Seconds(180), 1);

  histogram_tester.ExpectTotalCount(histogram, 3);
}
