// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/system_signals/win/win_system_signals_service.h"

#include "base/win/windows_version.h"
#include "components/device_signals/core/common/win/wmi_client.h"
#include "components/device_signals/core/common/win/wmi_client_impl.h"
#include "components/device_signals/core/common/win/wsc_client.h"
#include "components/device_signals/core/common/win/wsc_client_impl.h"

namespace system_signals {

WinSystemSignalsService::WinSystemSignalsService(
    mojo::PendingReceiver<device_signals::mojom::SystemSignalsService> receiver)
    : WinSystemSignalsService(
          std::move(receiver),
          std::make_unique<device_signals::WmiClientImpl>(),
          std::make_unique<device_signals::WscClientImpl>()) {}

WinSystemSignalsService::WinSystemSignalsService(
    mojo::PendingReceiver<device_signals::mojom::SystemSignalsService> receiver,
    std::unique_ptr<device_signals::WmiClient> wmi_client,
    std::unique_ptr<device_signals::WscClient> wsc_client)
    : receiver_(this, std::move(receiver)),
      wmi_client_(std::move(wmi_client)),
      wsc_client_(std::move(wsc_client)) {}

WinSystemSignalsService::~WinSystemSignalsService() = default;

void WinSystemSignalsService::GetBinarySignals(
    std::vector<device_signals::mojom::BinarySignalsRequestPtr> requests,
    GetBinarySignalsCallback callback) {
  // TODO(b/231298500): Implement this.
  std::move(callback).Run({});
}

void WinSystemSignalsService::GetAntiVirusSignals(
    GetAntiVirusSignalsCallback callback) {
  // WSC is only supported on Win8+, and not server.
  base::win::OSInfo* os_info = base::win::OSInfo::GetInstance();
  if (os_info && os_info->version_type() != base::win::SUITE_SERVER &&
      os_info->version() >= base::win::Version::WIN8) {
    auto response = wsc_client_->GetAntiVirusProducts();

    // TODO(b/229737923): Collect metrics.
    std::move(callback).Run(std::move(response.av_products));
    return;
  }

  std::move(callback).Run({});
}

void WinSystemSignalsService::GetHotfixSignals(
    GetHotfixSignalsCallback callback) {
  auto response = wmi_client_->GetInstalledHotfixes();

  // TODO(b/229737923): Collect metrics.
  std::move(callback).Run(std::move(response.hotfixes));
}

}  // namespace system_signals
