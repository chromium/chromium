// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_MESSAGE_SENDER_IMPL_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_MESSAGE_SENDER_IMPL_H_

#include "chromeos/components/phonehub/message_sender.h"

#include <stdint.h>
#include <string>

#include "base/strings/string16.h"

namespace chromeos {
namespace phonehub {

class ConnectionManager;

class MessageSenderImpl : public MessageSender {
 public:
  MessageSenderImpl(ConnectionManager* connection_manager);
  ~MessageSenderImpl() override;

  // MessageSender:
  void SendCrosState(bool notification_setting_enabled) override;
  void SendUpdateNotificationModeRequest(bool do_not_disturb_enabled) override;
  void SendUpdateBatteryModeRequest(bool battery_saver_mode_enabled) override;
  void SendDismissNotificationRequest(int64_t notification_id) override;
  void SendNotificationInlineReplyRequest(
      int64_t notification_id,
      const base::string16& reply_text) override;
  void SendShowNotificationAccessSetupRequest() override;
  void SendRingDeviceRequest(bool device_ringing_enabled) override;

 private:
  ConnectionManager* connection_manager_;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_MESSAGE_SENDER_IMPL_H_