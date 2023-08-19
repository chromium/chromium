// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/util/histogram_util.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"

namespace ash {
namespace phonehub {
namespace util {

namespace {

std::string GetMessageResultHistogramName(proto::MessageType message_type) {
  switch (message_type) {
    case proto::MessageType::DISMISS_NOTIFICATION_REQUEST:
      [[fallthrough]];
    case proto::MessageType::DISMISS_NOTIFICATION_RESPONSE:
      return "PhoneHub.TaskCompletion.NotificationDismissal.Result";

    case proto::MessageType::NOTIFICATION_INLINE_REPLY_REQUEST:
      [[fallthrough]];
    case proto::MessageType::NOTIFICATION_INLINE_REPLY_RESPONSE:
      return "PhoneHub.TaskCompletion.NotificationInlineReply.Result";

    case proto::MessageType::UPDATE_NOTIFICATION_MODE_REQUEST:
      [[fallthrough]];
    case proto::MessageType::UPDATE_NOTIFICATION_MODE_RESPONSE:
      return "PhoneHub.TaskCompletion.SilencePhone.Result";

    case proto::MessageType::RING_DEVICE_REQUEST:
      [[fallthrough]];
    case proto::MessageType::RING_DEVICE_RESPONSE:
      return "PhoneHub.TaskCompletion.LocatePhone.Result";

    case proto::MessageType::SHOW_NOTIFICATION_ACCESS_SETUP_REQUEST:
      [[fallthrough]];
    case proto::MessageType::SHOW_NOTIFICATION_ACCESS_SETUP_RESPONSE:
      return "PhoneHub.TaskCompletion.ShowNotificationAccessSetup.Result";

    case proto::MessageType::UPDATE_BATTERY_MODE_REQUEST:
      [[fallthrough]];
    case proto::MessageType::UPDATE_BATTERY_MODE_RESPONSE:
      return "PhoneHub.TaskCompletion.UpdateBatteryMode.Result";

    case proto::MessageType::FETCH_CAMERA_ROLL_ITEMS_REQUEST:
      [[fallthrough]];
    case proto::MessageType::FETCH_CAMERA_ROLL_ITEMS_RESPONSE:
      return "PhoneHub.TaskCompletion.FetchCameraRollItems.Result";

    case proto::MessageType::FETCH_CAMERA_ROLL_ITEM_DATA_REQUEST:
      [[fallthrough]];
    case proto::MessageType::FETCH_CAMERA_ROLL_ITEM_DATA_RESPONSE:
      return "PhoneHub.TaskCompletion.FetchCameraRollItemData.Result";

    case proto::MessageType::INITIATE_CAMERA_ROLL_ITEM_TRANSFER_REQUEST:
      return "PhoneHub.TaskCompletion.InitiateCameraRollItemTransfer.Result";

    case proto::MessageType::FEATURE_SETUP_REQUEST:
      [[fallthrough]];
    case proto::MessageType::FEATURE_SETUP_RESPONSE:
      return "PhoneHub.TaskCompletion.FeatureSetup.Result";

    case proto::MessageType::APP_STREAM_UPDATE:
      return "PhoneHub.TaskCompletion.AppStreamUpdate.Result";

    default:
      // Note that PROVIDE_CROS_STATE, PHONE_STATUS_SNAPSHOT and
      // PHONE_STATUS_UPDATE message types are not logged as part of these
      // metrics.
      return std::string();
  }
}

}  // namespace

void LogFeatureOptInEntryPoint(OptInEntryPoint entry_point) {
  base::UmaHistogramEnumeration("PhoneHub.OptInEntryPoint", entry_point);
}

void LogCameraRollFeatureOptInEntryPoint(
    CameraRollOptInEntryPoint entry_point) {
  base::UmaHistogramEnumeration("PhoneHub.CameraRoll.OptInEntryPoint",
                                entry_point);
}

void LogTetherConnectionResult(TetherConnectionResult result) {
  base::UmaHistogramEnumeration(
      "PhoneHub.TaskCompletion.TetherConnection.Result", result);
}

void LogMessageResult(proto::MessageType message_type,
                      PhoneHubMessageResult result) {
  const std::string histogram_name =
      GetMessageResultHistogramName(message_type);
  if (!histogram_name.empty())
    base::UmaHistogramEnumeration(histogram_name, result);
}

void LogCameraRollDownloadResult(CameraRollDownloadResult result) {
  base::UmaHistogramEnumeration("PhoneHub.CameraRoll.DownloadItem.Result",
                                result);
}

void LogCameraRollAndroidHasStorageAccessPermission(bool has_permission) {
  base::UmaHistogramBoolean("PhoneHub.CameraRoll.AndroidHasStoragePermission",
                            has_permission);
}

void LogPermissionOnboardingPromoShown(PermissionsOnboardingSetUpMode mode) {
  base::UmaHistogramEnumeration(
      "PhoneHub.PermissionsOnboarding.SetUpMode.OnPromoShown", mode);
}

void LogPermissionOnboardingPromoAction(
    PermissionsOnboardingScreenEvent event) {
  base::UmaHistogramEnumeration(
      "PhoneHub.PermissionsOnboarding.DialogScreenEvents.PromoScreen", event);
}

void LogPermissionOnboardingSettingsClicked(
    PermissionsOnboardingSetUpMode mode) {
  base::UmaHistogramEnumeration(
      "PhoneHub.PermissionsOnboarding.SetUpMode.OnSettingsClicked", mode);
}

void LogPermissionOnboardingDialogAction(
    PermissionsOnboardingStep step,
    PermissionsOnboardingScreenEvent event) {
  switch (step) {
    case PermissionsOnboardingStep::kDialogIntroAction:
      base::UmaHistogramEnumeration(
          "PhoneHub.PermissionsOnboarding.DialogScreenEvents.IntroScreen",
          event);
      break;
    case PermissionsOnboardingStep::kDialogFinishOnPhoneAction:
      base::UmaHistogramEnumeration(
          "PhoneHub.PermissionsOnboarding.DialogScreenEvents."
          "FinishSetupOnYourPhoneScreen",
          event);
      break;
    case PermissionsOnboardingStep::kDialogConnectingAction:
      base::UmaHistogramEnumeration(
          "PhoneHub.PermissionsOnboarding.DialogScreenEvents"
          ".ConnectingToPhoneScreen",
          event);
      break;
    case PermissionsOnboardingStep::kDialogConnectionErrorAction:
      base::UmaHistogramEnumeration(
          "PhoneHub.PermissionsOnboarding.DialogScreenEvents"
          ".CouldNotEstablishConnectionScreen",
          event);
      break;
    case PermissionsOnboardingStep::kDialogConnectionTimeOutAction:
      base::UmaHistogramEnumeration(
          "PhoneHub.PermissionsOnboarding.DialogScreenEvents"
          ".ConnectionLostScreen",
          event);
      break;
    case PermissionsOnboardingStep::kDialogSetupFinished:
      base::UmaHistogramEnumeration(
          "PhoneHub.PermissionsOnboarding.DialogScreenEvents"
          ".SetUpFinishedScreen",
          event);
      break;
    case PermissionsOnboardingStep::kDialogSetAPinOrPassword:
      base::UmaHistogramEnumeration(
          "PhoneHub.PermissionsOnboarding.DialogScreenEvents"
          ".SetAPinOrPasswordScreen",
          event);
      break;
    case PermissionsOnboardingStep::kUnknown:
      PA_LOG(ERROR) << "Tried to emit event on invalid"
                    << " permissions onboarding dialog screen.";
      break;
  }
}

void LogPermissionOnboardingSetupMode(PermissionsOnboardingSetUpMode mode) {
  base::UmaHistogramEnumeration(
      "PhoneHub.PermissionsOnboarding.SetUpMode.IntroScreenShown", mode);
}

void LogPermissionOnboardingSetupResult(PermissionsOnboardingSetUpMode mode) {
  base::UmaHistogramEnumeration(
      "PhoneHub.PermissionsOnboarding.SetUpMode.SetUpFinishedScreenShown",
      mode);
}

void LogMultiDeviceSetupDialogEntryPoint(
    MultiDeviceSetupDialogEntrypoint entry_point) {
  base::UmaHistogramEnumeration("MultiDeviceSetup.SetupDialogEntryPoint",
                                entry_point);
}

}  // namespace util
}  // namespace phonehub
}  // namespace ash
