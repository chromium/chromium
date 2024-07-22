// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/eye_dropper/eye_dropper_view.h"

#include <algorithm>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/screen.h"
#include "ui/views/native_window_tracker.h"

namespace eye_dropper {
namespace {
gfx::Point ClampToDisplay(const gfx::Point& point) {
  gfx::Rect bounds =
      display::Screen::GetScreen()->GetDisplayNearestPoint(point).bounds();
  return gfx::Point(std::clamp(point.x(), bounds.x(), bounds.right() - 1),
                    std::clamp(point.y(), bounds.y(), bounds.bottom() - 1));
}
}  // namespace

class EyeDropperView::PreEventDispatchHandler::KeyboardHandler
    : public ui::EventHandler {
 public:
  KeyboardHandler(EyeDropperView* view, aura::Window* parent);
  KeyboardHandler(const KeyboardHandler&) = delete;
  KeyboardHandler& operator=(const KeyboardHandler&) = delete;
  ~KeyboardHandler() override;

 private:
  void OnKeyEvent(ui::KeyEvent* event) override;

  raw_ptr<EyeDropperView> view_;
  raw_ptr<aura::Window> parent_;
  std::unique_ptr<views::NativeWindowTracker> parent_tracker_;
};

EyeDropperView::PreEventDispatchHandler::KeyboardHandler::KeyboardHandler(
    EyeDropperView* view,
    aura::Window* parent)
    : view_(view),
      parent_(parent),
      parent_tracker_(views::NativeWindowTracker::Create(parent)) {
  // Because the eye dropper is not focused in order to not dismiss the color
  // popup, we need to listen for key events on the parent window that has
  // focus.
  parent_->AddPreTargetHandler(this, ui::EventTarget::Priority::kSystem);
}

EyeDropperView::PreEventDispatchHandler::KeyboardHandler::~KeyboardHandler() {
  if (!parent_tracker_->WasNativeWindowDestroyed()) {
    parent_->RemovePreTargetHandler(this);
  }
}

void EyeDropperView::PreEventDispatchHandler::KeyboardHandler::OnKeyEvent(
    ui::KeyEvent* event) {
  if (event->type() != ui::EventType::kKeyPressed) {
    return;
  }

  // Move when arrow keys are pressed, move faster if shift is down.
  const auto move_by = [&](int x, int y) {
    if (event->IsShiftDown()) {
      x *= 10;
      y *= 10;
    }
    const auto center =
        view_->GetWidget()->GetWindowBoundsInScreen().CenterPoint();
    view_->UpdatePosition(ClampToDisplay(center + gfx::Vector2d(x, y)));
  };

  switch (event->key_code()) {
    case ui::VKEY_ESCAPE:
      view_->OnColorSelectionCanceled();
      break;
    case ui::VKEY_RETURN:
    case ui::VKEY_SPACE:
      view_->OnColorSelected();
      break;
    case ui::VKEY_UP:
      move_by(0, -1);
      break;
    case ui::VKEY_DOWN:
      move_by(0, 1);
      break;
    case ui::VKEY_LEFT:
      move_by(-1, 0);
      break;
    case ui::VKEY_RIGHT:
      move_by(1, 0);
      break;
    default:
      return;
  }
  event->StopPropagation();
}

class EyeDropperView::PreEventDispatchHandler::FocusObserver
    : public aura::client::FocusChangeObserver {
 public:
  FocusObserver(EyeDropperView* view, aura::Window* parent);

  // aura::client::FocusChangeObserver
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

  raw_ptr<EyeDropperView> view_;
  base::ScopedObservation<aura::client::FocusClient,
                          aura::client::FocusChangeObserver>
      focus_observation_{this};
};

EyeDropperView::PreEventDispatchHandler::FocusObserver::FocusObserver(
    EyeDropperView* view,
    aura::Window* parent)
    : view_(view) {
  focus_observation_.Observe(
      aura::client::GetFocusClient(parent->GetRootWindow()));
}

void EyeDropperView::PreEventDispatchHandler::FocusObserver::OnWindowFocused(
    aura::Window* gained_focus,
    aura::Window* lost_focus) {
  view_->OnColorSelectionCanceled();
}

EyeDropperView::PreEventDispatchHandler::PreEventDispatchHandler(
    EyeDropperView* view,
    gfx::NativeView parent)
    : view_(view),
      keyboard_handler_(std::make_unique<KeyboardHandler>(view, parent)),
      focus_observer_(std::make_unique<FocusObserver>(view, parent)) {
  // Ensure that this handler is called before color popup handler by using
  // a higher priority.
  view->GetWidget()->GetNativeWindow()->AddPreTargetHandler(
      this, ui::EventTarget::Priority::kSystem);
}

EyeDropperView::PreEventDispatchHandler::~PreEventDispatchHandler() {
  view_->GetWidget()->GetNativeWindow()->RemovePreTargetHandler(this);
}

void EyeDropperView::PreEventDispatchHandler::OnMouseEvent(
    ui::MouseEvent* event) {
  if (event->type() == ui::EventType::kMousePressed) {
    // Avoid closing the color popup when the eye dropper is clicked.
    event->StopPropagation();
  }
  if (event->type() == ui::EventType::kMouseReleased) {
    view_->OnColorSelected();
    event->StopPropagation();
  }
}

void EyeDropperView::PreEventDispatchHandler::OnGestureEvent(
    ui::GestureEvent* event) {
  if (event->type() == ui::EventType::kGestureTap) {
    view_->OnColorSelected();
    event->StopPropagation();
  }
}

void EyeDropperView::PreEventDispatchHandler::OnTouchEvent(
    ui::TouchEvent* event) {
  if (event->type() == ui::EventType::kTouchPressed) {
    // For touch-move, we don't move the center of the EyeDropper to be at the
    // touch point, but rather maintain the offset from the first press.
    touch_offset_ = event->root_location() -
                    view_->GetWidget()->GetWindowBoundsInScreen().CenterPoint();
  }
  if (event->type() == ui::EventType::kTouchMoved) {
    // Keep EyeDropper always inside a display, but adjust offset when position
    // is clamped.
    gfx::Point position = event->root_location() - touch_offset_;
    gfx::Point clamped = ClampToDisplay(position);
    if (clamped != position) {
      touch_offset_ = event->root_location() - clamped;
    }
    view_->UpdatePosition(std::move(clamped));
  }
}

void EyeDropperView::CaptureInput() {
  // The eye dropper needs to capture input since it is not activated
  // in order to avoid dismissing the color picker.
  GetWidget()->GetNativeWindow()->SetCapture();
}

void EyeDropperView::HideCursor() {
  auto* cursor_client = aura::client::GetCursorClient(
      GetWidget()->GetNativeWindow()->GetRootWindow());
  cursor_client->HideCursor();
  cursor_client->LockCursor();
}

void EyeDropperView::ShowCursor() {
  aura::client::GetCursorClient(GetWidget()->GetNativeWindow()->GetRootWindow())
      ->UnlockCursor();
}

gfx::Size EyeDropperView::GetSize() const {
  return gfx::Size(100, 100);
}

float EyeDropperView::GetDiameter() const {
  return 90;
}

}  // namespace eye_dropper
