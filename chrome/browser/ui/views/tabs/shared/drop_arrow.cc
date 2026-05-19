// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/shared/drop_arrow.h"

#include <utility>

#include "base/notreached.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/shadow_value.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/widget.h"

namespace {

gfx::ShadowValues GetShadowValues() {
  constexpr int kShadowBlur = 3;
  return {gfx::ShadowValue(gfx::Vector2d(0, 0), kShadowBlur,
                           SkColorSetA(SK_ColorBLACK, 0xFF))};
}

const gfx::VectorIcon& GetDropArrowIcon(DropArrow::Direction direction) {
  switch (direction) {
    case DropArrow::Direction::kUp:
      return features::IsRoundedIconsEnabled() ? kArrowUpwardIcon
                                               : kArrowUpwardOldIcon;
    case DropArrow::Direction::kDown:
      return features::IsRoundedIconsEnabled() ? kArrowDownwardIcon
                                               : kArrowDownwardOldIcon;
    case DropArrow::Direction::kLeft:
      return features::IsRoundedIconsEnabled() ? kArrowBackIcon
                                               : kArrowBackOldIcon;
    case DropArrow::Direction::kRight:
      return features::IsRoundedIconsEnabled() ? kArrowForwardIcon
                                               : kArrowForwardOldIcon;
    default:
      NOTREACHED();
  }
}

}  // namespace

DropArrow::DropArrow(const BrowserRootView::DropIndex& index,
                     gfx::NativeWindow context,
                     BoundsCallback bounds_callback)
    : index_(index), bounds_callback_(std::move(bounds_callback)) {
  arrow_widget_ = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.accept_events = false;
  params.bounds = gfx::Rect(kSize, kSize);
  params.context = context;
  arrow_widget_->Init(std::move(params));
  arrow_view_ =
      arrow_widget_->SetContentsView(std::make_unique<views::ImageView>());
  scoped_observation_.Observe(arrow_widget_.get());

  UpdateBounds();

  arrow_widget_->Show();
}

DropArrow::~DropArrow() {
  // Close eventually deletes the window, which deletes arrow_view_ too.
  if (arrow_widget_) {
    arrow_widget_->Close();
  }
}

// static
void DropArrow::MaybeAdjustDisplayBounds(gfx::Rect& display_bounds) {
#if BUILDFLAG(IS_LINUX)
  // On Linux, `GetBoundsInScreen` returns coordinates relative to the browser
  // window (plus shadow elevation outsets) rather than the screen. To handle
  // this, we adjust the display bounds by the difference between the drop arrow
  // size and the window elevation. Note that this only works on the first
  // display.
  const int elevation = views::LayoutProvider::Get()->GetShadowElevationMetric(
      views::Emphasis::kHigh);
  display_bounds.Outset(DropArrow::kSize - elevation);
#endif
}

void DropArrow::SetIndex(const BrowserRootView::DropIndex& index) {
  index_ = index;
  UpdateBounds();
}

void DropArrow::OnWidgetDestroying(views::Widget* widget) {
  DCHECK(scoped_observation_.IsObservingSource(arrow_widget_.get()));
  scoped_observation_.Reset();
  arrow_view_ = nullptr;
  arrow_widget_ = nullptr;
}

void DropArrow::UpdateBounds() {
  Direction direction;
  gfx::Rect drop_bounds = bounds_callback_.Run(index_, &direction);

  if (!direction_.has_value() || direction_ != direction) {
    direction_ = direction;
    gfx::ImageSkia icon = gfx::CreateVectorIcon(GetDropArrowIcon(*direction_),
                                                kSize, SK_ColorWHITE);
    arrow_view_->SetImage(ui::ImageModel::FromImageSkia(
        gfx::ImageSkiaOperations::CreateImageWithDropShadow(
            icon, GetShadowValues())));
    arrow_view_->SchedulePaint();
  }

  arrow_widget_->SetBounds(drop_bounds);
}
