// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_CELLULAR_SETUP_CELLULAR_SETUP_DIALOG_LAUNCHER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_CELLULAR_SETUP_CELLULAR_SETUP_DIALOG_LAUNCHER_H_

#include <string>

namespace chromeos {

namespace cellular_setup {

// Opens the cellular setup dialog for the cellular network with the provided
// GUID; if the dialog is already open, this function focuses it. Note that this
// function may open a different dialog depending on whether the
// kUpdatedCellularActivationUi flag is enabled.
void OpenCellularSetupDialog(const std::string& cellular_network_guid);

}  // namespace cellular_setup

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_CELLULAR_SETUP_CELLULAR_SETUP_DIALOG_LAUNCHER_H_
