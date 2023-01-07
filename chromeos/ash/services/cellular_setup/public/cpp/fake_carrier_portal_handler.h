// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_CELLULAR_SETUP_PUBLIC_CPP_FAKE_CARRIER_PORTAL_HANDLER_H_
#define CHROMEOS_ASH_SERVICES_CELLULAR_SETUP_PUBLIC_CPP_FAKE_CARRIER_PORTAL_HANDLER_H_

#include <vector>

#include "chromeos/ash/services/cellular_setup/public/mojom/cellular_setup.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash::cellular_setup {

// Fake mojom::CarrierPortalHandler implementation.
class FakeCarrierPortalHandler : public mojom::CarrierPortalHandler {
 public:
  FakeCarrierPortalHandler();

  FakeCarrierPortalHandler(const FakeCarrierPortalHandler&) = delete;
  FakeCarrierPortalHandler& operator=(const FakeCarrierPortalHandler&) = delete;

  ~FakeCarrierPortalHandler() override;

  mojo::PendingRemote<mojom::CarrierPortalHandler> GenerateRemote();

  const std::vector<mojom::CarrierPortalStatus>& status_updates() const {
    return status_updates_;
  }

  // mojom::CarrierPortalHandler:
  void OnCarrierPortalStatusChange(
      mojom::CarrierPortalStatus carrier_portal_status) override;

 private:
  std::vector<mojom::CarrierPortalStatus> status_updates_;
  mojo::ReceiverSet<mojom::CarrierPortalHandler> receivers_;
};

}  // namespace ash::cellular_setup

#endif  // CHROMEOS_ASH_SERVICES_CELLULAR_SETUP_PUBLIC_CPP_FAKE_CARRIER_PORTAL_HANDLER_H_
