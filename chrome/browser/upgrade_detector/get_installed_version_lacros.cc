// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector/get_installed_version.h"

#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/version.h"
#include "chromeos/crosapi/mojom/browser_version.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/startup/browser_params_proxy.h"
#include "components/version_info/version_info.h"

void GetInstalledVersion(InstalledVersionCallback callback) {
  // In addition to checking that Ash supports the browser version API, we also
  // check that Ash supports loading the latest browser image on subsequent
  // starts by inspecting the ash capabilities entries.
  auto* lacros_service = chromeos::LacrosService::Get();
  if (lacros_service &&
      lacros_service->IsAvailable<crosapi::mojom::BrowserVersionService>()) {
    lacros_service->GetRemote<crosapi::mojom::BrowserVersionService>()
        ->GetInstalledBrowserVersion(base::BindOnce(
            [](InstalledVersionCallback callback,
               const std::string& version_str) {
              // Report the currently running version to indicate that no update
              // is available if crosapi returns an invalid version or an older
              // version and rootfs LaCrOS is running.
              base::Version version(version_str);
              if (!version.IsValid() ||
                  (version < version_info::GetVersion() &&
                   chromeos::BrowserParamsProxy::Get()->LacrosSelection() ==
                       crosapi::mojom::BrowserInitParams::LacrosSelection::
                           kRootfs)) {
                version = version_info::GetVersion();
              }

              std::move(callback).Run(InstalledAndCriticalVersion(version));
            },
            std::move(callback)));
    return;
  }

  // Invoking an Ash-Chrome version that predates the introduction of the
  // GetInstalledBrowserVersion API will result in this failure.
  DLOG(ERROR) << "Current lacros service does not support the browser version "
                 "api.";

  // We must return the current version as opposed to an invalid version so
  // that the InstalledVersionPoller can interpret that no update is
  // available.
  std::move(callback).Run(
      InstalledAndCriticalVersion(version_info::GetVersion()));
}
