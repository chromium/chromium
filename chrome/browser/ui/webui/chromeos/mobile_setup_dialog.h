// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_MOBILE_SETUP_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_MOBILE_SETUP_DIALOG_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"

namespace chromeos {

class NetworkState;

class MobileSetupDialog : public SystemWebDialogDelegate {
 public:
  static void ShowByNetworkId(const std::string& network_id);

 protected:
  explicit MobileSetupDialog(const NetworkState& network);
  ~MobileSetupDialog() override;

  // ui::WebDialogDelegate
  void GetDialogSize(gfx::Size* size) const override;
  std::string GetDialogArgs() const override;
  void OnCloseContents(content::WebContents* source,
                       bool* out_close_dialog) override;

  DISALLOW_COPY_AND_ASSIGN(MobileSetupDialog);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_MOBILE_SETUP_DIALOG_H_
