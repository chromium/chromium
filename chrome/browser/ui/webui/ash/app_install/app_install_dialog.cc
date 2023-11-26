// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/app_install/app_install_dialog.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/webui/ash/app_install/app_install.mojom.h"
#include "chrome/browser/ui/webui/ash/app_install/app_install_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/webapps/browser/installable/installable_data.h"
#include "components/webapps/common/constants.h"
#include "ui/gfx/text_elider.h"

namespace ash::app_install {

namespace {

const int kIconSize = 32;

// Gets the first `SkBitmap` larger than `kIconSize` from `iconMap`. If none
// exist, returns the largest bitmap in the map. Returns nullopt if map is
// empty.
std::optional<SkBitmap> GetDialogIcon(
    const std::map<web_app::SquareSizePx, SkBitmap>& icon_map) {
  if (icon_map.empty()) {
    return std::nullopt;
  }

  const SkBitmap* bitmap_ptr = nullptr;
  for (const auto& [size, bitmap] : icon_map) {
    bitmap_ptr = &bitmap;
    if (size >= kIconSize) {
      break;
    }
  }

  return *bitmap_ptr;
}
}  // namespace

ChromeOsAppInstallDialogParams::ChromeOsAppInstallDialogParams(
    std::unique_ptr<web_app::WebAppInstallInfo> web_app_info,
    std::vector<webapps::Screenshot> screenshots)
    : icon_bitmap(GetDialogIcon(web_app_info->icon_bitmaps.any)),
      name(base::UTF16ToUTF8(web_app_info->title)),
      url(web_app_info->start_url),
      description(base::UTF16ToUTF8(web_app_info->description)),
      screenshots(screenshots) {}

ChromeOsAppInstallDialogParams::~ChromeOsAppInstallDialogParams() = default;

// static
bool AppInstallDialog::Show(gfx::NativeWindow parent,
                            ChromeOsAppInstallDialogParams params) {
  CHECK(base::FeatureList::IsEnabled(
      chromeos::features::kCrosWebAppInstallDialog));
  // Allow no more than one upload dialog at a time.
  if (SystemWebDialogDelegate::HasInstance(
          GURL(chrome::kChromeUIAppInstallDialogURL))) {
    return false;
  }

  mojom::DialogArgsPtr args = mojom::DialogArgs::New();
  // TODO(crbug.com/1488697): Get app data passed in and set the dialog args.
  args->url = params.url;
  args->name = params.name;
  args->description = base::UTF16ToUTF8(gfx::TruncateString(
      base::UTF8ToUTF16(params.description), webapps::kMaximumDescriptionLength,
      gfx::CHARACTER_BREAK));

  // The pointer is managed by an instance of `views::WebDialogView` and removed
  // in `SystemWebDialogDelegate::OnDialogClosed`.
  AppInstallDialog* dialog = new AppInstallDialog(std::move(args));
  dialog->ShowSystemDialog(parent);
  return true;
}

void AppInstallDialog::OnDialogShown(content::WebUI* webui) {
  DCHECK(dialog_args_);
  static_cast<AppInstallDialogUI*>(webui->GetController())
      ->SetDialogArgs(std::move(dialog_args_));
}

AppInstallDialog::AppInstallDialog(mojom::DialogArgsPtr args)
    : SystemWebDialogDelegate(GURL(chrome::kChromeUIAppInstallDialogURL),
                              std::u16string() /* title */),
      dialog_args_(std::move(args)) {}

AppInstallDialog::~AppInstallDialog() = default;

bool AppInstallDialog::ShouldShowCloseButton() const {
  return false;
}

}  // namespace ash::app_install
