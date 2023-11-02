// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_ARC_FAKE_ARC_KEYMASTER_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_ARC_FAKE_ARC_KEYMASTER_CLIENT_H_

#include "chromeos/ash/components/dbus/arc/arc_keymaster_client.h"

namespace ash {

// A fake implementation of ArcKeymasterClient.
class COMPONENT_EXPORT(ASH_DBUS_ARC) FakeArcKeymasterClient
    : public ArcKeymasterClient {
 public:
  FakeArcKeymasterClient() = default;

  FakeArcKeymasterClient(const FakeArcKeymasterClient&) = delete;
  FakeArcKeymasterClient& operator=(const FakeArcKeymasterClient&) = delete;

  ~FakeArcKeymasterClient() override = default;

  // chromeos::DBusClient override:
  void Init(dbus::Bus* bus) override;

  // ArcKeymasterClient override:
  void BootstrapMojoConnection(
      base::ScopedFD fd,
      chromeos::VoidDBusMethodCallback callback) override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_ARC_FAKE_ARC_KEYMASTER_CLIENT_H_
