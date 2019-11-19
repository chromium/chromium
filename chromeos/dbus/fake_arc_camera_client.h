// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_ARC_CAMERA_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_ARC_CAMERA_CLIENT_H_

#include "chromeos/dbus/arc_camera_client.h"

namespace chromeos {

// Fake implementation of ArcCameraClient.
class COMPONENT_EXPORT(CHROMEOS_DBUS) FakeArcCameraClient
    : public ArcCameraClient {
 public:
  // Returns the fake global instance if initialized. May return null.
  static FakeArcCameraClient* Get();

  // ArcCameraClient override:
  void StartService(int fd,
                    const std::string& token,
                    VoidDBusMethodCallback callback) override;

 protected:
  friend class ArcCameraClient;

  FakeArcCameraClient();
  ~FakeArcCameraClient() override;

  DISALLOW_COPY_AND_ASSIGN(FakeArcCameraClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_ARC_CAMERA_CLIENT_H_
