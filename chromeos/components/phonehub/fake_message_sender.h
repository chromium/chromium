// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_FAKE_MESSAGE_SENDER_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_FAKE_MESSAGE_SENDER_H_

#include "chromeos/components/phonehub/message_sender.h"

#include <stdint.h>
#include <string>
#include <vector>

#include "base/strings/string16.h"

namespace chromeos {
namespace phonehub {

class FakeMessageSender : public MessageSender {
 public:
  FakeMessageSender();
  ~FakeMessageSender() override;

  // MessageSender:
  void SendCrosState(bool notification_enabled) override;
  void SendUpdateNotificationModeRequest(bool do_not_disturb_enabled) override;
  void SendUpdateBatteryModeRequest(bool battery_saver_mode_enabled) override;
  void SendDismissNotificationRequest(int64_t notification_id) override;
  void SendNotificationInlineReplyRequest(
      int64_t notification_id,
      const base::string16& reply_text) override;
  void SendShowNotificationAccessSetupRequest() override;
  void SendRingDeviceRequest(bool device_ringing_enabled) override;

  bool GetRecentCrosState() const;
  bool GetRecentUpdateNotificationModeRequest() const;
  bool GetRecentUpdateBatteryModeRequest() const;
  int64_t GetRecentDismissNotificationRequest() const;
  const std::pair<int64_t, base::string16>
  GetRecentNotificationInlineReplyRequest() const;
  bool GetRecentRingDeviceRequest() const;

  size_t GetCrosStateCallCount() const;

  size_t GetUpdateNotificationModeRequestCallCount() const;

  size_t GetUpdateBatteryModeRequestCallCount() const;

  size_t GetDismissNotificationRequestCallCount() const;

  size_t GetNotificationInlineReplyRequestCallCount() const;

  size_t show_notification_access_setup_request_count() const {
    return show_notification_access_setup_count_;
  }

  size_t GetRingDeviceRequestCallCount() const;

 private:
  std::vector<bool> cros_states_;
  std::vector<bool> update_notification_mode_requests_;
  std::vector<bool> update_battery_mode_requests_;
  std::vector<int64_t> dismiss_notification_requests_;
  std::vector<std::pair<int64_t, base::string16>>
      notification_inline_reply_requests_;
  std::vector<bool> ring_device_requests_;
  size_t show_notification_access_setup_count_ = 0;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_FAKE_MESSAGE_SENDER_H_