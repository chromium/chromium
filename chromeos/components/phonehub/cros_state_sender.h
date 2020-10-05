// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_CROS_STATE_SENDER_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_CROS_STATE_SENDER_H_

#include "chromeos/components/phonehub/connection_manager.h"
#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client.h"

namespace chromeos {
namespace phonehub {

class MessageSender;

// Responsible for sending the Chrome OS's device state to the user's
// phone.
class CrosStateSender
    : public ConnectionManager::Observer,
      public multidevice_setup::MultiDeviceSetupClient::Observer {
 public:
  CrosStateSender(
      MessageSender* message_sender,
      ConnectionManager* connection_manager,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client);
  ~CrosStateSender() override;

 private:
  void AttemptUpdateCrosState() const;

  // ConnectionManager::Observer:
  void OnConnectionStatusChanged() override;

  // MultiDeviceSetupClient::Observer:
  void OnFeatureStatesChanged(
      const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
          feature_states_map) override;

  MessageSender* message_sender_;
  ConnectionManager* connection_manager_;
  multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client_;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_CROS_STATE_SENDER_H_
