// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/federated/public/cpp/fake_service_connection.h"

#include "base/containers/flat_map.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace federated {

FakeServiceConnectionImpl::FakeServiceConnectionImpl() = default;
FakeServiceConnectionImpl::~FakeServiceConnectionImpl() = default;

void FakeServiceConnectionImpl::BindReceiver(
    mojo::PendingReceiver<chromeos::federated::mojom::FederatedService>
        receiver) {
  Clone(std::move(receiver));
}

void FakeServiceConnectionImpl::Clone(
    mojo::PendingReceiver<chromeos::federated::mojom::FederatedService>
        receiver) {
  receivers_.Add(this, std::move(receiver));
}

void FakeServiceConnectionImpl::ReportExample(
    const std::string& client_name,
    chromeos::federated::mojom::ExamplePtr example) {
  LOG(INFO) << "In FakeServiceConnectionImpl::ReportExample, does nothing";
  return;
}

void FakeServiceConnectionImpl::StartScheduling(
    const absl::optional<base::flat_map<std::string, std::string>>&
        client_launch_stage) {
  return;
}

}  // namespace federated
}  // namespace ash
