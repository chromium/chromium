// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_prefs.h"
#include "components/arc/arc_supervision_transition.h"

#include <string>

#include "components/prefs/pref_registry_simple.h"

namespace arc {
namespace prefs {

// A bool preference indicating whether traffic other than the VPN connection
// set via kAlwaysOnVpnPackage should be blackholed.
const char kAlwaysOnVpnLockdown[] = "arc.vpn.always_on.lockdown";
// A string preference indicating the Android app that will be used for
// "Always On VPN". Should be empty if "Always On VPN" is not enabled.
const char kAlwaysOnVpnPackage[] = "arc.vpn.always_on.vpn_package";
// Stores the user id received from DM Server when enrolling a Play user on an
// Active Directory managed device. Used to report to DM Server that the account
// is still used.
const char kArcActiveDirectoryPlayUserId[] =
    "arc.active_directory_play_user_id";
// A preference to keep list of Android apps and their state.
const char kArcApps[] = "arc.apps";
// A preference to store backup and restore state for Android apps.
const char kArcBackupRestoreEnabled[] = "arc.backup_restore.enabled";
// A preference to indicate that Android's data directory should be removed.
const char kArcDataRemoveRequested[] = "arc.data.remove_requested";
// A preference representing whether a user has opted in to use Google Play
// Store on ARC.
// TODO(hidehiko): For historical reason, now the preference name does not
// directly reflect "Google Play Store". We should get and set the values via
// utility methods (IsArcPlayStoreEnabledForProfile() and
// SetArcPlayStoreEnabledForProfile()) in chrome/browser/chromeos/arc/arc_util.
const char kArcEnabled[] = "arc.enabled";
// A preference that indicates that initial settings need to be applied. Initial
// settings are applied only once per new OptIn once mojo settings instance is
// ready. Each OptOut resets this preference. Note, its sense is close to
// |kArcSignedIn|, however due the asynchronous nature of initializing mojo
// components, timing of triggering |kArcSignedIn| and
// |kArcInitialSettingsPending| can be different and
// |kArcInitialSettingsPending| may even be handled in the next user session.
const char kArcInitialSettingsPending[] = "arc.initial.settings.pending";
// A preference that indicated whether Android reported it's compliance status
// with provided policies. This is used only as a signal to start Android kiosk.
const char kArcPolicyComplianceReported[] = "arc.policy_compliance_reported";
// A preference that indicates that a supervision transition is necessary, in
// response to a CHILD_ACCOUNT transiting to a REGULAR_ACCOUNT or vice-versa.
const char kArcSupervisionTransition[] = "arc.supervision_transition";
// A preference that indicates that user accepted PlayStore terms.
const char kArcTermsAccepted[] = "arc.terms.accepted";
// A preference that indicates that ToS was shown in OOBE flow.
const char kArcTermsShownInOobe[] = "arc.terms.shown_in_oobe";
// A preference to keep user's consent to use location service.
const char kArcLocationServiceEnabled[] = "arc.location_service.enabled";
// A preference to keep list of Android packages and their infomation.
const char kArcPackages[] = "arc.packages";
// A preference that indicates that Play Auto Install flow was already started.
const char kArcPaiStarted[] = "arc.pai.started";
// A preference that indicates that Play Fast App Reinstall flow was already
// started.
const char kArcFastAppReinstallStarted[] = "arc.fast.app.reinstall.started";
// A preference to keep list of Play Fast App Reinstall packages.
const char kArcFastAppReinstallPackages[] = "arc.fast.app.reinstall.packages";
// A preference that holds the list of apps that the admin requested to be
// push-installed.
const char kArcPushInstallAppsRequested[] = "arc.push_install.requested";
// A preference that holds the list of apps that the admin requested to be
// push-installed, but which have not been successfully installed yet.
const char kArcPushInstallAppsPending[] = "arc.push_install.pending";
// A preference to keep deferred requests of setting notifications enabled flag.
const char kArcSetNotificationsEnabledDeferred[] =
    "arc.set_notifications_enabled_deferred";
// A preference that indicates status of Android sign-in.
const char kArcSignedIn[] = "arc.signedin";
// A preference that indicates that ARC skipped the setup UI flows that
// contain a notice related to reporting of diagnostic information.
const char kArcSkippedReportingNotice[] = "arc.skipped.reporting.notice";
// A preference that indicates an ARC comaptible filesystem was chosen for
// the user directory (i.e., the user finished required migration.)
const char kArcCompatibleFilesystemChosen[] =
    "arc.compatible_filesystem.chosen";
// A preference that indicates that user accepted Voice Interaction Value Prop.
const char kArcVoiceInteractionValuePropAccepted[] =
    "arc.voice_interaction_value_prop.accepted";
// Integer pref indicating the ecryptfs to ext4 migration strategy. One of
// options: forbidden = 0, migrate = 1, wipe = 2 or ask the user = 3.
const char kEcryptfsMigrationStrategy[] = "ecryptfs_migration_strategy";
// A preference that indicates the user has accepted voice interaction activity
// control settings.
const char kVoiceInteractionActivityControlAccepted[] =
    "settings.voice_interaction.activity_control.accepted";
// A preference that indicates the user has allowed voice interaction services
// to access the "context" (text and graphic content that is currently on
// screen).
const char kVoiceInteractionContextEnabled[] =
    "settings.voice_interaction.context.enabled";
// A preference that indicates the user has enabled voice interaction services.
const char kVoiceInteractionEnabled[] = "settings.voice_interaction.enabled";
// A preference that indicates the user has allowed voice interaction services
// to use hotword listening.
const char kVoiceInteractionHotwordEnabled[] =
    "settings.voice_interaction.hotword.enabled";
// A preference that indicates whether microphone should be open when the voice
// interaction launches.
const char kVoiceInteractionLaunchWithMicOpen[] =
    "settings.voice_interaction.launch_with_mic_open";
// A preference that indicates the user has allowed voice interaction services
// to send notification.
const char kVoiceInteractionNotificationEnabled[] =
    "settings.voice_interaction.notification.enabled";

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

