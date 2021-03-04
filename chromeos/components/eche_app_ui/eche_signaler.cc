// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/eche_app_ui/eche_signaler.h"

namespace chromeos {
namespace eche_app {

EcheSignaler::EcheSignaler(
    EcheConnector* eche_connector,
    secure_channel::ConnectionManager* connection_manager)
    : eche_connector_(eche_connector), connection_manager_(connection_manager) {
  connection_manager_->AddObserver(this);
}

EcheSignaler::~EcheSignaler() {
  connection_manager_->RemoveObserver(this);
}

void EcheSignaler::SendSignalingMessage(const std::vector<uint8_t>& signal) {
  std::string encoded_signal(signal.begin(), signal.end());
  eche_connector_->SendMessage(encoded_signal);
}

void EcheSignaler::SetSignalingMessageObserver(
    mojo::PendingRemote<mojom::SignalingMessageObserver> observer) {
  observer_.Bind(std::move(observer));
}

void EcheSignaler::Bind(
    mojo::PendingReceiver<mojom::SignalingMessageExchanger> receiver) {
  exchanger_.Bind(std::move(receiver));
}

void EcheSignaler::OnMessageReceived(const std::string& payload) {
  std::vector<uint8_t> encoded_payload(payload.begin(), payload.end());
  if (observer_)
    std::move(observer_)->OnReceivedSignalingMessage(encoded_payload);
}

}  // namespace eche_app
}  // namespace chromeos
