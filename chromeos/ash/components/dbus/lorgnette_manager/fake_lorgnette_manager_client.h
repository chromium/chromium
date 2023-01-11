// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_LORGNETTE_MANAGER_FAKE_LORGNETTE_MANAGER_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_LORGNETTE_MANAGER_FAKE_LORGNETTE_MANAGER_CLIENT_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "chromeos/ash/components/dbus/lorgnette/lorgnette_service.pb.h"
#include "chromeos/ash/components/dbus/lorgnette_manager/lorgnette_manager_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

class COMPONENT_EXPORT(LORGNETTE_MANAGER) FakeLorgnetteManagerClient
    : public LorgnetteManagerClient {
 public:
  FakeLorgnetteManagerClient();
  FakeLorgnetteManagerClient(const FakeLorgnetteManagerClient&) = delete;
  FakeLorgnetteManagerClient& operator=(const FakeLorgnetteManagerClient&) =
      delete;
  ~FakeLorgnetteManagerClient() override;

  void Init(dbus::Bus* bus) override;

  void ListScanners(
      chromeos::DBusMethodCallback<lorgnette::ListScannersResponse> callback)
      override;
  void GetScannerCapabilities(
      const std::string& device_name,
      chromeos::DBusMethodCallback<lorgnette::ScannerCapabilities> callback)
      override;
  void StartScan(
      const std::string& device_name,
      const lorgnette::ScanSettings& settings,
      base::OnceCallback<void(lorgnette::ScanFailureMode)> completion_callback,
      base::RepeatingCallback<void(std::string, uint32_t)> page_callback,
      base::RepeatingCallback<void(uint32_t, uint32_t)> progress_callback)
      override;
  void CancelScan(
      chromeos::VoidDBusMethodCallback completion_callback) override;

  // Sets the response returned by ListScanners().
  void SetListScannersResponse(
      const absl::optional<lorgnette::ListScannersResponse>&
          list_scanners_response);

  // Sets the response returned by GetScannerCapabilities().
  void SetScannerCapabilitiesResponse(
      const absl::optional<lorgnette::ScannerCapabilities>&
          capabilities_response);

  // Sets the response returned by StartScan().
  void SetScanResponse(
      const absl::optional<std::vector<std::string>>& scan_response);

 private:
  absl::optional<lorgnette::ListScannersResponse> list_scanners_response_;
  absl::optional<lorgnette::ScannerCapabilities> capabilities_response_;
  absl::optional<std::vector<std::string>> scan_response_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_LORGNETTE_MANAGER_FAKE_LORGNETTE_MANAGER_CLIENT_H_
