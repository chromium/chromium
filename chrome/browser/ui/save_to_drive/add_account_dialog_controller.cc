// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/save_to_drive/add_account_dialog_controller.h"

#include "chrome/browser/signin/signin_promo.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/display/screen.h"

namespace save_to_drive {

namespace {
// Computes the bounds of the popup window. The popup window is centered in the
// source window, if the source window is large enough to contain the popup
// window. Otherwise, the popup window is centered in the screen.
gfx::Rect ComputePopupWindowBounds(content::WebContents* source_window) {
  gfx::Rect source_window_bounds = source_window->GetContainerBounds();
  const int kPopupWindowWidth = 400;
  const int kPopupWindowHeight = 484;
  int x_coordinate;
  int y_coordinate;

  if (source_window_bounds.width() >= kPopupWindowWidth &&
      source_window_bounds.height() >= kPopupWindowHeight) {
    x_coordinate = source_window_bounds.x() +
                   ((source_window_bounds.width() - kPopupWindowWidth) / 2);
    y_coordinate = source_window_bounds.y() +
                   ((source_window_bounds.height() - kPopupWindowHeight) / 2);
  } else {
    display::Screen* screen = display::Screen::Get();
    gfx::Rect source_display_bounds =
        screen->GetDisplayNearestView(source_window->GetNativeView())
            .work_area();
    x_coordinate = (source_display_bounds.width() - kPopupWindowWidth) / 2;
    y_coordinate = (source_display_bounds.height() - kPopupWindowHeight) / 2;
  }
  return gfx::Rect(x_coordinate, y_coordinate, kPopupWindowWidth,
                   kPopupWindowHeight);
}
}  // namespace

AddAccountDialogController::AddAccountDialogController(
    content::WebContents* web_contents)
    : source_window_(web_contents) {}

AddAccountDialogController::~AddAccountDialogController() {
  Close();
}

void AddAccountDialogController::Close() {
  if (!popup_window_) {
    return;
  }
  // Store this in a local variable to avoid triggering the dangling pointer
  // detector.
  content::WebContents* popup = popup_window_;
  popup_window_ = nullptr;
  popup->Close();
}

void AddAccountDialogController::Show() {
  if (popup_window_) {
    ResizeAndFocusPopupWindow();
    return;
  }
  content::OpenURLParams params(
      signin::GetAddAccountURLForDice("", GURL()), content::Referrer(),
      WindowOpenDisposition::NEW_POPUP, ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
      /*is_renderer_initiated=*/false);
  popup_window_ = source_window_->GetDelegate()->OpenURLFromTab(
      source_window_, params, /*navigation_handle_callback=*/{});
  ResizeAndFocusPopupWindow();
  Observe(popup_window_);
}

void AddAccountDialogController::WebContentsDestroyed() {
  // The popup window is going away, make sure we don't keep a dangling pointer.
  // This should happen before notifying the observer, where `this` will be
  // destroyed.
  popup_window_ = nullptr;
}

void AddAccountDialogController::ResizeAndFocusPopupWindow() {
  CHECK(popup_window_);
  gfx::Rect popup_window_bounds = ComputePopupWindowBounds(source_window_);
  popup_window_->GetDelegate()->SetContentsBounds(popup_window_,
                                                  popup_window_bounds);
  popup_window_->GetDelegate()->ActivateContents(popup_window_);
}

}  // namespace save_to_drive
