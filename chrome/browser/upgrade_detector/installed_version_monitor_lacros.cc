// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector/installed_version_monitor_lacros.h"

#include <memory>
#include <utility>

#include "base/check.h"

LacrosInstalledVersionMonitor::LacrosInstalledVersionMonitor(
    chromeos::LacrosService* lacros_service)
    : lacros_service_(lacros_service) {}

LacrosInstalledVersionMonitor::~LacrosInstalledVersionMonitor() = default;

void LacrosInstalledVersionMonitor::Start(Callback callback) {
  DCHECK(callback);
  callback_ = std::move(callback);

  if (lacros_service_->IsRegistered<crosapi::mojom::BrowserVersionService>() &&
      lacros_service_->IsAvailable<crosapi::mojom::BrowserVersionService>()) {
    lacros_service_->GetRemote<crosapi::mojom::BrowserVersionService>()
        ->AddBrowserVersionObserver(receiver_.BindNewPipeAndPassRemote());
  } else {
    callback_.Run(/*error=*/true);
  }
}

void LacrosInstalledVersionMonitor::OnBrowserVersionInstalled(
    const std::string& version) {
  callback_.Run(/*error=*/false);
}

// static
std::unique_ptr<InstalledVersionMonitor> InstalledVersionMonitor::Create() {
  auto* lacros_service = chromeos::LacrosService::Get();
  // TODO(crbug.com/40791106): Enable the DCHECK after the next release.
  // DCHECK(lacros_service->IsAvailable<crosapi::mojom::BrowserVersionService>());
  return std::make_unique<LacrosInstalledVersionMonitor>(lacros_service);
}
