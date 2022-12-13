// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/access_code_cast/common/access_code_cast_metrics.h"

#include "base/metrics/histogram_functions.h"

AccessCodeCastMetrics::AccessCodeCastMetrics() = default;
AccessCodeCastMetrics::~AccessCodeCastMetrics() = default;

// static
const char AccessCodeCastMetrics::kHistogramAccessCodeNotFoundCount[] =
    "AccessCodeCast.Ui.AccessCodeNotFoundCount";
const char AccessCodeCastMetrics::kHistogramAddSinkResultNew[] =
    "AccessCodeCast.Discovery.AddSinkResult.New";
const char AccessCodeCastMetrics::kHistogramAddSinkResultRemembered[] =
    "AccessCodeCast.Discovery.AddSinkResult.Remembered";
const char AccessCodeCastMetrics::kHistogramCastModeOnSuccess[] =
    "AccessCodeCast.Discovery.CastModeOnSuccess";
const char AccessCodeCastMetrics::kHistogramDeviceDurationOnRoute[] =
    "AccessCodeCast.Discovery.DeviceDurationOnRoute";
const char AccessCodeCastMetrics::kHistogramDialogCloseReason[] =
    "AccessCodeCast.Ui.DialogCloseReason";
const char AccessCodeCastMetrics::kHistogramDialogLoadTime[] =
    "AccessCodeCast.Ui.DialogLoadTime";
const char AccessCodeCastMetrics::kHistogramDialogOpenLocation[] =
    "AccessCodeCast.Ui.DialogOpenLocation";
const char AccessCodeCastMetrics::kHistogramRememberedDevicesCount[] =
    "AccessCodeCast.Discovery.RememberedDevicesCount";
const char AccessCodeCastMetrics::kHistogramRouteDuration[] =
    "AccessCodeCast.Session.RouteDuration";
const char AccessCodeCastMetrics::kHistogramUiTabSwitcherUsageType[] =
    "AccessCodeCast.Ui.TabSwitcherUsageType";
const char AccessCodeCastMetrics::kHistogramUiTabSwitchingCount[] =
    "AccessCodeCast.Ui.TabSwitchingCount";

// static
void AccessCodeCastMetrics::OnCastSessionResult(int route_request_result_code,
                                                AccessCodeCastCastMode mode) {
  if (route_request_result_code == 1 /* ResultCode::OK */) {
    base::UmaHistogramEnumeration(kHistogramCastModeOnSuccess, mode);
  }
}

// static
void AccessCodeCastMetrics::RecordAccessCodeNotFoundCount(int count) {
  // Do not record if there were no incorrect codes.
  if (count <= 0)
    return;

  base::UmaHistogramCounts100(kHistogramAccessCodeNotFoundCount, count);
}

// static
void AccessCodeCastMetrics::RecordAccessCodeRouteStarted(
    base::TimeDelta duration) {
  int64_t duration_seconds = duration.InSeconds();
  // Duration can take one of five values, ranging from zero (0 sec), up to
  // a year (31536000 sec). So, recording as a sparse histogram is best.
  base::UmaHistogramSparse(kHistogramDeviceDurationOnRoute, duration_seconds);
}

// static
void AccessCodeCastMetrics::RecordAddSinkResult(
    bool is_remembered,
    AccessCodeCastAddSinkResult result) {
  if (is_remembered) {
    base::UmaHistogramEnumeration(kHistogramAddSinkResultRemembered, result);
  } else {
    base::UmaHistogramEnumeration(kHistogramAddSinkResultNew, result);
  }
}

// static
void AccessCodeCastMetrics::RecordDialogCloseReason(
    AccessCodeCastDialogCloseReason reason) {
  base::UmaHistogramEnumeration(kHistogramDialogCloseReason, reason);
}

// static
void AccessCodeCastMetrics::RecordDialogLoadTime(base::TimeDelta load_time) {
  base::UmaHistogramTimes(kHistogramDialogLoadTime, load_time);
}

// static
void AccessCodeCastMetrics::RecordDialogOpenLocation(
    AccessCodeCastDialogOpenLocation location) {
  base::UmaHistogramEnumeration(kHistogramDialogOpenLocation, location);
}

// static
void AccessCodeCastMetrics::RecordRememberedDevicesCount(int count) {
  base::UmaHistogramCounts100(kHistogramRememberedDevicesCount, count);
}

// static
void AccessCodeCastMetrics::RecordRouteDuration(base::TimeDelta duration) {
  base::TimeDelta min_time = base::Seconds(1);
  base::UmaHistogramCustomTimes(
      /*name=*/kHistogramRouteDuration,
      /*sample=*/std::max(duration, min_time),
      /*min=*/min_time,
      /*max=*/base::Hours(8),
      /*buckets=*/100);
}

// static
void AccessCodeCastMetrics::RecordTabSwitchesCountInTabSession(int count) {
  base::UmaHistogramCounts100(kHistogramUiTabSwitchingCount, count);
}

// static
void AccessCodeCastMetrics::RecordTabSwitcherUsageCase(
    AccessCodeCastUiTabSwitcherUsage usage) {
  base::UmaHistogramEnumeration(kHistogramUiTabSwitcherUsageType, usage);
}
