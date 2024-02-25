// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_MESSAGE_SENDER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_MESSAGE_SENDER_IMPL_H_

#include "chromeos/ash/components/phonehub/message_sender.h"

#include <stdint.h>
#include <string>
#include "base/memory/raw_ptr.h"

#include "chromeos/ash/components/phonehub/phone_hub_structured_metrics_logger.h"
#include "chromeos/ash/components/phonehub/phone_hub_ui_readiness_recorder.h"
#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"

namespace ash {

namespace secure_channel {
class ConnectionManager;
}

namespace phonehub {

class MessageSenderImpl : public MessageSender {
 public:
  MessageSenderImpl(
      secure_channel::ConnectionManager* connection_manager,
      PhoneHubUiReadinessRecorder* phone_hub_ui_readiness_recorder,
      PhoneHubStructuredMetricsLogger* phone_hub_structured_metrics_logger);
  ~MessageSenderImpl() override;

  // MessageSender:
  void SendCrosState(
      bool notification_setting_enabled,
      bool camera_roll_setting_enabled,
      const std::vector<std::string>* attestation_certs) override;
  void SendUpdateNotificationModeRequest(bool do_not_disturb_enabled) override;
  void SendUpdateBatteryModeRequest(bool battery_saver_mode_enabled) override;
  void SendDismissNotificationRequest(int64_t notification_id) override;
  void SendNotificationInlineReplyRequest(
      int64_t notification_id,
      const std::u16string& reply_text) override;
  void SendShowNotificationAccessSetupRequest() override;
  void SendFeatureSetupRequest(bool camera_roll, bool notifications) override;
  void SendRingDeviceRequest(bool device_ringing_enabled) override;
  void SendFetchCameraRollItemsRequest(
      const proto::FetchCameraRollItemsRequest& request) override;
  void SendFetchCameraRollItemDataRequest(
      const proto::FetchCameraRollItemDataRequest& request) override;
  void SendInitiateCameraRollItemTransferRequest(
      const proto::InitiateCameraRollItemTransferRequest& request) override;
  void SendPingRequest(const proto::PingRequest& request) override;

 private:
  void SendMessage(proto::MessageType message_type,
                   const google::protobuf::MessageLite* request);

  raw_ptr<secure_channel::ConnectionManager> connection_manager_;
  raw_ptr<PhoneHubUiReadinessRecorder> phone_hub_ui_readiness_recorder_;
  raw_ptr<PhoneHubStructuredMetricsLogger> phone_hub_structured_metrics_logger_;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_MESSAGE_SENDER_IMPL_H_
