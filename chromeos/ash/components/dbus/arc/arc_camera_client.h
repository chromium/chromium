// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_ARC_ARC_CAMERA_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_ARC_ARC_CAMERA_CLIENT_H_

#include <string>

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "chromeos/dbus/common/dbus_callback.h"
#include "chromeos/dbus/common/dbus_client.h"

namespace ash {

// ArcCameraClient is used to communicate with the arc-camera service for ARC
// Camera HAL v1.
class COMPONENT_EXPORT(ASH_DBUS_ARC) ArcCameraClient {
 public:
  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance which must have been initialized.
  static void Shutdown();

  // Returns the global instance if initialized. May return null.
  static ArcCameraClient* Get();

  ArcCameraClient(const ArcCameraClient&) = delete;
  ArcCameraClient& operator=(const ArcCameraClient&) = delete;

  // Starts a new service process and establishes Mojo connection with the given
  // token and FD.
  virtual void StartService(int fd,
                            const std::string& token,
                            chromeos::VoidDBusMethodCallback callback) = 0;

 protected:
  // Initialize() should be used instead.
  ArcCameraClient();
  virtual ~ArcCameraClient();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_ARC_ARC_CAMERA_CLIENT_H_
