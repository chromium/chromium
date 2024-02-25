// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_LORGNETTE_MANAGER_FAKE_LORGNETTE_MANAGER_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_LORGNETTE_MANAGER_FAKE_LORGNETTE_MANAGER_CLIENT_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "chromeos/ash/components/dbus/lorgnette/lorgnette_service.pb.h"
#include "chromeos/ash/components/dbus/lorgnette_manager/lorgnette_manager_client.h"

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
      const std::string& client_id,
      bool local_only,
      bool preferred_only,
      chromeos::DBusMethodCallback<lorgnette::ListScannersResponse> callback)
      override;
  void GetScannerCapabilities(
      const std::string& device_name,
      chromeos::DBusMethodCallback<lorgnette::ScannerCapabilities> callback)
      override;
  void OpenScanner(const lorgnette::OpenScannerRequest& request,
                   chromeos::DBusMethodCallback<lorgnette::OpenScannerResponse>
                       callback) override;
  void CloseScanner(
      const lorgnette::CloseScannerRequest& request,
      chromeos::DBusMethodCallback<lorgnette::CloseScannerResponse> callback)
      override;
  void SetOptions(const lorgnette::SetOptionsRequest& request,
                  chromeos::DBusMethodCallback<lorgnette::SetOptionsResponse>
                      callback) override;
  void GetCurrentConfig(
      const lorgnette::GetCurrentConfigRequest& request,
      chromeos::DBusMethodCallback<lorgnette::GetCurrentConfigResponse>
          callback) override;
  void StartPreparedScan(
      const lorgnette::StartPreparedScanRequest& request,
      chromeos::DBusMethodCallback<lorgnette::StartPreparedScanResponse>
          callback) override;
  void StartScan(
      const std::string& device_name,
      const lorgnette::ScanSettings& settings,
      base::OnceCallback<void(lorgnette::ScanFailureMode)> completion_callback,
      base::RepeatingCallback<void(std::string, uint32_t)> page_callback,
      base::RepeatingCallback<void(uint32_t, uint32_t)> progress_callback)
      override;
  void ReadScanData(
      const lorgnette::ReadScanDataRequest& request,
      chromeos::DBusMethodCallback<lorgnette::ReadScanDataResponse> callback)
      override;
  void CancelScan(
      chromeos::VoidDBusMethodCallback completion_callback) override;
  void CancelScan(const lorgnette::CancelScanRequest& request,
                  chromeos::DBusMethodCallback<lorgnette::CancelScanResponse>
                      callback) override;
  void StartScannerDiscovery(
      const lorgnette::StartScannerDiscoveryRequest& request,
      base::RepeatingCallback<void(lorgnette::ScannerListChangedSignal)>
          signal_callback,
      chromeos::DBusMethodCallback<lorgnette::StartScannerDiscoveryResponse>
          callback) override;
  void StopScannerDiscovery(
      const lorgnette::StopScannerDiscoveryRequest& request,
      chromeos::DBusMethodCallback<lorgnette::StopScannerDiscoveryResponse>
          callback) override;

  // Sets the response returned by ListScanners().
  void SetListScannersResponse(
      const std::optional<lorgnette::ListScannersResponse>&
          list_scanners_response);

  // Sets the response returned by GetScannerCapabilities().
  void SetScannerCapabilitiesResponse(
      const std::optional<lorgnette::ScannerCapabilities>&
          capabilities_response);

  // Sets the response returned by OpenScanner().
  void SetOpenScannerResponse(
      const std::optional<lorgnette::OpenScannerResponse>& response);

  // Sets the response returned by CloseScanner().
  void SetCloseScannerResponse(
      const std::optional<lorgnette::CloseScannerResponse>& response);

  // Sets the response returned by SetOptions().
  void SetSetOptionsResponse(
      const std::optional<lorgnette::SetOptionsResponse>& response);

  // Sets the response returned by GetCurrentConfig().
  void SetGetCurrentConfigResponse(
      const std::optional<lorgnette::GetCurrentConfigResponse>& response);

  // Sets the response returned by StartPreparedScan()
  void SetStartPreparedScanResponse(
      const std::optional<lorgnette::StartPreparedScanResponse>& response);

  // Sets the response returned by StartScan().
  void SetScanResponse(
      const std::optional<std::vector<std::string>>& scan_response);

  // Sets the response returned by ReadScanData().
  void SetReadScanDataResponse(
      const std::optional<lorgnette::ReadScanDataResponse>& response);

  // Sets the response returned by CancelScan().
  void SetCancelScanResponse(
      const std::optional<lorgnette::CancelScanResponse>& response);

 private:
  std::optional<lorgnette::ListScannersResponse> list_scanners_response_;
  std::optional<lorgnette::ScannerCapabilities> capabilities_response_;
  std::optional<lorgnette::OpenScannerResponse> open_scanner_response_;
  std::optional<lorgnette::CloseScannerResponse> close_scanner_response_;
  std::optional<lorgnette::SetOptionsResponse> set_options_response_;
  std::optional<lorgnette::GetCurrentConfigResponse>
      get_current_config_response_;
  std::optional<lorgnette::StartPreparedScanResponse>
      start_prepared_scan_response_;
  std::optional<lorgnette::ReadScanDataResponse> read_scan_data_response_;
  std::optional<lorgnette::CancelScanResponse> cancel_scan_response_;
  std::optional<std::vector<std::string>> scan_response_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_LORGNETTE_MANAGER_FAKE_LORGNETTE_MANAGER_CLIENT_H_
