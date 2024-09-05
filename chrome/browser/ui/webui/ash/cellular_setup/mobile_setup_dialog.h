// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_CELLULAR_SETUP_MOBILE_SETUP_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_CELLULAR_SETUP_MOBILE_SETUP_DIALOG_H_

#include "chrome/browser/ui/ash/network/network_connect_delegate.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"

namespace ash {

class NetworkState;

namespace cellular_setup {

// DEPRECATED: Being replaced by new UI; see https://crbug.com/778021.
class MobileSetupDialog : public SystemWebDialogDelegate {
 public:
  MobileSetupDialog(const MobileSetupDialog&) = delete;
  MobileSetupDialog& operator=(const MobileSetupDialog&) = delete;

 protected:
  explicit MobileSetupDialog(const NetworkState& network);
  ~MobileSetupDialog() override;

  // ui::WebDialogDelegate
  void GetDialogSize(gfx::Size* size) const override;
  std::string GetDialogArgs() const override;
  void OnCloseContents(content::WebContents* source,
                       bool* out_close_dialog) override;

 private:
  friend void NetworkConnectDelegate::ShowCarrierAccountDetail(
      const std::string& network_id);
  static void ShowByNetworkId(const std::string& network_id);
};

}  // namespace cellular_setup
}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_CELLULAR_SETUP_MOBILE_SETUP_DIALOG_H_
