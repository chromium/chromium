// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/eche_app_ui/eche_signaler.h"

#include "chromeos/components/eche_app_ui/proto/exo_messages.pb.h"

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
  proto::SignalingRequest request;
  request.set_data(encoded_signal);
  proto::ExoMessage message;
  *message.mutable_request() = std::move(request);
  eche_connector_->SendMessage(message.SerializeAsString());
}

void EcheSignaler::SetSignalingMessageObserver(
    mojo::PendingRemote<mojom::SignalingMessageObserver> observer) {
  observer_.reset();
  observer_.Bind(std::move(observer));
}

void EcheSignaler::TearDownSignaling() {
  proto::SignalingAction action;
  action.set_action_type(proto::ActionType::ACTION_TEAR_DOWN);
  proto::ExoMessage message;
  *message.mutable_action() = std::move(action);
  eche_connector_->SendMessage(message.SerializeAsString());
  eche_connector_->Disconnect();
}

void EcheSignaler::Bind(
    mojo::PendingReceiver<mojom::SignalingMessageExchanger> receiver) {
  exchanger_.reset();
  exchanger_.Bind(std::move(receiver));
}

void EcheSignaler::OnMessageReceived(const std::string& payload) {
  if (!observer_.is_bound())
    return;

  proto::ExoMessage message;
  message.ParseFromString(payload);
  std::string signal;
  if (message.has_request()) {
    signal = message.request().data();
  } else if (message.has_response()) {
    signal = message.response().data();
  } else {
    signal = {};
  }
  std::vector<uint8_t> encoded_signal(signal.begin(), signal.end());
  observer_->OnReceivedSignalingMessage(encoded_signal);
}

}  // namespace eche_app
}  // namespace chromeos
