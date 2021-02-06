// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/eye_dropper/eye_dropper_view.h"

#include "base/time/time.h"
#include "chrome/browser/ui/views/eye_dropper/eye_dropper.h"
#include "content/public/browser/desktop_capture.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/widget.h"

class EyeDropperView::ViewPositionHandler {
 public:
  explicit ViewPositionHandler(EyeDropperView* owner);
  ViewPositionHandler(const ViewPositionHandler&) = delete;
  ViewPositionHandler& operator=(const ViewPositionHandler&) = delete;
  ~ViewPositionHandler();

 private:
  void UpdateViewPosition();

  // Timer used for updating the window location.
  base::RepeatingTimer timer_;

  EyeDropperView* owner_;
};

EyeDropperView::ViewPositionHandler::ViewPositionHandler(EyeDropperView* owner)
    : owner_(owner) {
  // Use a value close to the refresh rate @60hz.
  // TODO(iopopesc): Use SetCapture instead of a timer when support for
  // activating the eye dropper without closing the color popup is added.
  timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(16), this,
               &EyeDropperView::ViewPositionHandler::UpdateViewPosition);
}

EyeDropperView::ViewPositionHandler::~ViewPositionHandler() {
  timer_.AbandonAndStop();
}

void EyeDropperView::ViewPositionHandler::UpdateViewPosition() {
  owner_->UpdatePosition();
}

class EyeDropperView::ScreenCapturer
    : public webrtc::DesktopCapturer::Callback {
 public:
  ScreenCapturer();
  ScreenCapturer(const ScreenCapturer&) = delete;
  ScreenCapturer& operator=(const ScreenCapturer&) = delete;
  ~ScreenCapturer() override = default;

  // webrtc::DesktopCapturer::Callback:
  void OnCaptureResult(webrtc::DesktopCapturer::Result result,
                       std::unique_ptr<webrtc::DesktopFrame> frame) override;

  SkBitmap GetBitmap() const;

 private:
  std::unique_ptr<webrtc::DesktopCapturer> capturer_;
  SkBitmap frame_;
};

EyeDropperView::ScreenCapturer::ScreenCapturer() {
  // TODO(iopopesc): Update the captured frame after a period of time to match
  // latest content on screen.
  capturer_ = content::desktop_capture::CreateScreenCapturer();
  capturer_->Start(this);
  capturer_->CaptureFrame();
}

void EyeDropperView::ScreenCapturer::OnCaptureResult(
    webrtc::DesktopCapturer::Result result,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  if (result != webrtc::DesktopCapturer::Result::SUCCESS)
    return;

  frame_.allocN32Pixels(frame->size().width(), frame->size().height(), true);
  memcpy(frame_.getAddr32(0, 0), frame->data(),
         frame->size().height() * frame->stride());
  frame_.setImmutable();
}

SkBitmap EyeDropperView::ScreenCapturer::GetBitmap() const {
  return frame_;
}

EyeDropperView::EyeDropperView(content::RenderFrameHost* frame,
                               content::EyeDropperListener* listener)
    : render_frame_host_(frame),
      listener_(listener),
      view_position_handler_(std::make_unique<ViewPositionHandler>(this)),
      screen_capturer_(std::make_unique<ScreenCapturer>()) {
  SetModalType(ui::MODAL_TYPE_WINDOW);
  SetOwnedByWidget(false);
  SetPreferredSize(GetSize());
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  // Use software compositing to prevent situations when the widget is not
  // translucent when moved fast.
  // TODO(iopopesc): Investigate if this is a compositor bug or this is indeed
  // an intentional limitation.
  params.force_software_compositing = true;
  params.z_order = ui::ZOrderLevel::kFloatingWindow;
  params.name = "MagnifierHost";
  params.parent = content::WebContents::FromRenderFrameHost(render_frame_host_)
                      ->GetNativeView();
  params.delegate = this;
  views::Widget* widget = new views::Widget();
  widget->Init(std::move(params));
  widget->SetContentsView(this);
  MoveViewToFront();
  HideCursor();
  pre_dispatch_handler_ = std::make_unique<PreEventDispatchHandler>(this);
  widget->Show();
  // The ignore selection time should be long enough to allow the user to see
  // the UI.
  ignore_selection_time_ =
      base::TimeTicks::Now() + base::TimeDelta::FromMilliseconds(500);
}

EyeDropperView::~EyeDropperView() {
  if (GetWidget())
    GetWidget()->CloseNow();
}

