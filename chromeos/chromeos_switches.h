// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CHROMEOS_SWITCHES_H_
#define CHROMEOS_CHROMEOS_SWITCHES_H_

#include "base/feature_list.h"
#include "base/memory/memory_pressure_monitor_chromeos.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_switches.h"

namespace chromeos {
namespace switches {

// Switches that are used in src/chromeos must go here.
// Other switches that apply just to chromeos code should go here also (along
// with any code that is specific to the chromeos system). Chrome OS specific
// UI should be in src/ash.

// Note: If you add a switch, consider if it needs to be copied to a subsequent
// command line if the process executes a new copy of itself.  (For example,
// see chromeos::LoginUtil::GetOffTheRecordCommandLine().)

// Please keep alphabetized.
CHROMEOS_EXPORT extern const char kAggressiveCacheDiscardThreshold[];
CHROMEOS_EXPORT extern const char kAggressiveTabDiscardThreshold[];
CHROMEOS_EXPORT extern const char kAggressiveThreshold[];
CHROMEOS_EXPORT extern const char kAllowFailedPolicyFetchForTest[];
CHROMEOS_EXPORT extern const char kAllowRAInDevMode[];
CHROMEOS_EXPORT extern const char kAppAutoLaunched[];
CHROMEOS_EXPORT extern const char kAppOemManifestFile[];
CHROMEOS_EXPORT extern const char kArcAvailability[];
CHROMEOS_EXPORT extern const char kArcAvailable[];
CHROMEOS_EXPORT extern const char kArcDataCleanupOnStart[];
CHROMEOS_EXPORT extern const char kArcForceShowOptInUi[];
CHROMEOS_EXPORT extern const char kArcPackagesCacheMode[];
CHROMEOS_EXPORT extern const char kArcStartMode[];
CHROMEOS_EXPORT extern const char kArcTransitionMigrationRequired[];
CHROMEOS_EXPORT extern const char kArtifactsDir[];
CHROMEOS_EXPORT extern const char kCellularFirst[];
CHROMEOS_EXPORT extern const char kChildWallpaperLarge[];
CHROMEOS_EXPORT extern const char kChildWallpaperSmall[];
CHROMEOS_EXPORT extern const char kConservativeThreshold[];
CHROMEOS_EXPORT extern const char kCrosGaiaApiV1[];
CHROMEOS_EXPORT extern const char kCrosRegion[];
CHROMEOS_EXPORT extern const char kCrosRegionsMode[];
CHROMEOS_EXPORT extern const char kCrosRegionsModeHide[];
CHROMEOS_EXPORT extern const char kCrosRegionsModeOverride[];
CHROMEOS_EXPORT extern const char kCrostiniFiles[];
CHROMEOS_EXPORT extern const char kDataSaverPromptDemoMode[];
CHROMEOS_EXPORT extern const char kDefaultWallpaperIsOem[];
CHROMEOS_EXPORT extern const char kDefaultWallpaperLarge[];
CHROMEOS_EXPORT extern const char kDefaultWallpaperSmall[];
CHROMEOS_EXPORT extern const char kDerelictDetectionTimeout[];
CHROMEOS_EXPORT extern const char kDerelictIdleTimeout[];
CHROMEOS_EXPORT extern const char kDisableArcDataWipe[];
CHROMEOS_EXPORT extern const char kDisableArcOptInVerification[];
CHROMEOS_EXPORT extern const char kDisableCaptivePortalBypassProxy[];
CHROMEOS_EXPORT extern const char kDisableCloudImport[];
CHROMEOS_EXPORT extern const char kDisableDataSaverPrompt[];
CHROMEOS_EXPORT extern const char kDisableDemoMode[];
CHROMEOS_EXPORT extern const char kDisableDeviceDisabling[];
CHROMEOS_EXPORT extern const char kDisableEncryptionMigration[];
CHROMEOS_EXPORT extern const char kDisableEolNotification[];
CHROMEOS_EXPORT extern const char kDisableFileManagerTouchMode[];
CHROMEOS_EXPORT extern const char kDisableFineGrainedTimeZoneDetection[];
CHROMEOS_EXPORT extern const char kDisableGaiaServices[];
CHROMEOS_EXPORT extern const char kDisableHIDDetectionOnOOBE[];
CHROMEOS_EXPORT extern const char kDisableLockScreenApps[];
CHROMEOS_EXPORT extern const char kDisableLoginAnimations[];
CHROMEOS_EXPORT extern const char kDisableMachineCertRequest[];
CHROMEOS_EXPORT extern const char kDisableMtpWriteSupport[];
CHROMEOS_EXPORT extern const char kDisableMultiDisplayLayout[];
CHROMEOS_EXPORT extern const char kDisableNetworkPortalNotification[];
CHROMEOS_EXPORT extern const char kDisableNewKoreanIme[];
CHROMEOS_EXPORT extern const char kDisableNewZIPUnpacker[];
CHROMEOS_EXPORT extern const char kDisableOfficeEditingComponentApp[];
CHROMEOS_EXPORT extern const char kDisablePerUserTimezone[];
CHROMEOS_EXPORT extern const char kDisablePhysicalKeyboardAutocorrect[];
CHROMEOS_EXPORT extern const char kDisableRollbackOption[];
CHROMEOS_EXPORT extern const char kDisableSigninFrameClientCerts[];
CHROMEOS_EXPORT extern const char kDisableSigninFrameClientCertUserSelection[];
CHROMEOS_EXPORT extern const char
    kDisableSystemTimezoneAutomaticDetectionPolicy[];
CHROMEOS_EXPORT extern const char kDisableVolumeAdjustSound[];
CHROMEOS_EXPORT extern const char kDisableWakeOnWifi[];
CHROMEOS_EXPORT extern const char kEnableArc[];
CHROMEOS_EXPORT extern const char kEnableArcOobeOptinNoSkip[];
CHROMEOS_EXPORT extern const char kEnableCaptivePortalRandomUrl[];
CHROMEOS_EXPORT extern const char kEnableCastReceiver[];
CHROMEOS_EXPORT extern const char kEnableChromevoxDeveloperOption[];
CHROMEOS_EXPORT extern const char kEnableConsumerKiosk[];
CHROMEOS_EXPORT extern const char kEnableDataSaverPrompt[];
CHROMEOS_EXPORT extern const char kEnableDiscoverApp[];
CHROMEOS_EXPORT extern const char kEnableEncryptionMigration[];
CHROMEOS_EXPORT extern const char kEnableExperimentalAccessibilityFeatures[];
CHROMEOS_EXPORT extern const char kEnableExtensionAssetsSharing[];
CHROMEOS_EXPORT extern const char kEnableFileManagerTouchMode[];
CHROMEOS_EXPORT extern const char kEnableFirstRunUITransitions[];
CHROMEOS_EXPORT extern const char kEnableMarketingOptInScreen[];
CHROMEOS_EXPORT extern const char kEnableNetworkPortalNotification[];
CHROMEOS_EXPORT extern const char kEnableOfflineDemoMode[];
CHROMEOS_EXPORT extern const char kEnablePhysicalKeyboardAutocorrect[];
CHROMEOS_EXPORT extern const char kEnableRequestTabletSite[];
CHROMEOS_EXPORT extern const char kEnableScreenshotTestingWithMode[];
CHROMEOS_EXPORT extern const char kEnableTouchCalibrationSetting[];
CHROMEOS_EXPORT extern const char kEnableTouchpadThreeFingerClick[];
CHROMEOS_EXPORT extern const char kEnableVideoPlayerChromecastSupport[];
CHROMEOS_EXPORT extern const char kEnableVoiceInteraction[];
CHROMEOS_EXPORT extern const char kEnterpriseDisableArc[];
CHROMEOS_EXPORT extern const char kEnterpriseDisableLicenseTypeSelection[];
CHROMEOS_EXPORT extern const char kEnterpriseEnableForcedReEnrollment[];
CHROMEOS_EXPORT extern const char kEnterpriseEnableInitialEnrollment[];
CHROMEOS_EXPORT extern const char kEnterpriseEnableZeroTouchEnrollment[];
CHROMEOS_EXPORT extern const char kEnterpriseEnrollmentInitialModulus[];
CHROMEOS_EXPORT extern const char kEnterpriseEnrollmentModulusLimit[];
CHROMEOS_EXPORT extern const char kExternalMetricsCollectionInterval[];
CHROMEOS_EXPORT extern const char kFirstExecAfterBoot[];
CHROMEOS_EXPORT extern const char kFakeDriveFsLauncherChrootPath[];
CHROMEOS_EXPORT extern const char kFakeDriveFsLauncherSocketPath[];
CHROMEOS_EXPORT extern const char kForceDevToolsAvailable[];
CHROMEOS_EXPORT extern const char kForceFirstRunUI[];
CHROMEOS_EXPORT extern const char kForceHappinessTrackingSystem[];
CHROMEOS_EXPORT extern const char kForceLoginManagerInTests[];
CHROMEOS_EXPORT extern const char kForceSystemCompositorMode[];
CHROMEOS_EXPORT extern const char kGoldenScreenshotsDir[];
CHROMEOS_EXPORT extern const char kGuestSession[];
CHROMEOS_EXPORT extern const char kGuestWallpaperLarge[];
CHROMEOS_EXPORT extern const char kGuestWallpaperSmall[];
CHROMEOS_EXPORT extern const char kHasChromeOSDiamondKey[];
CHROMEOS_EXPORT extern const char kHasChromeOSKeyboard[];
CHROMEOS_EXPORT extern const char kHideActiveAppsFromShelf[];
CHROMEOS_EXPORT extern const char kHideAndroidFilesInFilesApp[];
CHROMEOS_EXPORT extern const char kHomedir[];
CHROMEOS_EXPORT extern const char kHostPairingOobe[];
CHROMEOS_EXPORT extern const char kIgnoreUserProfileMappingForTests[];
CHROMEOS_EXPORT extern const char kLoginManager[];
CHROMEOS_EXPORT extern const char kLoginProfile[];
CHROMEOS_EXPORT extern const char kLoginUser[];
CHROMEOS_EXPORT extern const char kMemoryPressureThresholds[];
CHROMEOS_EXPORT extern const char kNaturalScrollDefault[];
CHROMEOS_EXPORT extern const char kNeedArcMigrationPolicyCheck[];
CHROMEOS_EXPORT extern const char kNoteTakingAppIds[];
CHROMEOS_EXPORT extern const char kOobeBootstrappingMaster[];
CHROMEOS_EXPORT extern const char kOobeForceShowScreen[];
CHROMEOS_EXPORT extern const char kOobeGuestSession[];
CHROMEOS_EXPORT extern const char kOobeSkipPostLogin[];
CHROMEOS_EXPORT extern const char kOobeSkipToLogin[];
CHROMEOS_EXPORT extern const char kOobeTimerInterval[];
CHROMEOS_EXPORT extern const char kProfileRequiresPolicy[];
CHROMEOS_EXPORT extern const char kRlzPingDelay[];
CHROMEOS_EXPORT extern const char kShelfHoverPreviews[];
CHROMEOS_EXPORT extern const char kShowAndroidFilesInFilesApp[];
CHROMEOS_EXPORT extern const char kShowLoginDevOverlay[];
CHROMEOS_EXPORT extern const char kShowPlayInDemoMode[];
CHROMEOS_EXPORT extern const char kStubCrosSettings[];
CHROMEOS_EXPORT extern const char kTestEncryptionMigrationUI[];
CHROMEOS_EXPORT extern const char kTetherStub[];
CHROMEOS_EXPORT extern const char kVoiceInteractionLocales[];
CHROMEOS_EXPORT extern const char kWaitForInitialPolicyFetchForTest[];
CHROMEOS_EXPORT extern const char kWakeOnWifiPacket[];

// Controls whether to enable Chrome OS Account Manager.
CHROMEOS_EXPORT extern const base::Feature kAccountManager;

// Controls whether to enable Google Assistant feature.
CHROMEOS_EXPORT extern const base::Feature kAssistantFeature;

// Returns true if the system should wake in response to wifi traffic.
CHROMEOS_EXPORT bool WakeOnWifiEnabled();

// Returns true if memory pressure handling is enabled.
CHROMEOS_EXPORT bool MemoryPressureHandlingEnabled();

// Returns thresholds for determining if the system is under memory pressure.
CHROMEOS_EXPORT base::chromeos::MemoryPressureMonitor::MemoryPressureThresholds
GetMemoryPressureThresholds();

// Returns true if flags are set indicating that stored user keys are being
// converted to GAIA IDs.
CHROMEOS_EXPORT bool IsGaiaIdMigrationStarted();

// Returns true if this is a Cellular First device.
CHROMEOS_EXPORT bool IsCellularFirstDevice();

// Returns true if the locale is supported by voice interaction.
CHROMEOS_EXPORT bool IsVoiceInteractionLocalesSupported();

// Returns true if voice interaction flags are enabled.
CHROMEOS_EXPORT bool IsVoiceInteractionFlagsEnabled();

// Returns true if voice interaction is enabled.
CHROMEOS_EXPORT bool IsVoiceInteractionEnabled();

// Returns true if Chrome OS Account Manager is enabled.
CHROMEOS_EXPORT bool IsAccountManagerEnabled();

// Returns true if Google Assistant flags are enabled.
CHROMEOS_EXPORT bool IsAssistantFlagsEnabled();

// Returns true if Google Assistant is enabled.
CHROMEOS_EXPORT bool IsAssistantEnabled();

// Returns true if client certificate authentication for the sign-in frame on
// the Chrome OS sign-in screen is enabled.
CHROMEOS_EXPORT bool IsSigninFrameClientCertsEnabled();

// Returns true if user selection of certificates is enabled for the sign-in
// frame on the Chrome OS sign-in screen.
CHROMEOS_EXPORT bool IsSigninFrameClientCertUserSelectionEnabled();

// Returns true if experimental accessibility features are enabled.
CHROMEOS_EXPORT bool AreExperimentalAccessibilityFeaturesEnabled();

// Returns true if we should hide open apps that aren't pinned from the shelf.
CHROMEOS_EXPORT bool ShouldHideActiveAppsFromShelf();

// Returns true if we should show window previews when hovering over an app
// on the shelf.
CHROMEOS_EXPORT bool ShouldShowShelfHoverPreviews();

// Returns true if Instant Tethering should support hosts which use the
// background advertisement model
CHROMEOS_EXPORT bool IsInstantTetheringBackgroundAdvertisingSupported();

// Returns true if Play Store should be available in Demo Mode.
// TODO(michaelpg): Remove after M71 branch to re-enable Play Store by default.
CHROMEOS_EXPORT bool ShouldShowPlayStoreInDemoMode();

}  // namespace switches
}  // namespace chromeos

#endif  // CHROMEOS_CHROMEOS_SWITCHES_H_
