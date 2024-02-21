// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/lorgnette_manager/fake_lorgnette_manager_client.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"

namespace ash {

FakeLorgnetteManagerClient::FakeLorgnetteManagerClient() = default;

FakeLorgnetteManagerClient::~FakeLorgnetteManagerClient() = default;

void FakeLorgnetteManagerClient::Init(dbus::Bus* bus) {}

void FakeLorgnetteManagerClient::ListScanners(
    const std::string& client_id,
    bool local_only,
    bool preferred_only,
    chromeos::DBusMethodCallback<lorgnette::ListScannersResponse> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), list_scanners_response_));
}

void FakeLorgnetteManagerClient::GetScannerCapabilities(
    const std::string& device_name,
    chromeos::DBusMethodCallback<lorgnette::ScannerCapabilities> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), capabilities_response_));
}

void FakeLorgnetteManagerClient::OpenScanner(
    const lorgnette::OpenScannerRequest& request,
    chromeos::DBusMethodCallback<lorgnette::OpenScannerResponse> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), open_scanner_response_));
}

void FakeLorgnetteManagerClient::CloseScanner(
    const lorgnette::CloseScannerRequest& request,
    chromeos::DBusMethodCallback<lorgnette::CloseScannerResponse> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), close_scanner_response_));
}

void FakeLorgnetteManagerClient::SetOptions(
    const lorgnette::SetOptionsRequest& request,
    chromeos::DBusMethodCallback<lorgnette::SetOptionsResponse> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), set_options_response_));
}

void FakeLorgnetteManagerClient::GetCurrentConfig(
    const lorgnette::GetCurrentConfigRequest& request,
    chromeos::DBusMethodCallback<lorgnette::GetCurrentConfigResponse>
        callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), get_current_config_response_));
}

void FakeLorgnetteManagerClient::StartPreparedScan(
    const lorgnette::StartPreparedScanRequest& request,
    chromeos::DBusMethodCallback<lorgnette::StartPreparedScanResponse>
        callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), start_prepared_scan_response_));
}

void FakeLorgnetteManagerClient::StartScan(
    const std::string& device_name,
    const lorgnette::ScanSettings& settings,
    base::OnceCallback<void(lorgnette::ScanFailureMode)> completion_callback,
    base::RepeatingCallback<void(std::string, uint32_t)> page_callback,
    base::RepeatingCallback<void(uint32_t, uint32_t)> progress_callback) {
  if (scan_response_.has_value()) {
    uint32_t page_number = 1;
    for (const std::string& page_data : scan_response_.value()) {
      // Simulate progress reporting for the scan job.
      if (progress_callback) {
        for (const uint32_t progress : {7, 22, 40, 42, 59, 74, 95}) {
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(progress_callback, progress, page_number));
        }
      }

      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(page_callback, page_data, ++page_number));
    }
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(completion_callback),
                                scan_response_.has_value()
                                    ? lorgnette::SCAN_FAILURE_MODE_NO_FAILURE
                                    : lorgnette::SCAN_FAILURE_MODE_UNKNOWN));
  scan_response_ = std::nullopt;
}

void FakeLorgnetteManagerClient::ReadScanData(
    const lorgnette::ReadScanDataRequest& request,
    chromeos::DBusMethodCallback<lorgnette::ReadScanDataResponse> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), read_scan_data_response_));
}

void FakeLorgnetteManagerClient::CancelScan(
    chromeos::VoidDBusMethodCallback completion_callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(completion_callback), true));
}

void FakeLorgnetteManagerClient::CancelScan(
    const lorgnette::CancelScanRequest& request,
    chromeos::DBusMethodCallback<lorgnette::CancelScanResponse> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), cancel_scan_response_));
}

void FakeLorgnetteManagerClient::StartScannerDiscovery(
    const lorgnette::StartScannerDiscoveryRequest& request,
    base::RepeatingCallback<void(lorgnette::ScannerListChangedSignal)>
        signal_callback,
    chromeos::DBusMethodCallback<lorgnette::StartScannerDiscoveryResponse>
        callback) {
  NOTIMPLEMENTED();
}

void FakeLorgnetteManagerClient::StopScannerDiscovery(
    const lorgnette::StopScannerDiscoveryRequest& request,
    chromeos::DBusMethodCallback<lorgnette::StopScannerDiscoveryResponse>
        callback) {
  NOTIMPLEMENTED();
}

void FakeLorgnetteManagerClient::SetListScannersResponse(
    const std::optional<lorgnette::ListScannersResponse>&
        list_scanners_response) {
  list_scanners_response_ = list_scanners_response;
}

void FakeLorgnetteManagerClient::SetScannerCapabilitiesResponse(
    const std::optional<lorgnette::ScannerCapabilities>&
        capabilities_response) {
  capabilities_response_ = capabilities_response;
}

void FakeLorgnetteManagerClient::SetOpenScannerResponse(
    const std::optional<lorgnette::OpenScannerResponse>&
        open_scanner_response) {
  open_scanner_response_ = open_scanner_response;
}

void FakeLorgnetteManagerClient::SetCloseScannerResponse(
    const std::optional<lorgnette::CloseScannerResponse>&
        close_scanner_response) {
  close_scanner_response_ = close_scanner_response;
}

void FakeLorgnetteManagerClient::SetSetOptionsResponse(
    const std::optional<lorgnette::SetOptionsResponse>& response) {
  set_options_response_ = response;
}

void FakeLorgnetteManagerClient::SetGetCurrentConfigResponse(
    const std::optional<lorgnette::GetCurrentConfigResponse>& response) {
  get_current_config_response_ = response;
}

void FakeLorgnetteManagerClient::SetStartPreparedScanResponse(
    const std::optional<lorgnette::StartPreparedScanResponse>& response) {
  start_prepared_scan_response_ = response;
}

void FakeLorgnetteManagerClient::SetReadScanDataResponse(
    const std::optional<lorgnette::ReadScanDataResponse>& response) {
  read_scan_data_response_ = response;
}

void FakeLorgnetteManagerClient::SetScanResponse(
    const std::optional<std::vector<std::string>>& scan_response) {
  scan_response_ = scan_response;
}

void FakeLorgnetteManagerClient::SetCancelScanResponse(
    const std::optional<lorgnette::CancelScanResponse>& response) {
  cancel_scan_response_ = response;
}

}  // namespace ash
