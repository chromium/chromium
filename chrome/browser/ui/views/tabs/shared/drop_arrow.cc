// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/shared/drop_arrow.h"

#include <utility>

#include "base/notreached.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/widget/widget.h"

namespace {

int GetDropArrowImageResourceId(DropArrow::Direction direction) {
  switch (direction) {
    case DropArrow::Direction::kUp:
      return IDR_TAB_DROP_UP;
    case DropArrow::Direction::kDown:
      return IDR_TAB_DROP_DOWN;
    case DropArrow::Direction::kLeft:
      return IDR_TAB_DROP_LEFT;
    case DropArrow::Direction::kRight:
      return IDR_TAB_DROP_RIGHT;
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
  params.bounds = gfx::Rect(GetSize());
  params.context = context;
  arrow_widget_->Init(std::move(params));
  arrow_view_ =
      arrow_widget_->SetContentsView(std::make_unique<views::ImageView>());
  scoped_observation_.Observe(arrow_widget_.get());

  UpdateBounds();

  arrow_widget_->Show();
}

DropArrow::~DropArrow() {
  // Close eventually deletes the window, which deletes arrow_view too.
  if (arrow_widget_) {
    arrow_widget_->Close();
  }
}

// static
gfx::Size DropArrow::GetSize() {
  static const gfx::Size size = []() {
    // Direction doesn't matter, both images are the same size.
    const gfx::ImageSkia* drop_image =
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            GetDropArrowImageResourceId(Direction::kDown));
    return gfx::Size(drop_image->width(), drop_image->height());
  }();
  return size;
}

void DropArrow::SetIndex(const BrowserRootView::DropIndex& index) {
  index_ = index;
  UpdateBounds();
}

void DropArrow::UpdateBounds() {
  Direction direction;
  gfx::Rect drop_bounds = bounds_callback_.Run(index_, &direction);

  if (!direction_.has_value() || direction_ != direction) {
    direction_ = direction;
    arrow_view_->SetImage(ui::ImageModel::FromResourceId(
        GetDropArrowImageResourceId(*direction_)));
    arrow_view_->SchedulePaint();
  }

  arrow_widget_->SetBounds(drop_bounds);
}

void DropArrow::OnWidgetDestroying(views::Widget* widget) {
  DCHECK(scoped_observation_.IsObservingSource(arrow_widget_.get()));
  scoped_observation_.Reset();
  arrow_view_ = nullptr;
  arrow_widget_ = nullptr;
}
