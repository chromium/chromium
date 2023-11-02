// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_CELLULAR_SETUP_PUBLIC_CPP_FAKE_CELLULAR_SETUP_H_
#define CHROMEOS_ASH_SERVICES_CELLULAR_SETUP_PUBLIC_CPP_FAKE_CELLULAR_SETUP_H_

#include <memory>
#include <vector>

#include "chromeos/ash/services/cellular_setup/cellular_setup_base.h"
#include "chromeos/ash/services/cellular_setup/public/mojom/cellular_setup.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::cellular_setup {

class FakeCarrierPortalHandler;

// Fake mojom::CellularSetup implementation.
class FakeCellularSetup : public CellularSetupBase {
 public:
  class StartActivationInvocation {
   public:
    StartActivationInvocation(
        mojo::PendingRemote<mojom::ActivationDelegate> activation_delegate,
        StartActivationCallback callback);

    StartActivationInvocation(const StartActivationInvocation&) = delete;
    StartActivationInvocation& operator=(const StartActivationInvocation&) =
        delete;

    ~StartActivationInvocation();

    mojo::Remote<mojom::ActivationDelegate>& activation_delegate() {
      return activation_delegate_;
    }

    // Executes the provided callback by passing a FakeCarrierPortalHandler to
    // the provided callback and returning a pointer to it as the return valuel
    // for this function.
    FakeCarrierPortalHandler* ExecuteCallback();

   private:
    mojo::Remote<mojom::ActivationDelegate> activation_delegate_;
    StartActivationCallback callback_;

    // Null until ExecuteCallback() has been invoked.
    std::unique_ptr<FakeCarrierPortalHandler> fake_carrier_portal_observer_;
  };

  FakeCellularSetup();

  FakeCellularSetup(const FakeCellularSetup&) = delete;
  FakeCellularSetup& operator=(const FakeCellularSetup&) = delete;

  ~FakeCellularSetup() override;

  std::vector<std::unique_ptr<StartActivationInvocation>>&
  start_activation_invocations() {
    return start_activation_invocations_;
  }

 private:
  // mojom::CellularSetup:
  void StartActivation(
      mojo::PendingRemote<mojom::ActivationDelegate> activation_delegate,
      StartActivationCallback callback) override;

  std::vector<std::unique_ptr<StartActivationInvocation>>
      start_activation_invocations_;
};

}  // namespace ash::cellular_setup

#endif  // CHROMEOS_ASH_SERVICES_CELLULAR_SETUP_PUBLIC_CPP_FAKE_CELLULAR_SETUP_H_
