// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector/get_installed_version.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/version.h"
#include "chromeos/crosapi/mojom/browser_version.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/version_info/version_info.h"

void GetInstalledVersion(InstalledVersionCallback callback) {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (lacros_service &&
      lacros_service->IsAvailable<crosapi::mojom::BrowserVersionService>()) {
    lacros_service->GetRemote<crosapi::mojom::BrowserVersionService>()
        ->GetInstalledBrowserVersion(base::BindOnce(
            [](InstalledVersionCallback callback,
               const std::string& version_str) {
              std::move(callback).Run(
                  InstalledAndCriticalVersion(base::Version(version_str)));
            },
            std::move(callback)));
  } else {
    // Invoking an Ash-Chrome version that predates the introduction of the
    // GetInstalledBrowserVersion API will result in this failure.
    DLOG(ERROR)
        << "Current lacros service does not support the browser version api.";

    // We must return the current version as opposed to an invalid version so
    // that the InstalledVersionPoller can interpret that no update is
    // available.
    std::move(callback).Run(
        InstalledAndCriticalVersion(version_info::GetVersion()));
  }
}
