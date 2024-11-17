// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/app_window_base.h"

#include "chrome/browser/ui/ash/shelf/app_window_shelf_item_controller.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/views/widget/widget.h"

AppWindowBase::AppWindowBase(const ash::ShelfID& shelf_id,
                             views::Widget* widget)
    : shelf_id_(shelf_id), widget_(widget) {}

AppWindowBase::~AppWindowBase() {
  if (controller_)
    controller_->RemoveWindow(this);
}

void AppWindowBase::SetController(AppWindowShelfItemController* controller) {
  DCHECK(!controller_ || !controller);
  if (!controller && controller_)
    controller_->RemoveWindow(this);
  controller_ = controller;
}

bool AppWindowBase::IsActive() const {
  return widget_->IsActive();
}

bool AppWindowBase::IsMaximized() const {
  NOTREACHED();
}

bool AppWindowBase::IsMinimized() const {
  NOTREACHED();
}

bool AppWindowBase::IsFullscreen() const {
  NOTREACHED();
}

gfx::NativeWindow AppWindowBase::GetNativeWindow() const {
  return widget_ ? widget_->GetNativeWindow() : nullptr;
}

gfx::Rect AppWindowBase::GetRestoredBounds() const {
  NOTREACHED();
}

ui::mojom::WindowShowState AppWindowBase::GetRestoredState() const {
  NOTREACHED();
}

gfx::Rect AppWindowBase::GetBounds() const {
  NOTREACHED();
}

void AppWindowBase::Show() {
  widget_->Show();
}

void AppWindowBase::ShowInactive() {
  NOTREACHED();
}

void AppWindowBase::Hide() {
  NOTREACHED();
}

bool AppWindowBase::IsVisible() const {
  NOTREACHED();
}

void AppWindowBase::Close() {
  widget_->Close();
}

void AppWindowBase::Activate() {
  widget_->Activate();
}

void AppWindowBase::Deactivate() {
  NOTREACHED();
}

void AppWindowBase::Maximize() {
  NOTREACHED();
}

void AppWindowBase::Minimize() {
  widget_->Minimize();
}

void AppWindowBase::Restore() {
  NOTREACHED();
}

void AppWindowBase::SetBounds(const gfx::Rect& bounds) {
  NOTREACHED();
}

void AppWindowBase::FlashFrame(bool flash) {
  NOTREACHED();
}

ui::ZOrderLevel AppWindowBase::GetZOrderLevel() const {
  NOTREACHED();
}

void AppWindowBase::SetZOrderLevel(ui::ZOrderLevel level) {
  NOTREACHED();
}