void EyeDropperView::OnPaint(gfx::Canvas* view_canvas) {
  if (screen_capturer_->GetBitmap().drawsNothing())
    return;

  const float diameter = GetDiameter();
  constexpr float kPixelSize = 10;
  const gfx::Size padding((size().width() - diameter) / 2,
                          (size().height() - diameter) / 2);

  if (GetWidget()->IsTranslucentWindowOpacitySupported()) {
    // Clip circle for magnified projection only when the widget
    // supports translucency.
    SkPath clip_path;
    clip_path.addOval(SkRect::MakeXYWH(padding.width(), padding.height(),
                                       diameter, diameter));
    clip_path.close();
    view_canvas->ClipPath(clip_path, true);
  }

  // Project pixels.
  const int pixel_count = diameter / kPixelSize;
  const SkBitmap frame = screen_capturer_->GetBitmap();
  // The captured frame is not scaled so we need to use widget's bounds in
  // pixels to have the magnified region match cursor position.
  const gfx::Point center_position =
      display::Screen::GetScreen()
          ->DIPToScreenRectInWindow(GetWidget()->GetNativeWindow(),
                                    GetWidget()->GetWindowBoundsInScreen())
          .CenterPoint();
  view_canvas->DrawImageInt(gfx::ImageSkia::CreateFrom1xBitmap(frame),
                            center_position.x() - pixel_count / 2,
                            center_position.y() - pixel_count / 2, pixel_count,
                            pixel_count, padding.width(), padding.height(),
                            diameter, diameter, false);

  // Store the pixel color under the cursor as it is the last color seen
  // by the user before selection.
  selected_color_ = frame.getColor(center_position.x(), center_position.y());

  // Paint grid.
  cc::PaintFlags flags;
  flags.setStrokeWidth(1);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  // TODO(iopopesc): Get all colors from theming object.
  flags.setColor(SK_ColorGRAY);
  for (int i = 0; i < pixel_count; ++i) {
    view_canvas->DrawLine(
        gfx::PointF(padding.width() + i * kPixelSize, padding.height()),
        gfx::PointF(padding.width() + i * kPixelSize,
                    size().height() - padding.height()),
        flags);
    view_canvas->DrawLine(
        gfx::PointF(padding.width(), padding.height() + i * kPixelSize),
        gfx::PointF(size().width() - padding.width(),
                    padding.height() + i * kPixelSize),
        flags);
  }

  // Paint central pixel.
  gfx::RectF pixel((size().width() - kPixelSize) / 2,
                   (size().height() - kPixelSize) / 2, kPixelSize, kPixelSize);
  flags.setAntiAlias(true);
  flags.setColor(SK_ColorWHITE);
  flags.setStrokeWidth(2);
  pixel.Inset(-0.5f, -0.5f);
  view_canvas->DrawRect(pixel, flags);
  flags.setColor(SK_ColorBLACK);
  flags.setStrokeWidth(1);
  pixel.Inset(0.5f, 0.5f);
  view_canvas->DrawRect(pixel, flags);

  // Paint outline.
  flags.setStrokeWidth(2);
  flags.setColor(SK_ColorDKGRAY);
  flags.setAntiAlias(true);
  if (GetWidget()->IsTranslucentWindowOpacitySupported()) {
    view_canvas->DrawCircle(
        gfx::PointF(size().width() / 2, size().height() / 2), diameter / 2,
        flags);
  } else {
    view_canvas->DrawRect(bounds(), flags);
  }

  OnPaintBorder(view_canvas);
}

void EyeDropperView::WindowClosing() {
  ShowCursor();
  pre_dispatch_handler_.reset();
}

void EyeDropperView::OnWidgetMove() {
  // Trigger a repaint since because the widget was moved, the content of the
  // view needs to be updated.
  SchedulePaint();
}

void EyeDropperView::UpdatePosition() {
  if (screen_capturer_->GetBitmap().drawsNothing() || !GetWidget())
    return;

  gfx::Point cursor_position =
      display::Screen::GetScreen()->GetCursorScreenPoint();
  if (cursor_position == GetWidget()->GetWindowBoundsInScreen().CenterPoint())
    return;

  GetWidget()->SetBounds(
      gfx::Rect(gfx::Point(cursor_position.x() - size().width() / 2,
                           cursor_position.y() - size().height() / 2),
                size()));
}

void EyeDropperView::OnColorSelected() {
  if (!selected_color_.has_value()) {
    listener_->ColorSelectionCanceled();
    return;
  }

  // Prevent the user from selecting a color for a period of time.
  if (base::TimeTicks::Now() <= ignore_selection_time_)
    return;

  // Use the last selected color and notify listener.
  listener_->ColorSelected(selected_color_.value());
}
