// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/cellular_setup/public/cpp/fake_activation_delegate.h"

namespace ash::cellular_setup {

FakeActivationDelegate::FakeActivationDelegate() = default;

FakeActivationDelegate::~FakeActivationDelegate() = default;

mojo::PendingRemote<mojom::ActivationDelegate>
FakeActivationDelegate::GenerateRemote() {
  mojo::PendingRemote<mojom::ActivationDelegate> remote;
  receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void FakeActivationDelegate::DisconnectReceivers() {
  receivers_.Clear();
}

void FakeActivationDelegate::OnActivationStarted(
    mojom::CellularMetadataPtr cellular_metadata) {
  cellular_metadata_list_.push_back(std::move(cellular_metadata));
}

void FakeActivationDelegate::OnActivationFinished(
    mojom::ActivationResult activation_result) {
  activation_results_.push_back(activation_result);
}

}  // namespace ash::cellular_setup
