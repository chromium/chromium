// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/voice/voice_plate.h"

#include <algorithm>
#include <memory>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace voice {

VoicePlate::VoicePlate(views::View* anchor_view)
    : BubbleDialogDelegate(anchor_view, views::BubbleBorder::NONE) {
  SetBackgroundColor(ui::kColorBubbleBackground);
  DialogDelegate::SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  set_corner_radius(ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_TOAST_BUBBLE_HEIGHT));
  set_close_on_deactivate(false);
}

VoicePlate::~VoicePlate() = default;

void VoicePlate::Show() {
  CHECK(widget_);
  widget_->ShowInactive();
}

gfx::Rect VoicePlate::GetBubbleBounds() {
  views::View* anchor_view = GetAnchorView();
  if (!anchor_view || !GetWidget()) {
    return gfx::Rect();
  }

  const gfx::Size preferred_size =
      GetWidget()->GetContentsView()->GetPreferredSize();
  const gfx::Rect anchor_bounds = anchor_view->GetBoundsInScreen();

  const int minimum_margin = ChromeLayoutProvider::Get()->GetDistanceMetric(
                                 DISTANCE_TOAST_BUBBLE_BROWSER_WINDOW_MARGIN) -
                             views::BubbleBorder::kShadowBlur;
  const int width =
      std::min(preferred_size.width(),
               std::max(anchor_bounds.width() - 2 * minimum_margin, 0));
  const int x = anchor_bounds.x() + ((anchor_bounds.width() - width) / 2);

  // Overlap the bottom of the toolbar/omnibox by only a few pixels (e.g. 10
  // dip).
  constexpr int kOverlapAmount = 10;
  const int y = anchor_bounds.bottom() - kOverlapAmount;
  return gfx::Rect(x, y, width, preferred_size.height());
}

void VoicePlate::Init() {
  const auto* const layout_provider = ChromeLayoutProvider::Get();
  const gfx::Insets insets = layout_provider->GetInsetsMetric(
      views::InsetsMetric::INSETS_VECTOR_IMAGE_BUTTON);
  const int max_child_height = std::max(
      {layout_provider->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_HEIGHT_CONTENT),
       layout_provider->GetDistanceMetric(
           DISTANCE_TOAST_BUBBLE_HEIGHT_ACTION_BUTTON),
       layout_provider->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_ICON_SIZE) +
           insets.height()});

  const int total_vertical_margins =
      layout_provider->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_HEIGHT) -
      max_child_height;
  const int top_margin = total_vertical_margins / 2;

  set_margins(gfx::Insets::TLBR(
      top_margin,
      layout_provider->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_MARGIN_LEFT),
      total_vertical_margins - top_margin,
      layout_provider->GetDistanceMetric(
          DISTANCE_TOAST_BUBBLE_MARGIN_RIGHT_CLOSE_BUTTON)));
}

void VoicePlate::CreateWidget() {
  CHECK(!widget_);
  CHECK(GetContentsView());

  // TODO(crbug.com/509983464): Update this to call an undeprecated factory
  // function when this bug is fixed.
  widget_ =
      base::WrapUnique(views::BubbleDialogDelegate::CreateBubbleDeprecated(
          this, views::Widget::InitParams::CLIENT_OWNS_WIDGET));

  GetBubbleFrameView()->bubble_border()->set_draw_border_stroke(false);
}

}  // namespace voice
