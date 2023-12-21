// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/app_install/app_install_dialog.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/webui/ash/app_install/app_install.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/webapps/common/constants.h"
#include "ui/gfx/text_elider.h"

namespace ash::app_install {

// static
base::WeakPtr<AppInstallDialog> AppInstallDialog::CreateDialog() {
  CHECK(base::FeatureList::IsEnabled(
            chromeos::features::kCrosWebAppInstallDialog) ||
        base::FeatureList::IsEnabled(
            chromeos::features::kCrosOmniboxInstallDialog));

  return (new AppInstallDialog())->GetWeakPtr();
}

void AppInstallDialog::Show(
    gfx::NativeWindow parent,
    mojom::DialogArgsPtr args,
    std::string expected_app_id,
    base::OnceCallback<void(bool accepted)> dialog_accepted_callback) {
  expected_app_id_ = std::move(expected_app_id);
  dialog_accepted_callback_ = std::move(dialog_accepted_callback);

  dialog_args_ = std::move(args);
  dialog_args_->description = base::UTF16ToUTF8(gfx::TruncateString(
      base::UTF8ToUTF16(dialog_args_->description),
      webapps::kMaximumDescriptionLength, gfx::CHARACTER_BREAK));

  this->ShowSystemDialog(parent);
}

void AppInstallDialog::SetInstallComplete(const std::string* app_id) {
  if (dialog_ui_) {
    dialog_ui_->SetInstallComplete(app_id);
  }
}

void AppInstallDialog::OnDialogShown(content::WebUI* webui) {
  DCHECK(dialog_args_);
  DCHECK(dialog_accepted_callback_);
  SystemWebDialogDelegate::OnDialogShown(webui);
  dialog_ui_ = static_cast<AppInstallDialogUI*>(webui->GetController());
  dialog_ui_->SetDialogArgs(std::move(dialog_args_));
  dialog_ui_->SetExpectedAppId(std::move(expected_app_id_));
  dialog_ui_->SetDialogCallback(std::move(dialog_accepted_callback_));
}

void AppInstallDialog::CleanUpDialogIfNotShown() {
  if (!dialog_ui_) {
    delete this;
  }
}

AppInstallDialog::AppInstallDialog()
    : SystemWebDialogDelegate(GURL(chrome::kChromeUIAppInstallDialogURL),
                              std::u16string() /* title */) {}

AppInstallDialog::~AppInstallDialog() = default;

bool AppInstallDialog::ShouldShowCloseButton() const {
  return false;
}

base::WeakPtr<AppInstallDialog> AppInstallDialog::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace ash::app_install
