// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_NOTIFICATION_MANAGER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_NOTIFICATION_MANAGER_IMPL_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/phonehub/notification.h"
#include "chromeos/ash/components/phonehub/notification_manager.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"

namespace ash {
namespace phonehub {

class MessageSender;
class UserActionRecorder;

class NotificationManagerImpl
    : public NotificationManager,
      public multidevice_setup::MultiDeviceSetupClient::Observer {
 public:
  NotificationManagerImpl(
      MessageSender* message_sender,
      UserActionRecorder* user_action_recorder,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client);
  ~NotificationManagerImpl() override;

 private:
  // NotificationManager:
  void DismissNotification(int64_t notification_id) override;
  void SendInlineReply(int64_t notification_id,
                       const std::u16string& inline_reply_text) override;

  // MultiDeviceSetupClient::Observer:
  void OnFeatureStatesChanged(
      const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
          feature_states_map) override;

  raw_ptr<MessageSender> message_sender_;
  raw_ptr<UserActionRecorder> user_action_recorder_;
  raw_ptr<multidevice_setup::MultiDeviceSetupClient> multidevice_setup_client_;
  std::optional<multidevice_setup::mojom::FeatureState>
      notifications_feature_status_;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_NOTIFICATION_MANAGER_IMPL_H_
