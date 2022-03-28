// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/lens/lens_region_search_instructions_view.h"

#include "base/bind.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents_observer.h"
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

LensRegionSearchInstructionsView::LensRegionSearchInstructionsView(
    views::View* anchor_view,
    base::OnceClosure close_callback,
    base::OnceClosure escape_callback)
    : views::BubbleDialogDelegateView(anchor_view,
                                      views::BubbleBorder::Arrow::TOP_CENTER) {
  // The cancel close_callback is called when VKEY_ESCAPE is hit.
  SetCancelCallback(std::move(escape_callback));

  // Create our own close button to align with label. We need to rebind our
  // OnceClosure to repeating due tot ImageButton::PressedCallback inheritance.
  // However, this callback should still only be called once and this is
  // verified with a DCHECK.
  close_button_ = views::CreateVectorImageButtonWithNativeTheme(
      base::BindRepeating(
          [](base::OnceClosure callback) {
            DCHECK(callback);
            std::move(callback).Run();
          },
          base::Passed(std::move(close_callback))),
      views::kIcCloseIcon, kCloseButtonSize);
  close_button_->SetTooltipText(l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE));
}

LensRegionSearchInstructionsView::~LensRegionSearchInstructionsView() = default;

void LensRegionSearchInstructionsView::Init() {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCollapseMargins(true);

  ChromeLayoutProvider* const layout_provider = ChromeLayoutProvider::Get();
  set_margins(gfx::Insets::TLBR(
      layout_provider->GetInsetsMetric(views::InsetsMetric::INSETS_LABEL_BUTTON)
          .top(),
      layout_provider->GetDistanceMetric(
          views::DistanceMetric::DISTANCE_RELATED_LABEL_HORIZONTAL) +
          kLabelExtraLeftMargin,
      layout_provider->GetInsetsMetric(views::InsetsMetric::INSETS_LABEL_BUTTON)
          .bottom(),
      layout_provider->GetDistanceMetric(
          views::DistanceMetric::DISTANCE_CLOSE_BUTTON_MARGIN) +
          kCloseButtonExtraMargin));
  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_close_on_deactivate(false);
  set_corner_radius(kCornerRadius);

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
  AddChildView(std::move(label));

  close_button_->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  close_button_->SetProperty(
      views::kMarginsKey, gfx::Insets::TLBR(0, kCloseButtonExtraMargin, 0, 0));
  AddChildView(std::move(close_button_));
}

gfx::Rect LensRegionSearchInstructionsView::GetBubbleBounds() {
  // Adjust the anchor_rect height to provide a margin between the anchor view
  // and the instruction view.
  gfx::Rect anchor_rect = GetAnchorRect();
  bool has_anchor = GetAnchorView() || anchor_rect != gfx::Rect();
  bool anchor_minimized = anchor_widget() && anchor_widget()->IsMinimized();
  gfx::Rect bubble_rect = GetBubbleFrameView()->GetUpdatedWindowBounds(
      anchor_rect, arrow(), GetWidget()->client_view()->GetPreferredSize(),
      !anchor_minimized && has_anchor);
  // Since we should be centered and positioned above the viewport, adjust the
  // bubble position to be within the viewport while also maintaining a margin
  // to the top of the viewport.
  bubble_rect.set_y(bubble_rect.y() + bubble_rect.height() +
                    ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_RELATED_CONTROL_VERTICAL_SMALL));
  return bubble_rect;
}

}  // namespace lens
