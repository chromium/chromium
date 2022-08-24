// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/price_tracking_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets_outsets_base.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout_view.h"

namespace {
constexpr int kProductImageSize = 56;
constexpr int kLableSpacing = 4;
constexpr int kHorizontalSpacing = 16;
}  // namespace

PriceTrackingView::PriceTrackingView() {
  // image column
  auto product_image_containter = std::make_unique<views::BoxLayoutView>();
  product_image_containter->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  product_image_containter->SetProperty(
      views::kMarginsKey, gfx::Insets::TLBR(0, 0, 0, kHorizontalSpacing));
  // place holder product image
  auto* placeholder = product_image_containter->AddChildView(
      std::make_unique<views::ImageView>());
  placeholder->SetImageSize(gfx::Size(kProductImageSize, kProductImageSize));
  placeholder->SetPreferredSize(
      gfx::Size(kProductImageSize, kProductImageSize));
  // TODO(meiliang@): Use correct color and corner radius.
  placeholder->SetBorder(views::CreateRoundedRectBorder(
      1, 4, SkColorSetA(gfx::kGoogleGrey900, 0x24)));
  placeholder->SetBackground(
      views::CreateSolidBackground(SkColorSetA(gfx::kGoogleGrey900, 0x24)));
  AddChildView(std::move(product_image_containter));
  // TODO(meiliang): Use image fetcher to fetch image.

  // Text column
  auto text_container = std::make_unique<views::FlexLayoutView>();
  text_container->SetOrientation(views::LayoutOrientation::kVertical);
  // Title label
  auto* title_label =
      text_container->AddChildView(std::make_unique<views::Label>(
          l10n_util::GetStringUTF16(IDS_OMNIBOX_TRACK_PRICE_DIALOG_TITLE),
          views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_PRIMARY));
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  // Body label
  body_label_ = text_container->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_OMNIBOX_TRACK_PRICE_DIALOG_DESCRIPTION),
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_SECONDARY));
  body_label_->SetProperty(views::kMarginsKey,
                           gfx::Insets::TLBR(kLableSpacing, 0, 0, 0));
  body_label_->SetAllowCharacterBreak(true);
  body_label_->SetMultiLine(true);
  body_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(std::move(text_container));

  // Toggle button column
  toggle_button_ = AddChildView(std::make_unique<views::ToggleButton>());
  toggle_button_->SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_PRICE_TRACKING_TRACK_PRODUCT_ACCESSIBILITY));
  toggle_button_->SetProperty(views::kMarginsKey,
                              gfx::Insets::TLBR(0, kHorizontalSpacing, 0, 0));

  const int bubble_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH);

  const int label_width = bubble_width - kHorizontalSpacing * 4 -
                          kProductImageSize -
                          toggle_button_->GetPreferredSize().width();
  body_label_->SizeToFit(label_width);
}
