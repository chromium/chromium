// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CONSTANTS_CHROMEOS_FEATURES_H_
#define CHROMEOS_CONSTANTS_CHROMEOS_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace chromeos {
namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file. If a feature is
// being rolled out via Finch, add a comment in the .cc file.

COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kAmbientModeFeature;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kArcAdbSideloadingFeature;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kAutoScreenBrightness;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kBluetoothAggressiveAppearanceFilter;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kBluetoothPhoneFilter;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kBlueZLongTermKeyBlocklist;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kBlueZLongTermKeyBlocklistParamName[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kCameraSystemWebApp;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kCrostiniBackup;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kCrostiniUseBusterImage;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kCrostiniGpuSupport;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kCrostiniUsbAllowUnsupported;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kCrostiniWebUIInstaller;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kCryptAuthV1DeviceSyncDeprecate;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kCryptAuthV2DeviceActivityStatus;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kCrostiniWebUIUpgrader;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kCryptAuthV2DeviceSync;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kCryptAuthV2Enrollment;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kDisableOfficeEditingComponentApp;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kDiscoverApp;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kDriveFs;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kDriveFsMirroring;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kEolWarningNotifications;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kEduCoexistence;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kEnableFileManagerFeedbackPanel;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kEnableFileManagerPiexWasm;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kExoPointerLock;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kFilesNG;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kMojoDBusRelay;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kEnableSupervisionTransitionScreens;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kFsNosymfollow;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kGaiaActionButtons;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kGesturePropertiesDBusService;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kHelpAppV2;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kImeInputLogicHmm;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kImeInputLogicFst;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kImeInputLogicFstNonEnglish;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kImeInputLogicMozc;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kImeDecoderWithSandbox;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kVirtualKeyboardFloatingDefault;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kInstantTethering;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kMediaApp;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kParentalControlsSettings;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kReleaseNotes;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kReleaseNotesNotification;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kSessionManagerLongKillTimeout;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kShelfScrollable;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kShelfHotseat;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kShowBluetoothDebugLogToggle;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kShowBluetoothDeviceBattery;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kShowPlayInDemoMode;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kSmartDimModelV3;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kSplitSettings;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kSplitSettingsSync;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kUpdatedCellularActivationUi;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kUseMessagesGoogleComDomain;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kUseMessagesStagingUrl;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kUserActivityPrediction;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kUseSearchClickForRightClick;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kVideoPlayerNativeControls;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kVirtualKeyboardBorderedKey;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kVirtualKeyboardFloatingResizable;

// Keep alphabetized.

COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsAmbientModeEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsAssistantEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsEduCoexistenceEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsImeDecoderWithSandboxEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
bool IsInstantTetheringBackgroundAdvertisingSupported();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsParentalControlsSettingsEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsSplitSettingsEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsSplitSettingsSyncEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool ShouldDeprecateV1DeviceSync();

// TODO(michaelpg): Remove after M71 branch to re-enable Play Store by default.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool ShouldShowPlayStoreInDemoMode();

COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool ShouldUseV2DeviceSync();

// Keep alphabetized.

}  // namespace features
}  // namespace chromeos

#endif  // CHROMEOS_CONSTANTS_CHROMEOS_FEATURES_H_
