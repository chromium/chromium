// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_ECHE_APP_UI_ECHE_MESSAGE_RECEIVER_H_
#define CHROMEOS_COMPONENTS_ECHE_APP_UI_ECHE_MESSAGE_RECEIVER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/components/eche_app_ui/eche_connector.h"
#include "chromeos/components/eche_app_ui/proto/exo_messages.pb.h"
#include "chromeos/services/secure_channel/public/cpp/client/connection_manager.h"

namespace chromeos {
namespace eche_app {

class EcheMessageReceiver : public secure_channel::ConnectionManager::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Called when the apps access state response sent by the remote phone.
    virtual void onGetAppsAccessStateResponseReceived(
        proto::GetAppsAccessStateResponse apps_access_state_response) {}

    // Called when the apps setup response sent by the remote phone.
    virtual void onSendAppsSetupResponseReceived(
        proto::SendAppsSetupResponse apps_setup_response) {}
  };

  EcheMessageReceiver(secure_channel::ConnectionManager* connection_manager);
  ~EcheMessageReceiver() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  void NotifyGetAppsAccessStateResponse(
      proto::GetAppsAccessStateResponse apps_access_state_response);
  void NotifySendAppsSetupResponse(
      proto::SendAppsSetupResponse apps_setup_response);
  // secure_channel::ConnectionManager::Observer:
  void OnMessageReceived(const std::string& payload) override;
  secure_channel::ConnectionManager* connection_manager_;

  base::ObserverList<Observer> observer_list_;
};

}  // namespace eche_app
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_ECHE_APP_UI_ECHE_MESSAGE_RECEIVER_H
