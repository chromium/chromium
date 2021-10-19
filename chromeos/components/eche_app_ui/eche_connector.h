// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_ECHE_APP_UI_ECHE_CONNECTOR_H_
#define CHROMEOS_COMPONENTS_ECHE_APP_UI_ECHE_CONNECTOR_H_

#include "base/containers/queue.h"
#include "chromeos/components/eche_app_ui/eche_feature_status_provider.h"
#include "chromeos/components/eche_app_ui/feature_status_provider.h"

namespace chromeos {
namespace eche_app {

// Provides interface to connect to target device when a message is made
// available to send (queuing messages if the connection is not yet ready), and
// disconnects (dropping all pending messages) when requested.
class EcheConnector {
 public:
  virtual ~EcheConnector() = default;

  virtual void SendMessage(const std::string& message) = 0;
  virtual void Disconnect() = 0;
  virtual void SendAppsSetupRequest() = 0;
  virtual void GetAppsAccessStateRequest() = 0;
  virtual void AttemptNearbyConnection() = 0;

 protected:
  EcheConnector() = default;

 private:
  friend class FakeEcheConnector;
};

}  // namespace eche_app
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_ECHE_APP_UI_ECHE_CONNECTOR_H_
