// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/default_browser/guided_setter_overlay_window_win.h"

#include <cmath>

#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/display/win/screen_win.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/win/hwnd_util.h"

namespace {

class OverlayArrowView : public views::View {
  METADATA_HEADER(OverlayArrowView, views::View)
 public:
  OverlayArrowView() = default;
  OverlayArrowView(const OverlayArrowView&) = delete;
  OverlayArrowView& operator=(const OverlayArrowView&) = delete;
  ~OverlayArrowView() override = default;

  static SkPath GetStemPath() {
    return SkPathBuilder()
        .moveTo(75.007f, 26.0467f)
        .lineTo(76.8242f, 28.8725f)
        .cubicTo(64.3189f, 37.2663f, 42.1f, 53.6201f, 7.63684f, 45.0465f)
        .lineTo(8.35546f, 41.7782f)
        .lineTo(9.07408f, 38.51f)
        .cubicTo(40.5679f, 46.3449f, 60.6419f, 31.643f, 73.1897f, 23.2208f)
        .lineTo(75.007f, 26.0467f)
        .close()
        .detach();
  }

  static SkPath GetHeadPath() {
    return SkPathBuilder()
        .moveTo(4.8164f, 42.8729f)
        .lineTo(2.78675f, 45.1224f)
        .lineTo(-0.000137253f, 42.4539f)
        .lineTo(3.10647f, 40.2981f)
        .lineTo(4.8164f, 42.8729f)
        .close()
        .moveTo(29.8877f, 66.8789f)
        .lineTo(27.8581f, 69.1284f)
        .lineTo(2.78675f, 45.1224f)
        .lineTo(4.8164f, 42.8729f)
        .lineTo(6.84604f, 40.6234f)
        .lineTo(31.9173f, 64.6294f)
        .lineTo(29.8877f, 66.8789f)
        .close()
        .moveTo(4.8164f, 42.8729f)
        .lineTo(3.10647f, 40.2981f)
        .lineTo(33.6391f, 19.1103f)
        .lineTo(35.349f, 21.6851f)
        .lineTo(37.0589f, 24.2599f)
        .lineTo(6.52632f, 45.4477f)
        .lineTo(4.8164f, 42.8729f)
        .close()
        .detach();
  }

  static gfx::Rect GetVisualBoundsDIP(const gfx::Point& end_dip) {
    SkRect path_bounds = GetStemPath().getBounds();
    path_bounds.join(GetHeadPath().getBounds());

    gfx::RectF arrow_rect(path_bounds.fLeft + end_dip.x(),
                          path_bounds.fTop + end_dip.y() - kArrowYOffsetDip,
                          path_bounds.width(), path_bounds.height());

    gfx::Rect bounds = gfx::ToEnclosingRect(arrow_rect);
    bounds.Outset(kPaddingDip);
    return bounds;
  }

  void SetEndpoints(const gfx::Point& start, const gfx::Point& end) {
    start_ = start;
    end_ = end;
    SchedulePaint();
  }

  void SetColor(SkColor color) {
    color_ = color;
    SchedulePaint();
  }

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    return GetVisualBoundsDIP(end_).size();
  }

  void OnPaint(gfx::Canvas* canvas) override {
    views::View::OnPaint(canvas);

    if (start_ == end_ || !GetColorProvider()) {
      return;
    }

    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(
        color_.value_or(GetColorProvider()->GetColor(ui::kColorAccent)));
    flags.setAntiAlias(true);

    gfx::ScopedCanvas scoped_canvas(canvas);
    canvas->Translate(gfx::Vector2d(end_.x(), end_.y() - kArrowYOffsetDip));
    canvas->DrawPath(GetStemPath(), flags);
    canvas->DrawPath(GetHeadPath(), flags);
  }

 private:
  static constexpr int kArrowYOffsetDip = 65;
  static constexpr int kPaddingDip = 15;

  gfx::Point start_;
  gfx::Point end_;

  std::optional<SkColor> color_;
};

BEGIN_METADATA(OverlayArrowView)
END_METADATA

}  // namespace

GuidedSetterOverlayWindowWin::GuidedSetterOverlayWindowWin(
    gfx::NativeWindow parent_context) {
  CreateWidget(parent_context);
}

GuidedSetterOverlayWindowWin::~GuidedSetterOverlayWindowWin() {
  arrow_view_ = nullptr;
  widget_.reset();
}

void GuidedSetterOverlayWindowWin::Hide() {
  widget_->Hide();
}

void GuidedSetterOverlayWindowWin::UpdateAndShow(const gfx::Point& start_screen,
                                                 const gfx::Point& end_screen) {
  const display::win::ScreenWin* screen_win = display::win::GetScreenWin();

  const gfx::Point start_dip =
      screen_win ? gfx::ToFlooredPoint(
                       screen_win->ScreenToDIPPoint(gfx::PointF(start_screen)))
                 : start_screen;
  const gfx::Point end_dip =
      screen_win ? gfx::ToFlooredPoint(
                       screen_win->ScreenToDIPPoint(gfx::PointF(end_screen)))
                 : end_screen;

  gfx::Rect bounds_dip = OverlayArrowView::GetVisualBoundsDIP(end_dip);
  widget_->SetBounds(bounds_dip);

  gfx::Point start_local = start_dip - bounds_dip.OffsetFromOrigin();
  gfx::Point end_local = end_dip - bounds_dip.OffsetFromOrigin();

  static_cast<OverlayArrowView*>(arrow_view_.get())
      ->SetEndpoints(start_local, end_local);

  widget_->ShowInactive();
}

void GuidedSetterOverlayWindowWin::SetArrowColor(SkColor color) {
  static_cast<OverlayArrowView*>(arrow_view_.get())->SetColor(color);
}

void GuidedSetterOverlayWindowWin::CreateWidget(
    gfx::NativeWindow parent_context) {
  widget_ = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
  params.accept_events = false;
  params.show_state = ui::mojom::WindowShowState::kInactive;
  params.parent = parent_context;
  params.name = "GuidedSetterOverlay";

  widget_->Init(std::move(params));

  arrow_view_ = widget_->SetContentsView(std::make_unique<OverlayArrowView>());
}
