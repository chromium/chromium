// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_PING_MANAGER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_PING_MANAGER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/phonehub/feature_status.h"
#include "chromeos/ash/components/phonehub/feature_status_provider.h"
#include "chromeos/ash/components/phonehub/message_receiver.h"
#include "chromeos/ash/components/phonehub/ping_manager.h"
#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"

namespace ash::secure_channel {
class ConnectionManager;
}

namespace ash::phonehub {

class MessageSender;

class PingManagerImpl : public PingManager,
                        public MessageReceiver::Observer,
                        public FeatureStatusProvider::Observer {
 public:
  PingManagerImpl(secure_channel::ConnectionManager* connection_manager,
                  FeatureStatusProvider* feature_status_provider,
                  MessageReceiver* message_receiver,
                  MessageSender* message_sender);
  ~PingManagerImpl() override;

  // MessageReceiver::Observer:
  void OnPhoneStatusSnapshotReceived(
      proto::PhoneStatusSnapshot phone_status_snapshot) override;
  void OnPhoneStatusUpdateReceived(
      proto::PhoneStatusUpdate phone_status_update) override;
  void OnPingResponseReceived() override;

  // // FeatureStatusProvider::Observer:
  void OnFeatureStatusChanged() override;

  // PingManager:
  void SendPingRequest() override;

  bool IsPingTimeoutTimerRunning();

  // Test Helpers
  bool is_ping_supported_by_phone_for_test() {
    return is_ping_supported_by_phone_;
  }
  bool is_waiting_for_response_for_test() { return is_waiting_for_response_; }
  void set_is_ping_supported_by_phone_for_test(
      bool is_ping_supported_by_phone) {
    is_ping_supported_by_phone_ = is_ping_supported_by_phone;
  }
  void set_is_waiting_for_response_for_test(bool is_waiting_for_response) {
    is_waiting_for_response_ = is_waiting_for_response;
  }

 private:
  friend class PingManagerImplTest;

  // PingManager:
  void Reset() override;

  void OnPingTimerFired();
  void UpdatePhoneSupport(proto::PhoneProperties phone_properties);

  base::OneShotTimer ping_timeout_timer_;
  base::TimeTicks ping_sent_timestamp_;
  raw_ptr<secure_channel::ConnectionManager> connection_manager_;
  raw_ptr<FeatureStatusProvider> feature_status_provider_;
  raw_ptr<MessageReceiver> message_receiver_;
  raw_ptr<MessageSender> message_sender_;
  bool is_ping_supported_by_phone_ = false;
  bool is_waiting_for_response_ = false;
};

}  // namespace ash::phonehub

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_PING_MANAGER_IMPL_H_
