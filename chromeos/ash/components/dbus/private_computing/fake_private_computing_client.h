// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_PRIVATE_COMPUTING_FAKE_PRIVATE_COMPUTING_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_PRIVATE_COMPUTING_FAKE_PRIVATE_COMPUTING_CLIENT_H_

#include <optional>
#include <string>

#include "chromeos/ash/components/dbus/private_computing/private_computing_client.h"
#include "chromeos/ash/components/dbus/private_computing/private_computing_service.pb.h"
#include "dbus/object_proxy.h"

namespace ash {

class COMPONENT_EXPORT(PRIVATE_COMPUTING) FakePrivateComputingClient
    : public PrivateComputingClient,
      public PrivateComputingClient::TestInterface {
 public:
  FakePrivateComputingClient();
  FakePrivateComputingClient(const FakePrivateComputingClient&) = delete;
  FakePrivateComputingClient& operator=(const FakePrivateComputingClient&) =
      delete;
  ~FakePrivateComputingClient() override;

  // PrivateComputingClient implementation:
  void SaveLastPingDatesStatus(
      const private_computing::SaveStatusRequest& request,
      SaveStatusCallback callback) override;
  void GetLastPingDatesStatus(GetStatusCallback callback) override;

  PrivateComputingClient::TestInterface* GetTestInterface() override;

  // PrivateComputingClient::TestInterface implementation:
  void SetSaveLastPingDatesStatusResponse(
      private_computing::SaveStatusResponse response) override;
  void SetGetLastPingDatesStatusResponse(
      private_computing::GetStatusResponse response) override;

 private:
  std::optional<private_computing::SaveStatusResponse> save_status_response_;
  std::optional<private_computing::GetStatusResponse> get_status_response_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_PRIVATE_COMPUTING_FAKE_PRIVATE_COMPUTING_CLIENT_H_
