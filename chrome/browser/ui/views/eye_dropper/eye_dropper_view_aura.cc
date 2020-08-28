// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/eye_dropper/eye_dropper_view_aura.h"

#include "base/time/time.h"
#include "chrome/browser/ui/views/eye_dropper/eye_dropper.h"
#include "content/public/browser/desktop_capture.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/window.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/widget.h"

class EyeDropperViewAura::PreEventDispatchHandler : public ui::EventHandler {
 public:
  explicit PreEventDispatchHandler(EyeDropperViewAura* view);
  PreEventDispatchHandler(const PreEventDispatchHandler&) = delete;
  PreEventDispatchHandler& operator=(const PreEventDispatchHandler&) = delete;
  ~PreEventDispatchHandler() override;

 private:
  void OnMouseEvent(ui::MouseEvent* event) override;

  EyeDropperViewAura* view_;
};

EyeDropperViewAura::PreEventDispatchHandler::PreEventDispatchHandler(
    EyeDropperViewAura* view)
    : view_(view) {
  // Ensure that this handler is called before color popup handler by using
  // a higher priority.
  view->GetWidget()->GetNativeWindow()->AddPreTargetHandler(
      this, ui::EventTarget::Priority::kSystem);
}

EyeDropperViewAura::PreEventDispatchHandler::~PreEventDispatchHandler() {
  view_->GetWidget()->GetNativeWindow()->RemovePreTargetHandler(this);
}

void EyeDropperViewAura::PreEventDispatchHandler::OnMouseEvent(
    ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED) {
    // Avoid closing the color popup when the eye dropper is clicked.
    event->StopPropagation();
  }
  if (event->type() == ui::ET_MOUSE_RELEASED) {
    view_->OnColorSelected();
    event->StopPropagation();
  }
}

class EyeDropperViewAura::ViewPositionHandler {
 public:
  explicit ViewPositionHandler(EyeDropperViewAura* owner);
  ViewPositionHandler(const ViewPositionHandler&) = delete;
  ViewPositionHandler& operator=(const ViewPositionHandler&) = delete;
  ~ViewPositionHandler();

 private:
  void UpdateViewPosition();

  // Timer used for updating the window location.
  base::RepeatingTimer timer_;

  EyeDropperViewAura* owner_;
};

EyeDropperViewAura::ViewPositionHandler::ViewPositionHandler(
    EyeDropperViewAura* owner)
    : owner_(owner) {
  // Use a value close to the refresh rate @60hz.
  // TODO(iopopesc): Use SetCapture instead of a timer when support for
  // activating the eye dropper without closing the color popup is added.
  timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(16), this,
               &EyeDropperViewAura::ViewPositionHandler::UpdateViewPosition);
}

EyeDropperViewAura::ViewPositionHandler::~ViewPositionHandler() {
  timer_.AbandonAndStop();
}

void EyeDropperViewAura::ViewPositionHandler::UpdateViewPosition() {
  owner_->UpdatePosition();
}

class EyeDropperViewAura::ScreenCapturer
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

EyeDropperViewAura::ScreenCapturer::ScreenCapturer() {
  // TODO(iopopesc): Update the captured frame after a period of time to match
  // latest content on screen.
  capturer_ = content::desktop_capture::CreateScreenCapturer();
  capturer_->Start(this);
  capturer_->CaptureFrame();
}

void EyeDropperViewAura::ScreenCapturer::OnCaptureResult(
    webrtc::DesktopCapturer::Result result,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  if (result != webrtc::DesktopCapturer::Result::SUCCESS)
    return;

  frame_.allocN32Pixels(frame->size().width(), frame->size().height(), true);
  memcpy(frame_.getAddr32(0, 0), frame->data(),
         frame->size().height() * frame->stride());
  frame_.setImmutable();
}

SkBitmap EyeDropperViewAura::ScreenCapturer::GetBitmap() const {
  return frame_;
}

