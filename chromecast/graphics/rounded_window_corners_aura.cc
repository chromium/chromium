// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/rounded_window_corners.h"

#include <vector>

#include "base/threading/thread_checker.h"
#include "chromecast/graphics/cast_window_manager.h"
#include "chromecast/ui/mojom/ui_service.mojom.h"
#include "ui/aura/window.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace chromecast {
namespace {

const int kCornerRadius = 10;

// A view that draws a black rounded corner. One should be used for each corner
// of the main view.
class BlackCornerView : public views::View {
 public:
  METADATA_HEADER(BlackCornerView);
  BlackCornerView(int radius, bool on_right, bool on_top)
      : radius_(radius), on_right_(on_right), on_top_(on_top) {}

  ~BlackCornerView() override {}

  void SetColorInversion(bool enable) {
    // In order to show as black we need to paint white when inversion is on.
    color_ = enable ? SK_ColorWHITE : SK_ColorBLACK;
    OnPropertyChanged(&color_, views::kPropertyEffectsPaint);
  }
  bool GetColorInversion() const { return color_ == SK_ColorWHITE; }

 private:
  void OnPaint(gfx::Canvas* canvas) override {
    // Draw a black rectangle over everything.
    canvas->DrawColor(color_);
    // Then clear out the inner corner.
    cc::PaintFlags flags;
    flags.setStrokeWidth(0);
    flags.setColor(color_);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setAntiAlias(true);
    flags.setBlendMode(SkBlendMode::kClear);
    gfx::PointF center_point(on_right_ ? radius_ : 0, on_top_ ? radius_ : 0);
    canvas->DrawCircle(center_point, radius_, flags);
  }

  void Layout() override {
    int x = on_right_ ? 0 : parent()->width() - radius_;
    int y = on_top_ ? 0 : parent()->height() - radius_;
    SetBounds(x, y, radius_, radius_);
  }

  SkColor color_ = SK_ColorBLACK;
  int radius_;
  bool on_right_;
  bool on_top_;
};

BEGIN_METADATA(BlackCornerView, views::View)
ADD_PROPERTY_METADATA(bool, ColorInversion)
END_METADATA

// Aura based implementation of RoundedWindowCorners.
class RoundedWindowCornersAura : public RoundedWindowCorners {
 public:
  explicit RoundedWindowCornersAura(CastWindowManager* window_manager);

  RoundedWindowCornersAura(const RoundedWindowCornersAura&) = delete;
  RoundedWindowCornersAura& operator=(const RoundedWindowCornersAura&) = delete;

  ~RoundedWindowCornersAura() override;

  void SetEnabled(bool enable) override;
  bool IsEnabled() const override;
  void SetColorInversion(bool enable) override;

 private:
  std::unique_ptr<views::Widget> widget_;
  views::LayoutProvider layout_provider_;

  std::vector<BlackCornerView*> corners_;

  THREAD_CHECKER(thread_checker_);
};

RoundedWindowCornersAura::RoundedWindowCornersAura(
    CastWindowManager* window_manager) {
  auto main_view = std::make_unique<views::View>();
  auto add_view = [this, &main_view](int radius, bool on_right, bool on_top) {
    BlackCornerView* view = new BlackCornerView(radius, on_right, on_top);
    main_view->AddChildView(view);
    corners_.push_back(view);
  };
  add_view(kCornerRadius, false, false);
  add_view(kCornerRadius, true, false);
  add_view(kCornerRadius, false, true);
  add_view(kCornerRadius, true, true);

  widget_ = std::make_unique<views::Widget>();
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.context = window_manager->GetRootWindow();
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.bounds = window_manager->GetRootWindow()->GetBoundsInRootWindow();
  params.accept_events = false;
  widget_->Init(std::move(params));
  widget_->SetContentsView(std::move(main_view));
  widget_->GetNativeWindow()->SetName("RoundCorners");

  window_manager->SetZOrder(widget_->GetNativeView(),
                            mojom::ZOrder::CORNERS_OVERLAY);

  // Remain hidden until explicitly shown. Rounded corners are only needed for
  // specific circumstances such as webviews.
  widget_->Hide();
}

RoundedWindowCornersAura::~RoundedWindowCornersAura() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void RoundedWindowCornersAura::SetEnabled(bool enable) {
  if (enable) {
    widget_->Show();
  } else {
    widget_->Hide();
  }
}

bool RoundedWindowCornersAura::IsEnabled() const {
  return widget_->IsVisible();
}

void RoundedWindowCornersAura::SetColorInversion(bool enable) {
  for (auto* view : corners_)
    view->SetColorInversion(enable);
}

}  // namespace

// static
std::unique_ptr<RoundedWindowCorners> RoundedWindowCorners::Create(
    CastWindowManager* window_manager) {
  return std::make_unique<RoundedWindowCornersAura>(window_manager);
}

}  // namespace chromecast
