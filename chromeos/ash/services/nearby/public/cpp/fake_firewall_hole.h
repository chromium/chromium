// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_FAKE_FIREWALL_HOLE_H_
#define CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_FAKE_FIREWALL_HOLE_H_

#include "chromeos/ash/services/nearby/public/mojom/firewall_hole.mojom.h"

namespace ash {
namespace nearby {

// A trivial implementation of ::sharing::mojom::FirewallHole used for testing.
class FakeFirewallHole : public ::sharing::mojom::FirewallHole {
 public:
  FakeFirewallHole() = default;
  ~FakeFirewallHole() override = default;
};

}  // namespace nearby
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_FAKE_FIREWALL_HOLE_H_