  // This is used to decide whether migration from ecryptfs to ext4 is allowed.
  registry->RegisterIntegerPref(prefs::kEcryptfsMigrationStrategy, 0);

  registry->RegisterIntegerPref(
      kArcSupervisionTransition,
      static_cast<int>(ArcSupervisionTransition::NO_TRANSITION));

  // Sorted in lexicographical order.
  registry->RegisterBooleanPref(kVoiceInteractionActivityControlAccepted,
                                false);
  registry->RegisterBooleanPref(kAlwaysOnVpnLockdown, false);
  registry->RegisterStringPref(kAlwaysOnVpnPackage, std::string());
  registry->RegisterBooleanPref(kArcDataRemoveRequested, false);
  registry->RegisterBooleanPref(kArcEnabled, false);
  registry->RegisterBooleanPref(kArcInitialSettingsPending, false);
  registry->RegisterBooleanPref(kArcPaiStarted, false);
  registry->RegisterBooleanPref(kArcFastAppReinstallStarted, false);
  registry->RegisterListPref(kArcFastAppReinstallPackages);
  registry->RegisterBooleanPref(kArcPolicyComplianceReported, false);
  registry->RegisterBooleanPref(kArcSignedIn, false);
  registry->RegisterBooleanPref(kArcSkippedReportingNotice, false);
  registry->RegisterBooleanPref(kArcTermsAccepted, false);
  registry->RegisterBooleanPref(kArcTermsShownInOobe, false);
  registry->RegisterBooleanPref(kArcVoiceInteractionValuePropAccepted, false);
  registry->RegisterBooleanPref(kVoiceInteractionContextEnabled, false);
  registry->RegisterBooleanPref(kVoiceInteractionEnabled, false);
  registry->RegisterBooleanPref(kVoiceInteractionHotwordEnabled, false);
  registry->RegisterBooleanPref(kVoiceInteractionNotificationEnabled, true);
  registry->RegisterBooleanPref(kVoiceInteractionLaunchWithMicOpen, false);
}

}  // namespace prefs
}  // namespace arc
