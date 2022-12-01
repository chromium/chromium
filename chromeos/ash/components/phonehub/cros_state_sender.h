// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_CROS_STATE_SENDER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_CROS_STATE_SENDER_H_

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/connection_manager.h"

namespace ash {
namespace phonehub {

class MessageSender;
class PhoneModel;

// Responsible for sending the Chrome OS's device state to the user's
// phone.
class CrosStateSender
    : public secure_channel::ConnectionManager::Observer,
      public multidevice_setup::MultiDeviceSetupClient::Observer {
 public:
  CrosStateSender(
      MessageSender* message_sender,
      secure_channel::ConnectionManager* connection_manager,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      PhoneModel* phone_model);
  ~CrosStateSender() override;

 private:
  friend class CrosStateSenderTest;

  CrosStateSender(
      MessageSender* message_sender,
      secure_channel::ConnectionManager* connection_manager,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      PhoneModel* phone_model,
      std::unique_ptr<base::OneShotTimer> timer);

  void AttemptUpdateCrosState();

  // secure_channel::ConnectionManager::Observer:
  void OnConnectionStatusChanged() override;

  // MultiDeviceSetupClient::Observer:
  void OnFeatureStatesChanged(
      const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
          feature_states_map) override;

  // Sends the cros state to the phone, and initiates a retry after
  // |retry_delay_| if the message was not successfully sent.
  void PerformUpdateCrosState();
  void OnRetryTimerFired();

  MessageSender* message_sender_;
  secure_channel::ConnectionManager* connection_manager_;
  multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client_;
  PhoneModel* phone_model_;
  std::unique_ptr<base::OneShotTimer> retry_timer_;
  base::TimeDelta retry_delay_;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_CROS_STATE_SENDER_H_
