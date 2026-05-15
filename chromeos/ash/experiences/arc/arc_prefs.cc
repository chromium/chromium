// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/arc/arc_prefs.h"

#include <string>

#include "chromeos/ash/experiences/arc/session/arc_management_transition.h"
#include "chromeos/ash/experiences/arc/session/arc_vm_data_migration_status.h"
#include "components/guest_os/guest_os_prefs.h"
#include "components/metrics/daily_event.h"
#include "components/prefs/pref_registry_simple.h"

namespace arc {
namespace prefs {

namespace {

void RegisterDailyMetricsPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kArcDailyMetricsKills);
  metrics::DailyEvent::RegisterPref(registry, prefs::kArcDailyMetricsSample);
}

}  // anonymous namespace

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  // Sorted in lexicographical order.
  RegisterDailyMetricsPrefs(registry);
  registry->RegisterStringPref(kArcSerialNumberSalt, std::string());
  registry->RegisterDictionaryPref(kArcSnapshotInfo);
  registry->RegisterTimePref(kArcVmmSwapOutTime, base::Time());
  registry->RegisterDictionaryPref(kStabilityMetrics);

  registry->RegisterIntegerPref(kAnrPendingCount, 0);
  registry->RegisterTimeDeltaPref(kAnrPendingDuration, base::TimeDelta());
  registry->RegisterBooleanPref(kWebViewProcessStarted, false);
}

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  // TODO(dspaid): Implement a mechanism to allow this to sync on first boot
  // only.

  // This is used to delete the Play user ID if ARC is disabled for an
  // Active Directory managed device.
  registry->RegisterStringPref(kArcActiveDirectoryPlayUserId, std::string());

  // Note that ArcBackupRestoreEnabled and ArcLocationServiceEnabled prefs have
  // to be off by default, until an explicit gesture from the user to enable
  // them is received. This is crucial in the cases when these prefs transition
  // from a previous managed state to the unmanaged.
  registry->RegisterBooleanPref(kArcBackupRestoreEnabled, false);
  registry->RegisterBooleanPref(kArcLocationServiceEnabled, false);

  registry->RegisterIntegerPref(
      kArcManagementTransition,
      static_cast<int>(ArcManagementTransition::NO_TRANSITION));

  registry->RegisterBooleanPref(kArcIsManaged, false);

  guest_os::prefs::RegisterEngagementProfilePrefs(registry,
                                                  kEngagementPrefsPrefix);

  // Sorted in lexicographical order.
  registry->RegisterBooleanPref(kAlwaysOnVpnLockdown, false);
  registry->RegisterStringPref(kAlwaysOnVpnPackage, std::string());
  registry->RegisterBooleanPref(kArcDataRemoveRequested, false);
  registry->RegisterBooleanPref(kArcEnabled, false);
  registry->RegisterBooleanPref(kArcHasAccessToRemovableMedia, false);
  registry->RegisterBooleanPref(kArcInitialSettingsPending, false);
  registry->RegisterBooleanPref(kArcInitialLocationSettingSyncRequired, true);
  registry->RegisterStringPref(kArcLastSetAppLocale, std::string());
  registry->RegisterBooleanPref(kArcOpenLinksInBrowserByDefault, false);
  registry->RegisterBooleanPref(kArcPaiStarted, false);
  registry->RegisterBooleanPref(kArcFastAppReinstallStarted, false);
  registry->RegisterListPref(kArcFastAppReinstallPackages);
  registry->RegisterListPref(
      kArcFirstActivationDuringUserSessionStartUpHistory);
  registry->RegisterBooleanPref(kArcPolicyComplianceReported, false);
  registry->RegisterBooleanPref(kArcProvisioningInitiatedFromOobe, false);
  registry->RegisterBooleanPref(kArcSignedIn, false);
  registry->RegisterBooleanPref(kArcSkippedReportingNotice, false);
  registry->RegisterBooleanPref(kArcTermsAccepted, false);
  registry->RegisterListPref(kArcVisibleExternalStorages);
  registry->RegisterIntegerPref(kArcVmDataMigrationAutoResumeCount, 0);
  registry->RegisterTimePref(kArcVmDataMigrationNotificationFirstShownTime,
                             base::Time());
  registry->RegisterIntegerPref(
      kArcVmDataMigrationStatus,
      static_cast<int>(ArcVmDataMigrationStatus::kUnnotified));
  registry->RegisterIntegerPref(
      kArcVmDataMigrationStrategy,
      static_cast<int>(ArcVmDataMigrationStrategy::kDoNotPrompt));
  registry->RegisterBooleanPref(kUnaffiliatedDeviceArcAllowed, true);
}

}  // namespace prefs
}  // namespace arc
