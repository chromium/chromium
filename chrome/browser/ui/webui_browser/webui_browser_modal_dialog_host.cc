// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui_browser/webui_browser_modal_dialog_host.h"

#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/webui_browser/webui_browser_window.h"
#include "ui/gfx/native_ui_util.h"

WebUIBrowserModalDialogHost::WebUIBrowserModalDialogHost(
    WebUIBrowserWindow* window)
    : browser_window_(window) {}

WebUIBrowserModalDialogHost::~WebUIBrowserModalDialogHost() {
  observers_.Notify(&web_modal::ModalDialogHostObserver::OnHostDestroying);
}

void WebUIBrowserModalDialogHost::NotifyPositionRequiresUpdate() {
  observers_.Notify(
      &web_modal::ModalDialogHostObserver::OnPositionRequiresUpdate);
}

gfx::Size WebUIBrowserModalDialogHost::GetMaximumDialogSize() {
  return browser_window_->GetContentsBoundsInScreen().size();
}

gfx::NativeView WebUIBrowserModalDialogHost::GetHostView() const {
  return gfx::GetViewForWindow(browser_window_->GetNativeWindow());
}

bool WebUIBrowserModalDialogHost::ShouldActivateDialog() const {
  // ref. same in BrowerViewLayout:::WebContentsModalDialogHostViews.
  return chrome::FindLastActive() == browser_window_->browser();
}

gfx::Point WebUIBrowserModalDialogHost::GetDialogPosition(
    const gfx::Size& size) {
  gfx::Rect contents_bounds = browser_window_->GetContentsBoundsInScreen();
  gfx::Rect window_bounds = browser_window_->GetBounds();

  // We're expected to provide coordinates relative to the hosting view.
  contents_bounds -= window_bounds.OffsetFromOrigin();

  return gfx::Point(
      contents_bounds.x() + (contents_bounds.width() - size.width()) / 2,
      contents_bounds.y() - 10);
}

void WebUIBrowserModalDialogHost::AddObserver(
    web_modal::ModalDialogHostObserver* observer) {
  observers_.AddObserver(observer);
}

void WebUIBrowserModalDialogHost::RemoveObserver(
    web_modal::ModalDialogHostObserver* observer) {
  observers_.RemoveObserver(observer);
}
