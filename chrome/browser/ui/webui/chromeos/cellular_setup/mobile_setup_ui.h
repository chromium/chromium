// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_CELLULAR_SETUP_MOBILE_SETUP_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_CELLULAR_SETUP_MOBILE_SETUP_UI_H_

#include "base/macros.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace chromeos {

namespace cellular_setup {

// A custom WebUI that defines datasource for mobile setup registration page
// that is used in Chrome OS activate modem and perform plan subscription tasks.
// This WebUI is being replaced and is only shown when the
// kUpdatedCellularActivationUi flag is disabled; see go/cros-cellular-design.
class MobileSetupUI : public ui::WebDialogUI {
 public:
  explicit MobileSetupUI(content::WebUI* web_ui);
  ~MobileSetupUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MobileSetupUI);
};

}  // namespace cellular_setup

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_CELLULAR_SETUP_MOBILE_SETUP_UI_H_
