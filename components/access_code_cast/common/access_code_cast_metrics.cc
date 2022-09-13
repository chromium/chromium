// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/access_code_cast/common/access_code_cast_metrics.h"

#include "base/metrics/histogram_functions.h"

AccessCodeCastMetrics::AccessCodeCastMetrics() = default;
AccessCodeCastMetrics::~AccessCodeCastMetrics() = default;

// static
const char AccessCodeCastMetrics::kHistogramAddSinkResultNew[] =
    "AccessCodeCast.Discovery.AddSinkResult.New";
const char AccessCodeCastMetrics::kHistogramAddSinkResultRemembered[] =
    "AccessCodeCast.Discovery.AddSinkResult.Remembered";
const char AccessCodeCastMetrics::kHistogramCastModeOnSuccess[] =
    "AccessCodeCast.Discovery.CastModeOnSuccess";
const char AccessCodeCastMetrics::kHistogramDialogCloseReason[] =
    "AccessCodeCast.Ui.DialogCloseReason";
const char AccessCodeCastMetrics::kHistogramDialogLoadTime[] =
    "AccessCodeCast.Ui.DialogLoadTime";
const char AccessCodeCastMetrics::kHistogramDialogOpenLocation[] =
    "AccessCodeCast.Ui.DialogOpenLocation";
const char AccessCodeCastMetrics::kHistogramRememberedDevicesCount[] =
    "AccessCodeCast.Discovery.RememberedDevicesCount";

// static
void AccessCodeCastMetrics::OnCastSessionResult(int route_request_result_code,
                                                AccessCodeCastCastMode mode) {
  if (route_request_result_code == 1 /* ResultCode::OK */) {
    base::UmaHistogramEnumeration(kHistogramCastModeOnSuccess, mode);
  }
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
