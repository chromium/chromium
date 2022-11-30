// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/cellular_setup/public/cpp/fake_cellular_setup.h"

#include "chromeos/ash/services/cellular_setup/public/cpp/fake_carrier_portal_handler.h"

#include <utility>

namespace ash::cellular_setup {

FakeCellularSetup::StartActivationInvocation::StartActivationInvocation(
    mojo::PendingRemote<mojom::ActivationDelegate> activation_delegate,
    StartActivationCallback callback)
    : activation_delegate_(std::move(activation_delegate)),
      callback_(std::move(callback)) {}

FakeCellularSetup::StartActivationInvocation::~StartActivationInvocation() =
    default;

FakeCarrierPortalHandler*
FakeCellularSetup::StartActivationInvocation::ExecuteCallback() {
  DCHECK(callback_);
  DCHECK(!fake_carrier_portal_observer_);

  fake_carrier_portal_observer_ = std::make_unique<FakeCarrierPortalHandler>();
  std::move(callback_).Run(fake_carrier_portal_observer_->GenerateRemote());

  return fake_carrier_portal_observer_.get();
}

FakeCellularSetup::FakeCellularSetup() = default;

FakeCellularSetup::~FakeCellularSetup() = default;

void FakeCellularSetup::StartActivation(
    mojo::PendingRemote<mojom::ActivationDelegate> activation_delegate,
    StartActivationCallback callback) {
  DCHECK(activation_delegate);
  DCHECK(callback);

  start_activation_invocations_.emplace_back(
      std::make_unique<StartActivationInvocation>(
          std::move(activation_delegate), std::move(callback)));
}

}  // namespace ash::cellular_setup
