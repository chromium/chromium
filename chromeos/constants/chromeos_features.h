// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CONSTANTS_CHROMEOS_FEATURES_H_
#define CHROMEOS_CONSTANTS_CHROMEOS_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace chromeos {
namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file. If a feature is
// being rolled out via Finch, add a comment in the .cc file.

COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kAllowScrollSettings;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kAmbientModeFeature;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::FeatureParam<bool> kAmbientModeCapturedOnPixelAlbumEnabled;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::FeatureParam<bool> kAmbientModeFineArtAlbumEnabled;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::FeatureParam<bool> kAmbientModeFeaturedPhotoAlbumEnabled;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::FeatureParam<bool> kAmbientModeEarthAndSpaceAlbumEnabled;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::FeatureParam<bool> kAmbientModeStreetArtAlbumEnabled;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::FeatureParam<bool> kAmbientModeDefaultFeedEnabled;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::FeatureParam<bool> kAmbientModePersonalPhotosEnabled;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::FeatureParam<bool> kAmbientModeFeaturedPhotosEnabled;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::FeatureParam<bool> kAmbientModeGeoPhotosEnabled;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::FeatureParam<bool>
    kAmbientModeCulturalInstitutePhotosEnabled;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::FeatureParam<bool> kAmbientModeRssPhotosEnabled;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::FeatureParam<bool> kAmbientModeCapturedOnPixelPhotosEnabled;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kAmbientModePhotoPreviewFeature;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kAmbientModeDevUseProdFeature;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kArcAdbSideloadingFeature;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kArcManagedAdbSideloadingSupport;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kArcPreImeKeyEventSupport;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kAutoScreenBrightness;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kAssistAutoCorrect;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kAssistPersonalInfo;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kAssistPersonalInfoAddress;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kAssistPersonalInfoEmail;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kAssistPersonalInfoName;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kAssistPersonalInfoPhoneNumber;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kAvatarToolbarButton;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kBetterUpdateScreen;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kBluetoothAggressiveAppearanceFilter;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kBluetoothFixA2dpPacketSize;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kBluetoothPhoneFilter;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kBluetoothNextHandsfreeProfile;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kCameraSystemWebApp;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kCdmFactoryDaemon;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kChildSpecificSignin;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kCrostiniDiskResizing;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kCrostiniUseBusterImage;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kCrostiniGpuSupport;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kCrostiniUseDlc;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kCrostiniEnableDlc;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kDiagnosticsApp;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kDisableCryptAuthV1DeviceSync;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kCryptAuthV2DeviceActivityStatus;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kDisableIdleSocketsCloseOnMemoryPressure;
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
extern const base::Feature kDriveFsBidirectionalNativeMessaging;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kDriveFsMirroring;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kEmojiSuggestAddition;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kEolWarningNotifications;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kExoOrdinalMotion;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kExoPointerLock;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kFamilyLinkOnSchoolDevice;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kFilesCameraFolder;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kFilesNG;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kFilesSinglePartitionFormat;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kFilesSWA;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kFilesTransferDetails;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kFilesZipMount;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kFilesZipPack;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kFilesZipUnpack;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kMojoDBusRelay;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kClipboardHistory;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kClipboardHistorySimpleRender;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kEnableFilesAppCopyImage;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kEnableImeSandbox;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kFsNosymfollow;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kHandwritingGestureEditing;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kGamepadVibration;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kGesturePropertiesDBusService;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kGuestOsExternalProtocol;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kHelpAppFirstRun;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kHelpAppReleaseNotes;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kHelpAppSearchServiceIntegration;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kImeInputLogicHmm;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kImeInputLogicFst;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kImeInputLogicMozc;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kImeOptionsInSettings;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kVirtualKeyboardFloatingDefault;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kInstantTethering;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kLacrosSupport;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kLanguageSettingsUpdate;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kLoginDeviceManagementDisclosure;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kLoginDisplayPasswordButton;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kMediaApp;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kMinimumChromeVersion;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kOsSettingsDeepLinking;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kOsSettingsPolymer3;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kParentalControlsSettings;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kPhoneHub;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kPluginVmShowCameraPermissions;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kPluginVmShowMicrophonePermissions;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kPrintJobManagementApp;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kPrintSaveToDrive;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kPrinterStatus;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kPrinterStatusDialog;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kQuickAnswers;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kQuickAnswersDogfood;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kQuickAnswersRichUi;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kQuickAnswersTextAnnotator;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kQuickAnswersSubToggle;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kQuickAnswersTranslation;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kQuickAnswersTranslationCloudAPI;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kQuickAnswersOnEditableText;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kQuickUnlockPinAutosubmit;
// TODO(crbug.com/1104164) - Remove this once most
// users have their preferences backfilled.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kQuickUnlockPinAutosubmitBackfill;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kReleaseNotesNotification;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kReleaseNotesNotificationAllChannels;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kFiltersInRecents;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kScanningUI;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kSessionManagerLongKillTimeout;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kShelfHotseat;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kShowBluetoothDebugLogToggle;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kShowBluetoothDeviceBattery;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kShowPlayInDemoMode;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kShowStepsInDemoModeSetup;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kSmartDimExperimentalComponent;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kSmartDimNewMlAgent;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kSmartDimModelV3;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kSplitSettingsSync;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kSuggestedContentToggle;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kSystemLatinPhysicalTyping;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kTelemetryExtension;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kUnifiedMediaView;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kUpdatedCellularActivationUi;
// Visible for testing. Call UseBrowserSyncConsent() to check the flag.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kUseBrowserSyncConsent;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kUseWallpaperStagingUrl;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kUseMessagesStagingUrl;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kUserActivityPrediction;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kUseSearchClickForRightClick;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kViewBasedMultiprofileLogin;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kVirtualKeyboardBorderedKey;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kVmCameraMicIndicatorsAndNotifications;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kWifiSyncAndroid;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kImeMozcProto;

// Keep alphabetized.

COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsAmbientModeEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsAmbientModePhotoPreviewEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsAmbientModeDevUseProdEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsAssistantEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsBetterUpdateEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsChildSpecificSigninEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsDeepLinkingEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsDiagnosticsAppEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsFamilyLinkOnSchoolDeviceEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsImeSandboxEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
bool IsInstantTetheringBackgroundAdvertisingSupported();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsLacrosSupportEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
bool IsLoginDeviceManagementDisclosureEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsLoginDisplayPasswordButtonEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsMinimumChromeVersionEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsClipboardHistoryEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
bool IsClipboardHistorySimpleRenderEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsOobeScreensPriorityEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsParentalControlsSettingsEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsPhoneHubEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsPinAutosubmitFeatureEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
bool IsPinAutosubmitBackfillFeatureEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsQuickAnswersDogfood();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsQuickAnswersEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsQuickAnswersRichUiEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsQuickAnswersSettingToggleEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
bool IsQuickAnswersTextAnnotatorEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsQuickAnswersTranslationEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
bool IsQuickAnswersTranslationCloudAPIEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsQuickAnswersOnEditableTextEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsSplitSettingsSyncEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsViewBasedMultiprofileLoginEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsWifiSyncAndroidEnabled();
// TODO(michaelpg): Remove after M71 branch to re-enable Play Store by default.
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool ShouldShowPlayStoreInDemoMode();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool ShouldUseBrowserSyncConsent();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool ShouldUseV1DeviceSync();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool ShouldUseV2DeviceSync();

// Keep alphabetized.

}  // namespace features
}  // namespace chromeos

#endif  // CHROMEOS_CONSTANTS_CHROMEOS_FEATURES_H_
