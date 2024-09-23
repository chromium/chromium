// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARING_MESSAGE_SHARING_FCM_HANDLER_H_
#define COMPONENTS_SHARING_MESSAGE_SHARING_FCM_HANDLER_H_

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/gcm_driver/gcm_app_handler.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/sharing_send_message_result.h"
#include "components/sync_device_info/device_info.h"

namespace gcm {
class GCMDriver;
}  // namespace gcm

namespace syncer {
class DeviceInfoTracker;
}  // namespace syncer

namespace sharing_message {
enum MessageType : int;
}  // namespace sharing_message

enum class SharingChannelType;
class SharingFCMSender;
class SharingHandlerRegistry;

enum class SharingDevicePlatform;

// SharingFCMHandler is responsible for receiving SharingMessage from GCMDriver
// and delegate it to the payload specific handler.
class SharingFCMHandler : public gcm::GCMAppHandler {
 public:
  SharingFCMHandler(gcm::GCMDriver* gcm_driver,
                    syncer::DeviceInfoTracker* device_info_tracker,
                    SharingFCMSender* sharing_fcm_sender,
                    SharingHandlerRegistry* handler_registry);

  SharingFCMHandler(const SharingFCMHandler&) = delete;
  SharingFCMHandler& operator=(const SharingFCMHandler&) = delete;

  ~SharingFCMHandler() override;

  // Registers itself as app handler for sharing messages.
  virtual void StartListening();

  // Unregisters itself as app handler for sharing messages.
  virtual void StopListening();

  // GCMAppHandler overrides.
  void ShutdownHandler() override;
  void OnStoreReset() override;

  // Responsible for delegating the message to the registered
  // SharingMessageHandler. Also sends back an ack to original sender after
  // delegating the message.
  void OnMessage(const std::string& app_id,
                 const gcm::IncomingMessage& message) override;

  void OnSendError(const std::string& app_id,
                   const gcm::GCMClient::SendErrorDetails& details) override;

  void OnSendAcknowledged(const std::string& app_id,
                          const std::string& message_id) override;

  void OnMessagesDeleted(const std::string& app_id) override;

 private:
  std::optional<components_sharing_message::FCMChannelConfiguration>
  GetFCMChannel(
      const components_sharing_message::SharingMessage& original_message);

  std::optional<components_sharing_message::ServerChannelConfiguration>
  GetServerChannel(
      const components_sharing_message::SharingMessage& original_message);

  SharingDevicePlatform GetSenderPlatform(
      const components_sharing_message::SharingMessage& original_message);

  // Ack message sent back to the original sender of message.
  void SendAckMessage(
      std::string original_message_id,
      sharing_message::MessageType original_message_type,
      std::optional<components_sharing_message::FCMChannelConfiguration>
          fcm_channel,
      std::optional<components_sharing_message::ServerChannelConfiguration>
          server_channel,
      SharingDevicePlatform sender_device_type,
      std::unique_ptr<components_sharing_message::ResponseMessage> response);

  void OnAckMessageSent(std::string original_message_id,
                        sharing_message::MessageType original_message_type,
                        SharingDevicePlatform sender_device_type,
                        int trace_id,
                        SharingSendMessageResult result,
                        std::optional<std::string> message_id,
                        SharingChannelType channel_type);

  const raw_ptr<gcm::GCMDriver, AcrossTasksDanglingUntriaged> gcm_driver_;
  raw_ptr<syncer::DeviceInfoTracker, DanglingUntriaged> device_info_tracker_;
  raw_ptr<SharingFCMSender, DanglingUntriaged> sharing_fcm_sender_;
  raw_ptr<SharingHandlerRegistry, DanglingUntriaged> handler_registry_;

  bool is_listening_ = false;

  base::WeakPtrFactory<SharingFCMHandler> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_SHARING_MESSAGE_SHARING_FCM_HANDLER_H_
