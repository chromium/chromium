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
      bool local_only,
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
      const absl::optional<lorgnette::ListScannersResponse>&
          list_scanners_response);

  // Sets the response returned by GetScannerCapabilities().
  void SetScannerCapabilitiesResponse(
      const absl::optional<lorgnette::ScannerCapabilities>&
          capabilities_response);

  // Sets the response returned by OpenScanner().
  void SetOpenScannerResponse(
      const absl::optional<lorgnette::OpenScannerResponse>& response);

  // Sets the response returned by CloseScanner().
  void SetCloseScannerResponse(
      const absl::optional<lorgnette::CloseScannerResponse>& response);

  // Sets the response returned by StartPreparedScan()
  void SetStartPreparedScanResponse(
      const absl::optional<lorgnette::StartPreparedScanResponse>& response);

  // Sets the response returned by StartScan().
  void SetScanResponse(
      const absl::optional<std::vector<std::string>>& scan_response);

  // Sets the response returned by ReadScanData().
  void SetReadScanDataResponse(
      const absl::optional<lorgnette::ReadScanDataResponse>& response);

  // Sets the response returned by CancelScan().
  void SetCancelScanResponse(
      const absl::optional<lorgnette::CancelScanResponse>& response);

 private:
  absl::optional<lorgnette::ListScannersResponse> list_scanners_response_;
  absl::optional<lorgnette::ScannerCapabilities> capabilities_response_;
  absl::optional<lorgnette::OpenScannerResponse> open_scanner_response_;
  absl::optional<lorgnette::CloseScannerResponse> close_scanner_response_;
  absl::optional<lorgnette::StartPreparedScanResponse>
      start_prepared_scan_response_;
  absl::optional<lorgnette::ReadScanDataResponse> read_scan_data_response_;
  absl::optional<lorgnette::CancelScanResponse> cancel_scan_response_;
  absl::optional<std::vector<std::string>> scan_response_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_LORGNETTE_MANAGER_FAKE_LORGNETTE_MANAGER_CLIENT_H_
