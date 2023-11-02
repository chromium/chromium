// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESS_CODE_CAST_COMMON_ACCESS_CODE_CAST_METRICS_H_
#define COMPONENTS_ACCESS_CODE_CAST_COMMON_ACCESS_CODE_CAST_METRICS_H_

#include "base/time/time.h"

// NOTE: Do not renumber enums as that would confuse interpretation of
// previously logged data. When making changes, also update the enum list
// in tools/metrics/histograms/enums.xml to keep it in sync.

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AccessCodeCastAddSinkResult {
  kUnknownError = 0,
  kOk = 1,
  kAuthError = 2,
  kHttpResponseCodeError = 3,
  kResponseMalformed = 4,
  kEmptyResponse = 5,
  kInvalidAccessCode = 6,
  kAccessCodeNotFound = 7,
  kTooManyRequests = 8,
  kServiceNotPresent = 9,
  kServerError = 10,
  kSinkCreationError = 11,
  kChannelOpenError = 12,
  kProfileSyncError = 13,
  kInternalMediaRouterError = 14,

  // NOTE: Do not reorder existing entries, and add entries only immediately
  // above this line.
  kMaxValue = kInternalMediaRouterError
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AccessCodeCastCastMode {
  kPresentation = 0,
  kTabMirror = 1,
  kDesktopMirror = 2,
  kRemotePlayback = 3,

  // NOTE: Do not reorder existing entries, and add entries only immediately
  // above this line.
  kMaxValue = kRemotePlayback,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AccessCodeCastDialogCloseReason {
  kFocus = 0,
  kCancel = 1,
  kCastSuccess = 2,

  // NOTE: Do not reorder existing entries, and add entries only immediately
  // above this line.
  kMaxValue = kCastSuccess
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AccessCodeCastDialogOpenLocation {
  kBrowserCastMenu = 0,
  kSystemTrayCastFeaturePod = 1,
  kSystemTrayCastMenu = 2,

  // NOTE: Do not reorder existing entries, and add entries only immediately
  // above this line.
  kMaxValue = kSystemTrayCastMenu
};

class AccessCodeCastMetrics {
 public:
  AccessCodeCastMetrics();
  ~AccessCodeCastMetrics();

  // UMA histogram names.
  static const char kHistogramAccessCodeNotFoundCount[];
  static const char kHistogramAddSinkResultNew[];
  static const char kHistogramAddSinkResultRemembered[];
  static const char kHistogramCastModeOnSuccess[];
  static const char kHistogramDialogCloseReason[];
  static const char kHistogramDialogLoadTime[];
  static const char kHistogramDialogOpenLocation[];
  static const char kHistogramRememberedDevicesCount[];

  // Records metrics relating to starting a cast session (route). Mode is
  // media_router::MediaCastMode.
  static void OnCastSessionResult(int route_request_result_code,
                                  AccessCodeCastCastMode mode);

  // Records the count of ACCESS_CODE_NOT_FOUND errors per instance of dialog.
  static void RecordAccessCodeNotFoundCount(int count);

  // Records the result of adding an access code sink.
  static void RecordAddSinkResult(bool is_remembered,
                                  AccessCodeCastAddSinkResult result);

  // Records the time it takes for the AccessCodeCast dialog to load.
  static void RecordDialogLoadTime(base::TimeDelta load_time);

  // Records the reason that the AccessCodeCast dialog closed.
  static void RecordDialogCloseReason(AccessCodeCastDialogCloseReason reason);

  // Records where the user clicked to open the AccessCodeCast dialog.
  static void RecordDialogOpenLocation(
      AccessCodeCastDialogOpenLocation location);

  // Records the count of cast devices which are currently being remembered
  // being the AccessCodeCastSinkService.
  static void RecordRememberedDevicesCount(int count);
};

#endif  // COMPONENTS_ACCESS_CODE_CAST_COMMON_ACCESS_CODE_CAST_METRICS_H_
