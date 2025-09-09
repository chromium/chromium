// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_native_widget_factory.h"

#import "chrome/browser/ui/views/frame/browser_native_widget_mac.h"

BrowserNativeWidget* BrowserNativeWidgetFactory::Create(
    BrowserWidget* browser_widget,
    BrowserView* browser_view) {
  return new BrowserNativeWidgetMac(browser_widget, browser_view);
}
