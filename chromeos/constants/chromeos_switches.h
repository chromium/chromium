// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CONSTANTS_CHROMEOS_SWITCHES_H_
#define CHROMEOS_CONSTANTS_CHROMEOS_SWITCHES_H_

#include "base/component_export.h"
#include "chromeos/dbus/constants/dbus_switches.h"

namespace chromeos {
namespace switches {

// Switches that are used in src/chromeos must go here.
// Other switches that apply just to chromeos code should go here also (along
// with any code that is specific to the chromeos system). Chrome OS specific
// UI should be in src/ash.
//
// Prefer adding Features over switches. Features go in chromeos_features.h.
//
// Note: If you add a switch, consider if it needs to be copied to a subsequent
// command line if the process executes a new copy of itself.  (For example,
// see chromeos::LoginUtil::GetOffTheRecordCommandLine().)

// Please keep alphabetized.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kAggressiveCacheDiscardThreshold[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kAggressiveTabDiscardThreshold[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kAggressiveThreshold[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kAllowFailedPolicyFetchForTest[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kAllowRAInDevMode[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kAppAutoLaunched[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kAppOemManifestFile[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kArcAvailability[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kArcAvailable[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kArcBuildProperties[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kArcDataCleanupOnStart[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kArcDisableAppSync[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kArcDisableGmsCoreCache[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kArcDisableLocaleSync[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kArcDisableSystemDefaultApps[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kArcDisablePlayAutoInstall[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kArcEnableNativeBridge64BitSupportExperiment[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kArcForceCacheAppIcons[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kArcForceShowOptInUi[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kArcPackagesCacheMode[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kArcPlayStoreAutoUpdate[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kArcScale[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kArcStartMode[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kArcTosHostForTests[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kArcTransitionMigrationRequired[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kCellularFirst[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kChildWallpaperLarge[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kChildWallpaperSmall[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kConservativeThreshold[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kCrosRegion[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kCrosRegionsMode[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kCrosRegionsModeHide[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kCrosRegionsModeOverride[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kDefaultWallpaperIsOem[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kDefaultWallpaperLarge[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kDefaultWallpaperSmall[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kDerelictDetectionTimeout[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kDerelictIdleTimeout[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kDisableArcDataWipe[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kDisableArcOptInVerification[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kDisableDemoMode[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kDisableDeviceDisabling[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kDisableEncryptionMigration[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kDisableFineGrainedTimeZoneDetection[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kDisableGaiaServices[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kDisableHIDDetectionOnOOBE[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kDisableLoginAnimations[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kDisableMachineCertRequest[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kDisableMultiDisplayLayout[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kDisableNewZIPUnpacker[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kDisablePerUserTimezone[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kDisableRollbackOption[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kDisableSigninFrameClientCerts[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kDisableVolumeAdjustSound[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kDisableWakeOnWifi[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kEnableArc[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kEnableArcVm[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kEnableCastReceiver[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kEnableChromevoxDeveloperOption[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kEnableConsumerKiosk[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kEnableTabletFormFactor[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kEnableEncryptionMigration[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kEnableExtensionAssetsSharing[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kEnableHoudini[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kEnableHoudini64[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kEnableNdkTranslation[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kEnableNdkTranslation64[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kEnableRequestTabletSite[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kEnableTouchCalibrationSetting[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kEnableTouchpadThreeFingerClick[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kEnterpriseDisableArc[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kEnterpriseEnableForcedReEnrollment[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kEnterpriseEnableInitialEnrollment[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kEnterpriseEnablePrivateSetMembership[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kEnterpriseEnableZeroTouchEnrollment[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kEnterpriseEnrollmentInitialModulus[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kEnterpriseEnrollmentModulusLimit[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kExternalMetricsCollectionInterval[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kFirstExecAfterBoot[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kFakeDriveFsLauncherChrootPath[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kFakeDriveFsLauncherSocketPath[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kFakeArcRecommendedAppsForTesting[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kFingerprintSensorLocation[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kForceCertVerifierBuiltin[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kForceDevToolsAvailable[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kForceFirstRunUI[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kForceHappinessTrackingSystem[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kForceHWIDCheckFailureForTest[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kForceLoginManagerInTests[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kForceSystemCompositorMode[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kGuestSession[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kGuestWallpaperLarge[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kGuestWallpaperSmall[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kHasChromeOSKeyboard[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kHideAndroidFilesInFilesApp[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kHomedir[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kIgnoreArcVmDevConf[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kIgnoreUserProfileMappingForTests[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kKernelnextRestrictVMs[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kLacrosChromeAdditionalArgs[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kLacrosChromePath[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kLacrosMojoSocketForTesting[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kLoginManager[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kLoginProfile[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kLoginUser[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kMarketingOptInUrl[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kNaturalScrollDefault[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kNeedArcMigrationPolicyCheck[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kNoteTakingAppIds[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kOobeEulaUrlForTests[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kOobeForceShowScreen[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kOobeForceTabletFirstRun[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kOobeGuestSession[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kOobeSkipPostLogin[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kOobeSkipToLogin[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kOobeTimerInterval[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kOobeTimezoneOverrideForTests[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kPublicAccountsSamlAclUrl[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kDisableArcCpuRestriction[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kProfileRequiresPolicy[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kRedirectLibassistantLogging[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kRegulatoryLabelDir[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kRlzPingDelay[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kSafeMode[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kSamlPasswordChangeUrl[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kShelfHoverPreviews[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kShelfHotseat[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kShowAndroidFilesInFilesApp[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kShowLoginDevOverlay[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kShowOobeDevOverlay[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kOobeScreenshotDirectory[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kTelemetryExtensionDirectory[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kTestEncryptionMigrationUI[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kTestWallpaperServer[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kTetherStub[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kTetherHostScansIgnoreWiredConnections[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kWaitForInitialPolicyFetchForTest[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const char kWakeOnWifiPacket[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kUnfilteredBluetoothDevices[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kUpdateRequiredAueForTest[];

////////////////////////////////////////////////////////////////////////////////

// Returns true if the system should wake in response to wifi traffic.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool WakeOnWifiEnabled();

// Returns true if memory pressure handling is enabled.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool MemoryPressureHandlingEnabled();

// Returns true if flags are set indicating that stored user keys are being
// converted to GAIA IDs.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsGaiaIdMigrationStarted();

// Returns true if this is a Cellular First device.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsCellularFirstDevice();

// Returns true if client certificate authentication for the sign-in frame on
// the Chrome OS sign-in screen is enabled.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsSigninFrameClientCertsEnabled();

// Returns true if we should show the modular shelf with the hotseat UI and
// a smaller shelf in clamshell mode.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool ShouldShowShelfHotseat();

// Returns true if we should show window previews when hovering over an app
// on the shelf.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool ShouldShowShelfHoverPreviews();

// Returns true if the Chromebook should ignore its wired connections when
// deciding whether to run scans for tethering hosts. Should be used only for
// testing.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
bool ShouldTetherHostScansIgnoreWiredConnections();

// Returns true if we should skip all other OOBE pages after user login.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool ShouldSkipOobePostLogin();

// Returns true if the device is of tablet form factor.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsTabletFormFactor();

// Returns true if GAIA services has been disabled.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsGaiaServicesDisabled();

// Returns true if |kDisableArcCpuRestriction| is true.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsArcCpuRestrictionDisabled();

// Returns true if all Bluetooth devices in UI (System Tray/Settings Page.)
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsUnfilteredBluetoothDevicesEnabled();

// Returns whether the first user run OOBE flow (sequence of screens shown to
// the user on their first login) should show tablet mode screens when the
// device is not in tablet mode.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool ShouldOobeUseTabletModeFirstRun();

// Returns true if device policy DeviceMinimumVersion should assume that
// Auto Update Expiration is reached. This should only be used for testing.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
bool IsAueReachedForUpdateRequiredForTest();

}  // namespace switches
}  // namespace chromeos

#endif  // CHROMEOS_CONSTANTS_CHROMEOS_SWITCHES_H_
