// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/constants/chromeos_features.h"

namespace chromeos {
namespace features {
namespace {

// Controls whether Instant Tethering supports hosts which use the background
// advertisement model.
const base::Feature kInstantTetheringBackgroundAdvertisementSupport{
    "InstantTetheringBackgroundAdvertisementSupport",
    base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace

// Controls whether to enable Ambient mode feature.
const base::Feature kAmbientModeFeature{"ChromeOSAmbientMode",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable ARC ADB sideloading support.
const base::Feature kArcAdbSideloadingFeature{
    "ArcAdbSideloading", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables auto screen-brightness adjustment when ambient light
// changes.
const base::Feature kAutoScreenBrightness{"AutoScreenBrightness",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables more aggressive filtering out of Bluetooth devices with
// "appearances" that are less likely to be pairable or useful.
const base::Feature kBluetoothAggressiveAppearanceFilter{
    "BluetoothAggressiveAppearanceFilter", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables more filtering out of phones from the Bluetooth UI.
const base::Feature kBluetoothPhoneFilter{"BluetoothPhoneFilter",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Feature containing param to block provided long term keys.
const base::Feature kBlueZLongTermKeyBlocklist{
    "BlueZLongTermKeyBlocklist", base::FEATURE_DISABLED_BY_DEFAULT};
const char kBlueZLongTermKeyBlocklistParamName[] = "ltk_blocklist";

// Enable or disables running the Camera App as a System Web App.
const base::Feature kCameraSystemWebApp{"CameraSystemWebApp",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables Crostini Backup.
const base::Feature kCrostiniBackup{"CrostiniBackup",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables Crostini using Buster container images.
const base::Feature kCrostiniUseBusterImage{"CrostiniUseBusterImage",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables Crostini GPU support.
const base::Feature kCrostiniGpuSupport{"CrostiniGpuSupport",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables Crostini usb mounting for unsupported devices.
const base::Feature kCrostiniUsbAllowUnsupported{
    "CrostiniUsbAllowUnsupported", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the new WebUI Crostini installer.
const base::Feature kCrostiniWebUIInstaller{"CrostiniWebUIInstaller",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables the new WebUI Crostini upgrader.
const base::Feature kCrostiniWebUIUpgrader{"CrostiniWebUIUpgrader",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Deprecates the CryptAuth v1 DeviceSync flow. Note: During the first phase
// of the v2 DeviceSync rollout, v1 and v2 DeviceSync run in parallel. This flag
// is needed to deprecate the v1 service during the second phase of the rollout.
// kCryptAuthV2DeviceSync should be enabled before this flag is flipped.
const base::Feature kCryptAuthV1DeviceSyncDeprecate{
    "CryptAuthV1DeviceSyncDeprecate", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables using Cryptauth's GetDevicesActivityStatus API.
const base::Feature kCryptAuthV2DeviceActivityStatus{
    "CryptAuthV2DeviceActivityStatus", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the CryptAuth v2 DeviceSync flow. Regardless of this
// flag, v1 DeviceSync will continue to operate until it is deprecated via the
// feature flag kCryptAuthV1DeviceSyncDeprecate.
const base::Feature kCryptAuthV2DeviceSync{"CryptAuthV2DeviceSync",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the CryptAuth v2 Enrollment flow.
const base::Feature kCryptAuthV2Enrollment{"CryptAuthV2Enrollment",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Disables "Office Editing for Docs, Sheets & Slides" component app so handlers
// won't be registered, making it possible to install another version for
// testing.
const base::Feature kDisableOfficeEditingComponentApp{
    "DisableOfficeEditingComponentApp", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables Discover Application on Chrome OS.
// If enabled, Discover App will be shown in launcher.
const base::Feature kDiscoverApp{"DiscoverApp",
                                 base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, DriveFS will be used for Drive sync.
const base::Feature kDriveFs{"DriveFS", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables DriveFS' experimental local files mirroring functionality.
const base::Feature kDriveFsMirroring{"DriveFsMirroring",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, allows Unicorn users to add secondary EDU accounts.
const base::Feature kEduCoexistence{"EduCoexistence",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled shows the visual signals feedback panel.
const base::Feature kEnableFileManagerFeedbackPanel{
    "EnableFeedbackPanel", base::FEATURE_ENABLED_BY_DEFAULT};

// Enable the piex-wasm module for raw image preview image extraction.
const base::Feature kEnableFileManagerPiexWasm{
    "PiexWasm", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables Device End Of Lifetime warning notifications.
const base::Feature kEolWarningNotifications{"EolWarningNotifications",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Enable or disable pointer lock for exo windows.
const base::Feature kExoPointerLock{"ExoPointerLock",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the next generation file manager.
const base::Feature kFilesNG{"FilesNG", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the use of Mojo by Chrome-process code to communicate with Power
// Manager. In order to use mojo, this feature must be turned on and a callsite
// must use PowerManagerMojoClient::Get().
const base::Feature kMojoDBusRelay{"MojoDBusRelay",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, will display blocking screens during re-authentication after a
// supervision transition occurred.
const base::Feature kEnableSupervisionTransitionScreens{
    "EnableSupervisionTransitionScreens", base::FEATURE_ENABLED_BY_DEFAULT};

// Enable restriction of symlink traversal on user-supplied filesystems.
const base::Feature kFsNosymfollow{"FsNosymfollow",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Enable a D-Bus service for accessing gesture properties.
const base::Feature kGesturePropertiesDBusService{
    "GesturePropertiesDBusService", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable primary/secondary action buttons on Gaia login screen.
const base::Feature kGaiaActionButtons{"GaiaActionButtons",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// The new ChromeOS Help App. https://crbug.com/1012578.
const base::Feature kHelpAppV2{"HelpAppV2", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable or disable Unified Input Logic for HMM decoder in the IME extension
// on Chrome OS.
const base::Feature kImeInputLogicHmm{"ImeInputLogicHmm",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Enable or disable Unified Input Logic for FST decoder in the IME extension
// on Chrome OS.
const base::Feature kImeInputLogicFst{"ImeInputLogicFst",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Enable or disable Unified Input Logic for FST decoder for non-English in
// the IME extension on Chrome OS.
const base::Feature kImeInputLogicFstNonEnglish{
    "ImeInputLogicFstNonEnglish", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable or disable Unified Input Logic for Mozc decoder in the IME extension
// on Chrome OS.
const base::Feature kImeInputLogicMozc{"ImeInputLogicMozc",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Enable or disable IME service decoder engine and 'ime' sandbox on Chrome OS.
const base::Feature kImeDecoderWithSandbox{"ImeDecoderWithSandbox",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Enable or disable using the floating virtual keyboard as the default option
// on Chrome OS.
const base::Feature kVirtualKeyboardFloatingDefault{
    "VirtualKeyboardFloatingDefault", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables Instant Tethering on Chrome OS.
const base::Feature kInstantTethering{"InstantTethering",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// ChromeOS Media App. https://crbug.com/996088.
const base::Feature kMediaApp{"MediaApp", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable the Parental Controls section of settings.
const base::Feature kParentalControlsSettings{
    "ChromeOSParentalControlsSettings", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables Release Notes on Chrome OS.
const base::Feature kReleaseNotes{"ReleaseNotes",
                                  base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables Release Notes notifications on Chrome OS.
const base::Feature kReleaseNotesNotification{"ReleaseNotesNotification",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables long kill timeout for session manager daemon. When
// enabled, session manager daemon waits for a longer time (e.g. 12s) for chrome
// to exit before sending SIGABRT. Otherwise, it uses the default time out
// (currently 3s).
const base::Feature kSessionManagerLongKillTimeout{
    "SessionManagerLongKillTimeout", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the scrollable shelf.
const base::Feature kShelfScrollable{"ShelfScrollable",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the shelf hotseat.
const base::Feature kShelfHotseat{"ShelfHotseat",
                                  base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables a toggle to enable Bluetooth debug logs.
const base::Feature kShowBluetoothDebugLogToggle{
    "ShowBluetoothDebugLogToggle", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables showing the battery level in the System Tray and Settings
// UI for supported Bluetooth Devices.
const base::Feature kShowBluetoothDeviceBattery{
    "ShowBluetoothDeviceBattery", base::FEATURE_DISABLED_BY_DEFAULT};

// Shows the Play Store icon in Demo Mode.
const base::Feature kShowPlayInDemoMode{"ShowPlayInDemoMode",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Uses the V3 (~2019-05 era) Smart Dim model instead of the default V2
// (~2018-11) model.
const base::Feature kSmartDimModelV3{"SmartDimModelV3",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// Splits OS settings (display, mouse, keyboard, etc.) out from browser settings
// into a separate window.
const base::Feature kSplitSettings{"SplitSettings",
                                   base::FEATURE_ENABLED_BY_DEFAULT};

// Enables separate sync controls for OS settings (display, keyboard, etc.).
// For example, the user could choose to sync OS settings but not browser
// settings.
const base::Feature kSplitSettingsSync{"SplitSettingsSync",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the updated cellular activation UI; see go/cros-cellular-design.
const base::Feature kUpdatedCellularActivationUi{
    "UpdatedCellularActivationUi", base::FEATURE_DISABLED_BY_DEFAULT};

// Use the messages.google.com domain as part of the "Messages" feature under
// "Connected Devices" settings.
const base::Feature kUseMessagesGoogleComDomain{
    "UseMessagesGoogleComDomain", base::FEATURE_ENABLED_BY_DEFAULT};

// Use the staging URL as part of the "Messages" feature under "Connected
// Devices" settings.
const base::Feature kUseMessagesStagingUrl{"UseMessagesStagingUrl",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables user activity prediction for power management on
// Chrome OS.
// Defined here rather than in //chrome alongside other related features so that
// PowerPolicyController can check it.
const base::Feature kUserActivityPrediction{"UserActivityPrediction",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Remap search+click to right click instead of the legacy alt+click on
// Chrome OS.
const base::Feature kUseSearchClickForRightClick{
    "UseSearchClickForRightClick", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable or disable native controls in video player on Chrome OS.
const base::Feature kVideoPlayerNativeControls{
    "VideoPlayerNativeControls", base::FEATURE_ENABLED_BY_DEFAULT};

// Enable or disable bordered key for virtual keyboard on Chrome OS.
const base::Feature kVirtualKeyboardBorderedKey{
    "VirtualKeyboardBorderedKey", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable or disable resizable floating virtual keyboard on Chrome OS.
const base::Feature kVirtualKeyboardFloatingResizable{
    "VirtualKeyboardFloatingResizable", base::FEATURE_DISABLED_BY_DEFAULT};

////////////////////////////////////////////////////////////////////////////////

bool IsAmbientModeEnabled() {
  return base::FeatureList::IsEnabled(kAmbientModeFeature);
}

bool IsEduCoexistenceEnabled() {
  return base::FeatureList::IsEnabled(kEduCoexistence);
}

bool IsImeDecoderWithSandboxEnabled() {
  return base::FeatureList::IsEnabled(kImeDecoderWithSandbox);
}

bool IsInstantTetheringBackgroundAdvertisingSupported() {
  return base::FeatureList::IsEnabled(
      kInstantTetheringBackgroundAdvertisementSupport);
}

bool IsParentalControlsSettingsEnabled() {
  return base::FeatureList::IsEnabled(kParentalControlsSettings);
}

bool IsSplitSettingsEnabled() {
  return base::FeatureList::IsEnabled(kSplitSettings);
}

bool IsSplitSettingsSyncEnabled() {
  return base::FeatureList::IsEnabled(kSplitSettingsSync);
}

bool ShouldDeprecateV1DeviceSync() {
  return ShouldUseV2DeviceSync() &&
         base::FeatureList::IsEnabled(
             chromeos::features::kCryptAuthV1DeviceSyncDeprecate);
}

bool ShouldShowPlayStoreInDemoMode() {
  return base::FeatureList::IsEnabled(kShowPlayInDemoMode);
}

bool ShouldUseV2DeviceSync() {
  return base::FeatureList::IsEnabled(
             chromeos::features::kCryptAuthV2Enrollment) &&
         base::FeatureList::IsEnabled(
             chromeos::features::kCryptAuthV2DeviceSync);
}

}  // namespace features
}  // namespace chromeos
