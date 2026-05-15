// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_ARC_ARC_PREFS_H_
#define CHROMEOS_ASH_EXPERIENCES_ARC_ARC_PREFS_H_

class PrefRegistrySimple;

namespace arc::prefs {

// ------------------------------------------------------------------------
// Profile prefs in lexicographical order. See below for local state prefs.
// ------------------------------------------------------------------------

// A bool preference indicating whether traffic other than the VPN connection
// set via kAlwaysOnVpnPackage should be blackholed.
inline constexpr char kAlwaysOnVpnLockdown[] = "arc.vpn.always_on.lockdown";

// A string preference indicating the Android app that will be used for
// "Always On VPN". Should be empty if "Always On VPN" is not enabled.
inline constexpr char kAlwaysOnVpnPackage[] = "arc.vpn.always_on.vpn_package";

// Stores the user id received from DM Server when enrolling a Play user on an
// Active Directory managed device. Used to report to DM Server that the account
// is still used.
inline constexpr char kArcActiveDirectoryPlayUserId[] =
    "arc.active_directory_play_user_id";

// Stores whether ARC app is requested in the session. Used for UMA.
// -1 indicates no data. 0 or greaters are the number of app launch requests.
inline constexpr char kArcAppRequestedInSession[] =
    "arc.app_requested_in_session";

// A preference to keep list of Android apps and their state.
inline constexpr char kArcApps[] = "arc.apps";

// A preference to store backup and restore state for Android apps.
inline constexpr char kArcBackupRestoreEnabled[] = "arc.backup_restore.enabled";

// A preference that indicates an ARC compatible filesystem was chosen for
// the user directory (i.e., the user finished required migration.)
inline constexpr char kArcCompatibleFilesystemChosen[] =
    "arc.compatible_filesystem.chosen";

// Cumulative daily counts of app kills by priority and with other VM context.
inline constexpr char kArcDailyMetricsKills[] = "arc.dialy_metrics_kills";

//  Timestamp of the last time daily metrics have been reported.
inline constexpr char kArcDailyMetricsSample[] = "arc.daily_metrics_sample";

// A preference to indicate that Android's data directory should be removed.
inline constexpr char kArcDataRemoveRequested[] = "arc.data.remove_requested";

// A preference representing whether a user has opted in to use Google Play
// Store on ARC.
// TODO(hidehiko): For historical reason, now the preference name does not
// directly reflect "Google Play Store". We should get and set the values via
// utility methods (IsArcPlayStoreEnabledForProfile() and
// SetArcPlayStoreEnabledForProfile()) in chrome/browser/ash/arc/arc_util.h.
inline constexpr char kArcEnabled[] = "arc.enabled";

// A preference to keep list of Play Fast App Reinstall packages.
inline constexpr char kArcFastAppReinstallPackages[] =
    "arc.fast.app.reinstall.packages";

// A preference that indicates that Play Fast App Reinstall flow was already
// started.
inline constexpr char kArcFastAppReinstallStarted[] =
    "arc.fast.app.reinstall.started";

// Stores the history of whether the first ARC activation during user session
// start up. A list of booleans; true if the first activation is done during
// the user session start up.
inline constexpr char kArcFirstActivationDuringUserSessionStartUpHistory[] =
    "arc.first_activation_during_user_session_start_up_history";

// A preference to keep the current Android framework version. Note, that value
// is only available after first packages update.
inline constexpr char kArcFrameworkVersion[] = "arc.framework.version";

// A preference to control if ARC can access removable media on the host side.
// TODO(fukino): Remove this pref once "Play Store applications can't access
// this device" toast in Files app becomes aware of kArcVisibleExternalStorages.
// crbug.com/998512.
inline constexpr char kArcHasAccessToRemovableMedia[] =
    "arc.has_access_to_removable_media";

// A preference that indicates that initial settings need to be applied. Initial
// settings are applied only once per new OptIn once mojo settings instance is
// ready. Each OptOut resets this preference. Note, its sense is close to
// |kArcSignedIn|, however due the asynchronous nature of initializing mojo
// components, timing of triggering |kArcSignedIn| and
// |kArcInitialSettingsPending| can be different and
// |kArcInitialSettingsPending| may even be handled in the next user session.
inline constexpr char kArcInitialSettingsPending[] =
    "arc.initial.settings.pending";

// Tells us whether the initial location setting sync is required or not. With
// Privacy Hub for ChromeOS this setting is needed to migrate the location
// settings from existing android settings to ChromeOS.
// Default value is true, once done we set it to false as we want to honor the
// ChromeOS settings at boot from now on. Also in case of first time login or
// arc opt-in, we will set this value to false.
inline constexpr char kArcInitialLocationSettingSyncRequired[] =
    "arc.initial.location.setting.sync.required";

// A boolean preference that indicates ARC management state.
inline constexpr char kArcIsManaged[] = "arc.is_managed";

// A preference indicating the last locale set for any apps. This will be used
// as part of suggested locales for other apps' locale setting.
inline constexpr char kArcLastSetAppLocale[] = "arc.last_set_app_locale";

// A preference to keep user's consent to use location service.
inline constexpr char kArcLocationServiceEnabled[] =
    "arc.location_service.enabled";

// A preference that indicates that a management transition is necessary, in
// response to account management state change.
inline constexpr char kArcManagementTransition[] = "arc.management_transition";

// A preference that indicates whether links supported by Android apps should be
// opened in the browser by default.
inline constexpr char kArcOpenLinksInBrowserByDefault[] =
    "arc.open_links_in_browser_by_default";

// A preference to keep list of Android packages and their information.
inline constexpr char kArcPackages[] = "arc.packages";

// A preference that indicates that arc.packages is up to date.
inline constexpr char kArcPackagesIsUpToDate[] = "arc.packages_is_up_to_date";

// A preference that indicates that Play Auto Install flow was already started.
inline constexpr char kArcPaiStarted[] = "arc.pai.started";

// A preference to know whether or not the Arc.PlayStoreLaunchWithinAWeek
// metric can been recorded.
inline constexpr char kArcPlayStoreLaunchMetricCanBeRecorded[] =
    "arc.playstore_launched_by_user";

// A preference that indicated whether Android reported it's compliance status
// with provided policies. This is used only as a signal to start Android kiosk.
inline constexpr char kArcPolicyComplianceReported[] =
    "arc.policy_compliance_reported";

// A preference that indicates that provisioning was initiated from OOBE. This
// is preserved across Chrome restart.
inline constexpr char kArcProvisioningInitiatedFromOobe[] =
    "arc.provisioning.initiated.from.oobe";

// A preference that holds the list of apps that the admin requested to be
// push-installed, but which have not been successfully installed yet.
inline constexpr char kArcPushInstallAppsPending[] = "arc.push_install.pending";

// A preference that holds the list of apps that the admin requested to be
// push-installed.
inline constexpr char kArcPushInstallAppsRequested[] =
    "arc.push_install.requested";

// A preference to keep deferred requests of setting notifications enabled flag.
inline constexpr char kArcSetNotificationsEnabledDeferred[] =
    "arc.set_notifications_enabled_deferred";

// A counter preference that indicates number of ARC resize-lock splash screen.
inline constexpr char kArcShowResizeLockSplashScreenLimits[] =
    "arc.show_resize_lock_splash_screen_limits";

// A preference that indicates status of Android sign-in.
inline constexpr char kArcSignedIn[] = "arc.signedin";

// A preference that indicates that ARC skipped the setup UI flows that
// contain a notice related to reporting of diagnostic information.
inline constexpr char kArcSkippedReportingNotice[] =
    "arc.skipped.reporting.notice";

// A preference that indicates that user accepted PlayStore terms.
inline constexpr char kArcTermsAccepted[] = "arc.terms.accepted";

// A preference to keep list of external storages which are visible to Android
// apps. (i.e. can be read/written by Android apps.)
inline constexpr char kArcVisibleExternalStorages[] =
    "arc.visible_external_storages";

// An integer preference to count how many times ARCVM /data migration has been
// automatically resumed.
inline constexpr char kArcVmDataMigrationAutoResumeCount[] =
    "arc.vm_data_migration_auto_resume_count";

// A time preference to indicate when the ARCVM /data migration notification is
// shown for the first time.
inline constexpr char kArcVmDataMigrationNotificationFirstShownTime[] =
    "arc.vm_data_migration_notification_first_shown_time";

// An integer preference to indicate the status of ARCVM /data migration.
inline constexpr char kArcVmDataMigrationStatus[] =
    "arc.vm_data_migration_status";

// An integer preference to indicate the strategy of ARCVM /data migration for
// enterprise user.
inline constexpr char kArcVmDataMigrationStrategy[] =
    "arc.vm_data_migration_strategy";

// Preferences for storing engagement time data, as per
// GuestOsEngagementMetrics.
inline constexpr char kEngagementPrefsPrefix[] = "arc.metrics";

// A preference representing if ARC is allowed on unaffiliated devices
// of an enterprise account
inline constexpr char kUnaffiliatedDeviceArcAllowed[] =
    "arc.unaffiliated.device.allowed";

// -------------------------------------------
// Local state prefs in lexicographical order.
// -------------------------------------------

// Boolean pref indicating whether the notification informing the user that
// adb sideloading had been disabled by their admin was shown.
inline constexpr char kAdbSideloadingDisallowedNotificationShown[] =
    "adb_sideloading_disallowed_notification_shown";

// Boolean pref indicating whether the notification informing the user about a
// change in adb sideloading policy that will clear all user data was shown.
inline constexpr char kAdbSideloadingPowerwashOnNextRebootNotificationShown[] =
    "adb_sideloading_powerwash_on_next_reboot_notification_shown";

// Int64 pref indicating the time in microseconds since Windows epoch
// (1601-01-01 00:00:00 UTC) when the notification informing the user about a
// change in adb sideloading policy that will clear all user data was shown.
// If the notification was not yet shown the pref holds the value Time::Min().
inline constexpr char kAdbSideloadingPowerwashPlannedNotificationShownTime[] =
    "adb_sideloading_powerwash_planned_notification_shown_time";

// ANR count which is currently pending, not flashed to UMA.
inline constexpr char kAnrPendingCount[] = "arc.anr_pending_count";

// Keeps the duration of the current ANR rate period.
inline constexpr char kAnrPendingDuration[] = "arc.anr_pending_duration";

// A preference to keep the salt for generating ro.serialno and ro.boot.serialno
// Android properties. Used only in ARCVM.
inline constexpr char kArcSerialNumberSalt[] = "arc.serialno_salt";

// A preferece to keep ARC snapshot related info in dictionary.
inline constexpr char kArcSnapshotInfo[] = "arc.snapshot";

// A time pref indicating the time in microseconds when ARCVM success executed
// vmm swap out. If it never swapped out, the pref holds the default value
// base::Time().
inline constexpr char kArcVmmSwapOutTime[] = "arc_vmm_swap_out_time";

// Indicates that the user has requested that ARC APK Sideloading be enabled.
inline constexpr char kEnableAdbSideloadingRequested[] =
    "EnableAdbSideloadingRequested";

// A dictionary preference that keeps track of stability metric values, which is
// maintained by StabilityMetricsManager. Persisting values in local state is
// required to include these metrics in the initial stability log in case of a
// crash.
inline constexpr char kStabilityMetrics[] = "arc.metrics.stability";

// A preference to keep track of whether or not Android WebView was used in the
// current ARC session.
inline constexpr char kWebViewProcessStarted[] = "arc.webview.started";

void RegisterLocalStatePrefs(PrefRegistrySimple* registry);
void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace arc::prefs

#endif  // CHROMEOS_ASH_EXPERIENCES_ARC_ARC_PREFS_H_
