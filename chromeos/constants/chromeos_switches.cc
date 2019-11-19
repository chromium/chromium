// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/constants/chromeos_switches.h"

#include <string>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "chromeos/constants/chromeos_features.h"
#include "third_party/icu/source/common/unicode/locid.h"

namespace chromeos {
namespace switches {

namespace {

// The memory pressure thresholds selection which is used to decide whether and
// when a memory pressure event needs to get fired.
const char kMemoryPressureExperimentName[] = "ChromeOSMemoryPressureHandling";
const char kMemoryPressureHandlingOff[] = "memory-pressure-off";

// Controls CrOS GaiaId migration for tests ("" is default).
const char kTestCrosGaiaIdMigration[] = "test-cros-gaia-id-migration";

// Value for kTestCrosGaiaIdMigration indicating that migration is started (i.e.
// all stored user keys will be converted to GaiaId)
const char kTestCrosGaiaIdMigrationStarted[] = "started";

}  // namespace

// Please keep the order of these switches synchronized with the header file
// (i.e. in alphabetical order).

const char kAggressiveCacheDiscardThreshold[] = "aggressive-cache-discard";

const char kAggressiveTabDiscardThreshold[] = "aggressive-tab-discard";

const char kAggressiveThreshold[] = "aggressive";

// If this flag is passed, failed policy fetches will not cause profile
// initialization to fail. This is useful for tests because it means that
// tests don't have to mock out the policy infrastructure.
const char kAllowFailedPolicyFetchForTest[] =
    "allow-failed-policy-fetch-for-test";

// Allows remote attestation (RA) in dev mode for testing purpose. Usually RA
// is disabled in dev mode because it will always fail. However, there are cases
// in testing where we do want to go through the permission flow even in dev
// mode. This can be enabled by this flag.
const char kAllowRAInDevMode[] = "allow-ra-in-dev-mode";

// Specifies whether an app launched in kiosk mode was auto launched with zero
// delay. Used in order to properly restore auto-launched state during session
// restore flow.
const char kAppAutoLaunched[] = "app-auto-launched";

// Path for app's OEM manifest file.
const char kAppOemManifestFile[] = "app-mode-oem-manifest";

// Signals ARC support status on this device. This can take one of the
// following three values.
// - none: ARC is not installed on this device. (default)
// - installed: ARC is installed on this device, but not officially supported.
//   Users can enable ARC only when Finch experiment is turned on.
// - officially-supported: ARC is installed and supported on this device. So
//   users can enable ARC via settings etc.
const char kArcAvailability[] = "arc-availability";

// DEPRECATED: Please use --arc-availability=installed.
// Signals the availability of the ARC instance on this device.
const char kArcAvailable[] = "arc-available";

// A JSON dictionary whose content is the same as cros config's
// /arc/build-properties.
const char kArcBuildProperties[] = "arc-build-properties";

// Flag that forces ARC data be cleaned on each start.
const char kArcDataCleanupOnStart[] = "arc-data-cleanup-on-start";

// Flag that disables ARC app sync flow that installs some apps silently. Used
// in autotests to resolve racy conditions.
const char kArcDisableAppSync[] = "arc-disable-app-sync";

// Used in autotest to disable GMS-core caches which is on by default.
const char kArcDisableGmsCoreCache[] = "arc-disable-gms-core-cache";

// Flag that disables ARC locale sync with Android container. Used in autotest
// to prevent conditions when certain apps, including Play Store may get
// restarted. Restarting Play Store may cause random test failures. Enabling
// this flag would also forces ARC container to use 'en-US' as a locale and
// 'en-US,en' as preferred languages.
const char kArcDisableLocaleSync[] = "arc-disable-locale-sync";

// Flag that disables ARC Play Auto Install flow that installs set of predefined
// apps silently. Used in autotests to resolve racy conditions.
const char kArcDisablePlayAutoInstall[] = "arc-disable-play-auto-install";

// Flag that forces ARC to cache icons for apps.
const char kArcForceCacheAppIcons[] = "arc-force-cache-app-icons";

// Flag that forces the OptIn ui to be shown. Used in tests.
const char kArcForceShowOptInUi[] = "arc-force-show-optin-ui";

// Used in autotest to specifies how to handle packages cache. Can be
// copy - copy resulting packages.xml to the temporary directory.
// skip-copy - skip initial packages cache setup and copy resulting packages.xml
//             to the temporary directory.
const char kArcPackagesCacheMode[] = "arc-packages-cache-mode";

// Used in autotest to forces Play Store auto-update state. Can be
// on - auto-update is forced on.
// off - auto-update is forced off.
const char kArcPlayStoreAutoUpdate[] = "arc-play-store-auto-update";

// Defines how to start ARC. This can take one of the following values:
// - always-start automatically start with Play Store UI support.
// - always-start-with-no-play-store automatically start without Play Store UI.
// If it is not set, then ARC is started in default mode.
const char kArcStartMode[] = "arc-start-mode";

// If this flag is present then the device had ARC M available and gets ARC N
// when updating.
// TODO(pmarko): Remove this when we assess that it's not necessary anymore:
// crbug.com/761348.
const char kArcTransitionMigrationRequired[] =
    "arc-transition-migration-required";

// If this flag is set, it indicates that this device is a "Cellular First"
// device. Cellular First devices use cellular telephone data networks as
// their primary means of connecting to the internet.
// Setting this flag has two consequences:
// 1. Cellular data roaming will be enabled by default.
// 2. UpdateEngine will be instructed to allow auto-updating over cellular
//    data connections.
const char kCellularFirst[] = "cellular-first";

// Default large wallpaper to use for kids accounts (as path to trusted,
// non-user-writable JPEG file).
const char kChildWallpaperLarge[] = "child-wallpaper-large";

// Default small wallpaper to use for kids accounts (as path to trusted,
// non-user-writable JPEG file).
const char kChildWallpaperSmall[] = "child-wallpaper-small";

const char kConservativeThreshold[] = "conservative";

// Forces CrOS region value.
const char kCrosRegion[] = "cros-region";

// Control regions data load ("" is default).
const char kCrosRegionsMode[] = "cros-regions-mode";

// "Hide" value for kCrosRegionsMode (VPD values are hidden).
const char kCrosRegionsModeHide[] = "hide";

// "Override" value for kCrosRegionsMode (region's data is read first).
const char kCrosRegionsModeOverride[] = "override";

// Indicates that the wallpaper images specified by
// kAshDefaultWallpaper{Large,Small} are OEM-specific (i.e. they are not
// downloadable from Google).
const char kDefaultWallpaperIsOem[] = "default-wallpaper-is-oem";

// Default large wallpaper to use (as path to trusted, non-user-writable JPEG
// file).
const char kDefaultWallpaperLarge[] = "default-wallpaper-large";

// Default small wallpaper to use (as path to trusted, non-user-writable JPEG
// file).
const char kDefaultWallpaperSmall[] = "default-wallpaper-small";

// Time in seconds before a machine at OOBE is considered derelict.
const char kDerelictDetectionTimeout[] = "derelict-detection-timeout";

// Time in seconds before a derelict machines starts demo mode.
const char kDerelictIdleTimeout[] = "derelict-idle-timeout";

// Disables android user data wipe on opt out.
const char kDisableArcDataWipe[] = "disable-arc-data-wipe";

// Disables ARC Opt-in verification process and ARC is enabled by default.
const char kDisableArcOptInVerification[] = "disable-arc-opt-in-verification";

// Disables the Chrome OS demo.
const char kDisableDemoMode[] = "disable-demo-mode";

// If this switch is set, the device cannot be remotely disabled by its owner.
const char kDisableDeviceDisabling[] = "disable-device-disabling";

// Disable encryption migration for user's cryptohome to run latest Arc.
const char kDisableEncryptionMigration[] = "disable-encryption-migration";

// Disables fine grained time zone detection.
const char kDisableFineGrainedTimeZoneDetection[] =
    "disable-fine-grained-time-zone-detection";

// Disables GAIA services such as enrollment and OAuth session restore. Used by
// 'fake' telemetry login.
const char kDisableGaiaServices[] = "disable-gaia-services";

// Disables HID-detection OOBE screen.
const char kDisableHIDDetectionOnOOBE[] = "disable-hid-detection-on-oobe";

// Avoid doing expensive animations upon login.
const char kDisableLoginAnimations[] = "disable-login-animations";

// Disables requests for an enterprise machine certificate during attestation.
const char kDisableMachineCertRequest[] = "disable-machine-cert-request";

// Disables the multiple display layout UI.
const char kDisableMultiDisplayLayout[] = "disable-multi-display-layout";

// Disables per-user timezone.
const char kDisablePerUserTimezone[] = "disable-per-user-timezone";

// Disables rollback option on reset screen.
const char kDisableRollbackOption[] = "disable-rollback-option";

// Disables client certificate authentication on the sign-in frame on the Chrome
// OS sign-in profile.
// TODO(pmarko): Remove this flag in M-66 if no issues are found
// (https://crbug.com/723849).
const char kDisableSigninFrameClientCerts[] =
    "disable-signin-frame-client-certs";

// Disables user selection of client certificate on the sign-in frame on the
// Chrome OS sign-in profile.
// TODO(pmarko): Remove this flag in M-65 when the
// DeviceLoginScreenAutoSelectCertificateForUrls policy is enabled on the server
// side (https://crbug.com/723849) and completely disable user selection of
// certificates on the sign-in frame.
const char kDisableSigninFrameClientCertUserSelection[] =
    "disable-signin-frame-client-cert-user-selection";

// Disables volume adjust sound.
const char kDisableVolumeAdjustSound[] = "disable-volume-adjust-sound";

// Disables wake on wifi features.
const char kDisableWakeOnWifi[] = "disable-wake-on-wifi";

// DEPRECATED. Please use --arc-availability=officially-supported.
// Enables starting the ARC instance upon session start.
const char kEnableArc[] = "enable-arc";

// Enables "hide Skip button" for ARC setup in the OOBE flow.
const char kEnableArcOobeOptinNoSkip[] = "enable-arc-oobe-optin-no-skip";

// Enables ARC VM.
const char kEnableArcVm[] = "enable-arcvm";

// Enables the Cast Receiver.
const char kEnableCastReceiver[] = "enable-cast-receiver";

// Enables consumer kiosk mode for Chrome OS.
const char kEnableConsumerKiosk[] = "enable-consumer-kiosk";

// Enables encryption migration for user's cryptohome to run latest Arc.
const char kEnableEncryptionMigration[] = "enable-encryption-migration";

// Enables sharing assets for installed default apps.
const char kEnableExtensionAssetsSharing[] = "enable-extension-assets-sharing";

// Enables animated transitions during first-run tutorial.
// TODO(https://crbug.com/945966): Remove this.
const char kEnableFirstRunUITransitions[] = "enable-first-run-ui-transitions";

// Enables the marketing opt-in screen in OOBE.
const char kEnableMarketingOptInScreen[] = "enable-market-opt-in";

// Enables request of tablet site (via user agent override).
const char kEnableRequestTabletSite[] = "enable-request-tablet-site";

// Enables tablet form factor.
const char kEnableTabletFormFactor[] = "enable-tablet-form-factor";

// Enables the touch calibration option in MD settings UI for valid touch
// displays.
const char kEnableTouchCalibrationSetting[] =
    "enable-touch-calibration-setting";

// Enables touchpad three-finger-click as middle button.
const char kEnableTouchpadThreeFingerClick[] =
    "enable-touchpad-three-finger-click";

// Disables ARC for managed accounts.
const char kEnterpriseDisableArc[] = "enterprise-disable-arc";

// Disable license type selection by user during enrollment.
const char kEnterpriseDisableLicenseTypeSelection[] =
    "enterprise-disable-license-type-selection";

// Whether to enable forced enterprise re-enrollment.
const char kEnterpriseEnableForcedReEnrollment[] =
    "enterprise-enable-forced-re-enrollment";

// Whether to enable initial enterprise enrollment.
const char kEnterpriseEnableInitialEnrollment[] =
    "enterprise-enable-initial-enrollment";

// Enables the zero-touch enterprise enrollment flow.
const char kEnterpriseEnableZeroTouchEnrollment[] =
    "enterprise-enable-zero-touch-enrollment";

// Power of the power-of-2 initial modulus that will be used by the
// auto-enrollment client. E.g. "4" means the modulus will be 2^4 = 16.
const char kEnterpriseEnrollmentInitialModulus[] =
    "enterprise-enrollment-initial-modulus";

// Power of the power-of-2 maximum modulus that will be used by the
// auto-enrollment client.
const char kEnterpriseEnrollmentModulusLimit[] =
    "enterprise-enrollment-modulus-limit";

// Interval in seconds between Chrome reading external metrics from
// /var/lib/metrics/uma-events.
const char kExternalMetricsCollectionInterval[] =
    "external-metrics-collection-interval";

// An absolute path to the chroot hosting the DriveFS to use. This is only used
// when running on Linux, i.e. when IsRunningOnChromeOS() returns false.
const char kFakeDriveFsLauncherChrootPath[] =
    "fake-drivefs-launcher-chroot-path";

// A relative path to socket to communicat with the fake DriveFS launcher within
// the chroot specified by kFakeDriveFsLauncherChrootPath. This is only used
// when running on Linux, i.e. when IsRunningOnChromeOS() returns false.
const char kFakeDriveFsLauncherSocketPath[] =
    "fake-drivefs-launcher-socket-path";

// Fingerprint sensor location indicates the physical sensor's location. The
// value is a string with possible values: "power-button-top-left",
// "keyboard-top-right", "keyboard-bottom-right".
const char kFingerprintSensorLocation[] = "fingerprint-sensor-location";

// Forces Chrome to use CertVerifyProcBuiltin for verification of server
// certificates, ignoring the status of
// net::features::kCertVerifierBuiltinFeature.
const char kForceCertVerifierBuiltin[] = "force-cert-verifier-builtin";

// Passed to Chrome the first time that it's run after the system boots.
// Not passed on restart after sign out.
const char kFirstExecAfterBoot[] = "first-exec-after-boot";

// Forces developer tools availability, no matter what values the enterprise
// policies DeveloperToolsDisabled and DeveloperToolsAvailability are set to.
const char kForceDevToolsAvailable[] = "force-devtools-available";

// Forces first-run UI to be shown for every login.
const char kForceFirstRunUI[] = "force-first-run-ui";

// Force enables the Happiness Tracking System for the device. This ignores
// user profile check and time limits and shows the notification every time
// for any type of user. Should be used only for testing.
const char kForceHappinessTrackingSystem[] = "force-happiness-tracking-system";

// Usually in browser tests the usual login manager bringup is skipped so that
// tests can change how it's brought up. This flag disables that.
const char kForceLoginManagerInTests[] = "force-login-manager-in-tests";

// Force system compositor mode when set.
const char kForceSystemCompositorMode[] = "force-system-compositor-mode";

// Indicates that the browser is in "browse without sign-in" (Guest session)
// mode. Should completely disable extensions, sync and bookmarks.
const char kGuestSession[] = "bwsi";

// Large wallpaper to use in guest mode (as path to trusted, non-user-writable
// JPEG file).
const char kGuestWallpaperLarge[] = "guest-wallpaper-large";

// Small wallpaper to use in guest mode (as path to trusted, non-user-writable
// JPEG file).
const char kGuestWallpaperSmall[] = "guest-wallpaper-small";

// If set, the system is a Chromebook with a "standard Chrome OS keyboard",
// which generally means one with a Search key in the standard Caps Lock
// location above the Left Shift key. It should be unset for Chromebooks with
// both Search and Caps Lock keys (e.g. stout) and for devices like Chromeboxes
// that only use external keyboards.
const char kHasChromeOSKeyboard[] = "has-chromeos-keyboard";

// Defines user homedir. This defaults to primary user homedir.
const char kHomedir[] = "homedir";

// If true, profile selection in UserManager will always return active user's
// profile.
// TODO(nkostlyev): http://crbug.com/364604 - Get rid of this switch after we
// turn on multi-profile feature on ChromeOS.
const char kIgnoreUserProfileMappingForTests[] =
    "ignore-user-profile-mapping-for-tests";

// If set, the Chrome settings will not expose the option to enable crostini
// unless the enable-experimental-kernel-vm-support flag is set in
// chrome://flags
const char kKernelnextRestrictVMs[] = "kernelnext-restrict-vms";

// Enables Chrome-as-a-login-manager behavior.
const char kLoginManager[] = "login-manager";

// Specifies the profile to use once a chromeos user is logged in.
// This parameter is ignored if user goes through login screen since user_id
// hash defines which profile directory to use.
// In case of browser restart within active session this parameter is used
// to pass user_id hash for primary user.
const char kLoginProfile[] = "login-profile";

// Specifies the user which is already logged in.
const char kLoginUser[] = "login-user";

// Enables natural scroll by default.
const char kNaturalScrollDefault[] = "enable-natural-scroll-default";

// If present, the device needs to check the policy to see if the migration to
// ext4 for ARC is allowed. It should be present only on devices that have been
// initially issued with ecrypfs encryption and have ARC (N+) available. For the
// devices in other categories this flag must be missing.
const char kNeedArcMigrationPolicyCheck[] = "need-arc-migration-policy-check";

// An optional comma-separated list of IDs of apps that can be used to take
// notes. If unset, a hardcoded list is used instead.
const char kNoteTakingAppIds[] = "note-taking-app-ids";

// Forces OOBE/login to force show a comma-separated list of screens from
// chromeos::kScreenNames in oobe_screen.cc. Supported screens are:
//   user-image
const char kOobeForceShowScreen[] = "oobe-force-show-screen";

// Indicates that a guest session has been started before OOBE completion.
const char kOobeGuestSession[] = "oobe-guest-session";

// Skips all other OOBE pages after user login.
const char kOobeSkipPostLogin[] = "oobe-skip-postlogin";

// Skip to login screen.
const char kOobeSkipToLogin[] = "oobe-skip-to-login";

// Interval at which we check for total time on OOBE.
const char kOobeTimerInterval[] = "oobe-timer-interval";

// SAML assertion consumer URL, used to detect when Gaia-less SAML flows end
// (e.g. for SAML managed guest sessions)
// TODO(984021): Remove when URL is sent by DMServer.
const char kPublicAccountsSamlAclUrl[] = "public-accounts-saml-acl-url";

// If set to "true", the profile requires policy during restart (policy load
// must succeed, otherwise session restart should fail).
const char kProfileRequiresPolicy[] = "profile-requires-policy";

// Redirects libassistant logging to /var/log/chrome/.
const char kRedirectLibassistantLogging[] = "redirect-libassistant-logging";

// The rlz ping delay (in seconds) that overwrites the default value.
const char kRlzPingDelay[] = "rlz-ping-delay";

// Password change url for SAML users.
// TODO(941489): Remove when the bug is fixed.
const char kSamlPasswordChangeUrl[] = "saml-password-change-url";

// New modular design for the shelf with apps separated into a hotseat UI and
// smaller shelf in clamshell mode.
const char kShelfHotseat[] = "shelf-hotseat";

// App window previews when hovering over the shelf.
const char kShelfHoverPreviews[] = "shelf-hover-previews";

// If true, files in Android internal storage will be shown in Files app.
const char kShowAndroidFilesInFilesApp[] = "show-android-files-in-files-app";

// If true, files in Android internal storage will be hidden in Files app.
const char kHideAndroidFilesInFilesApp[] = "hide-android-files-in-files-app";

// The name of the per-model directory which contains per-region
// subdirectories with regulatory label files for this model.
// The per-model directories (if there are any) are located under
// "/usr/share/chromeos-assets/regulatory_labels/".
const char kRegulatoryLabelDir[] = "regulatory-label-dir";

// If true, the developer tool overlay will be shown for the login/lock screen.
// This makes it easier to test layout logic.
const char kShowLoginDevOverlay[] = "show-login-dev-overlay";

// Enables testing for encryption migration UI.
const char kTestEncryptionMigrationUI[] = "test-encryption-migration-ui";

// Enables the wallpaper picker to fetch images from the test server.
const char kTestWallpaperServer[] = "test-wallpaper-server";

// Overrides Tether with stub service. Provide integer arguments for the number
// of fake networks desired, e.g. 'tether-stub=2'.
const char kTetherStub[] = "tether-stub";

// Tells the Chromebook to scan for a tethering host even if there is already a
// wired connection. This allows end-to-end tests to be deployed over ethernet
// without that connection preventing scans and thereby blocking the testing of
// cases with no preexisting connection. Should be used only for testing.
const char kTetherHostScansIgnoreWiredConnections[] =
    "tether-host-scans-ignore-wired-connections";

// Shows all Bluetooth devices in UI (System Tray/Settings Page.)
const char kUnfilteredBluetoothDevices[] = "unfiltered-bluetooth-devices";

// Used to tell the policy infrastructure to not let profile initialization
// complete until policy is manually set by a test. This is used to provide
// backward compatibility with a few tests that incorrectly use the
// synchronously-initialized login profile to run their tests - do not add new
// uses of this flag.
const char kWaitForInitialPolicyFetchForTest[] =
    "wait-for-initial-policy-fetch-for-test";

// Enables wake on wifi packet feature, which wakes the device on the receipt
// of network packets from whitelisted sources.
const char kWakeOnWifiPacket[] = "wake-on-wifi-packet";

// Prevents any CPU restrictions being set on the ARC container. Only meant to
// be used by tests as some tests may time out if the ARC container is
// throttled.
const char kDisableArcCpuRestriction[] = "disable-arc-cpu-restriction";

bool WakeOnWifiEnabled() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(kDisableWakeOnWifi);
}

bool MemoryPressureHandlingEnabled() {
  if (base::FieldTrialList::FindFullName(kMemoryPressureExperimentName) ==
      kMemoryPressureHandlingOff) {
    return false;
  }
  return true;
}

bool IsGaiaIdMigrationStarted() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(kTestCrosGaiaIdMigration))
    return false;

  return command_line->GetSwitchValueASCII(kTestCrosGaiaIdMigration) ==
         kTestCrosGaiaIdMigrationStarted;
}

