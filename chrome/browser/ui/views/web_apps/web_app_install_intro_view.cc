// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_install_intro_view.h"

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/views/controls/site_icon_text_and_origin_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_icon_name_and_origin_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_dialog_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "url/gurl.h"

namespace web_app {

// static
std::unique_ptr<WebAppInstallIntroView> WebAppInstallIntroView::Create(
    InstallDialogType install_type,
    const gfx::ImageSkia& icon_image,
    const std::u16string& app_name,
    const GURL& start_url,
    bool is_maskable,
    base::RepeatingCallback<void(const std::u16string&)>
        text_tracker_callback) {
  return base::WrapUnique(new WebAppInstallIntroView(
      install_type, icon_image, app_name, start_url, is_maskable,
      std::move(text_tracker_callback)));
}

WebAppInstallIntroView::WebAppInstallIntroView(
    InstallDialogType install_type,
    const gfx::ImageSkia& icon_image,
    const std::u16string& app_name,
    const GURL& start_url,
    bool is_maskable,
    base::RepeatingCallback<void(const std::u16string&)>
        text_tracker_callback) {
  int vertical_spacing = views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      vertical_spacing));

  // TODO(b/473080055): Use a translated string.
  auto* title = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(install_type == InstallDialogType::kDiy
                                    ? IDS_DIY_APP_INSTALL_DIALOG_TITLE
                                    : IDS_INSTALL_PWA_DIALOG_TITLE),
      views::style::CONTEXT_DIALOG_TITLE, views::style::STYLE_PRIMARY));
  title->SetMultiLine(false);
  title->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // TODO(b/473080055): Use a translated string.
  auto* description = AddChildView(std::make_unique<views::Label>(
      u"Access this site on a dedicated window on your computer",
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_SECONDARY));
  description->SetMultiLine(true);
  description->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  switch (install_type) {
    case InstallDialogType::kDiy:
      AddChildView(std::make_unique<SiteIconTextAndOriginView>(
          icon_image, app_name,
          l10n_util::GetStringUTF16(IDS_DIY_APP_AX_BUBBLE_NAME_LABEL),
          start_url, nullptr, std::move(text_tracker_callback)));
      break;
    case InstallDialogType::kDetailed:
      AddChildView(std::make_unique<views::Label>(
          u"TODO - extract and add ImageCarouselView",
          views::style::CONTEXT_DIALOG_BODY_TEXT,
          views::style::STYLE_SECONDARY));
      break;
    case InstallDialogType::kSimple:
      AddChildView(WebAppIconNameAndOriginView::Create(icon_image, app_name,
                                                       start_url, is_maskable));
      break;
  }
}

WebAppInstallIntroView::~WebAppInstallIntroView() = default;

BEGIN_METADATA(WebAppInstallIntroView)
END_METADATA

}  // namespace web_app
