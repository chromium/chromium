// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_ARC_FAKE_ARC_SENSOR_SERVICE_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_ARC_FAKE_ARC_SENSOR_SERVICE_CLIENT_H_

#include "chromeos/ash/components/dbus/arc/arc_sensor_service_client.h"

namespace ash {

// Fake implementation of ArcSensorServiceClient.
class COMPONENT_EXPORT(ASH_DBUS_ARC) FakeArcSensorServiceClient
    : public ArcSensorServiceClient {
 public:
  // Returns the fake global instance if initialized. May return null.
  static FakeArcSensorServiceClient* Get();

  // ArcSensorServiceClient override:
  void BootstrapMojoConnection(
      int fd,
      const std::string& token,
      chromeos::VoidDBusMethodCallback callback) override;

 protected:
  friend class ArcSensorServiceClient;

  FakeArcSensorServiceClient();
  ~FakeArcSensorServiceClient() override;
  FakeArcSensorServiceClient(const FakeArcSensorServiceClient&) = delete;
  FakeArcSensorServiceClient& operator=(const FakeArcSensorServiceClient&) =
      delete;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_ARC_FAKE_ARC_SENSOR_SERVICE_CLIENT_H_
