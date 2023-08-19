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

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AccessCodeCastDiscoveryTypeAndSource {
  kUnknown = 0,
  kSavedDevicePresentation = 1,
  kSavedDeviceTabMirror = 2,
  kSavedDeviceDesktopMirror = 3,
  kSavedDeviceRemotePlayback = 4,
  kNewDevicePresentation = 5,
  kNewDeviceTabMirror = 6,
  kNewDeviceDesktopMirror = 7,
  kNewDeviceRemotePlayback = 8,

  // NOTE: Do not reorder existing entries, and add entries only immediately
  // above this line.
  kMaxValue = kNewDeviceRemotePlayback
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AccessCodeCastUiTabSwitcherUsage {
  kTabSwitcherUiShownAndNotUsed = 0,
  kTabSwitcherUiShownAndUsedToSwitchTabs = 1,

  // NOTE: Do not reorder existing entries, and add entries only immediately
  // above this line.
  kMaxValue = kTabSwitcherUiShownAndUsedToSwitchTabs,
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
  static const char kHistogramDeviceDurationOnRoute[];
  static const char kHistogramDialogCloseReason[];
  static const char kHistogramDialogLoadTime[];
  static const char kHistogramDialogOpenLocation[];
  static const char kHistogramFreezeCount[];
  static const char kHistogramFreezeDuration[];
  static const char kHistogramNewDeviceRouteCreationDuration[];
  static const char kHistogramRememberedDevicesCount[];
  static const char kHistogramRouteDiscoveryTypeAndSource[];
  static const char kHistogramRouteDuration[];
  static const char kHistogramSavedDeviceRouteCreationDuration[];
  static const char kHistogramUiTabSwitcherUsageType[];
  static const char kHistogramUiTabSwitchingCount[];

  // Records metrics relating to starting a cast session (route). Mode is
  // media_router::MediaCastMode.
  static void OnCastSessionResult(int route_request_result_code,
                                  AccessCodeCastCastMode mode);

  // Records the count of ACCESS_CODE_NOT_FOUND errors per instance of dialog.
  static void RecordAccessCodeNotFoundCount(int count);

  // Records the value of the device duration pref on successful creation of
  // an access code route. Also records the discovery type and cast source.
  static void RecordAccessCodeRouteStarted(base::TimeDelta duration,
                                           bool is_saved,
                                           AccessCodeCastCastMode mode);

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

  // Records the number of times a mirroring session is paused during its
  // duration.
  static void RecordMirroringPauseCount(int count);

  // Records the duration of time that a mirroring session is paused.
  static void RecordMirroringPauseDuration(base::TimeDelta duration);

  // Records the count of cast devices which are currently being remembered
  // being the AccessCodeCastSinkService.
  static void RecordRememberedDevicesCount(int count);

  // Records the length of time that a route to an access code device lasts.
  // The minimum length of time reported is one second, so any lower durations
  // will be rounded up. Also, the largest time reported is 8 hours, and any
  // longer times will be bucketed down.
  static void RecordRouteDuration(base::TimeDelta duration);

  // Records the count of tabs a user switches to during a tab mirroring
  // session.
  static void RecordTabSwitchesCountInTabSession(int count);

  // Records the usage type of tab switcher UI, i.e. shown only and not used or
  // shown and actually used to switch tabs.
  static void RecordTabSwitcherUsageCase(
      AccessCodeCastUiTabSwitcherUsage usage);

  // Records the time that it takes to connect to a saved device. It is a
  // combination of the time to request a mirroring route + waiting for a
  // success. It is only recorded if the request was successful.
  static void RecordSavedDeviceConnectDuration(base::TimeDelta duration);

  // Records the time it takes to connect to a new device. It is the combination
  // of connecting to our server, validating the access code, constructing a
  // cast device, opening a channel to that device, and then waiting for
  // success.
  static void RecordNewDeviceConnectDuration(base::TimeDelta duration);
};

#endif  // COMPONENTS_ACCESS_CODE_CAST_COMMON_ACCESS_CODE_CAST_METRICS_H_
