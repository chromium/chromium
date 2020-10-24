// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/cros_state_sender.h"

#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/phonehub/message_sender.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"

namespace chromeos {
namespace phonehub {

using multidevice_setup::mojom::Feature;
using multidevice_setup::mojom::FeatureState;

CrosStateSender::CrosStateSender(
    MessageSender* message_sender,
    ConnectionManager* connection_manager,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client)
    : message_sender_(message_sender),
      connection_manager_(connection_manager),
      multidevice_setup_client_(multidevice_setup_client) {
  DCHECK(message_sender_);
  DCHECK(connection_manager_);
  DCHECK(multidevice_setup_client_);

  connection_manager_->AddObserver(this);
  multidevice_setup_client_->AddObserver(this);
}

CrosStateSender::~CrosStateSender() {
  connection_manager_->RemoveObserver(this);
  multidevice_setup_client_->RemoveObserver(this);
}

void CrosStateSender::AttemptUpdateCrosState() const {
  // Wait for connection to be established.
  if (connection_manager_->GetStatus() !=
      ConnectionManager::Status::kConnected) {
    PA_LOG(INFO) << "Could not start AttemptUpdateCrosState() because "
                 << "connection manager status is: "
                 << connection_manager_->GetStatus();
    return;
  }

  bool are_notifications_enabled =
      multidevice_setup_client_->GetFeatureState(
          Feature::kPhoneHubNotifications) == FeatureState::kEnabledByUser;

  PA_LOG(INFO) << "Attempting to send cros state with notifications enabled "
               << "state as: " << are_notifications_enabled;
  message_sender_->SendCrosState(are_notifications_enabled);
}

void CrosStateSender::OnConnectionStatusChanged() {
  AttemptUpdateCrosState();
}

void CrosStateSender::OnFeatureStatesChanged(
    const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
        feature_states_map) {
  AttemptUpdateCrosState();
}

}  // namespace phonehub
}  // namespace chromeos
