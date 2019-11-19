// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_ARC_PREFS_H_
#define COMPONENTS_ARC_ARC_PREFS_H_

#include "components/arc/arc_export.h"

class PrefRegistrySimple;

namespace arc {
namespace prefs {

// Profile prefs in lexicographical order. See below for local state prefs.
ARC_EXPORT extern const char kAlwaysOnVpnLockdown[];
ARC_EXPORT extern const char kAlwaysOnVpnPackage[];
ARC_EXPORT extern const char kArcActiveDirectoryPlayUserId[];
ARC_EXPORT extern const char kArcApps[];
ARC_EXPORT extern const char kArcBackupRestoreEnabled[];
ARC_EXPORT extern const char kArcCompatibleFilesystemChosen[];
ARC_EXPORT extern const char kArcDataRemoveRequested[];
ARC_EXPORT extern const char kArcEnabled[];
ARC_EXPORT extern const char kArcFastAppReinstallPackages[];
ARC_EXPORT extern const char kArcFastAppReinstallStarted[];
ARC_EXPORT extern const char kArcFrameworkVersion[];
ARC_EXPORT extern const char kArcHasAccessToRemovableMedia[];
ARC_EXPORT extern const char kArcInitialSettingsPending[];
ARC_EXPORT extern const char kArcLocationServiceEnabled[];
ARC_EXPORT extern const char kArcPackages[];
ARC_EXPORT extern const char kArcPaiStarted[];
ARC_EXPORT extern const char kArcPolicyComplianceReported[];
ARC_EXPORT extern const char kArcProvisioningInitiatedFromOobe[];
ARC_EXPORT extern const char kArcPushInstallAppsPending[];
ARC_EXPORT extern const char kArcPushInstallAppsRequested[];
ARC_EXPORT extern const char kArcSerialNumber[];
ARC_EXPORT extern const char kArcSetNotificationsEnabledDeferred[];
ARC_EXPORT extern const char kArcSignedIn[];
ARC_EXPORT extern const char kArcSkippedReportingNotice[];
ARC_EXPORT extern const char kArcSupervisionTransition[];
ARC_EXPORT extern const char kArcTermsAccepted[];
ARC_EXPORT extern const char kArcTermsShownInOobe[];
ARC_EXPORT extern const char kArcVisibleExternalStorages[];
ARC_EXPORT extern const char kEcryptfsMigrationStrategy[];
ARC_EXPORT extern const char kEngagementPrefsPrefix[];

// Local state prefs in lexicographical order.
ARC_EXPORT extern const char kStabilityMetrics[];

void RegisterLocalStatePrefs(PrefRegistrySimple* registry);
void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace prefs
}  // namespace arc

#endif  // COMPONENTS_ARC_ARC_PREFS_H_
