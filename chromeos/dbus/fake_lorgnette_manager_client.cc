// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_lorgnette_manager_client.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"

namespace chromeos {

FakeLorgnetteManagerClient::FakeLorgnetteManagerClient() = default;

FakeLorgnetteManagerClient::~FakeLorgnetteManagerClient() = default;

void FakeLorgnetteManagerClient::Init(dbus::Bus* bus) {}

void FakeLorgnetteManagerClient::ListScanners(
    DBusMethodCallback<lorgnette::ListScannersResponse> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), list_scanners_response_));
}

void FakeLorgnetteManagerClient::GetScannerCapabilities(
    const std::string& device_name,
    DBusMethodCallback<lorgnette::ScannerCapabilities> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), capabilities_response_));
}

void FakeLorgnetteManagerClient::StartScan(
    const std::string& device_name,
    const lorgnette::ScanSettings& settings,
    VoidDBusMethodCallback completion_callback,
    base::RepeatingCallback<void(std::string, uint32_t)> page_callback,
    base::RepeatingCallback<void(int)> progress_callback) {
  if (scan_response_.has_value()) {
    uint32_t page_number = 0;
    for (const std::string& page_data : scan_response_.value()) {
      // Simulate progress reporting for the scan job.
      if (progress_callback) {
        for (int progress : {7, 22, 40, 42, 59, 74, 95}) {
          base::ThreadTaskRunnerHandle::Get()->PostTask(
              FROM_HERE, base::BindOnce(progress_callback, progress));
        }
      }

      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(page_callback, page_data, ++page_number));
    }
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(completion_callback),
                                scan_response_.has_value()));
  scan_response_ = base::nullopt;
}

void FakeLorgnetteManagerClient::SetListScannersResponse(
    const base::Optional<lorgnette::ListScannersResponse>&
        list_scanners_response) {
  list_scanners_response_ = list_scanners_response;
}

void FakeLorgnetteManagerClient::SetScannerCapabilitiesResponse(
    const base::Optional<lorgnette::ScannerCapabilities>&
        capabilities_response) {
  capabilities_response_ = capabilities_response;
}

void FakeLorgnetteManagerClient::SetScanResponse(
    const base::Optional<std::vector<std::string>>& scan_response) {
  scan_response_ = scan_response;
}

}  // namespace chromeos
