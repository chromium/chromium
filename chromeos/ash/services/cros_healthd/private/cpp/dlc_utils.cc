// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/cros_healthd/private/cpp/dlc_utils.h"

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"

namespace ash::cros_healthd::internal {

namespace {

constexpr char kFioDlcId[] = "fio-dlc";

}  // namespace

void TriggerDlcInstall() {
  for (const auto& dlc_id : std::vector<std::string>{kFioDlcId}) {
    dlcservice::InstallRequest install_request;
    install_request.set_id(dlc_id);
    DlcserviceClient::Get()->Install(
        install_request,
        base::BindOnce(
            [](const std::string& dlc_id,
               const DlcserviceClient::InstallResult& install_result) {
              if (install_result.error != dlcservice::kErrorNone) {
                LOG(ERROR) << "Failed to install DLC (" << dlc_id
                           << "): " << install_result.error;
              }
            },
            dlc_id),
        /*progress_callback=*/base::DoNothing());
  }
}

}  // namespace ash::cros_healthd::internal
