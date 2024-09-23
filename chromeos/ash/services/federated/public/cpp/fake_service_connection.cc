// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/federated/public/cpp/fake_service_connection.h"

#include <optional>

#include "base/containers/flat_map.h"

namespace ash::federated {

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
    const std::string& table_name,
    chromeos::federated::mojom::ExamplePtr example) {
  LOG(INFO) << "In FakeServiceConnectionImpl::ReportExample, does nothing";
  return;
}

void FakeServiceConnectionImpl::StartScheduling(
    const std::optional<base::flat_map<std::string, std::string>>&
        client_launch_stage) {
  return;
}

void FakeServiceConnectionImpl::ReportExampleToTable(
    chromeos::federated::mojom::FederatedExampleTableId table_id,
    chromeos::federated::mojom::ExamplePtr example) {
  LOG(INFO)
      << "In FakeServiceConnectionImpl::ReportExampleToTable, does nothing";
  return;
}

void FakeServiceConnectionImpl::StartSchedulingWithConfig(
    std::vector<chromeos::federated::mojom::ClientScheduleConfigPtr>
        client_configs) {
  return;
}

}  // namespace ash::federated
