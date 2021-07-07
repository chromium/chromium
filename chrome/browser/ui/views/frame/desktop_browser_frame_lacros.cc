// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/desktop_browser_frame_lacros.h"

#include "chrome/browser/ui/views/frame/browser_desktop_window_tree_host_linux.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/native_browser_frame_factory.h"

DesktopBrowserFrameLacros::DesktopBrowserFrameLacros(
    BrowserFrame* browser_frame,
    BrowserView* browser_view)
    : DesktopBrowserFrameAura(browser_frame, browser_view) {}

DesktopBrowserFrameLacros::~DesktopBrowserFrameLacros() = default;

views::Widget::InitParams DesktopBrowserFrameLacros::GetWidgetParams() {
  views::Widget::InitParams params;
  params.native_widget = this;
  params.remove_standard_frame = true;
  return params;
}

void DesktopBrowserFrameLacros::TabDraggingKindChanged(
    TabDragKind tab_drag_kind) {
  if (host_)
    host_->TabDraggingKindChanged(tab_drag_kind);
}

NativeBrowserFrame* NativeBrowserFrameFactory::Create(
    BrowserFrame* browser_frame,
    BrowserView* browser_view) {
  return new DesktopBrowserFrameLacros(browser_frame, browser_view);
}
