// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_ARC_CAMERA_CLIENT_H_
#define CHROMEOS_DBUS_ARC_CAMERA_CLIENT_H_

#include <string>

#include "base/component_export.h"
#include "base/macros.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"

#include "base/files/scoped_file.h"

namespace chromeos {

// ArcCameraClient is used to communicate with the arc-camera service for ARC
// Camera HAL v1.
class COMPONENT_EXPORT(CHROMEOS_DBUS) ArcCameraClient {
 public:
  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance which must have been initialized.
  static void Shutdown();

  // Returns the global instance if initialized. May return null.
  static ArcCameraClient* Get();

  // Starts a new service process and establishes Mojo connection with the given
  // token and FD.
  virtual void StartService(int fd,
                            const std::string& token,
                            VoidDBusMethodCallback callback) = 0;

 protected:
  // Initialize() should be used instead.
  ArcCameraClient();
  virtual ~ArcCameraClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcCameraClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_ARC_CAMERA_CLIENT_H_
