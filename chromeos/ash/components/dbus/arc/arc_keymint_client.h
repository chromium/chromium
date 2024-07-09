// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_ARC_ARC_KEYMINT_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_ARC_ARC_KEYMINT_CLIENT_H_

#include "base/files/scoped_file.h"
#include "chromeos/dbus/common/dbus_callback.h"
#include "chromeos/dbus/common/dbus_client.h"

namespace ash {

// ArcKeyMintClient is used to bootstrap a Mojo connection with the
// arc-keymintd daemon in Chrome OS.
class COMPONENT_EXPORT(ASH_DBUS_ARC) ArcKeyMintClient
    : public chromeos::DBusClient {
 public:
  // Returns the global instance if initialized. May return null.
  static ArcKeyMintClient* Get();

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance.
  static void InitializeFake();

  // Destroys the global instance if it has been initialized.
  static void Shutdown();

  ArcKeyMintClient(const ArcKeyMintClient&) = delete;
  ArcKeyMintClient& operator=(const ArcKeyMintClient&) = delete;

  // Bootstrap the Mojo connection between Chrome and the keymint service.
  // Should pass in the child end of the Mojo pipe.
  virtual void BootstrapMojoConnection(
      base::ScopedFD fd,
      chromeos::VoidDBusMethodCallback callback) = 0;

 protected:
  // Initialize() should be used instead.
  ArcKeyMintClient();
  ~ArcKeyMintClient() override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_ARC_ARC_KEYMINT_CLIENT_H_
