// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_ARC_FAKE_ARC_KEYMASTER_CLIENT_H_
#define CHROMEOS_DBUS_ARC_FAKE_ARC_KEYMASTER_CLIENT_H_

#include "chromeos/dbus/arc/arc_keymaster_client.h"

namespace chromeos {

// A fake implementation of ArcKeymasterClient.
class COMPONENT_EXPORT(CHROMEOS_DBUS_ARC) FakeArcKeymasterClient
    : public ArcKeymasterClient {
 public:
  FakeArcKeymasterClient() = default;

  FakeArcKeymasterClient(const FakeArcKeymasterClient&) = delete;
  FakeArcKeymasterClient& operator=(const FakeArcKeymasterClient&) = delete;

  ~FakeArcKeymasterClient() override = default;

  // DBusClient override:
  void Init(dbus::Bus* bus) override;

  // ArcKeymasterClient override:
  void BootstrapMojoConnection(base::ScopedFD fd,
                               VoidDBusMethodCallback callback) override;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_ARC_FAKE_ARC_KEYMASTER_CLIENT_H_
