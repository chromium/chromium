// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_ARC_FAKE_ARC_SENSOR_SERVICE_CLIENT_H_
#define CHROMEOS_DBUS_ARC_FAKE_ARC_SENSOR_SERVICE_CLIENT_H_

#include "chromeos/dbus/arc/arc_sensor_service_client.h"

namespace chromeos {

// Fake implementation of ArcSensorServiceClient.
class COMPONENT_EXPORT(CHROMEOS_DBUS_ARC) FakeArcSensorServiceClient
    : public ArcSensorServiceClient {
 public:
  // Returns the fake global instance if initialized. May return null.
  static FakeArcSensorServiceClient* Get();

  // ArcSensorServiceClient override:
  void BootstrapMojoConnection(int fd,
                               const std::string& token,
                               VoidDBusMethodCallback callback) override;

 protected:
  friend class ArcSensorServiceClient;

  FakeArcSensorServiceClient();
  ~FakeArcSensorServiceClient() override;
  FakeArcSensorServiceClient(const FakeArcSensorServiceClient&) = delete;
  FakeArcSensorServiceClient& operator=(const FakeArcSensorServiceClient&) =
      delete;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_ARC_FAKE_ARC_SENSOR_SERVICE_CLIENT_H_
