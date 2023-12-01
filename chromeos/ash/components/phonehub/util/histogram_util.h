// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_UTIL_HISTOGRAM_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_UTIL_HISTOGRAM_UTIL_H_

#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"

namespace ash {
namespace phonehub {
namespace util {

// Enumeration of possible opt-in entry points for Phone Hub feature. Keep in
// sync with the corresponding PhoneHubOptInEntryPoint enum in
// //tools/metrics/histograms/metadata/phonehub/enums.xml. These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
enum class OptInEntryPoint {
  kSetupFlow = 0,
  kOnboardingFlow = 1,
  kSettings = 2,
  kMaxValue = kSettings,
};

// Enumeration of possible opt-in entry points for Phone Hub Camera Roll
// feature. Keep in sync with the corresponding
// PhoneHubCameraRollOptInEntryPoint enum in
// //tools/metrics/histograms/metadata/phonehub/enums.xml. These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
enum class CameraRollOptInEntryPoint {
  kSetupFlow = 0,
  kOnboardingDialog = 1,
  kSettings = 2,
  kMaxValue = kSettings,
};

// Enumeration of results of attempting to download a file from Phone Hub's
// Camera Roll. Keep in sync with the corresponding
// PhoneHubCameraRollDownloadResult enum in
// //tools/metrics/histograms/metadata/phonehub/enums.xml. These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
enum class CameraRollDownloadResult {
  // The download was successful.
  kSuccess = 0,
  // The download was canceled likely due to connection loss.
  kTransferCanceled = 1,
  // Failed to transfer the file from the phone.
  kTransferFailed = 2,
  // The file was no longer available on the phone.
  kFileNotAvailable = 3,
  // The file could not be downloaded because the file name provided was
  // invalid.
  kInvalidFileName = 4,
  // The file could not be downloaded because it was already being downloaded in
  // a previous attempt.
  kPayloadAlreadyExists = 5,
  // The file could not be downloaded because there was not enough free disk
  // space for the item requested.
  kInsufficientDiskSpace = 6,
  // The file could not be downloaded because a file already exists at the
  // target path, likely a result of some race conditions.
  kNotUniqueFilePath = 7,
  // The file could not be downloaded because the destination path could not be
  // opened for I/O.
  kTargetFileNotAccessible = 8,
  kMaxValue = kTargetFileNotAccessible,
};

// Enumeration of results of a tethering connection attempt.
enum class TetherConnectionResult {
  kAttemptConnection = 0,
  kSuccess = 1,
  kMaxValue = kSuccess,
};

// Keep in sync with the corresponding PhoneHubMessageResult enum in
// //tools/metrics/histograms/metadata/phonehub/enums.xml. These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
enum class PhoneHubMessageResult {
  kRequestAttempted = 0,
  kResponseReceived = 1,
  kMaxValue = kResponseReceived,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Keep in sync with the corresponding PhoneHubPermissionsOnboardingSetUpMode
// enum in //tools/metrics/histograms/metadata/phonehub/enums.xml.
// Keep in sync with PhoneHubPermissionsSetupMode enum in
// //chrome/browser/resources/ash/settings/multidevice_page/
// multidevice_constants.ts
enum class PermissionsOnboardingSetUpMode {
  kNone = 0,
  kNotification = 1,
  kMessagingApps = 2,
  kCameraRoll = 3,
  kNotificationAndMessagingApps = 4,
  kNotificationAndCameraRoll = 5,
  kMessagingAppsAndCameraRoll = 6,
  kAllPermissions = 7,
  kMaxValue = kAllPermissions
};

// Keep in sync with the PhoneHubPermissionsSetupFlowScreens enum in
// //chrome/browser/resources/ash/settings/multidevice_page/
// multidevice_constants.ts
enum class PermissionsOnboardingStep {
  kUnknown = 0,
  kDialogIntroAction = 1,
  kDialogFinishOnPhoneAction = 2,
  kDialogConnectingAction = 3,
  kDialogConnectionErrorAction = 4,
  kDialogConnectionTimeOutAction = 5,
  kDialogSetupFinished = 6,
  kDialogSetAPinOrPassword = 7
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Keep in sync with the corresponding PhoneHubPermissionsOnboardingScreenEvent
// enum in //tools/metrics/histograms/metadata/phonehub/enums.xml.
// Keep in sync with the PhoneHubPermissionsSetupAction enum in
// //chrome/browser/resources/ash/settings/multidevice_page/
// multidevice_constants.ts
enum class PermissionsOnboardingScreenEvent {
  kUnknown = 0,
  kShown = 1,
  kLearnMore = 2,
  kDismissOrCancel = 3,
  kSetUpOrDone = 4,
  kNextOrTryAgain = 5,
  kMaxValue = kNextOrTryAgain
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Keep in sync with corresponding enum in tools/metrics/histograms/enums.xml.
enum class MultiDeviceSetupDialogEntrypoint {
  kSettingsPage = 0,
  kSetupNotification = 1,
  kPhoneHubBubble = 2,
  kPhoneHubBubbleAferNudge = 3,
  kMaxValue = kPhoneHubBubbleAferNudge
};

// Logs a given opt-in |entry_point| for the PhoneHub feature.
void LogFeatureOptInEntryPoint(OptInEntryPoint entry_point);

// Logs a given opt-in |entry_point| for the PhoneHub Camera Roll feature.
void LogCameraRollFeatureOptInEntryPoint(CameraRollOptInEntryPoint entry_point);

// Logs a given |result| of a tethering connection attempt.
void LogTetherConnectionResult(TetherConnectionResult result);

// Logs a given |result| for a request message.
void LogMessageResult(proto::MessageType message, PhoneHubMessageResult result);

// Logs if the Android component has storage access permission. If not, Camera
// Roll is hidden.
void LogCameraRollAndroidHasStorageAccessPermission(bool has_permission);

// Logs the result of a file download from Camera Roll.
void LogCameraRollDownloadResult(CameraRollDownloadResult result);

// Log multidevice permissions setup onboarding promotion in Phonehub tray.
void LogPermissionOnboardingPromoShown(PermissionsOnboardingSetUpMode mode);

// Log user action in multidevice permissions set up onboarding dialog.
void LogPermissionOnboardingPromoAction(PermissionsOnboardingScreenEvent event);

// Log click [Setup] in multidevice settings page for setting up multidevice
// permissions.
void LogPermissionOnboardingSettingsClicked(
    PermissionsOnboardingSetUpMode mode);

// Log user action in multidevice permissions set up dialog.
void LogPermissionOnboardingDialogAction(
    PermissionsOnboardingStep step,
    PermissionsOnboardingScreenEvent event);

// Log setup mode when multidevice permissions set up dialog is displayed.
void LogPermissionOnboardingSetupMode(PermissionsOnboardingSetUpMode mode);

// Log setup result when multidevice permissions set up dialog is finished.
void LogPermissionOnboardingSetupResult(PermissionsOnboardingSetUpMode mode);

// Logs a given |entry_point| for MultiDevice setup dialog.
void LogMultiDeviceSetupDialogEntryPoint(
    MultiDeviceSetupDialogEntrypoint entry_point);

}  // namespace util
}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_UTIL_HISTOGRAM_UTIL_H_
