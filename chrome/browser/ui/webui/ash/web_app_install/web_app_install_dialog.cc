// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/web_app_install/web_app_install_dialog.h"

#include "chrome/browser/ui/webui/ash/web_app_install/web_app_install.mojom.h"
#include "chrome/browser/ui/webui/ash/web_app_install/web_app_install_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/constants/chromeos_features.h"

namespace ash::web_app_install {

// static
bool WebAppInstallDialog::Show() {
  CHECK(base::FeatureList::IsEnabled(
      chromeos::features::kCrosWebAppInstallDialog));
  // Allow no more than one upload dialog at a time.
  if (SystemWebDialogDelegate::HasInstance(
          GURL(chrome::kChromeUIWebAppInstallDialogURL))) {
    return false;
  }

  mojom::DialogArgsPtr args = mojom::DialogArgs::New();
  // TODO(crbug.com/1488697): Get app data passed in and set the dialog args.

  // The pointer is managed by an instance of `views::WebDialogView` and removed
  // in `SystemWebDialogDelegate::OnDialogClosed`.
  WebAppInstallDialog* dialog = new WebAppInstallDialog(std::move(args));
  dialog->ShowSystemDialog();
  return true;
}

void WebAppInstallDialog::OnDialogShown(content::WebUI* webui) {
  DCHECK(dialog_args_);
  static_cast<WebAppInstallDialogUI*>(webui->GetController())
      ->SetDialogArgs(std::move(dialog_args_));
}

WebAppInstallDialog::WebAppInstallDialog(mojom::DialogArgsPtr args)
    : SystemWebDialogDelegate(GURL(chrome::kChromeUIWebAppInstallDialogURL),
                              std::u16string() /* title */),
      dialog_args_(std::move(args)) {}

WebAppInstallDialog::~WebAppInstallDialog() = default;

bool WebAppInstallDialog::ShouldShowCloseButton() const {
  return false;
}

}  // namespace ash::web_app_install
