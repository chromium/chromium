// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_CELLULAR_SETUP_PUBLIC_CPP_FAKE_ACTIVATION_DELEGATE_H_
#define CHROMEOS_SERVICES_CELLULAR_SETUP_PUBLIC_CPP_FAKE_ACTIVATION_DELEGATE_H_

#include <vector>

#include "base/macros.h"
#include "chromeos/services/cellular_setup/public/mojom/cellular_setup.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace chromeos {

namespace cellular_setup {

// Fake mojom::ActivationDelegate implementation.
class FakeActivationDelegate : public mojom::ActivationDelegate {
 public:
  FakeActivationDelegate();
  ~FakeActivationDelegate() override;

  mojo::PendingRemote<mojom::ActivationDelegate> GenerateRemote();
  void DisconnectReceivers();

  const std::vector<mojom::CellularMetadataPtr>& cellular_metadata_list()
      const {
    return cellular_metadata_list_;
  }

  const std::vector<mojom::ActivationResult>& activation_results() const {
    return activation_results_;
  }

 private:
  // mojom::ActivationDelegate:
  void OnActivationStarted(
      mojom::CellularMetadataPtr cellular_metadata) override;
  void OnActivationFinished(mojom::ActivationResult activation_result) override;

  std::vector<mojom::CellularMetadataPtr> cellular_metadata_list_;
  std::vector<mojom::ActivationResult> activation_results_;

  mojo::ReceiverSet<mojom::ActivationDelegate> receivers_;

  DISALLOW_COPY_AND_ASSIGN(FakeActivationDelegate);
};

}  // namespace cellular_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_CELLULAR_SETUP_PUBLIC_CPP_FAKE_ACTIVATION_DELEGATE_H_
