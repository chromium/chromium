// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/app_install/app_install_dialog.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/webui/ash/app_install/app_install.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/webapps/browser/installable/installable_data.h"
#include "components/webapps/common/constants.h"
#include "ui/gfx/text_elider.h"

namespace ash::app_install {

namespace {

const int kIconSize = 32;

// Gets the first icon larger than `kIconSize` from `manifest_icons` and returns
// the url. If none exist, returns the url of the largest icon. Returns empty
// GURL if vector is empty.
GURL GetIconUrl(std::vector<apps::IconInfo> manifest_icons) {
  if (manifest_icons.empty()) {
    return GURL::EmptyGURL();
  }

  GURL icon_url = GURL::EmptyGURL();
  for (const auto& icon_info : manifest_icons) {
    icon_url = icon_info.url;
    if (icon_info.square_size_px > kIconSize) {
      break;
    }
  }

  return icon_url;
}
}  // namespace

ChromeOsAppInstallDialogParams::ChromeOsAppInstallDialogParams(
    const web_app::WebAppInstallInfo& web_app_info,
    std::vector<webapps::Screenshot> screenshots)
    : icon_url(GetIconUrl(web_app_info.manifest_icons)),
      name(base::UTF16ToUTF8(web_app_info.title)),
      url(web_app_info.start_url),
      description(base::UTF16ToUTF8(web_app_info.description)),
      screenshots(screenshots) {}

ChromeOsAppInstallDialogParams::~ChromeOsAppInstallDialogParams() = default;

ChromeOsAppInstallDialogParams::ChromeOsAppInstallDialogParams(
    ChromeOsAppInstallDialogParams&&) = default;

// static
base::WeakPtr<AppInstallDialog> AppInstallDialog::CreateDialog() {
  CHECK(base::FeatureList::IsEnabled(
      chromeos::features::kCrosWebAppInstallDialog));

  return (new AppInstallDialog())->GetWeakPtr();
}

void AppInstallDialog::Show(
    gfx::NativeWindow parent,
    ChromeOsAppInstallDialogParams params,
    base::OnceCallback<void(bool accepted)> dialog_accepted_callback) {
  dialog_accepted_callback_ = std::move(dialog_accepted_callback);

  dialog_args_ = mojom::DialogArgs::New();
  dialog_args_->url = params.url;
  dialog_args_->name = params.name;
  dialog_args_->description = base::UTF16ToUTF8(gfx::TruncateString(
      base::UTF8ToUTF16(params.description), webapps::kMaximumDescriptionLength,
      gfx::CHARACTER_BREAK));
  dialog_args_->iconUrl = params.icon_url;

  this->ShowSystemDialog(parent);
}

void AppInstallDialog::SetInstallSuccess(bool success) {
  if (dialog_ui_) {
    dialog_ui_->SetInstallSuccess(success);
  }
}

void AppInstallDialog::OnDialogShown(content::WebUI* webui) {
  DCHECK(dialog_args_);
  DCHECK(dialog_accepted_callback_);
  SystemWebDialogDelegate::OnDialogShown(webui);
  dialog_ui_ = static_cast<AppInstallDialogUI*>(webui->GetController());
  dialog_ui_->SetDialogArgs(std::move(dialog_args_));
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
