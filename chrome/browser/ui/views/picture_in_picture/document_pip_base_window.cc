// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/picture_in_picture/document_pip_base_window.h"

#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/views/widget/widget.h"

DocumentPipBaseWindow::DocumentPipBaseWindow(views::Widget& widget)
    : widget_(widget) {}

DocumentPipBaseWindow::~DocumentPipBaseWindow() = default;

bool DocumentPipBaseWindow::IsActive() const {
  return widget_->IsActive();
}

bool DocumentPipBaseWindow::IsMaximized() const {
  return widget_->IsMaximized();
}

bool DocumentPipBaseWindow::IsMinimized() const {
  return widget_->IsMinimized();
}

bool DocumentPipBaseWindow::IsFullscreen() const {
  return widget_->IsFullscreen();
}

gfx::NativeWindow DocumentPipBaseWindow::GetNativeWindow() const {
  return widget_->GetNativeWindow();
}

gfx::Rect DocumentPipBaseWindow::GetRestoredBounds() const {
  return widget_->GetRestoredBounds();
}

ui::mojom::WindowShowState DocumentPipBaseWindow::GetRestoredState() const {
  return ui::mojom::WindowShowState::kNormal;
}

gfx::Rect DocumentPipBaseWindow::GetBounds() const {
  return widget_->GetWindowBoundsInScreen();
}

void DocumentPipBaseWindow::Show() {
  if (widget_->IsVisible()) {
    widget_->Activate();
    return;
  }
  widget_->Show();
}

void DocumentPipBaseWindow::Hide() {
  widget_->Hide();
}

bool DocumentPipBaseWindow::IsVisible() const {
  return widget_->IsVisible();
}

void DocumentPipBaseWindow::ShowInactive() {
  if (widget_->IsVisible()) {
    return;
  }
  widget_->ShowInactive();
}

void DocumentPipBaseWindow::Close() {
  // The host's synchronous close callback can destroy this adapter.
  widget_->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
}

void DocumentPipBaseWindow::Activate() {
  widget_->Activate();
}

void DocumentPipBaseWindow::Deactivate() {
  widget_->Deactivate();
}

void DocumentPipBaseWindow::Maximize() {
  // Document PiP does not support maximizing its floating viewport.
}

void DocumentPipBaseWindow::Minimize() {
  // Document PiP does not support minimizing its floating viewport.
}

void DocumentPipBaseWindow::Restore() {
  widget_->Restore();
}

void DocumentPipBaseWindow::SetBounds(const gfx::Rect& bounds) {
  widget_->SetBounds(bounds);
}

void DocumentPipBaseWindow::FlashFrame(bool flash) {
  widget_->FlashFrame(flash);
}

ui::ZOrderLevel DocumentPipBaseWindow::GetZOrderLevel() const {
  return widget_->GetZOrderLevel();
}

void DocumentPipBaseWindow::SetZOrderLevel(ui::ZOrderLevel level) {
  widget_->SetZOrderLevel(level);
}
