// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/star_rating_view.h"

#include <cmath>
#include <string_view>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_class_properties.h"

StarRatingView::StarRatingView() {
  SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);

  rating_label_ = AddChildView(std::make_unique<views::Label>());
  rating_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  rating_label_->SetTextStyle(views::style::TextStyle::STYLE_BODY_4);
  rating_label_->SetEnabledColor(kColorPageInfoSubtitleForeground);

  const int distance_between_label_icons =
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_VECTOR_ICON_PADDING);
  rating_label_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(0, 0, 0, distance_between_label_icons));

  for (int i = 0; i < kStarCount; i++) {
    star_views_.push_back(
        AddChildView(std::make_unique<NonAccessibleImageView>()));
  }
}

StarRatingView::~StarRatingView() = default;

void StarRatingView::SetRating(double rating) {
  CHECK_LE(rating, kStarCount);

  // Round the rating to one decimal point to match the display value.
  // (Example: 3.97 will be rounded to 4.0 and should be displayed as 4 full
  // stars).
  double rounded_rating = std::round(rating * 10.0) / 10.0;
  rating_label_->SetText(base::UTF8ToUTF16(base::StringPrintf("%.1f", rating)));
  for (int i = 0; i < kStarCount; i++) {
    star_views_[i]->SetImage(GetImageModel(rounded_rating, i));
  }
}

ui::VectorIconModel StarRatingView::GetVectorIconModelForIndexForTesting(
    int index) const {
  return star_views_[index]->GetImageModel().GetVectorIcon();
}

std::u16string_view StarRatingView::GetTextForTesting() const {
  return rating_label_->GetText();
}

ui::ImageModel StarRatingView::GetImageModel(double rating, int index) {
  const int icon_size = GetLayoutConstant(STAR_RATING_ICON_SIZE);
  double rest;
  if (rating >= index + 1) {
    // Full icon.
    return ui::ImageModel::FromVectorIcon(vector_icons::kStarIcon,
                                          kColorStarRatingFullIcon, icon_size);
  } else if (rating >= index && std::modf(rating, &rest) >= 0.5) {
    // Half icon.
    return ui::ImageModel::FromVectorIcon(vector_icons::kStarHalfIcon,
                                          kColorStarRatingFullIcon, icon_size);
  }
  // Empty icon.
  return ui::ImageModel::FromVectorIcon(vector_icons::kStarIcon,
                                        kColorStarRatingEmptyIcon, icon_size);
}

BEGIN_METADATA(StarRatingView)
END_METADATA
