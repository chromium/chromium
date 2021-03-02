// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_ECHE_APP_UI_ECHE_CONNECTOR_H_
#define CHROMEOS_COMPONENTS_ECHE_APP_UI_ECHE_CONNECTOR_H_

#include "chromeos/components/eche_app_ui/eche_feature_status_provider.h"
#include "chromeos/components/eche_app_ui/feature_status_provider.h"

namespace chromeos {
namespace secure_channel {
class ConnectionManager;
}  // namespace secure_channel
namespace eche_app {

// Connects to target device when a message is made available to send (queuing
// messages if the connection is not yet ready), and disconnects (dropping all
// pending messages) when requested.
class EcheConnector : public FeatureStatusProvider::Observer {
 public:
  EcheConnector(EcheFeatureStatusProvider* eche_feature_status_provider,
                secure_channel::ConnectionManager* connection_manager);
  ~EcheConnector() override;

  void SendMessage(const std::string& message);
  void Disconnect();

 private:
  // FeatureStatusProvider::Observer:
  void OnFeatureStatusChanged() override;

  void FlushQueue();

  EcheFeatureStatusProvider* eche_feature_status_provider_;
  secure_channel::ConnectionManager* connection_manager_;
  base::queue<std::string> queue_;
};

}  // namespace eche_app
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_ECHE_APP_UI_ECHE_CONNECTOR_H_
