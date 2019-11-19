// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_ARC_KEYMASTER_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_ARC_KEYMASTER_CLIENT_H_

#include "chromeos/dbus/arc_keymaster_client.h"

namespace chromeos {

// A fake implementation of ArcKeymasterClient.
class COMPONENT_EXPORT(CHROMEOS_DBUS) FakeArcKeymasterClient
    : public ArcKeymasterClient {
 public:
  FakeArcKeymasterClient() = default;
  ~FakeArcKeymasterClient() override = default;

  // DBusClient override:
  void Init(dbus::Bus* bus) override;

  // ArcKeymasterClient override:
  void BootstrapMojoConnection(base::ScopedFD fd,
                               VoidDBusMethodCallback callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeArcKeymasterClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_ARC_KEYMASTER_CLIENT_H_
