// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_MESSAGE_SENDER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_MESSAGE_SENDER_H_

#include <stdint.h>
#include <string>

#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"

namespace ash {
namespace phonehub {

// Provides interface to send messages from the local device (this Chrome OS
// device) to the remote device (user phone).
class MessageSender {
 public:
  MessageSender(const MessageSender&) = delete;
  MessageSender& operator=(const MessageSender&) = delete;
  virtual ~MessageSender() = default;

  // Sends whether the notification setting is enabled in the Chrome OS device.
  virtual void SendCrosState(
      bool notification_setting_enabled,
      bool camera_roll_setting_enabled,
      const std::vector<std::string>* attestation_certs) = 0;

  // Requests that the phone enables or disables Do Not Disturb mode.
  virtual void SendUpdateNotificationModeRequest(
      bool do_not_disturb_enabled) = 0;

  // Requests that the phone enables or disables battery power saver mode.
  virtual void SendUpdateBatteryModeRequest(
      bool battery_saver_mode_enabled) = 0;

  // Requests that the phone should dismiss a notification based by the
  // |notification_id|.
  virtual void SendDismissNotificationRequest(int64_t notification_id) = 0;

  // Requests that the phone should send |reply_text| to a notification of
  // |notification_id|.
  virtual void SendNotificationInlineReplyRequest(
      int64_t notification_id,
      const std::u16string& reply_text) = 0;

  // Requests that the phone should show the notification access set up.
  virtual void SendShowNotificationAccessSetupRequest() = 0;

  // Requests that the phone should show the feature access set up.
  virtual void SendFeatureSetupRequest(bool camera_roll,
                                       bool notifications) = 0;

  // Requests that the phone enables or disables ringing.
  virtual void SendRingDeviceRequest(bool device_ringing_enabled) = 0;

  // Sends a request to fetch the latest set of camera roll items from the
  // connected Android phone.
  virtual void SendFetchCameraRollItemsRequest(
      const proto::FetchCameraRollItemsRequest& request) = 0;

  // Sends a request to let the connected Android phone prepare for a
  // full-quality file transfer of a photo or video item from camera roll.
  virtual void SendFetchCameraRollItemDataRequest(
      const proto::FetchCameraRollItemDataRequest& request) = 0;

  // Sends a request to let the connected Android phone start the file transfer
  // of the requested camera roll item.
  virtual void SendInitiateCameraRollItemTransferRequest(
      const proto::InitiateCameraRollItemTransferRequest& request) = 0;

  virtual void SendPingRequest(const proto::PingRequest& request) = 0;

 protected:
  MessageSender() = default;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_MESSAGE_SENDER_H_
