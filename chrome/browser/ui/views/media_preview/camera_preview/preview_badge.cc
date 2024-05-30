// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/camera_preview/preview_badge.h"

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_class_properties.h"

namespace preview_badge {

std::unique_ptr<views::View> CreatePreviewBadge() {
  auto badge_view = std::make_unique<views::BoxLayoutView>();
  badge_view->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  badge_view->SetProperty(views::kMarginsKey, gfx::Insets(8));
  badge_view->SetFlipCanvasOnPaintForRTLUI(true);
  badge_view->SetBetweenChildSpacing(
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_VECTOR_ICON_PADDING));

  const int kLeftInset = 6;
  const int kRightInset = 8;
  const int kVerticalInset = 2;
  const int kBorderThickness = 0;
  const int kRoundedRadius =
      GetLayoutConstant(LOCATION_BAR_CHILD_CORNER_RADIUS);
  badge_view->SetBorder(views::CreateThemedRoundedRectBorder(
      kBorderThickness, kRoundedRadius,
      gfx::Insets::TLBR(kVerticalInset, kLeftInset, kVerticalInset,
                        kRightInset),
      ui::kColorSysTertiaryContainer));
  badge_view->SetBackground(views::CreateThemedRoundedRectBackground(
      ui::kColorSysTertiaryContainer, kRoundedRadius));

  const int kIconSize = 12;
  const auto& icon = vector_icons::kVideocamChromeRefreshIcon;
  const auto image_model = ui::ImageModel::FromVectorIcon(
      icon, ui::kColorSysOnTertiaryContainer, kIconSize);
  badge_view->AddChildView(
      std::make_unique<views::ImageView>(std::move(image_model)));

  const std::u16string text =
      l10n_util::GetStringUTF16(IDS_MEDIA_PREVIEW_VIDEO_STREAM_PREVIEW_BADGE);
  auto* text_label =
      badge_view->AddChildView(std::make_unique<views::Label>(text));
  text_label->SetTextStyle(views::style::TextStyle::STYLE_BODY_5);
  text_label->SetEnabledColorId(ui::kColorSysOnTertiaryContainer);
  text_label->GetViewAccessibility().SetIsIgnored(true);

  return badge_view;
}

}  // namespace preview_badge
