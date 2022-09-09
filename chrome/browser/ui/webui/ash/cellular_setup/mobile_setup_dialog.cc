// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cellular_setup/mobile_setup_dialog.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/mobile/mobile_activator.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

namespace ash::cellular_setup {

namespace {

constexpr int kMobileSetupDialogWidth = 850;
constexpr int kMobileSetupDialogHeight = 650;

GURL GetURL(const NetworkState& network) {
  std::string url(chrome::kChromeUIMobileSetupURL);
  // TODO(stevenjb): Use GUID instead.
  url.append(network.path());
  return GURL(url);
}

MobileSetupDialog* dialog_instance = nullptr;

}  // namespace

// static
void MobileSetupDialog::ShowByNetworkId(const std::string& network_id) {
  if (dialog_instance) {
    NET_LOG(EVENT) << "Only one active MobileSetupDialog instance supported.";
    dialog_instance->dialog_window()->Focus();
    return;
  }
  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->GetNetworkStateFromGuid(
          network_id);
  if (!network) {
    NET_LOG(ERROR) << "MobileSetupDialog: Network ID not found: " << network_id;
    return;
  }
  NET_LOG(EVENT) << "Opening MobileSetupDialog, ID: " << NetworkId(network);
  dialog_instance = new MobileSetupDialog(*network);
  dialog_instance->ShowSystemDialog();
}

MobileSetupDialog::MobileSetupDialog(const NetworkState& network)
    : SystemWebDialogDelegate(
          GetURL(network),
          l10n_util::GetStringUTF16(IDS_MOBILE_SETUP_TITLE)) {
  set_can_resize(true);
}

MobileSetupDialog::~MobileSetupDialog() {
  dialog_instance = nullptr;
}

void MobileSetupDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kMobileSetupDialogWidth, kMobileSetupDialogHeight);
}

std::string MobileSetupDialog::GetDialogArgs() const {
  return std::string();
}

void MobileSetupDialog::OnCloseContents(content::WebContents* source,
                                        bool* out_close_dialog) {
  // If we're exiting, popping up the confirmation dialog can cause a
  // crash. Note: IsTryingToQuit can be cancelled on other platforms by the
  // onbeforeunload handler, except on ChromeOS. So IsTryingToQuit is the
  // appropriate check to use here.
  bool running_activation = MobileActivator::GetInstance()->RunningActivation();
  NET_LOG(EVENT) << "Closing MobileSetupDialog. Activation running = "
                 << running_activation;
  if (!dialog_window() || !running_activation ||
      browser_shutdown::IsTryingToQuit()) {
    *out_close_dialog = true;
    return;
  }

  *out_close_dialog = chrome::ShowQuestionMessageBoxSync(
      dialog_window(), l10n_util::GetStringUTF16(IDS_MOBILE_SETUP_TITLE),
      l10n_util::GetStringUTF16(IDS_MOBILE_CANCEL_ACTIVATION));
}

}  // namespace ash::cellular_setup
