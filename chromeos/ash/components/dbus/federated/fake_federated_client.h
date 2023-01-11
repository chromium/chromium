// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_FEDERATED_FAKE_FEDERATED_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_FEDERATED_FAKE_FEDERATED_CLIENT_H_

#include "base/files/scoped_file.h"
#include "base/functional/callback_forward.h"
#include "chromeos/ash/components/dbus/federated/federated_client.h"

namespace ash {

// Fake implementation of FederatedClient. This is currently a no-op fake.
class FakeFederatedClient : public FederatedClient {
 public:
  FakeFederatedClient();
  ~FakeFederatedClient() override;
  FakeFederatedClient(const FakeFederatedClient&) = delete;
  FakeFederatedClient& operator=(const FakeFederatedClient&) = delete;

  // FederatedClient:
  void BootstrapMojoConnection(
      base::ScopedFD fd,
      base::OnceCallback<void(bool success)> result_callback) override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_FEDERATED_FAKE_FEDERATED_CLIENT_H_
