// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_view_linux.h"

BrowserFrameViewLinux::BrowserFrameViewLinux(
    BrowserFrame* frame,
    BrowserView* browser_view,
    BrowserFrameViewLayoutLinux* layout)
    : OpaqueBrowserFrameView(frame, browser_view, layout), layout_(layout) {
  if (views::LinuxUI* ui = views::LinuxUI::instance())
    ui->AddWindowButtonOrderObserver(this);
}

BrowserFrameViewLinux::~BrowserFrameViewLinux() {
  if (views::LinuxUI* ui = views::LinuxUI::instance())
    ui->RemoveWindowButtonOrderObserver(this);
}

void BrowserFrameViewLinux::OnWindowButtonOrderingChange(
    const std::vector<views::FrameButton>& leading_buttons,
    const std::vector<views::FrameButton>& trailing_buttons) {
  layout_->SetButtonOrdering(leading_buttons, trailing_buttons);

  // We can receive OnWindowButtonOrderingChange events before we've been added
  // to a Widget. We need a Widget because layout crashes due to dependencies
  // on a ui::ThemeProvider().
  if (auto* widget = GetWidget()) {
    // A relayout on |view_| is insufficient because it would neglect
    // a relayout of the tabstrip.  Do a full relayout to handle the
    // frame buttons as well as open tabs.
    views::View* root_view = widget->GetRootView();
    root_view->Layout();
    root_view->SchedulePaint();
  }
}
