// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_CELLULAR_SETUP_PUBLIC_CPP_FAKE_CELLULAR_SETUP_H_
#define CHROMEOS_SERVICES_CELLULAR_SETUP_PUBLIC_CPP_FAKE_CELLULAR_SETUP_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "chromeos/services/cellular_setup/cellular_setup_base.h"
#include "chromeos/services/cellular_setup/public/mojom/cellular_setup.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

namespace cellular_setup {

class FakeCarrierPortalHandler;

// Fake mojom::CellularSetup implementation.
class FakeCellularSetup : public CellularSetupBase {
 public:
  class StartActivationInvocation {
   public:
    StartActivationInvocation(
        mojo::PendingRemote<mojom::ActivationDelegate> activation_delegate,
        StartActivationCallback callback);
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

    DISALLOW_COPY_AND_ASSIGN(StartActivationInvocation);
  };

  FakeCellularSetup();
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

  DISALLOW_COPY_AND_ASSIGN(FakeCellularSetup);
};

}  // namespace cellular_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_CELLULAR_SETUP_PUBLIC_CPP_FAKE_CELLULAR_SETUP_H_