bool IsCellularFirstDevice() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kCellularFirst);
}

bool IsSigninFrameClientCertsEnabled() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      kDisableSigninFrameClientCerts);
}

bool IsSigninFrameClientCertUserSelectionEnabled() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      kDisableSigninFrameClientCertUserSelection);
}

bool ShouldShowShelfHotseat() {
  return base::FeatureList::IsEnabled(features::kShelfHotseat) ||
         base::CommandLine::ForCurrentProcess()->HasSwitch(kShelfHotseat);
}

bool ShouldShowShelfHoverPreviews() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kShelfHoverPreviews);
}

bool ShouldShowScrollableShelf() {
  // If we're showing the new shelf design, also enable scrollable shelf.
  if (ShouldShowShelfHotseat())
    return true;

  return base::FeatureList::IsEnabled(features::kShelfScrollable);
}

bool ShouldTetherHostScansIgnoreWiredConnections() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kTetherHostScansIgnoreWiredConnections);
}

bool ShouldSkipOobePostLogin() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kOobeSkipPostLogin);
}

bool IsTabletFormFactor() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kEnableTabletFormFactor);
}

bool IsGaiaServicesDisabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kDisableGaiaServices);
}

bool IsArcCpuRestrictionDisabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kDisableArcCpuRestriction);
}

bool IsUnfilteredBluetoothDevicesEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kUnfilteredBluetoothDevices);
}

}  // namespace switches
}  // namespace chromeos
