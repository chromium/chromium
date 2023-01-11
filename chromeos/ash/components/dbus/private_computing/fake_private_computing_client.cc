// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/private_computing/fake_private_computing_client.h"

#include <string>

#include "base/functional/bind.h"
#include "chromeos/ash/components/dbus/private_computing/private_computing_service.pb.h"

namespace ash {

FakePrivateComputingClient::FakePrivateComputingClient() = default;

FakePrivateComputingClient::~FakePrivateComputingClient() = default;

void FakePrivateComputingClient::SaveLastPingDatesStatus(
    const private_computing::SaveStatusRequest& request,
    SaveStatusCallback callback) {
  private_computing::SaveStatusResponse response;
  if (save_status_response_.has_value())
    response = save_status_response_.value();
  std::move(callback).Run(response);
}

void FakePrivateComputingClient::GetLastPingDatesStatus(
    GetStatusCallback callback) {
  private_computing::GetStatusResponse response;
  if (get_status_response_.has_value())
    response = get_status_response_.value();
  std::move(callback).Run(response);
}

PrivateComputingClient::TestInterface*
FakePrivateComputingClient::GetTestInterface() {
  return this;
}

void FakePrivateComputingClient::SetSaveLastPingDatesStatusResponse(
    private_computing::SaveStatusResponse response) {
  save_status_response_ = response;
}

void FakePrivateComputingClient::SetGetLastPingDatesStatusResponse(
    private_computing::GetStatusResponse response) {
  get_status_response_ = response;
}

}  // namespace ash
