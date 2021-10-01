// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/eche_app_ui/eche_message_receiver.h"

#include "chromeos/components/eche_app_ui/proto/exo_messages.pb.h"
#include "chromeos/components/multidevice/logging/logging.h"

namespace chromeos {
namespace eche_app {
EcheMessageReceiver::EcheMessageReceiver(
    secure_channel::ConnectionManager* connection_manager)
    : connection_manager_(connection_manager) {
  connection_manager_->AddObserver(this);
}

EcheMessageReceiver::~EcheMessageReceiver() {
  connection_manager_->RemoveObserver(this);
}

void EcheMessageReceiver::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void EcheMessageReceiver::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void EcheMessageReceiver::NotifyGetAppsAccessStateResponse(
    proto::GetAppsAccessStateResponse apps_access_state_response) {
  for (auto& observer : observer_list_)
    observer.onGetAppsAccessStateResponseReceived(apps_access_state_response);
}

void EcheMessageReceiver::NotifySendAppsSetupResponse(
    proto::SendAppsSetupResponse apps_setup_response) {
  for (auto& observer : observer_list_)
    observer.onSendAppsSetupResponseReceived(apps_setup_response);
}

void EcheMessageReceiver::OnMessageReceived(const std::string& payload) {
  proto::ExoMessage message;
  message.ParseFromString(payload);
  if (message.has_apps_access_state_response()) {
    NotifyGetAppsAccessStateResponse(message.apps_access_state_response());
  } else if (message.has_apps_setup_response()) {
    NotifySendAppsSetupResponse(message.apps_setup_response());
  }
}

}  // namespace eche_app
}  // namespace chromeos
