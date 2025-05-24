// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/arc/test/fake_keymint_instance.h"

#include <utility>

namespace arc {

FakeKeyMintInstance::FakeKeyMintInstance() = default;
FakeKeyMintInstance::~FakeKeyMintInstance() = default;

void FakeKeyMintInstance::Init(
    mojo::PendingRemote<mojom::keymint::KeyMintHost> host_remote,
    InitCallback callback) {
  ++num_init_called_;
  host_remote_.reset();
  host_remote_.Bind(std::move(host_remote));
  std::move(callback).Run();
}

}  // namespace arc
