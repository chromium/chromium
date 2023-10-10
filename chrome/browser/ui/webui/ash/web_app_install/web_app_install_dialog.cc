// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/web_app_install/web_app_install_dialog.h"

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

  // The pointer is managed by an instance of `views::WebDialogView` and removed
  // in `SystemWebDialogDelegate::OnDialogClosed`.
  WebAppInstallDialog* dialog = new WebAppInstallDialog();
  dialog->ShowSystemDialog();
  return true;
}

WebAppInstallDialog::WebAppInstallDialog()
    : SystemWebDialogDelegate(GURL(chrome::kChromeUIWebAppInstallDialogURL),
                              std::u16string() /* title */) {}

WebAppInstallDialog::~WebAppInstallDialog() = default;

bool WebAppInstallDialog::ShouldShowCloseButton() const {
  return false;
}

}  // namespace ash::web_app_install
