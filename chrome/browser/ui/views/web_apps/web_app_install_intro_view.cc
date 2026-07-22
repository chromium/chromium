// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_install_intro_view.h"

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/controls/site_icon_text_and_origin_view.h"
#include "chrome/browser/ui/views/web_apps/image_carousel_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_icon_name_and_origin_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_dialog_delegate.h"
#include "chrome/browser/web_applications/web_app_screenshot_fetcher.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "components/webapps/common/constants.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/screen.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_manager_base.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view_class_properties.h"
#include "url/gurl.h"

namespace web_app {

namespace {

// Spacing and radius for the detailed manifest view.
constexpr int kManifestDetailsPadding = 16;
constexpr int kManifestDetailsCornerRadius = 8;

}  // namespace

// static
std::unique_ptr<WebAppInstallIntroView> WebAppInstallIntroView::Create(
    InstallDialogType install_type,
    const gfx::ImageSkia& icon_image,
    const std::u16string& app_name,
    const GURL& start_url,
    bool is_maskable,
    const std::u16string& description,
    base::WeakPtr<WebAppScreenshotFetcher> fetcher,
    content::WebContents* web_contents,
    base::RepeatingCallback<void(const std::u16string&)>
        text_tracker_callback) {
  return base::WrapUnique(new WebAppInstallIntroView(
      install_type, icon_image, app_name, start_url, is_maskable, description,
      fetcher, web_contents, std::move(text_tracker_callback)));
}

WebAppInstallIntroView::WebAppInstallIntroView(
    InstallDialogType install_type,
    const gfx::ImageSkia& icon_image,
    const std::u16string& app_name,
    const GURL& start_url,
    bool is_maskable,
    const std::u16string& description,
    base::WeakPtr<WebAppScreenshotFetcher> fetcher,
    content::WebContents* web_contents,
    base::RepeatingCallback<void(const std::u16string&)>
        text_tracker_callback) {
  int vertical_spacing = views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      vertical_spacing));

  switch (install_type) {
    case InstallDialogType::kDiy: {
      auto site_icon_view = std::make_unique<SiteIconTextAndOriginView>(
          icon_image, app_name,
          l10n_util::GetStringUTF16(IDS_DIY_APP_AX_BUBBLE_NAME_LABEL),
          start_url, web_contents, std::move(text_tracker_callback));
      textfield_ = site_icon_view->title_field();
      AddChildView(std::move(site_icon_view));
      break;
    }
    case InstallDialogType::kDetailed: {
      CHECK(fetcher);

      // Highlighted box containing developer-provided metadata from the
      // manifest.
      auto manifest_details_container =
          std::make_unique<views::BoxLayoutView>();
      manifest_details_container->SetOrientation(
          views::BoxLayout::Orientation::kVertical);
      manifest_details_container->SetInsideBorderInsets(gfx::Insets::TLBR(
          kManifestDetailsPadding, 0, kManifestDetailsPadding, 0));
      manifest_details_container->SetBetweenChildSpacing(vertical_spacing);
      manifest_details_container->SetBackground(
          views::CreateRoundedRectBackground(ui::kColorSysNeutralContainer,
                                             kManifestDetailsCornerRadius));

      // Inner container for elements that need horizontal padding.
      auto padded_content = std::make_unique<views::BoxLayoutView>();
      padded_content->SetOrientation(views::BoxLayout::Orientation::kVertical);
      padded_content->SetInsideBorderInsets(
          gfx::Insets::VH(0, kManifestDetailsPadding));
      padded_content->SetBetweenChildSpacing(vertical_spacing);

      padded_content->AddChildView(WebAppIconNameAndOriginView::Create(
          icon_image, app_name, start_url, is_maskable));

      if (!description.empty()) {
        auto description_label = std::make_unique<views::Label>(description);
        description_label->SetMultiLine(true);
        description_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
        description_label->SetTextStyle(views::style::STYLE_SECONDARY);
        padded_content->AddChildView(std::move(description_label));
      }
      manifest_details_container->AddChildView(std::move(padded_content));

      // Carousel is added directly to main container to span full width.
      manifest_details_container->AddChildView(
          std::make_unique<ImageCarouselView>(
              fetcher, gfx::Insets::VH(0, kManifestDetailsPadding)));
      AddChildView(std::move(manifest_details_container));
      break;
    }
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
