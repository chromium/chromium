// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui_browser/browser_elements_webui_browser.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/views/interaction/element_tracker_views.h"

DEFINE_FRAMEWORK_SPECIFIC_METADATA_SUBCLASS(BrowserElementsWebUiBrowser,
                                            BrowserElementsViews)

BrowserElementsWebUiBrowser::BrowserElementsWebUiBrowser(
    BrowserWindowInterface& browser)
    : BrowserElementsViews(browser) {}

BrowserElementsWebUiBrowser::~BrowserElementsWebUiBrowser() = default;

// static
BrowserElementsWebUiBrowser* BrowserElementsWebUiBrowser::From(
    BrowserWindowInterface* browser) {
  auto* const base = BrowserElements::From(browser);
  return base ? base->AsA<BrowserElementsWebUiBrowser>() : nullptr;
}

void BrowserElementsWebUiBrowser::Init(views::Widget* browser_widget) {
  // TODO(webium): Fix ChromeOS. On ChromeOS, browser_widget is null because of
  // a memory issue in WebUIBrowserWindow::GetNativeWindow(). See the comment
  // there.
#if !BUILDFLAG(IS_CHROMEOS)
  CHECK(browser_widget);
#endif
  browser_widget_ = browser_widget;
}

void BrowserElementsWebUiBrowser::TearDown() {
  browser_widget_ = nullptr;
}

ui::ElementContext BrowserElementsWebUiBrowser::GetContext() {
  // TODO(webium): Remove this after fixing ChromeOS.
#if BUILDFLAG(IS_CHROMEOS)
  if (!browser_widget_) {
    return ui::ElementContext();
  }
#endif

  return views::ElementTrackerViews::GetContextForWidget(browser_widget_);
}

views::Widget* BrowserElementsWebUiBrowser::GetPrimaryWindowWidget() {
  return browser_widget_;
}

bool BrowserElementsWebUiBrowser::IsInitialized() const {
  return browser_widget_ != nullptr;
}
