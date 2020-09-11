// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/eye_dropper/eye_dropper_view.h"

#include "ui/aura/client/cursor_client.h"
#include "ui/aura/window.h"

EyeDropperView::PreEventDispatchHandler::PreEventDispatchHandler(
    EyeDropperView* view)
    : view_(view) {
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
  if (event->type() == ui::ET_MOUSE_PRESSED) {
    // Avoid closing the color popup when the eye dropper is clicked.
    event->StopPropagation();
  }
  if (event->type() == ui::ET_MOUSE_RELEASED) {
    view_->OnColorSelected();
    event->StopPropagation();
  }
}

void EyeDropperView::MoveViewToFront() {
  // The view is already topmost when Aura is used.
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

std::unique_ptr<content::EyeDropper> ShowEyeDropper(
    content::RenderFrameHost* frame,
    content::EyeDropperListener* listener) {
  return std::make_unique<EyeDropperView>(frame, listener);
}
