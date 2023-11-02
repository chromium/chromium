// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_ARC_FAKE_ARC_CAMERA_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_ARC_FAKE_ARC_CAMERA_CLIENT_H_

#include "chromeos/ash/components/dbus/arc/arc_camera_client.h"

namespace ash {

// Fake implementation of ArcCameraClient.
class COMPONENT_EXPORT(ASH_DBUS_ARC) FakeArcCameraClient
    : public ArcCameraClient {
 public:
  // Returns the fake global instance if initialized. May return null.
  static FakeArcCameraClient* Get();

  FakeArcCameraClient(const FakeArcCameraClient&) = delete;
  FakeArcCameraClient& operator=(const FakeArcCameraClient&) = delete;

  // ArcCameraClient override:
  void StartService(int fd,
                    const std::string& token,
                    chromeos::VoidDBusMethodCallback callback) override;

 protected:
  friend class ArcCameraClient;

  FakeArcCameraClient();
  ~FakeArcCameraClient() override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_ARC_FAKE_ARC_CAMERA_CLIENT_H_
