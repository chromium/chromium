// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_ARC_OEMCRYPTO_CLIENT_H_
#define CHROMEOS_DBUS_ARC_OEMCRYPTO_CLIENT_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"

namespace chromeos {

// ArcOemCryptoClient is used to communicate with the ArcOemCrypto service
// which performs Widevine L1 DRM operations for ARC. The only purpose of
// the D-Bus service is to bootstrap a Mojo IPC connection.
// All methods should be called from the origin thread (UI thread) which
// initializes the DBusThreadManager instance.
class COMPONENT_EXPORT(CHROMEOS_DBUS) ArcOemCryptoClient : public DBusClient {
 public:
  ArcOemCryptoClient();
  ~ArcOemCryptoClient() override;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static std::unique_ptr<ArcOemCryptoClient> Create();

  // Bootstraps the Mojo IPC connection between Chrome and the service daemon.
  // This should pass in the child end of a Mojo pipe.
  virtual void BootstrapMojoConnection(base::ScopedFD fd,
                                       VoidDBusMethodCallback callback) = 0;
};
}  // namespace chromeos

#endif  // CHROMEOS_DBUS_ARC_OEMCRYPTO_CLIENT_H_
