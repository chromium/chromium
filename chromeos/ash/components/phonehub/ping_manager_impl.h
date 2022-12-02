// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_PING_MANAGER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_PING_MANAGER_IMPL_H_

#include "base/timer/timer.h"
#include "chromeos/ash/components/phonehub/message_receiver.h"
#include "chromeos/ash/components/phonehub/ping_manager.h"
#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"

namespace ash::secure_channel {
class ConnectionManager;
}

namespace ash::phonehub {

class MessageSender;

class PingManagerImpl : public PingManager, public MessageReceiver::Observer {
 public:
  PingManagerImpl(secure_channel::ConnectionManager* connection_manager,
                  MessageReceiver* message_receiver,
                  MessageSender* message_sender);
  ~PingManagerImpl() override;

  // MessageReceiver::Observer:
  void OnPhoneStatusSnapshotReceived(
      proto::PhoneStatusSnapshot phone_status_snapshot) override;
  void OnPhoneStatusUpdateReceived(
      proto::PhoneStatusUpdate phone_status_update) override;
  void OnPingResponseReceived() override;

  // PingManager:
  void SendPingRequest() override;

  bool IsPingTimeoutTimerRunning();

  bool is_ping_supported_by_phone() { return is_ping_supported_by_phone_; }
  bool is_waiting_for_response() { return is_waiting_for_response_; }
  void set_is_ping_supported_by_phone(bool is_ping_supported_by_phone) {
    is_ping_supported_by_phone_ = is_ping_supported_by_phone;
  }
  void set_is_waiting_for_response(bool is_waiting_for_response) {
    is_waiting_for_response_ = is_waiting_for_response;
  }

 private:
  friend class PingManagerImplTest;

  void OnPingTimerFired();
  void UpdatePhoneSupport(proto::PhoneProperties phone_properties);

  base::OneShotTimer ping_timeout_timer_;
  base::TimeTicks ping_sent_timestamp_;
  secure_channel::ConnectionManager* connection_manager_;
  MessageReceiver* message_receiver_;
  MessageSender* message_sender_;
  bool is_ping_supported_by_phone_ = false;
  bool is_waiting_for_response_ = false;
};

}  // namespace ash::phonehub

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_PING_MANAGER_IMPL_H_
