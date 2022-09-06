// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/lens/lens_region_search_instructions_view.h"

#include "base/bind.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "components/lens/lens_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
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
constexpr int kLabelExtraLeftMargin = 2;
constexpr int kSelectionIconSize = 16;

int GetLensInstructionChipString() {
  if (features::UseAltChipString()) {
    return IDS_LENS_REGION_SEARCH_BUBBLE_TEXT_ALT2;
  }
  return features::IsLensInstructionChipImprovementsEnabled()
             ? IDS_LENS_REGION_SEARCH_BUBBLE_TEXT_ALT1
             : IDS_LENS_REGION_SEARCH_BUBBLE_TEXT;
}

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

  if (features::IsLensInstructionChipImprovementsEnabled()) {
    // Create a close button that is always white instead of conforming to
    // native theme.
    // TODO(crbug/1353948): Refactor/migrate this callback away from using
    // base::Passed.
    close_button_ = views::CreateVectorImageButton(base::BindRepeating(
        [](base::OnceClosure callback) {
          DCHECK(callback);
          std::move(callback).Run();
        },
        base::Passed(std::move(close_callback))));
  }

  // Create our own close button to align with label. We need to rebind our
  // OnceClosure to repeating due tot ImageButton::PressedCallback
  // inheritance. However, this callback should still only be called once and
  // this is verified with a DCHECK.
  if (!close_button_) {
    // TODO(crbug/1353948): Refactor/migrate this callback away from using
    // base::Passed.
    close_button_ = views::CreateVectorImageButtonWithNativeTheme(
        base::BindRepeating(
            [](base::OnceClosure callback) {
              DCHECK(callback);
              std::move(callback).Run();
            },
            base::Passed(std::move(close_callback))),
        views::kIcCloseIcon, kCloseButtonSize);
  }
  close_button_->SetTooltipText(l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE));
}

LensRegionSearchInstructionsView::~LensRegionSearchInstructionsView() = default;

void LensRegionSearchInstructionsView::Init() {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCollapseMargins(true);

  ChromeLayoutProvider* const layout_provider = ChromeLayoutProvider::Get();
  int left_margin =
      features::IsLensInstructionChipImprovementsEnabled()
          ? layout_provider->GetDistanceMetric(
                views::DistanceMetric::DISTANCE_RELATED_CONTROL_HORIZONTAL)
          : layout_provider->GetDistanceMetric(
                views::DistanceMetric::DISTANCE_RELATED_LABEL_HORIZONTAL) +
                kLabelExtraLeftMargin;
  set_margins(gfx::Insets::TLBR(
      layout_provider->GetInsetsMetric(views::InsetsMetric::INSETS_LABEL_BUTTON)
          .top(),
      left_margin,
      layout_provider->GetInsetsMetric(views::InsetsMetric::INSETS_LABEL_BUTTON)
          .bottom(),
      layout_provider->GetDistanceMetric(
          views::DistanceMetric::DISTANCE_CLOSE_BUTTON_MARGIN) +
          kCloseButtonExtraMargin));
  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_close_on_deactivate(false);
  set_corner_radius(kCornerRadius);

  // Add the leading drag selection icon if enabled.
  if (features::IsLensInstructionChipImprovementsEnabled()) {
    const gfx::VectorIcon& selection_icon =
        features::UseSelectionIconWithImage()
            ? views::kDragImageSelectionIcon
            : views::kDragGeneralSelectionIcon;
    auto selection_icon_view =
        std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
            selection_icon, kColorFeatureLensPromoBubbleForeground,
            kSelectionIconSize));
    AddChildView(std::move(selection_icon_view));
  }

  gfx::Font default_font;
  // We need to derive a font size delta between our desired font size and the
  // platform font size. There is no option to specify a constant font size in
  // the font list.
  int font_size_delta = kTextFontSize - default_font.GetFontSize();
  auto label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(GetLensInstructionChipString()));
  label->SetFontList(gfx::FontList().Derive(font_size_delta, gfx::Font::NORMAL,
                                            gfx::Font::Weight::MEDIUM));
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);
  label->SetVerticalAlignment(gfx::VerticalAlignment::ALIGN_MIDDLE);
  // If chip improvements are enabled, we force the label color to white.
  if (features::IsLensInstructionChipImprovementsEnabled()) {
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
  }
  label_ = AddChildView(std::move(label));

  close_button_->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  close_button_->SetProperty(
      views::kMarginsKey, gfx::Insets::TLBR(0, kCloseButtonExtraMargin, 0, 0));
  constructed_close_button_ = AddChildView(std::move(close_button_));
}

void LensRegionSearchInstructionsView::OnThemeChanged() {
  BubbleDialogDelegateView::OnThemeChanged();
  if (!features::IsLensInstructionChipImprovementsEnabled())
    return;
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

}  // namespace lens
