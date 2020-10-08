// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_LORGNETTE_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_LORGNETTE_MANAGER_CLIENT_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/optional.h"
#include "chromeos/dbus/lorgnette/lorgnette_service.pb.h"
#include "chromeos/dbus/lorgnette_manager_client.h"

namespace chromeos {

class COMPONENT_EXPORT(CHROMEOS_DBUS) FakeLorgnetteManagerClient
    : public LorgnetteManagerClient {
 public:
  FakeLorgnetteManagerClient();
  FakeLorgnetteManagerClient(const FakeLorgnetteManagerClient&) = delete;
  FakeLorgnetteManagerClient& operator=(const FakeLorgnetteManagerClient&) =
      delete;
  ~FakeLorgnetteManagerClient() override;

  void Init(dbus::Bus* bus) override;

  void ListScanners(
      DBusMethodCallback<lorgnette::ListScannersResponse> callback) override;
  void GetScannerCapabilities(
      const std::string& device_name,
      DBusMethodCallback<lorgnette::ScannerCapabilities> callback) override;
  void StartScan(
      const std::string& device_name,
      const lorgnette::ScanSettings& settings,
      VoidDBusMethodCallback completion_callback,
      base::RepeatingCallback<void(std::string, uint32_t)> page_callback,
      base::RepeatingCallback<void(int)> progress_callback) override;

  // Sets the response returned by ListScanners().
  void SetListScannersResponse(
      const base::Optional<lorgnette::ListScannersResponse>&
          list_scanners_response);

  // Sets the response returned by GetScannerCapabilities().
  void SetScannerCapabilitiesResponse(
      const base::Optional<lorgnette::ScannerCapabilities>&
          capabilities_response);

  // Sets the response returned by StartScan().
  void SetScanResponse(
      const base::Optional<std::vector<std::string>>& scan_response);

 private:
  base::Optional<lorgnette::ListScannersResponse> list_scanners_response_;
  base::Optional<lorgnette::ScannerCapabilities> capabilities_response_;
  base::Optional<std::vector<std::string>> scan_response_;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_LORGNETTE_MANAGER_CLIENT_H_
