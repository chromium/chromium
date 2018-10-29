// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_ARC_PREFS_H_
#define COMPONENTS_ARC_ARC_PREFS_H_

#include "components/arc/arc_export.h"

class PrefRegistrySimple;

namespace arc {
namespace prefs {

// Sorted in lexicographical order.
ARC_EXPORT extern const char kAlwaysOnVpnLockdown[];
ARC_EXPORT extern const char kAlwaysOnVpnPackage[];
ARC_EXPORT extern const char kArcActiveDirectoryPlayUserId[];
ARC_EXPORT extern const char kArcApps[];
ARC_EXPORT extern const char kArcBackupRestoreEnabled[];
ARC_EXPORT extern const char kArcDataRemoveRequested[];
ARC_EXPORT extern const char kArcEnabled[];
ARC_EXPORT extern const char kArcFastAppReinstallPackages[];
ARC_EXPORT extern const char kArcFastAppReinstallStarted[];
ARC_EXPORT extern const char kArcInitialSettingsPending[];
ARC_EXPORT extern const char kArcPolicyComplianceReported[];
ARC_EXPORT extern const char kArcTermsAccepted[];
ARC_EXPORT extern const char kArcTermsShownInOobe[];
ARC_EXPORT extern const char kArcLocationServiceEnabled[];
ARC_EXPORT extern const char kArcPackages[];
ARC_EXPORT extern const char kArcPaiStarted[];
ARC_EXPORT extern const char kArcPushInstallAppsRequested[];
ARC_EXPORT extern const char kArcPushInstallAppsPending[];
ARC_EXPORT extern const char kArcSetNotificationsEnabledDeferred[];
ARC_EXPORT extern const char kArcSignedIn[];
ARC_EXPORT extern const char kArcSkippedReportingNotice[];
ARC_EXPORT extern const char kArcSupervisionTransition[];
ARC_EXPORT extern const char kArcCompatibleFilesystemChosen[];
ARC_EXPORT extern const char kArcVoiceInteractionValuePropAccepted[];
ARC_EXPORT extern const char kEcryptfsMigrationStrategy[];

// TODO(b/110211045): Move Assistant related prefs to ash.
ARC_EXPORT extern const char kVoiceInteractionActivityControlAccepted[];
ARC_EXPORT extern const char kVoiceInteractionContextEnabled[];
ARC_EXPORT extern const char kVoiceInteractionEnabled[];
ARC_EXPORT extern const char kVoiceInteractionHotwordEnabled[];
ARC_EXPORT extern const char kVoiceInteractionLaunchWithMicOpen[];
ARC_EXPORT extern const char kVoiceInteractionNotificationEnabled[];

void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace prefs
}  // namespace arc

#endif  // COMPONENTS_ARC_ARC_PREFS_H_
