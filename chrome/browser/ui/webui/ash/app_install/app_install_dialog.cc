// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/app_install/app_install_dialog.h"

#include <cmath>
#include <utility>
#include <vector>

#include "ash/style/typography.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/almanac_api_client/almanac_app_icon_loader.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/package_id_util.h"
#include "chrome/browser/ui/webui/ash/app_install/app_install.mojom.h"
#include "chrome/browser/ui/webui/ash/app_install/app_install_page_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/webapps/common/constants.h"
#include "ui/aura/window.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"

namespace ash::app_install {

namespace {

// Amount of vertical padding from the top of the parent window to show the
// app install dialog. Chosen to overlap the search bar in browser as security
// measure to show that the dialog is not spoofed.
constexpr int kPaddingFromParentTop = 75;

constexpr int kErrorDialogHeight = 228;
constexpr int kMinimumDialogHeight = 282;
constexpr int kDescriptionContainerWidth = 408;
constexpr int kDescriptionLineHeight = 18;
constexpr int kDescriptionVerticalPadding = 24;
constexpr int kScreenshotPadding = 20;
constexpr int kDividerHeight = 1;

int GetDialogHeight(const AppInstallDialogArgs& dialog_args) {
  if (const AppInfoArgs* app_info_args =
          absl::get_if<AppInfoArgs>(&dialog_args)) {
    int height = kMinimumDialogHeight;
    // TODO(b/329515116): Adjust height for long URLs that wrap multiple
    // lines.
    if (app_info_args->data->description.length()) {
      const gfx::FontList font_list =
          TypographyProvider::Get()->ResolveTypographyToken(
              TypographyToken::kCrosAnnotation1);
      float description_width = gfx::GetStringWidth(
          base::UTF8ToUTF16(app_info_args->data->description), font_list);
      int num_lines = std::ceil(description_width / kDescriptionContainerWidth);
      height += (kDescriptionLineHeight * num_lines);
    }
    if (!app_info_args->data->screenshots.empty()) {
      // TODO(b/329515116): This won't work when we show more than one
      // screenshot, if the screenshots are different sizes. The screenshot is
      // displayed at a width of 408px, so calculate the height given that
      // width.
      CHECK(app_info_args->data->screenshots[0]->size.width() != 0);
      height += std::ceil(app_info_args->data->screenshots[0]->size.height() /
                          (app_info_args->data->screenshots[0]->size.width() /
                           float(kDescriptionContainerWidth)));
      height += kScreenshotPadding;
    }
    if (app_info_args->data->description.length() ||
        !app_info_args->data->screenshots.empty()) {
      height += kDividerHeight;
      // The description padding is there even when there is no description.
      height += kDescriptionVerticalPadding;
    }
    return height;
  }

  return kErrorDialogHeight;
}

}  // namespace

// static
base::WeakPtr<AppInstallDialog> AppInstallDialog::CreateDialog() {
  return (new AppInstallDialog())->GetWeakPtr();
}

void AppInstallDialog::ShowApp(
    Profile* profile,
    gfx::NativeWindow parent,
    apps::PackageId package_id,
    std::string app_name,
    GURL app_url,
    std::string app_description,
    std::optional<apps::AppInstallIcon> icon,
    std::vector<mojom::ScreenshotPtr> screenshots,
    base::OnceCallback<void(bool accepted)> dialog_accepted_callback) {
  profile_ = profile->GetWeakPtr();

  if (parent) {
    parent_window_tracker_ = views::NativeWindowTracker::Create(parent);
  }
  parent_ = std::move(parent);

  app_info_args_.package_id = std::move(package_id);
  app_info_args_.data = ash::app_install::mojom::AppInfoData::New();
  app_info_args_.data->url = std::move(app_url);
  app_info_args_.data->name = std::move(app_name);
  app_info_args_.data->description = base::UTF16ToUTF8(gfx::TruncateString(
      base::UTF8ToUTF16(app_description), webapps::kMaximumDescriptionLength,
      gfx::CHARACTER_BREAK));

  // Filter out portrait screenshots.
  app_info_args_.data->screenshots = std::move(screenshots);
  std::erase_if(app_info_args_.data->screenshots,
                [](const mojom::ScreenshotPtr& screenshot) {
                  return screenshot->size.width() < screenshot->size.height() ||
                         screenshot->size.width() == 0;
                });

  app_info_args_.data->is_already_installed =
      apps_util::GetAppWithPackageId(&*profile_, app_info_args_.package_id)
          .has_value();

  app_info_args_.dialog_accepted_callback = std::move(dialog_accepted_callback);

  if (!icon.has_value()) {
    Show(parent, std::move(app_info_args_));
    return;
  }

  icon_loader_ = std::make_unique<apps::AlmanacAppIconLoader>(*profile_.get());
  icon_loader_->GetAppIcon(icon->url, icon->mime_type, icon->is_masking_allowed,
                           base::BindOnce(&AppInstallDialog::OnAppIconLoaded,
                                          weak_factory_.GetWeakPtr()));
}

void AppInstallDialog::ShowNoAppError(gfx::NativeWindow parent) {
  Show(parent, NoAppErrorArgs());
}

void AppInstallDialog::ShowConnectionError(
    gfx::NativeWindow parent,
    base::OnceClosure try_again_callback) {
  ConnectionErrorArgs args;
  args.try_again_callback = std::move(try_again_callback);
  Show(parent, std::move(args));
}

void AppInstallDialog::SetInstallSucceeded() {
  if (dialog_ui_) {
    dialog_ui_->SetInstallComplete(/*success=*/true, std::nullopt);
  }
}

void AppInstallDialog::SetInstallFailed(
    base::OnceCallback<void(bool accepted)> retry_callback) {
  if (dialog_ui_) {
    dialog_ui_->SetInstallComplete(/*success=*/false,
                                   std::move(retry_callback));
  }
}

void AppInstallDialog::CleanUpDialogIfNotShown() {
  if (!dialog_ui_) {
    delete this;
  }
}

void AppInstallDialog::OnDialogShown(content::WebUI* webui) {
  CHECK(dialog_args_.has_value());

  SystemWebDialogDelegate::OnDialogShown(webui);
  dialog_ui_ = static_cast<AppInstallDialogUI*>(webui->GetController());
  dialog_ui_->SetDialogArgs(std::move(dialog_args_).value());
}

bool AppInstallDialog::ShouldShowCloseButton() const {
  return false;
}

void AppInstallDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(SystemWebDialogDelegate::kDialogWidth, dialog_height_);
}

