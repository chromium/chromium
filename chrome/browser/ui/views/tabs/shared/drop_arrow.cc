// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/shared/drop_arrow.h"

#include <utility>

#include "chrome/grit/theme_resources.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/widget/widget.h"

namespace {

int GetDropArrowImageResourceId(bool is_down) {
  return is_down ? IDR_TAB_DROP_DOWN : IDR_TAB_DROP_UP;
}

}  // namespace

DropArrow::DropArrow(const BrowserRootView::DropIndex& index,
                     bool point_down,
                     views::Widget* context)
    : index_(index), point_down_(point_down) {
  arrow_window_ = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.accept_events = false;
  params.bounds = gfx::Rect(GetSize());
  params.context = context->GetNativeWindow();
  arrow_window_->Init(std::move(params));
  arrow_view_ =
      arrow_window_->SetContentsView(std::make_unique<views::ImageView>());
  arrow_view_->SetImage(
      ui::ImageModel::FromResourceId(GetDropArrowImageResourceId(point_down_)));
  scoped_observation_.Observe(arrow_window_.get());

  arrow_window_->Show();
}

DropArrow::~DropArrow() {
  // Close eventually deletes the window, which deletes arrow_view too.
  if (arrow_window_) {
    arrow_window_->Close();
  }
}

// static
gfx::Size DropArrow::GetSize() {
  static const gfx::Size size = []() {
    // Direction doesn't matter, both images are the same size.
    const gfx::ImageSkia* drop_image =
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            GetDropArrowImageResourceId(true));
    return gfx::Size(drop_image->width(), drop_image->height());
  }();
  return size;
}

void DropArrow::SetPointDown(bool down) {
  if (point_down_ == down) {
    return;
  }

  point_down_ = down;
  arrow_view_->SetImage(
      ui::ImageModel::FromResourceId(GetDropArrowImageResourceId(point_down_)));
}

void DropArrow::SetWindowBounds(const gfx::Rect& bounds) {
  arrow_window_->SetBounds(bounds);
}

void DropArrow::OnWidgetDestroying(views::Widget* widget) {
  DCHECK(scoped_observation_.IsObservingSource(arrow_window_.get()));
  scoped_observation_.Reset();
  arrow_window_ = nullptr;
}
