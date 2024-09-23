// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/content_setting_site_row_view.h"

#include <memory>

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/favicon/core/favicon_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_provider.h"

ContentSettingSiteRowView::~ContentSettingSiteRowView() = default;

ContentSettingSiteRowView::ContentSettingSiteRowView(
    favicon::FaviconService* favicon_service,
    const net::SchemefulSite& site,
    bool allowed,
    ToggleCallback toggle_callback)
    : site_(site), toggle_callback_(toggle_callback) {
  auto* layout = SetLayoutManager(std::make_unique<views::FlexLayout>());

  const int favicon_margin = views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);

  const int icon_size = GetLayoutConstant(PAGE_INFO_ICON_SIZE);

  if (favicon_service) {
    favicon_ = AddChildView(std::make_unique<NonAccessibleImageView>());
    favicon_->SetImageSize({icon_size, icon_size});
    favicon_->SetProperty(views::kMarginsKey,
                          gfx::Insets().set_right(favicon_margin));
    // Fetch raw favicon to set |fallback_to_host| since we otherwise might
    // not get a result if the user never visited the root URL of |site|.
    favicon_service->GetRawFaviconForPageURL(
        site.GetURL(), {favicon_base::IconType::kFavicon}, icon_size,
        /*fallback_to_host=*/true,
        base::BindOnce(&ContentSettingSiteRowView::OnFaviconLoaded,
                       base::Unretained(this)),
        &favicon_tracker_);
  }

  auto title = url_formatter::FormatUrlForSecurityDisplay(
      site.GetURL(), url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);

  auto* title_label = AddChildView(std::make_unique<views::Label>(title));
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
  title_label->SetTextStyle(views::style::STYLE_BODY_3_MEDIUM);

  toggle_button_ = AddChildView(std::make_unique<views::ToggleButton>(
      base::BindRepeating(&ContentSettingSiteRowView::OnToggleButtonPressed,
                          base::Unretained(this))));
  toggle_button_->SetIsOn(allowed);
  toggle_button_->GetViewAccessibility().SetName(title);

  layout->SetInteriorMargin(ChromeLayoutProvider::Get()->GetInsetsMetric(
      ChromeInsetsMetric::INSETS_PAGE_INFO_HOVER_BUTTON));
}

void ContentSettingSiteRowView::OnToggleButtonPressed() {
  toggle_callback_.Run(site_, toggle_button_->GetIsOn());
}

void ContentSettingSiteRowView::OnFaviconLoaded(
    const favicon_base::FaviconRawBitmapResult& favicon_result) {
  if (favicon_result.is_valid()) {
    favicon_->SetImage(ui::ImageModel::FromImage(
        gfx::Image::CreateFrom1xPNGBytes(favicon_result.bitmap_data)));
  } else {
    favicon_->SetImage(ui::ImageModel::FromVectorIcon(
        kGlobeIcon, ui::kColorIcon, GetLayoutConstant(PAGE_INFO_ICON_SIZE)));
  }
}

BEGIN_METADATA(ContentSettingSiteRowView)
END_METADATA
