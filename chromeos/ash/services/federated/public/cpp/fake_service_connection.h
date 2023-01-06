// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_FEDERATED_PUBLIC_CPP_FAKE_SERVICE_CONNECTION_H_
#define CHROMEOS_ASH_SERVICES_FEDERATED_PUBLIC_CPP_FAKE_SERVICE_CONNECTION_H_

#include <string>

#include "base/containers/flat_map.h"
#include "chromeos/ash/services/federated/public/cpp/service_connection.h"
#include "chromeos/ash/services/federated/public/mojom/example.mojom.h"
#include "chromeos/ash/services/federated/public/mojom/federated_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace federated {

// Fake implementation of ash::federated::ServiceConnection.
// Handles BindReceiver by binding the receiver to itself.
// For use with ServiceConnection::UseFakeServiceConnectionForTesting().
class FakeServiceConnectionImpl
    : public ServiceConnection,
      public chromeos::federated::mojom::FederatedService {
 public:
  FakeServiceConnectionImpl();
  FakeServiceConnectionImpl(const FakeServiceConnectionImpl&) = delete;
  FakeServiceConnectionImpl& operator=(const FakeServiceConnectionImpl&) =
      delete;
  ~FakeServiceConnectionImpl() override;

  // ServiceConnection:
  void BindReceiver(
      mojo::PendingReceiver<chromeos::federated::mojom::FederatedService>
          receiver) override;

  // mojom::FederatedService:
  void Clone(mojo::PendingReceiver<chromeos::federated::mojom::FederatedService>
                 receiver) override;
  void ReportExample(const std::string& client_name,
                     chromeos::federated::mojom::ExamplePtr example) override;
  void StartScheduling(
      const absl::optional<base::flat_map<std::string, std::string>>&
          client_launch_stage) override;

 private:
  mojo::ReceiverSet<chromeos::federated::mojom::FederatedService> receivers_;
};

}  // namespace federated
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_FEDERATED_PUBLIC_CPP_FAKE_SERVICE_CONNECTION_H_
