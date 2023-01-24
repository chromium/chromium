// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_ARC_FAKE_ARC_KEYMINT_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_ARC_FAKE_ARC_KEYMINT_CLIENT_H_

#include "chromeos/ash/components/dbus/arc/arc_keymint_client.h"

namespace ash {

// A fake implementation of ArcKeyMintClient.
class COMPONENT_EXPORT(ASH_DBUS_ARC) FakeArcKeyMintClient
    : public ArcKeyMintClient {
 public:
  FakeArcKeyMintClient() = default;

  FakeArcKeyMintClient(const FakeArcKeyMintClient&) = delete;
  FakeArcKeyMintClient& operator=(const FakeArcKeyMintClient&) = delete;

  ~FakeArcKeyMintClient() override = default;

  // chromeos::DBusClient override:
  void Init(dbus::Bus* bus) override;

  // ArcKeyMintClient override:
  void BootstrapMojoConnection(
      base::ScopedFD fd,
      chromeos::VoidDBusMethodCallback callback) override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_ARC_FAKE_ARC_KEYMINT_CLIENT_H_
