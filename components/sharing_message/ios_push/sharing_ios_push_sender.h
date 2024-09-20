// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARING_MESSAGE_IOS_PUSH_SHARING_IOS_PUSH_SENDER_H_
#define COMPONENTS_SHARING_MESSAGE_IOS_PUSH_SHARING_IOS_PUSH_SENDER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/sharing_message_sender.h"
#include "components/sharing_message/sharing_send_message_result.h"
#include "components/sync/service/sync_service.h"
#include "components/sync_device_info/device_info.h"

namespace syncer {
class DeviceInfoTracker;
class LocalDeviceInfoProvider;
}  // namespace syncer

namespace sync_pb {
class SharingMessageCommitError;
}

namespace components_sharing_message {
class SharingMessage;
}

enum class SharingChannelType;
class SharingMessageBridge;

namespace sharing_message {

// Responsible for sending iOS Push messages within Sharing infrastructure.
class SharingIOSPushSender : public SharingMessageSender::SendMessageDelegate {
 public:
  using SharingMessage = components_sharing_message::SharingMessage;
  using SendMessageCallback =
      base::OnceCallback<void(SharingSendMessageResult result,
                              std::optional<std::string> message_id,
                              SharingChannelType channel_type)>;

  SharingIOSPushSender(
      SharingMessageBridge* sharing_message_bridge,
      const syncer::DeviceInfoTracker* device_info_tracker,
      const syncer::LocalDeviceInfoProvider* local_device_info_provider,
      const syncer::SyncService* sync_service);
  SharingIOSPushSender(const SharingIOSPushSender&) = delete;
  SharingIOSPushSender& operator=(const SharingIOSPushSender&) = delete;
  ~SharingIOSPushSender() override;

 protected:
  // SharingMessageSender::SendMessageDelegate:
  void DoSendMessageToDevice(const SharingTargetDeviceInfo& device,
                             base::TimeDelta time_to_live,
                             components_sharing_message::SharingMessage message,
                             SendMessageCallback callback) override {}
  void DoSendUnencryptedMessageToDevice(
      const SharingTargetDeviceInfo& device,
      sync_pb::UnencryptedSharingMessage message,
      SendMessageCallback callback) override;

  bool CanSendSendTabPushMessage(const syncer::DeviceInfo& target_device_info);

  void OnMessageSent(SendMessageCallback callback,
                     const std::string& message_id,
                     SharingChannelType channel_type,
                     const sync_pb::SharingMessageCommitError& error);

 private:
  const raw_ptr<SharingMessageBridge> sharing_message_bridge_;
  const raw_ptr<const syncer::DeviceInfoTracker> device_info_tracker_;
  const raw_ptr<const syncer::LocalDeviceInfoProvider>
      local_device_info_provider_;
  const raw_ptr<const syncer::SyncService> sync_service_;

  base::WeakPtrFactory<SharingIOSPushSender> weak_ptr_factory_{this};
};

}  // namespace sharing_message

#endif  // COMPONENTS_SHARING_MESSAGE_IOS_PUSH_SHARING_IOS_PUSH_SENDER_H_
