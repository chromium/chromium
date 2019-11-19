// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_ARC_KEYMASTER_CLIENT_H_
#define CHROMEOS_DBUS_ARC_KEYMASTER_CLIENT_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"

namespace chromeos {

// ArcKeymasterClient is used to bootstrap a Mojo connection with the
// arc-keymasterd daemon in Chrome OS.
class COMPONENT_EXPORT(CHROMEOS_DBUS) ArcKeymasterClient : public DBusClient {
 public:
  ~ArcKeymasterClient() override;

  // Factory function.
  static std::unique_ptr<ArcKeymasterClient> Create();

  // Bootstrap the Mojo connection between Chrome and the keymaster service.
  // Should pass in the child end of the Mojo pipe.
  virtual void BootstrapMojoConnection(base::ScopedFD fd,
                                       VoidDBusMethodCallback callback) = 0;

 protected:
  // Create() should be used instead.
  ArcKeymasterClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcKeymasterClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_ARC_KEYMASTER_CLIENT_H_
