// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_IDLE_METRICS_H_
#define COMPONENTS_ENTERPRISE_IDLE_METRICS_H_

#include <string>

#include "base/time/time.h"

namespace enterprise_idle {

namespace metrics {

// IdleTimeout and IdleTimeout policies histogram names.
inline constexpr char kUMAIdleTimeoutActionSuccessTime[] =
    "Enterprise.IdleTimeoutPolicies.ActionTime.%s";
inline constexpr char kUMAIdleTimeoutActionSuccesStatus[] =
    "Enterprise.IdleTimeoutPolicies.Success.%s";
inline constexpr char kUMAIdleTimeoutActionCase[] =
    "Enterprise.IdleTimeoutPolicies.IdleTimeoutCase";
inline constexpr char kUMAIdleTimeoutDialogEvent[] =
    "Enterprise.IdleTimeoutPolicies.IdleTimeoutDialogEvent";
inline constexpr char kUMAIdleTimeoutLaunchScreenEvent[] =
    "Enterprise.IdleTimeoutPolicies.IdleTimeoutLaunchScreenEvent";

// UMA Histogram name suffixes for idle timeout action names.
inline constexpr char kUMAClearBrowsingDataSuffix[] = "ClearBrowsingData";
inline constexpr char kUMASignOutSuffix[] = "SignOut";
inline constexpr char kUMACloseBrowsersSuffix[] = "CloseBrowsers";
inline constexpr char kUMACloseTabsSuffix[] = "CloseTabs";
inline constexpr char kUMAReloadPagesSuffix[] = "ReloadPages";
inline constexpr char kUMAShowProfilePickerSuffix[] = "ShowProfilePicker";
inline constexpr char kUMAAllActionsSuffix[] = "AllActions";

// The different action types that can run on idle timeout. `kClearBrowsingData`
// encompasses all the clear_* actions that can be specified in the
// IdleTimeoutActions policy list value.
enum class IdleTimeoutActionType {
  kClearBrowsingData = 0,
  kSignOut = 1,
  kCloseBrowsers = 2,
  kCloseTabs = 3,
  kReloadPages = 4,
  kShowProfilePicker = 5,
  kAllActions = 6,
  kMaxValue = kAllActions,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Keep in sync with
// tools/metrics/histograms/metadata/enterprise/enums.xml.
enum class IdleTimeoutCase {
  kForeground = 0,
  kBackground = 1,
  kMaxValue = kBackground,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Keep in sync with
// tools/metrics/histograms/metadata/enterprise/enums.xml.
// The values represent different idle timeout dialog UI events that can
// happen.
// `kDialogShown` : the idle timeout dialog is shown when the browser times out
// in foreground.
// `kDialogDismissedByUser` : the dioalog is either dismissed by
// the user if they are active.
//  `kDialogExpired` :  the dualog counted down to expiry before being
//  dismissed.
enum class IdleTimeoutDialogEvent {
  kDialogShown = 0,
  kDialogDismissedByUser = 1,
  kDialogExpired = 2,
  kMaxValue = kDialogExpired,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Keep in sync with
// tools/metrics/histograms/metadata/enterprise/enums.xml.
// The values represent the different launch screen events that can happen on
// app foreground on mobile.
// `kLaunchScreenShown` : a launch scren is shown when idle timeout actions run
// on foreground.
/// `kLaunchScreenDismissedAfterActionCompletion` : the launch
// screen is dismissed when actions complete.
// `kLaunchScreenExpired` : the
// launch screen has been displayed until expiry. Actions failed to complete by
// deadline.
enum class IdleTimeoutLaunchScreenEvent {
  kLaunchScreenShown = 0,
  kLaunchScreenDismissedAfterActionCompletion = 1,
  kLaunchScreenExpired = 2,
  kMaxValue = kLaunchScreenExpired,
};

// Records the time needed for action `type` to run when the browser times out.
void RecordIdleTimeoutActionTimeTaken(IdleTimeoutActionType type,
                                      base::TimeDelta time_duration);

// Records whether idle timeout happens in foreground or background.
void RecordIdleTimeoutCase(IdleTimeoutCase timeout_case);

// Records whether action `type` ran successfully.
void RecordActionsSuccess(IdleTimeoutActionType type, bool success);

// Records an idle timeout dialog events.
void RecordIdleTimeoutDialogEvent(IdleTimeoutDialogEvent event);

// Records an idle timeout launch screen events.
void RecordIdleTimeoutLaunchScreenEvent(IdleTimeoutLaunchScreenEvent event);

// Returns the IdleTimeoutAction name from its `IdleTimeoutActionType`.
std::string GetActionNameFromActionType(IdleTimeoutActionType type);

}  // namespace metrics

}  // namespace enterprise_idle

#endif  // COMPONENTS_ENTERPRISE_IDLE_METRICS_H_
