// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ime/ime_window_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/ime/ime_window_frame_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_win.h"
#endif

namespace ui {

ImeNativeWindow* ImeWindow::CreateNativeWindow(ImeWindow* ime_window,
                                               const gfx::Rect& bounds,
                                               content::WebContents* contents) {
  return new ImeWindowView(ime_window, bounds, contents);
}

ImeWindowView::ImeWindowView(ImeWindow* ime_window,
                             const gfx::Rect& bounds,
                             content::WebContents* contents)
    : ime_window_(ime_window),
      dragging_pointer_type_(PointerType::MOUSE),
      dragging_state_(DragState::NO_DRAG),
      window_(nullptr),
      web_view_(nullptr) {
  window_ = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.delegate = this;
  params.wants_mouse_events_when_inactive = true;
  params.remove_standard_frame = false;
  params.z_order = ui::ZOrderLevel::kFloatingWindow;
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  params.visible_on_all_workspaces = false;
  params.bounds = bounds;
  window_->set_focus_on_creation(false);
  window_->set_frame_type(views::Widget::FrameType::kForceCustom);
  window_->Init(std::move(params));
  window_->UpdateWindowTitle();
  window_->UpdateWindowIcon();

  web_view_ = new views::WebView(nullptr);
  web_view_->SetWebContents(contents);
  web_view_->SetFocusBehavior(FocusBehavior::NEVER);
  AddChildView(web_view_);

  SetLayoutManager(std::make_unique<views::FillLayout>());
  Layout();

  // TODO(shuchen): supports auto cursor/composition aligning for
  // follow-cursor window.
}

ImeWindowView::~ImeWindowView() {}

void ImeWindowView::Show() {
  window_->ShowInactive();
}

void ImeWindowView::Hide() {
  window_->Hide();
}

void ImeWindowView::Close() {
  window_->Close();
}

void ImeWindowView::SetBounds(const gfx::Rect& bounds) {
  window_->SetBounds(bounds);
}

gfx::Rect ImeWindowView::GetBounds() const {
  return GetWidget()->GetWindowBoundsInScreen();
}

void ImeWindowView::UpdateWindowIcon() {
  window_->UpdateWindowIcon();
}

bool ImeWindowView::IsVisible() const {
  return GetWidget()->IsVisible();
}

void ImeWindowView::OnCloseButtonClicked() {
  ime_window_->Close();
}

bool ImeWindowView::OnTitlebarPointerPressed(
    const gfx::Point& pointer_location, PointerType pointer_type) {
  if (dragging_state_ != DragState::NO_DRAG &&
      dragging_pointer_type_ != pointer_type) {
    return false;
  }

  dragging_state_ = DragState::POSSIBLE_DRAG;
  pointer_location_on_press_ = pointer_location;
  dragging_pointer_type_ = pointer_type;
  return true;
}

bool ImeWindowView::OnTitlebarPointerDragged(
    const gfx::Point& pointer_location, PointerType pointer_type) {
  if (dragging_state_ == DragState::NO_DRAG)
    return false;
  if (dragging_pointer_type_ != pointer_type)
    return false;

  if (dragging_state_ == DragState::POSSIBLE_DRAG &&
      ExceededDragThreshold(pointer_location - pointer_location_on_press_)) {
    gfx::Rect bounds = GetWidget()->GetWindowBoundsInScreen();
    bounds_on_drag_start_ = bounds;
    dragging_state_ = DragState::ACTIVE_DRAG;
  }
  if (dragging_state_ == DragState::ACTIVE_DRAG) {
    gfx::Point target_position = pointer_location -
        (pointer_location_on_press_ - bounds_on_drag_start_.origin());
    gfx::Rect bounds = GetWidget()->GetWindowBoundsInScreen();
    bounds.set_origin(target_position);
    GetWidget()->SetBounds(bounds);
  }
  return true;
}

void ImeWindowView::OnTitlebarPointerReleased(PointerType pointer_type) {
  if (dragging_pointer_type_ == pointer_type &&
      dragging_state_ == DragState::ACTIVE_DRAG) {
    EndDragging();
  }
}

void ImeWindowView::OnTitlebarPointerCaptureLost() {
  if (dragging_state_ == DragState::ACTIVE_DRAG) {
    GetWidget()->SetBounds(bounds_on_drag_start_);
    EndDragging();
  }
}

views::NonClientFrameView* ImeWindowView::CreateNonClientFrameView(
    views::Widget* widget) {
  ImeWindowFrameView* frame_view = new ImeWindowFrameView(
      this, ime_window_->mode());
  frame_view->Init();
  return frame_view;
}

bool ImeWindowView::CanActivate() const {
  return false;
}

bool ImeWindowView::CanResize() const {
  return false;
}

bool ImeWindowView::CanMaximize() const {
  return false;
}

bool ImeWindowView::CanMinimize() const {
  return false;
}

base::string16 ImeWindowView::GetWindowTitle() const {
  return base::UTF8ToUTF16(ime_window_->title());
}

gfx::ImageSkia ImeWindowView::GetWindowIcon() {
  return ime_window_->icon() ? ime_window_->icon()->image_skia()
                             : gfx::ImageSkia();
}

void ImeWindowView::DeleteDelegate() {
  ime_window_->OnWindowDestroyed();
  delete this;
}

ImeWindowFrameView* ImeWindowView::GetFrameView() const {
  return static_cast<ImeWindowFrameView*>(
      window_->non_client_view()->frame_view());
}

void ImeWindowView::EndDragging() {
  dragging_state_ = DragState::NO_DRAG;
}

}  // namespace ui