AppInstallDialog::AppInstallDialog()
    : SystemWebDialogDelegate(GURL(chrome::kChromeUIAppInstallDialogURL),
                              /*title=*/u"") {}

AppInstallDialog::~AppInstallDialog() = default;

void AppInstallDialog::OnAppIconLoaded(apps::IconValuePtr icon_value) {
  icon_loader_.reset();

  if (icon_value) {
    app_info_args_.data->icon_url =
        GURL(webui::GetBitmapDataUrl(*icon_value->uncompressed.bitmap()));
  }

  gfx::NativeWindow parent =
      (parent_window_tracker_ &&
       parent_window_tracker_->WasNativeWindowDestroyed())
          ? nullptr
          : parent_;
  Show(parent, std::move(app_info_args_));
}

void AppInstallDialog::Show(gfx::NativeWindow parent,
                            AppInstallDialogArgs dialog_args) {
  dialog_args_ = std::move(dialog_args);
  dialog_height_ = GetDialogHeight(dialog_args_.value());

  if (absl::holds_alternative<AppInfoArgs>(dialog_args_.value())) {
    set_dialog_modal_type(ui::mojom::ModalType::kWindow);
  }

  ShowSystemDialog(parent);

  if (!parent) {
    return;
  }

  // Position near the top of the parent window.
  views::Widget* host_widget = views::Widget::GetWidgetForNativeWindow(parent);
  if (!host_widget) {
    return;
  }

  views::Widget* dialog_widget =
      views::Widget::GetWidgetForNativeWindow(dialog_window());

  gfx::Size size = dialog_widget->GetSize();

  int host_width = host_widget->GetWindowBoundsInScreen().width();
  int dialog_width = size.width();
  gfx::Point relative_dialog_position =
      gfx::Point(host_width / 2 - dialog_width / 2, kPaddingFromParentTop);

  gfx::Rect dialog_bounds(relative_dialog_position, size);

  const gfx::Rect absolute_bounds =
      dialog_bounds +
      host_widget->GetClientAreaBoundsInScreen().OffsetFromOrigin();

  dialog_widget->SetBounds(absolute_bounds);
}

base::WeakPtr<AppInstallDialog> AppInstallDialog::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace ash::app_install
