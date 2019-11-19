// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/dialog_test_browser_window.h"

#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/widget/widget.h"

using web_modal::WebContentsModalDialogHost;
using web_modal::ModalDialogHostObserver;

DialogTestBrowserWindow::DialogTestBrowserWindow() {
#if defined(OS_MACOSX)
  // Create a dummy Widget on Mac for parenting dialogs. On Aura, just parent
  // using the WebContents since creating a Widget here requires an Aura
  // RootWindow for context and it's tricky to get one here.
  host_window_.reset(new views::Widget);
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  host_window_->Init(std::move(params));
  // Leave the window hidden: unit tests shouldn't need it to be visible.
#endif
}

DialogTestBrowserWindow::~DialogTestBrowserWindow() {
}


WebContentsModalDialogHost*
DialogTestBrowserWindow::GetWebContentsModalDialogHost() {
  return this;
}

// The web contents modal dialog must be parented to *something*; use the
// WebContents window since there is no true browser window for unit tests.
gfx::NativeView DialogTestBrowserWindow::GetHostView() const {
  if (host_window_)
    return host_window_->GetNativeView();

  return FindBrowser()
      ->tab_strip_model()
      ->GetActiveWebContents()
      ->GetNativeView();
}

gfx::Point DialogTestBrowserWindow::GetDialogPosition(const gfx::Size& size) {
  return gfx::Point();
}

gfx::Size DialogTestBrowserWindow::GetMaximumDialogSize() {
#if defined(OS_MACOSX)
  // Zero-size windows aren't allowed on Mac.
  return gfx::Size(1, 1);
#endif
  return gfx::Size();
}

void DialogTestBrowserWindow::AddObserver(ModalDialogHostObserver* observer) {
}

void DialogTestBrowserWindow::RemoveObserver(
    ModalDialogHostObserver* observer) {
}

Browser* DialogTestBrowserWindow::FindBrowser() const {
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->window() == this)
      return browser;
  }
  NOTREACHED();
  return nullptr;
}

