// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_INTERNET_DETAIL_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_INTERNET_DETAIL_DIALOG_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"  // nogncheck
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace chromeos {

class NetworkState;

class InternetDetailDialog : public SystemWebDialogDelegate {
 public:
  // Returns whether the dialog is being shown.
  static bool IsShown();

  // Shows an internet details dialog for |network_id|. If no NetworkState
  // exists for |network_id|, does nothing.
  static void ShowDialog(const std::string& network_id);

 protected:
  explicit InternetDetailDialog(const NetworkState& network);
  ~InternetDetailDialog() override;

  // SystemWebDialogDelegate
  const std::string& Id() override;

  // ui::WebDialogDelegate
  void GetDialogSize(gfx::Size* size) const override;
  std::string GetDialogArgs() const override;

 private:
  std::string network_id_;
  std::string network_type_;
  std::string network_name_;

  DISALLOW_COPY_AND_ASSIGN(InternetDetailDialog);
};

// A WebUI to host a subset of the network details page to allow setting of
// proxy, IP address, and nameservers in the login screen.
class InternetDetailDialogUI : public ui::MojoWebDialogUI {
 public:
  explicit InternetDetailDialogUI(content::WebUI* web_ui);
  ~InternetDetailDialogUI() override;

 private:
  void BindCrosNetworkConfig(
      mojo::PendingReceiver<chromeos::network_config::mojom::CrosNetworkConfig>
          receiver);

  DISALLOW_COPY_AND_ASSIGN(InternetDetailDialogUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_INTERNET_DETAIL_DIALOG_H_
