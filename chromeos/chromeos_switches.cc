// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/chromeos_switches.h"

#include <string>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
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

// Controls whether to enable assistant for locale.
const base::Feature kAssistantFeatureForLocale{
    "ChromeOSAssistantForLocale", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable voice interaction feature.
const base::Feature kVoiceInteractionFeature{"ChromeOSVoiceInteraction",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether Instant Tethering supports hosts which use the background
// advertisement model.
const base::Feature kInstantTetheringBackgroundAdvertisementSupport{
    "InstantTetheringBackgroundAdvertisementSupport",
    base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace

// Controls whether to enable Chrome OS Account Manager.
const base::Feature kAccountManager{"ChromeOSAccountManager",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable Google Assistant feature.
const base::Feature kAssistantFeature{"ChromeOSAssistant",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

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

// Flag that forces ARC data be cleaned on each start.
const char kArcDataCleanupOnStart[] = "arc-data-cleanup-on-start";

// Flag that forces the OptIn ui to be shown. Used in tests.
const char kArcForceShowOptInUi[] = "arc-force-show-optin-ui";

// Used in autotest to specifies how to handle packages cache. Can be
// copy - copy resulting packages.xml to the temporary directory.
// skip-copy - skip initial packages cache setup and copy resulting packages.xml
//             to the temporary directory.
const char kArcPackagesCacheMode[] = "arc-packages-cache-mode";

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

// Screenshot testing: specifies the directoru where artifacts will be stored.
const char kArtifactsDir[] = "artifacts-dir";

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

// Forces use of Chrome OS Gaia API v1.
const char kCrosGaiaApiV1[] = "cros-gaia-api-v1";

// Forces CrOS region value.
const char kCrosRegion[] = "cros-region";

// Control regions data load ("" is default).
const char kCrosRegionsMode[] = "cros-regions-mode";

// "Hide" value for kCrosRegionsMode (VPD values are hidden).
const char kCrosRegionsModeHide[] = "hide";

// "Override" value for kCrosRegionsMode (region's data is read first).
const char kCrosRegionsModeOverride[] = "override";

// Enable Crostini file sharing in Files app.
const char kCrostiniFiles[] = "crostini-files";

// Optional value for Data Saver prompt on cellular networks.
const char kDataSaverPromptDemoMode[] = "demo";

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

// Disables bypass proxy for captive portal authorization.
const char kDisableCaptivePortalBypassProxy[] =
    "disable-captive-portal-bypass-proxy";

// Disables cloud backup feature.
const char kDisableCloudImport[] = "disable-cloud-import";

// Disables Data Saver prompt on cellular networks.
const char kDisableDataSaverPrompt[] = "disable-datasaver-prompt";

// Disables the Chrome OS demo.
const char kDisableDemoMode[] = "disable-demo-mode";

// If this switch is set, the device cannot be remotely disabled by its owner.
const char kDisableDeviceDisabling[] = "disable-device-disabling";

// Disable encryption migration for user's cryptohome to run latest Arc.
const char kDisableEncryptionMigration[] = "disable-encryption-migration";

// Disables notification when device is in end of life status.
const char kDisableEolNotification[] = "disable-eol-notification";

// Touchscreen-specific interactions of the Files app.
const char kDisableFileManagerTouchMode[] = "disable-file-manager-touch-mode";

// Disables fine grained time zone detection.
const char kDisableFineGrainedTimeZoneDetection[] =
    "disable-fine-grained-time-zone-detection";

// Disables GAIA services such as enrollment and OAuth session restore. Used by
// 'fake' telemetry login.
const char kDisableGaiaServices[] = "disable-gaia-services";

// Disables HID-detection OOBE screen.
const char kDisableHIDDetectionOnOOBE[] = "disable-hid-detection-on-oobe";

// Enables action handler apps (e.g. creating new notes) on lock screen.
const char kDisableLockScreenApps[] = "disable-lock-screen-apps";

// Avoid doing expensive animations upon login.
const char kDisableLoginAnimations[] = "disable-login-animations";

// Disables requests for an enterprise machine certificate during attestation.
const char kDisableMachineCertRequest[] = "disable-machine-cert-request";

// Disables mtp write support.
const char kDisableMtpWriteSupport[] = "disable-mtp-write-support";

// Disables the multiple display layout UI.
const char kDisableMultiDisplayLayout[] = "disable-multi-display-layout";

// Disables notifications about captive portals in session.
const char kDisableNetworkPortalNotification[] =
    "disable-network-portal-notification";

// Disables the new Korean IME in chrome://settings/languages.
const char kDisableNewKoreanIme[] = "disable-new-korean-ime";

// Disables the new File System Provider API based ZIP unpacker.
const char kDisableNewZIPUnpacker[] = "disable-new-zip-unpacker";

// Disables Office Editing for Docs, Sheets & Slides component app so handlers
// won't be registered, making it possible to install another version for
// testing.
const char kDisableOfficeEditingComponentApp[] =
    "disable-office-editing-component-extension";

// Disables per-user timezone.
const char kDisablePerUserTimezone[] = "disable-per-user-timezone";

// Disables suggestions while typing on a physical keyboard.
const char kDisablePhysicalKeyboardAutocorrect[] =
    "disable-physical-keyboard-autocorrect";

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

// Disables SystemTimezoneAutomaticDetection policy.
const char kDisableSystemTimezoneAutomaticDetectionPolicy[] =
    "disable-system-timezone-automatic-detection";

// Disables volume adjust sound.
const char kDisableVolumeAdjustSound[] = "disable-volume-adjust-sound";

// Disables wake on wifi features.
const char kDisableWakeOnWifi[] = "disable-wake-on-wifi";

// DEPRECATED. Please use --arc-availability=officially-supported.
// Enables starting the ARC instance upon session start.
const char kEnableArc[] = "enable-arc";

// Enables "hide Skip button" for ARC setup in the OOBE flow.
const char kEnableArcOobeOptinNoSkip[] = "enable-arc-oobe-optin-no-skip";

// Enables using a random url for captive portal detection.
const char kEnableCaptivePortalRandomUrl[] = "enable-captive-portal-random-url";

// Enables the Cast Receiver.
const char kEnableCastReceiver[] = "enable-cast-receiver";

// Enables the experimental chromevox developer option.
const char kEnableChromevoxDeveloperOption[] =
    "enable-chromevox-developer-option";

// Enables consumer kiosk mode for Chrome OS.
const char kEnableConsumerKiosk[] = "enable-consumer-kiosk";

// Enables Data Saver prompt on cellular networks.
const char kEnableDataSaverPrompt[] = "enable-datasaver-prompt";

// Enables encryption migration for user's cryptohome to run latest Arc.
const char kEnableEncryptionMigration[] = "enable-encryption-migration";

// Shows additional checkboxes in Settings to enable Chrome OS accessibility
// features that haven't launched yet.
const char kEnableExperimentalAccessibilityFeatures[] =
    "enable-experimental-accessibility-features";

// Enables sharing assets for installed default apps.
const char kEnableExtensionAssetsSharing[] = "enable-extension-assets-sharing";

// Touchscreen-specific interactions of the Files app.
const char kEnableFileManagerTouchMode[] = "enable-file-manager-touch-mode";

// Enabled Discover app.
const char kEnableDiscoverApp[] = "enable-discover-app";

// Enables animated transitions during first-run tutorial.
const char kEnableFirstRunUITransitions[] = "enable-first-run-ui-transitions";

// Enables the marketing opt-in screen in OOBE.
const char kEnableMarketingOptInScreen[] = "enable-market-opt-in";

// Enables notifications about captive portals in session.
const char kEnableNetworkPortalNotification[] =
    "enable-network-portal-notification";

// Enables offline demo mode. Demo mode still requires ARC++.
const char kEnableOfflineDemoMode[] = "enable-offline-demo-mode";

// Enables suggestions while typing on a physical keyboard.
const char kEnablePhysicalKeyboardAutocorrect[] =
    "enable-physical-keyboard-autocorrect";

// Enables request of tablet site (via user agent override).
const char kEnableRequestTabletSite[] = "enable-request-tablet-site";

// Enables using screenshots in tests and seets mode.
const char kEnableScreenshotTestingWithMode[] =
    "enable-screenshot-testing-with-mode";

// Enables the touch calibration option in MD settings UI for valid touch
// displays.
const char kEnableTouchCalibrationSetting[] =
    "enable-touch-calibration-setting";

// Enables touchpad three-finger-click as middle button.
const char kEnableTouchpadThreeFingerClick[] =
    "enable-touchpad-three-finger-click";

// Enables the chromecast support for video player app.
const char kEnableVideoPlayerChromecastSupport[] =
    "enable-video-player-chromecast-support";

// Enables the VoiceInteraction support.
const char kEnableVoiceInteraction[] = "enable-voice-interaction";

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

// Screenshot testing: specifies the directory where the golden screenshots are
// stored.
const char kGoldenScreenshotsDir[] = "golden-screenshots-dir";

// Indicates that the browser is in "browse without sign-in" (Guest session)
// mode. Should completely disable extensions, sync and bookmarks.
const char kGuestSession[] = "bwsi";

// Large wallpaper to use in guest mode (as path to trusted, non-user-writable
// JPEG file).
const char kGuestWallpaperLarge[] = "guest-wallpaper-large";

// Small wallpaper to use in guest mode (as path to trusted, non-user-writable
// JPEG file).
const char kGuestWallpaperSmall[] = "guest-wallpaper-small";

// If true, the Chromebook has a keyboard with a diamond key.
const char kHasChromeOSDiamondKey[] = "has-chromeos-diamond-key";

// If set, the system is a Chromebook with a "standard Chrome OS keyboard",
// which generally means one with a Search key in the standard Caps Lock
// location above the Left Shift key. It should be unset for Chromebooks with
// both Search and Caps Lock keys (e.g. stout) and for devices like Chromeboxes
// that only use external keyboards.
const char kHasChromeOSKeyboard[] = "has-chromeos-keyboard";

const char kHideActiveAppsFromShelf[] = "hide-active-apps-from-shelf";

// Defines user homedir. This defaults to primary user homedir.
const char kHomedir[] = "homedir";

// With this switch, start remora OOBE with the pairing screen.
const char kHostPairingOobe[] = "host-pairing-oobe";

// If true, profile selection in UserManager will always return active user's
// profile.
// TODO(nkostlyev): http://crbug.com/364604 - Get rid of this switch after we
// turn on multi-profile feature on ChromeOS.
const char kIgnoreUserProfileMappingForTests[] =
    "ignore-user-profile-mapping-for-tests";

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

// The memory pressure threshold selection which is used to decide whether and
// when a memory pressure event needs to get fired.
const char kMemoryPressureThresholds[] = "memory-pressure-thresholds";

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

// Indicates that if we should start bootstrapping Master OOBE.
const char kOobeBootstrappingMaster[] = "oobe-bootstrapping-master";

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

// If set to "true", the profile requires policy during restart (policy load
// must succeed, otherwise session restart should fail).
const char kProfileRequiresPolicy[] = "profile-requires-policy";

// The rlz ping delay (in seconds) that overwrites the default value.
const char kRlzPingDelay[] = "rlz-ping-delay";

// App window previews when hovering over the shelf.
const char kShelfHoverPreviews[] = "shelf-hover-previews";

// If true, files in Android internal storage will be shown in Files app.
const char kShowAndroidFilesInFilesApp[] = "show-android-files-in-files-app";

// If true, files in Android internal storage will be hidden in Files app.
const char kHideAndroidFilesInFilesApp[] = "hide-android-files-in-files-app";

// If true, the developer tool overlay will be shown for the login/lock screen.
// This makes it easier to test layout logic.
const char kShowLoginDevOverlay[] = "show-login-dev-overlay";

// Show Play Store in Demo Mode.
const char kShowPlayInDemoMode[] = "show-play-in-demo-mode";

// Indicates that a stub implementation of CrosSettings that stores settings in
// memory without signing should be used, treating current user as the owner.
// This also modifies OwnerSettingsServiceChromeOS::HandlesSetting such that no
// settings are handled by OwnerSettingsServiceChromeOS.
// This option is for testing the chromeos build of chrome on the desktop only.
const char kStubCrosSettings[] = "stub-cros-settings";

// Enables testing for encryption migration UI.
const char kTestEncryptionMigrationUI[] = "test-encryption-migration-ui";

// Overrides Tether with stub service. Provide integer arguments for the number
// of fake networks desired, e.g. 'tether-stub=2'.
const char kTetherStub[] = "tether-stub";

// List of locales supported by voice interaction.
const char kVoiceInteractionLocales[] = "voice-interaction-supported-locales";

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

base::chromeos::MemoryPressureMonitor::MemoryPressureThresholds
GetMemoryPressureThresholds() {
  using MemoryPressureMonitor = base::chromeos::MemoryPressureMonitor;

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          kMemoryPressureThresholds)) {
    const std::string group_name =
        base::FieldTrialList::FindFullName(kMemoryPressureExperimentName);
    if (group_name == kConservativeThreshold)
      return MemoryPressureMonitor::THRESHOLD_CONSERVATIVE;
    if (group_name == kAggressiveCacheDiscardThreshold)
      return MemoryPressureMonitor::THRESHOLD_AGGRESSIVE_CACHE_DISCARD;
    if (group_name == kAggressiveTabDiscardThreshold)
      return MemoryPressureMonitor::THRESHOLD_AGGRESSIVE_TAB_DISCARD;
    if (group_name == kAggressiveThreshold)
      return MemoryPressureMonitor::THRESHOLD_AGGRESSIVE;
    return MemoryPressureMonitor::THRESHOLD_DEFAULT;
  }

  const std::string option =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kMemoryPressureThresholds);
  if (option == kConservativeThreshold)
    return MemoryPressureMonitor::THRESHOLD_CONSERVATIVE;
  if (option == kAggressiveCacheDiscardThreshold)
    return MemoryPressureMonitor::THRESHOLD_AGGRESSIVE_CACHE_DISCARD;
  if (option == kAggressiveTabDiscardThreshold)
    return MemoryPressureMonitor::THRESHOLD_AGGRESSIVE_TAB_DISCARD;
  if (option == kAggressiveThreshold)
    return MemoryPressureMonitor::THRESHOLD_AGGRESSIVE;

  return MemoryPressureMonitor::THRESHOLD_DEFAULT;
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

bool IsVoiceInteractionLocalesSupported() {
  // We use Chromium variations to control locales for which assistant should
  // be enabled. But we still keep checking the previously hard-coded locales
  // for compatibility.
  if (base::FeatureList::IsEnabled(kAssistantFeatureForLocale))
    return true;

  // TODO(updowndota): Add DCHECK here to make sure the value never changes
  // after all the use case for this method has been moved into user session.

  // Disable voice interaction for non-supported locales.
  std::string kLocale = icu::Locale::getDefault().getName();
  if (kLocale != ULOC_US && kLocale != ULOC_UK && kLocale != ULOC_CANADA &&
      base::CommandLine::ForCurrentProcess()
              ->GetSwitchValueASCII(
                  chromeos::switches::kVoiceInteractionLocales)
              .find(kLocale) == std::string::npos) {
    return false;
  }
  return true;
}

bool IsVoiceInteractionFlagsEnabled() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return !IsAssistantFlagsEnabled() &&
         (command_line->HasSwitch(kEnableVoiceInteraction) ||
          base::FeatureList::IsEnabled(kVoiceInteractionFeature));
}

bool IsVoiceInteractionEnabled() {
  return IsVoiceInteractionLocalesSupported() &&
         IsVoiceInteractionFlagsEnabled();
}

bool IsAccountManagerEnabled() {
  return base::FeatureList::IsEnabled(kAccountManager);
}

bool IsAssistantFlagsEnabled() {
  return base::FeatureList::IsEnabled(kAssistantFeature);
}

bool IsAssistantEnabled() {
  // TODO(xiaohuic): We will add locale restrictions later.
  return IsAssistantFlagsEnabled();
}

bool IsSigninFrameClientCertsEnabled() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      kDisableSigninFrameClientCerts);
}

bool IsSigninFrameClientCertUserSelectionEnabled() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      kDisableSigninFrameClientCertUserSelection);
}

bool AreExperimentalAccessibilityFeaturesEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kEnableExperimentalAccessibilityFeatures);
}

bool ShouldHideActiveAppsFromShelf() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kHideActiveAppsFromShelf);
}

bool ShouldShowShelfHoverPreviews() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kShelfHoverPreviews);
}

bool IsInstantTetheringBackgroundAdvertisingSupported() {
  return base::FeatureList::IsEnabled(
      kInstantTetheringBackgroundAdvertisementSupport);
}

bool ShouldShowPlayStoreInDemoMode() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kShowPlayInDemoMode);
}

}  // namespace switches
}  // namespace chromeos
