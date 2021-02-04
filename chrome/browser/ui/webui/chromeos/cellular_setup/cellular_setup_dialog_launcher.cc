// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/cellular_setup/cellular_setup_dialog_launcher.h"

#include "ash/constants/ash_features.h"
#include "chrome/browser/ui/ash/system_tray_client.h"
#include "chrome/browser/ui/webui/chromeos/cellular_setup/mobile_setup_dialog.h"

namespace chromeos {

namespace cellular_setup {

void OpenCellularSetupDialog(const std::string& cellular_network_guid) {
  if (chromeos::features::IsCellularActivationUiEnabled())
    SystemTrayClient::Get()->ShowSettingsCellularSetup(/*show_psim_flow=*/true);
  else
    MobileSetupDialog::ShowByNetworkId(cellular_network_guid);
}

}  // namespace cellular_setup

}  // namespace chromeos
