// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_native_widget_factory.h"

#include "chrome/browser/ui/views/frame/browser_native_widget_ash.h"

BrowserNativeWidget* BrowserNativeWidgetFactory::Create(
    BrowserWidget* browser_widget,
    BrowserView* browser_view) {
  return new BrowserNativeWidgetAsh(browser_widget, browser_view);
}
