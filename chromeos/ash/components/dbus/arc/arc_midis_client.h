// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_ARC_ARC_MIDIS_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_ARC_ARC_MIDIS_CLIENT_H_

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "chromeos/dbus/common/dbus_callback.h"
#include "chromeos/dbus/common/dbus_client.h"

namespace ash {

// ArcMidisClient is used to pass an FD to the midis daemon for the purpose
// of setting up a Mojo channel. It is expected to be called once during browser
// initialization.
class COMPONENT_EXPORT(ASH_DBUS_ARC) ArcMidisClient
    : public chromeos::DBusClient {
 public:
  // Returns the global instance if initialized. May return null.
  static ArcMidisClient* Get();

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance.
  static void InitializeFake();

  // Destroys the global instance if it has been initialized.
  static void Shutdown();

  ArcMidisClient(const ArcMidisClient&) = delete;
  ArcMidisClient& operator=(const ArcMidisClient&) = delete;

  // Bootstrap the Mojo connection between Chrome and the MIDI service.
  // Should pass in the child end of the Mojo pipe.
  virtual void BootstrapMojoConnection(
      base::ScopedFD fd,
      chromeos::VoidDBusMethodCallback callback) = 0;

 protected:
  // Initialize() should be used instead.
  ArcMidisClient();
  ~ArcMidisClient() override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_ARC_ARC_MIDIS_CLIENT_H_
