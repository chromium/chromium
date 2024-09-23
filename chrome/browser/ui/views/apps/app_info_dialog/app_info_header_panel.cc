// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_header_panel.h"

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/chrome_app_icon.h"
#include "chrome/browser/extensions/chrome_app_icon_service.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_label.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/icons/extension_icon_set.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/manifest_url_handlers.h"
#include "net/base/url_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace {

// Size of extension icon in top left of dialog.
const int kAppIconSize = 64;

}  // namespace

AppInfoHeaderPanel::AppInfoHeaderPanel(Profile* profile,
                                       const extensions::Extension* app)
    : AppInfoPanel(profile, app) {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      provider->GetInsetsMetric(views::INSETS_DIALOG_SUBSECTION),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));

  CreateControls();
}

AppInfoHeaderPanel::~AppInfoHeaderPanel() {}

void AppInfoHeaderPanel::OnIconUpdated(extensions::ChromeAppIcon* icon) {
  app_icon_view_->SetImage(ui::ImageModel::FromImageSkia(icon->image_skia()));
}

void AppInfoHeaderPanel::CreateControls() {
  auto app_icon_view = std::make_unique<views::ImageView>();
  app_icon_view->SetImageSize(gfx::Size(kAppIconSize, kAppIconSize));
  app_icon_view_ = AddChildView(std::move(app_icon_view));

  app_icon_ = extensions::ChromeAppIconService::Get(profile_)->CreateIcon(
      this, app_->id(), extension_misc::EXTENSION_ICON_LARGE);

  // Create a vertical container to store the app's name and link.
  auto vertical_info_container = std::make_unique<views::View>();
  auto vertical_container_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical);
  vertical_container_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  vertical_info_container->SetLayoutManager(
      std::move(vertical_container_layout));
  auto* vertical_info_container_ptr =
      AddChildView(std::move(vertical_info_container));

  auto app_name_label = std::make_unique<AppInfoLabel>(
      base::UTF8ToUTF16(app_->name()), views::style::CONTEXT_DIALOG_TITLE);
  auto* app_name_label_ptr =
      vertical_info_container_ptr->AddChildView(std::move(app_name_label));

  if (CanShowAppInWebStore()) {
    auto* view_in_store_link =
        vertical_info_container_ptr->AddChildView(std::make_unique<views::Link>(
            l10n_util::GetStringUTF16(IDS_APPLICATION_INFO_WEB_STORE_LINK)));
    view_in_store_link->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    view_in_store_link->SetCallback(base::BindRepeating(
        &AppInfoHeaderPanel::ShowAppInWebStore, base::Unretained(this)));
    view_in_store_link->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  } else {
    // If there's no link, allow the app's name to take up multiple lines.
    // TODO(sashab): Limit the number of lines to 2.
    app_name_label_ptr->SetMultiLine(true);
  }
}

void AppInfoHeaderPanel::ShowAppInWebStore() {
  DCHECK(CanShowAppInWebStore());
  Close();
  OpenLink(net::AppendQueryParameter(
      extensions::ManifestURL::GetDetailsURL(app_),
      extension_urls::kWebstoreSourceField,
      extension_urls::kLaunchSourceAppListInfoDialog));
}

bool AppInfoHeaderPanel::CanShowAppInWebStore() const {
  // Hide the webstore link for apps which were installed by default,
  // since this could leak user counts for OEM-specific apps.
  // Also hide Shared Modules because they are automatically installed
  // by Chrome when dependent Apps are installed.
  return app_->from_webstore() && !app_->was_installed_by_default() &&
         !app_->is_shared_module();
}

BEGIN_METADATA(AppInfoHeaderPanel)
END_METADATA
