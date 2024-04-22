// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/nearby/public/cpp/fake_firewall_hole_factory.h"

#include "chromeos/ash/services/nearby/public/cpp/fake_firewall_hole.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace ash {
namespace nearby {

FakeFirewallHoleFactory::FakeFirewallHoleFactory() = default;

FakeFirewallHoleFactory::~FakeFirewallHoleFactory() = default;

// Immediately invokes |callback| with a fake firewall hole if
// |should_succeed_| is true and NullRemote if false.
void FakeFirewallHoleFactory::OpenFirewallHole(
    const ash::nearby::TcpServerSocketPort& port,
    OpenFirewallHoleCallback callback) {
  if (should_succeed_) {
    mojo::PendingRemote<::sharing::mojom::FirewallHole> firewall_hole;
    mojo::MakeSelfOwnedReceiver(std::make_unique<FakeFirewallHole>(),
                                firewall_hole.InitWithNewPipeAndPassReceiver());
    std::move(callback).Run(std::move(firewall_hole));
  } else {
    std::move(callback).Run(/*firewall_hole=*/mojo::NullRemote());
  }
}

}  // namespace nearby
}  // namespace ash
