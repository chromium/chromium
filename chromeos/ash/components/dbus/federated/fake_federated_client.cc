// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/federated/fake_federated_client.h"

#include "base/functional/callback.h"

namespace ash {

FakeFederatedClient::FakeFederatedClient() = default;

FakeFederatedClient::~FakeFederatedClient() = default;

void FakeFederatedClient::BootstrapMojoConnection(
    base::ScopedFD fd,
    base::OnceCallback<void(bool success)> result_callback) {
  const bool success = true;
  std::move(result_callback).Run(success);
}

}  // namespace ash
