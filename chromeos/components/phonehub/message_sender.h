// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_MESSAGE_SENDER_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_MESSAGE_SENDER_H_

#include <stdint.h>
#include <string>

#include "base/strings/string16.h"

namespace chromeos {
namespace phonehub {

// Provides interface to send messages from the local device (this Chrome OS
// device) to the remote device (user phone).
class MessageSender {
 public:
  MessageSender(const MessageSender&) = delete;
  MessageSender& operator=(const MessageSender&) = delete;
  virtual ~MessageSender() = default;

  // Sends whether the notification setting is enabled in the Chrome OS device.
  virtual void SendCrosState(bool notification_setting_enabled) = 0;

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
      const base::string16& reply_text) = 0;

  // Requests that the phone should show the notification access set up.
  virtual void SendShowNotificationAccessSetupRequest() = 0;

  // Requests that the phone enables or disables ringing.
  virtual void SendRingDeviceRequest(bool device_ringing_enabled) = 0;

 protected:
  MessageSender() = default;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_MESSAGE_SENDER_H_