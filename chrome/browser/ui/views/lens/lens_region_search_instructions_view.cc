// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/lens/lens_region_search_instructions_view.h"

#include "base/functional/bind.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "components/lens/lens_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/vector_icons.h"

namespace lens {

// Spec states a font size of 14px.
constexpr int kTextFontSize = 14;
constexpr int kCloseButtonExtraMargin = 4;
constexpr int kCloseButtonSize = 17;
constexpr int kCornerRadius = 18;

LensRegionSearchInstructionsView::LensRegionSearchInstructionsView(
    views::View* anchor_view,
    base::OnceClosure close_callback,
    base::OnceClosure escape_callback)
    : views::BubbleDialogDelegateView(
          anchor_view,
          views::BubbleBorder::Arrow::BOTTOM_CENTER,
          views::BubbleBorder::Shadow::STANDARD_SHADOW) {
  // The cancel close_callback is called when VKEY_ESCAPE is hit.
  SetCancelCallback(std::move(escape_callback));

  // Create a close button that is always white instead of conforming to
  // native theme.
  close_button_ = views::CreateVectorImageButton(std::move(close_callback));
  close_button_->SetTooltipText(l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE));
}

LensRegionSearchInstructionsView::~LensRegionSearchInstructionsView() = default;

void LensRegionSearchInstructionsView::Init() {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCollapseMargins(true);

  ChromeLayoutProvider* const layout_provider = ChromeLayoutProvider::Get();
  int left_margin = layout_provider->GetDistanceMetric(
      views::DistanceMetric::DISTANCE_RELATED_CONTROL_HORIZONTAL);
  set_margins(gfx::Insets::TLBR(
      layout_provider->GetInsetsMetric(views::InsetsMetric::INSETS_LABEL_BUTTON)
          .top(),
      left_margin,
      layout_provider->GetInsetsMetric(views::InsetsMetric::INSETS_LABEL_BUTTON)
          .bottom(),
      layout_provider->GetDistanceMetric(
          views::DistanceMetric::DISTANCE_CLOSE_BUTTON_MARGIN) +
          kCloseButtonExtraMargin));
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  set_close_on_deactivate(false);
  set_corner_radius(kCornerRadius);

  // Add the leading drag selection icon.
  auto selection_icon_view =
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          views::kDragGeneralSelectionIcon,
          kColorFeatureLensPromoBubbleForeground,
          layout_provider->GetDistanceMetric(
              DISTANCE_BUBBLE_HEADER_VECTOR_ICON_SIZE)));
  AddChildView(std::move(selection_icon_view));

  gfx::Font default_font;
  // We need to derive a font size delta between our desired font size and the
  // platform font size. There is no option to specify a constant font size in
  // the font list.
  int font_size_delta = kTextFontSize - default_font.GetFontSize();
  auto label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_LENS_REGION_SEARCH_BUBBLE_TEXT));
  label->SetFontList(gfx::FontList().Derive(font_size_delta, gfx::Font::NORMAL,
                                            gfx::Font::Weight::MEDIUM));
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);
  label->SetVerticalAlignment(gfx::VerticalAlignment::ALIGN_MIDDLE);
  // Set label margins to vector icons in chips, including adjustments for the
  // extra margin that the close button sets below.
  label->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(
          0,
          layout_provider->GetDistanceMetric(
              views::DistanceMetric::DISTANCE_RELATED_LABEL_HORIZONTAL),
          0,
          layout_provider->GetDistanceMetric(
              views::DistanceMetric::DISTANCE_RELATED_LABEL_HORIZONTAL) -
              kCloseButtonExtraMargin));
  label_ = AddChildView(std::move(label));

  close_button_->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  close_button_->SetProperty(
      views::kMarginsKey, gfx::Insets::TLBR(0, kCloseButtonExtraMargin, 0, 0));
  // Make sure the hover background behind the button is a circle, rather than a
  // rounded square.
  views::InstallCircleHighlightPathGenerator(close_button_.get());
  constructed_close_button_ = AddChildView(std::move(close_button_));
}

void LensRegionSearchInstructionsView::OnThemeChanged() {
  BubbleDialogDelegateView::OnThemeChanged();
  const auto* const color_provider = GetColorProvider();
  auto foreground_color =
      color_provider->GetColor(kColorFeatureLensPromoBubbleForeground);
  auto background_color =
      color_provider->GetColor(kColorFeatureLensPromoBubbleBackground);

  set_color(background_color);
  label_->SetBackgroundColor(background_color);
  label_->SetEnabledColor(foreground_color);
  views::SetImageFromVectorIconWithColor(constructed_close_button_,
                                         views::kIcCloseIcon, kCloseButtonSize,
                                         foreground_color, foreground_color);
}

gfx::Rect LensRegionSearchInstructionsView::GetBubbleBounds() {
  gfx::Rect bubble_rect = views::BubbleDialogDelegateView::GetBubbleBounds();
  // Since we should be centered and positioned on top of the web view, adjust
  // the bubble position to contain a top margin to the top container view.
  bubble_rect.set_y(bubble_rect.y() + bubble_rect.height() +
                    ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_RELATED_CONTROL_VERTICAL_SMALL));
  return bubble_rect;
}

BEGIN_METADATA(LensRegionSearchInstructionsView)
END_METADATA

}  // namespace lens
