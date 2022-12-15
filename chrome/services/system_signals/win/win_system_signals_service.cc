// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/system_signals/win/win_system_signals_service.h"

#include "base/win/windows_version.h"
#include "chrome/services/system_signals/win/metrics_utils.h"
#include "components/device_signals/core/common/common_types.h"
#include "components/device_signals/core/system_signals/executable_metadata_service.h"
#include "components/device_signals/core/system_signals/file_system_service.h"
#include "components/device_signals/core/system_signals/platform_delegate.h"
#include "components/device_signals/core/system_signals/win/win_platform_delegate.h"
#include "components/device_signals/core/system_signals/win/wmi_client.h"
#include "components/device_signals/core/system_signals/win/wmi_client_impl.h"
#include "components/device_signals/core/system_signals/win/wsc_client.h"
#include "components/device_signals/core/system_signals/win/wsc_client_impl.h"

namespace system_signals {

WinSystemSignalsService::WinSystemSignalsService(
    mojo::PendingReceiver<device_signals::mojom::SystemSignalsService> receiver)
    : WinSystemSignalsService(
          std::move(receiver),
          device_signals::FileSystemService::Create(
              std::make_unique<device_signals::WinPlatformDelegate>(),
              device_signals::ExecutableMetadataService::Create(
                  std::make_unique<device_signals::WinPlatformDelegate>())),
          std::make_unique<device_signals::WmiClientImpl>(),
          std::make_unique<device_signals::WscClientImpl>()) {}

WinSystemSignalsService::WinSystemSignalsService(
    mojo::PendingReceiver<device_signals::mojom::SystemSignalsService> receiver,
    std::unique_ptr<device_signals::FileSystemService> file_system_service,
    std::unique_ptr<device_signals::WmiClient> wmi_client,
    std::unique_ptr<device_signals::WscClient> wsc_client)
    : BaseSystemSignalsService(std::move(receiver),
                               std::move(file_system_service)),
      wmi_client_(std::move(wmi_client)),
      wsc_client_(std::move(wsc_client)) {}

WinSystemSignalsService::~WinSystemSignalsService() = default;

void WinSystemSignalsService::GetAntiVirusSignals(
    GetAntiVirusSignalsCallback callback) {
  // The AV signal is not supported on Win server builds.
  base::win::OSInfo* os_info = base::win::OSInfo::GetInstance();
  if (!os_info || os_info->version_type() == base::win::SUITE_SERVER) {
    std::move(callback).Run({});
    return;
  }

  std::vector<device_signals::AvProduct> av_products;
  auto response = wsc_client_->GetAntiVirusProducts();

  LogWscAvResponse(response);
  av_products = std::move(response.av_products);

  std::move(callback).Run(std::move(av_products));
}

void WinSystemSignalsService::GetHotfixSignals(
    GetHotfixSignalsCallback callback) {
  auto response = wmi_client_->GetInstalledHotfixes();

  LogWmiHotfixResponse(response);
  std::move(callback).Run(std::move(response.hotfixes));
}

}  // namespace system_signals