EyeDropperViewAura::EyeDropperViewAura(content::RenderFrameHost* frame,
                                       content::EyeDropperListener* listener)
    : render_frame_host_(frame),
      listener_(listener),
      view_position_handler_(std::make_unique<ViewPositionHandler>(this)),
      screen_capturer_(std::make_unique<ScreenCapturer>()) {
  SetOwnedByWidget(false);
  SetPreferredSize(gfx::Size(100, 100));
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
                      ->GetTopLevelNativeWindow();
  params.delegate = this;
  views::Widget* widget = new views::Widget();
  widget->Init(std::move(params));
  widget->SetContentsView(this);

  pre_dispatch_handler_ = std::make_unique<PreEventDispatchHandler>(this);

  auto* cursor_client =
      aura::client::GetCursorClient(widget->GetNativeWindow()->GetRootWindow());
  cursor_client->HideCursor();
  cursor_client->LockCursor();
  widget->Show();
}

EyeDropperViewAura::~EyeDropperViewAura() {
  if (GetWidget())
    GetWidget()->CloseNow();
}

void EyeDropperViewAura::OnPaint(gfx::Canvas* view_canvas) {
  if (screen_capturer_->GetBitmap().drawsNothing())
    return;

  constexpr float kDiameter = 90;
  constexpr float kPixelSize = 10;
  const gfx::Size padding((size().width() - kDiameter) / 2,
                          (size().height() - kDiameter) / 2);

  // Clip circle for magnified projection.
  SkPath clip_path;
  clip_path.addOval(SkRect::MakeXYWH(padding.width(), padding.height(),
                                     kDiameter, kDiameter));
  clip_path.close();
  view_canvas->ClipPath(clip_path, true);

  // Project pixels.
  constexpr int kPixelCount = kDiameter / kPixelSize;
  const SkBitmap frame = screen_capturer_->GetBitmap();
  // The captured frame is not scaled so we need to use widget's bounds in
  // pixels to have the magnified region match cursor position.
  const gfx::Point center_position =
      display::Screen::GetScreen()
          ->DIPToScreenRectInWindow(GetWidget()->GetNativeWindow(),
                                    GetWidget()->GetWindowBoundsInScreen())
          .CenterPoint();
  view_canvas->DrawImageInt(gfx::ImageSkia::CreateFrom1xBitmap(frame),
                            center_position.x() - kPixelCount / 2,
                            center_position.y() - kPixelCount / 2, kPixelCount,
                            kPixelCount, padding.width(), padding.height(),
                            kDiameter, kDiameter, false);

  // Store the pixel color under the cursor as it is the last color seen
  // by the user before selection.
  selected_color_ = frame.getColor(center_position.x(), center_position.y());

  // Paint grid.
  cc::PaintFlags flags;
  flags.setStrokeWidth(1);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  // TODO(iopopesc): Get all colors from theming object.
  flags.setColor(SK_ColorGRAY);
  for (int i = 0; i < kPixelCount; ++i) {
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

  // Paint central pixel in red.
  gfx::RectF pixel((size().width() - kPixelSize) / 2,
                   (size().height() - kPixelSize) / 2, kPixelSize, kPixelSize);
  flags.setColor(SK_ColorRED);
  view_canvas->DrawRect(pixel, flags);

  // Paint outline.
  flags.setStrokeWidth(2);
  flags.setColor(SK_ColorDKGRAY);
  flags.setAntiAlias(true);
  view_canvas->DrawCircle(gfx::PointF(size().width() / 2, size().height() / 2),
                          kDiameter / 2, flags);

  OnPaintBorder(view_canvas);
}

void EyeDropperViewAura::WindowClosing() {
  aura::client::GetCursorClient(GetWidget()->GetNativeWindow()->GetRootWindow())
      ->UnlockCursor();
  pre_dispatch_handler_.reset();
}

ui::ModalType EyeDropperViewAura::GetModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

void EyeDropperViewAura::OnWidgetMove() {
  // Trigger a repaint since because the widget was moved, the content of the
  // view needs to be updated.
  SchedulePaint();
}

void EyeDropperViewAura::UpdatePosition() {
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

void EyeDropperViewAura::OnColorSelected() {
  if (!selected_color_.has_value()) {
    listener_->ColorSelectionCanceled();
    return;
  }

  // Use the last selected color and notify listener.
  listener_->ColorSelected(selected_color_.value());
}

std::unique_ptr<content::EyeDropper> ShowEyeDropper(
    content::RenderFrameHost* frame,
    content::EyeDropperListener* listener) {
  return std::make_unique<EyeDropperViewAura>(frame, listener);
}
