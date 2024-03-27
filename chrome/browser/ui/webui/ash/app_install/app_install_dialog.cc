// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/app_install/app_install_dialog.h"

#include <cmath>

#include "ash/style/typography.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/webui/ash/app_install/app_install.mojom.h"
#include "chrome/browser/ui/webui/ash/app_install/app_install_page_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/webapps/common/constants.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"

namespace ash::app_install {

// static
bool AppInstallDialog::IsEnabled() {
  return base::FeatureList::IsEnabled(
             chromeos::features::kCrosWebAppInstallDialog) ||
         base::FeatureList::IsEnabled(
             chromeos::features::kCrosOmniboxInstallDialog) ||
         AppInstallPageHandler::GetAutoAcceptForTesting();
}

// static
base::WeakPtr<AppInstallDialog> AppInstallDialog::CreateDialog() {
  CHECK(IsEnabled());

  return (new AppInstallDialog())->GetWeakPtr();
}

AppInstallDialog::AppInstallDialog()
    : SystemWebDialogDelegate(GURL(chrome::kChromeUIAppInstallDialogURL),
                              /*title=*/u"") {}

AppInstallDialog::~AppInstallDialog() = default;

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
  dialog_ui_->SetDialogArgs(dialog_args_.Clone());
  dialog_ui_->SetExpectedAppId(std::move(expected_app_id_));
  dialog_ui_->SetDialogCallback(std::move(dialog_accepted_callback_));
}

void AppInstallDialog::CleanUpDialogIfNotShown() {
  if (!dialog_ui_) {
    delete this;
  }
}

bool AppInstallDialog::ShouldShowCloseButton() const {
  return false;
}

base::WeakPtr<AppInstallDialog> AppInstallDialog::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

namespace {
constexpr int kMinimumDialogHeight = 282;
constexpr int kDescriptionContainerWidth = 408;
constexpr int kDescriptionLineHeight = 18;
constexpr int kDescriptionVerticalPadding = 24;
constexpr int kScreenshotHeight = 256;
constexpr int kDividerHeight = 1;
}  // namespace

void AppInstallDialog::GetDialogSize(gfx::Size* size) const {
  int height = kMinimumDialogHeight;
  // TODO(b/329515116): Adjust height for long URLs that wrap multiple
  // lines.
  if (dialog_args_->description.length()) {
    const gfx::FontList font_list =
        TypographyProvider::Get()->ResolveTypographyToken(
            TypographyToken::kCrosAnnotation1);
    float description_width = gfx::GetStringWidth(
        base::UTF8ToUTF16(dialog_args_->description), font_list);
    int num_lines = std::ceil(description_width / kDescriptionContainerWidth);
    height +=
        ((kDescriptionLineHeight * num_lines) + kDescriptionVerticalPadding);
  }
  if (!dialog_args_->screenshot_urls.empty()) {
    // TODO(b/329515116): Account for different sized screenshots.
    height += kScreenshotHeight;
  }
  if (dialog_args_->description.length() ||
      !dialog_args_->screenshot_urls.empty()) {
    height += kDividerHeight;
  }

  size->SetSize(SystemWebDialogDelegate::kDialogWidth, height);
}

}  // namespace ash::app_install
