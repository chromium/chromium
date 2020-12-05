// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/util/histogram_util.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "chromeos/components/phonehub/proto/phonehub_api.pb.h"

namespace chromeos {
namespace phonehub {
namespace util {

namespace {

std::string GetMessageResultHistogramName(proto::MessageType message_type) {
  switch (message_type) {
    case proto::MessageType::DISMISS_NOTIFICATION_REQUEST:
      FALLTHROUGH;
    case proto::MessageType::DISMISS_NOTIFICATION_RESPONSE:
      return "PhoneHub.TaskCompletion.NotificationDismissal.Result";

    case proto::MessageType::NOTIFICATION_INLINE_REPLY_REQUEST:
      FALLTHROUGH;
    case proto::MessageType::NOTIFICATION_INLINE_REPLY_RESPONSE:
      return "PhoneHub.TaskCompletion.NotificationInlineReply.Result";

    case proto::MessageType::UPDATE_NOTIFICATION_MODE_REQUEST:
      FALLTHROUGH;
    case proto::MessageType::UPDATE_NOTIFICATION_MODE_RESPONSE:
      return "PhoneHub.TaskCompletion.SilencePhone.Result";

    case proto::MessageType::RING_DEVICE_REQUEST:
      FALLTHROUGH;
    case proto::MessageType::RING_DEVICE_RESPONSE:
      return "PhoneHub.TaskCompletion.LocatePhone.Result";

    case proto::MessageType::SHOW_NOTIFICATION_ACCESS_SETUP_REQUEST:
      FALLTHROUGH;
    case proto::MessageType::SHOW_NOTIFICATION_ACCESS_SETUP_RESPONSE:
      return "PhoneHub.TaskCompletion.ShowNotificationAccessSetup.Result";

    case proto::MessageType::UPDATE_BATTERY_MODE_REQUEST:
      FALLTHROUGH;
    case proto::MessageType::UPDATE_BATTERY_MODE_RESPONSE:
      return "PhoneHub.TaskCompletion.UpdateBatteryMode.Result";

    default:
      // Note that PROVIDE_CROS_STATE, PHONE_STATUS_SNAPSHOT and
      // PHONE_STATUS_UPDATE message types are not logged as part of this
      // metrics.
      return std::string();
  }
}

}  // namespace

void LogFeatureOptInEntryPoint(OptInEntryPoint entry_point) {
  base::UmaHistogramEnumeration("PhoneHub.OptInEntryPoint", entry_point);
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

}  // namespace util
}  // namespace phonehub
}  // namespace chromeos
