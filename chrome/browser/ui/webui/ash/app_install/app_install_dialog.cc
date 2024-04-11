// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/app_install/app_install_dialog.h"

#include <cmath>
#include <utility>
#include <vector>

#include "ash/style/typography.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/package_id_util.h"
#include "chrome/browser/ui/webui/ash/app_install/app_install.mojom.h"
#include "chrome/browser/ui/webui/ash/app_install/app_install_page_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/webapps/common/constants.h"
#include "ui/aura/window.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"

namespace ash::app_install {

namespace {

// Amount of vertical padding from the top of the parent window to show the
// app install dialog. Chosen to overlap the search bar in browser as security
// measure to show that the dialog is not spoofed.
const int kPaddingFromParentTop = 75;

}  // namespace

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

void AppInstallDialog::ShowApp(
    Profile* profile,
    gfx::NativeWindow parent,
    apps::PackageId package_id,
    std::string app_name,
    GURL app_url,
    std::string app_description,
    GURL icon_url,
    int icon_width,
    bool is_icon_maskable,
    std::vector<mojom::ScreenshotPtr> screenshots,
    base::OnceCallback<void(bool accepted)> dialog_accepted_callback) {
  profile_ = profile->GetWeakPtr();

  if (parent) {
    parent_window_tracker_ = views::NativeWindowTracker::Create(parent);
  }
  parent_ = std::move(parent);

  package_id_ = std::move(package_id);
  dialog_args_ = ash::app_install::mojom::DialogArgs::New();
  dialog_args_->url = std::move(app_url);
  dialog_args_->name = std::move(app_name);
  dialog_args_->description = base::UTF16ToUTF8(gfx::TruncateString(
      base::UTF8ToUTF16(app_description), webapps::kMaximumDescriptionLength,
      gfx::CHARACTER_BREAK));
  dialog_args_->icon_url = std::move(icon_url);

  // Filter out portrait screenshots.
  dialog_args_->screenshots = std::move(screenshots);
  std::erase_if(dialog_args_->screenshots,
                [](const mojom::ScreenshotPtr& screenshot) {
                  return screenshot->size.width() < screenshot->size.height() ||
                         screenshot->size.width() == 0;
                });

  dialog_args_->is_already_installed =
      apps_util::GetAppWithPackageId(&*profile_, package_id_).has_value();

  dialog_accepted_callback_ = std::move(dialog_accepted_callback);

  icon_cache_ =
      std::make_unique<apps::AlmanacIconCache>(profile_.get()->GetProfileKey());
  icon_cache_->GetIcon(
      dialog_args_->icon_url,
      base::BindOnce(&AppInstallDialog::OnIconDownloaded,
                     weak_factory_.GetWeakPtr(), icon_width, is_icon_maskable));
}

void AppInstallDialog::ShowNoAppError(gfx::NativeWindow parent,
                                      base::OnceClosure try_again_callback) {
  try_again_callback_ = std::move(try_again_callback);
  ShowSystemDialog(parent);
  RepositionNearTopOf(parent);
}

void AppInstallDialog::OnIconDownloaded(int icon_width,
                                        bool is_icon_maskable,
                                        const gfx::Image& icon) {
  apps::IconValuePtr icon_value = std::make_unique<apps::IconValue>();
  icon_value->icon_type = apps::IconType::kStandard;
  icon_value->is_placeholder_icon = false;
  icon_value->is_maskable_icon = is_icon_maskable;
  icon_value->uncompressed = icon.AsImageSkia();

  apps::ApplyIconEffects(profile_.get(), /*app_id=*/std::nullopt,
                         apps::IconEffects::kCrOsStandardIcon, icon_width,
                         std::move(icon_value),
                         base::BindOnce(&AppInstallDialog::OnLoadIcon,
                                        weak_factory_.GetWeakPtr()));
}

void AppInstallDialog::OnLoadIcon(apps::IconValuePtr icon_value) {
  dialog_args_->icon_url =
      GURL(webui::GetBitmapDataUrl(*icon_value->uncompressed.bitmap()));
  this->set_dialog_modal_type(ui::MODAL_TYPE_WINDOW);

  gfx::NativeWindow parent =
      (parent_window_tracker_ &&
       parent_window_tracker_->WasNativeWindowDestroyed())
          ? nullptr
          : parent_;
  this->ShowSystemDialog(parent);
  this->RepositionNearTopOf(parent);
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

void AppInstallDialog::OnDialogShown(content::WebUI* webui) {
  CHECK_EQ(bool{dialog_args_}, bool{dialog_accepted_callback_});
  CHECK_NE(bool{dialog_args_}, bool{try_again_callback_});

  SystemWebDialogDelegate::OnDialogShown(webui);
  dialog_ui_ = static_cast<AppInstallDialogUI*>(webui->GetController());
  dialog_ui_->SetDialogArgs(dialog_args_.Clone());
  dialog_ui_->SetPackageId(package_id_);
  dialog_ui_->SetDialogCallback(std::move(dialog_accepted_callback_));
  dialog_ui_->SetTryAgainCallback(std::move(try_again_callback_));
}

void AppInstallDialog::CleanUpDialogIfNotShown() {
  if (!dialog_ui_) {
    delete this;
  }
}

bool AppInstallDialog::ShouldShowCloseButton() const {
  return false;
}

void AppInstallDialog::RepositionNearTopOf(gfx::NativeWindow parent) {
  if (!parent) {
    return;
  }

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

namespace {
constexpr int kNoAppDataHeight = 228;
constexpr int kMinimumDialogHeight = 282;
constexpr int kDescriptionContainerWidth = 408;
constexpr int kDescriptionLineHeight = 18;
constexpr int kDescriptionVerticalPadding = 24;
constexpr int kScreenshotPadding = 20;
constexpr int kDividerHeight = 1;
}  // namespace

void AppInstallDialog::GetDialogSize(gfx::Size* size) const {
  int height = 0;

  if (dialog_args_) {
    height += kMinimumDialogHeight;
    // TODO(b/329515116): Adjust height for long URLs that wrap multiple
    // lines.
    if (dialog_args_->description.length()) {
      const gfx::FontList font_list =
          TypographyProvider::Get()->ResolveTypographyToken(
              TypographyToken::kCrosAnnotation1);
      float description_width = gfx::GetStringWidth(
          base::UTF8ToUTF16(dialog_args_->description), font_list);
      int num_lines = std::ceil(description_width / kDescriptionContainerWidth);
      height += (kDescriptionLineHeight * num_lines);
    }
    if (!dialog_args_->screenshots.empty()) {
      // TODO(b/329515116): This won't work when we show more than one
      // screenshot, if the screenshots are different sizes. The screenshot is
      // displayed at a width of 408px, so calculate the height given that
      // width.
      CHECK(dialog_args_->screenshots[0]->size.width() != 0);
      height += std::ceil(dialog_args_->screenshots[0]->size.height() /
                          (dialog_args_->screenshots[0]->size.width() /
                           float(kDescriptionContainerWidth)));
      height += kScreenshotPadding;
    }
    if (dialog_args_->description.length() ||
        !dialog_args_->screenshots.empty()) {
      height += kDividerHeight;
      // The description padding is there even when there is no description.
      height += kDescriptionVerticalPadding;
    }
  } else {
    height += kNoAppDataHeight;
  }

  size->SetSize(SystemWebDialogDelegate::kDialogWidth, height);
}

}  // namespace ash::app_install
