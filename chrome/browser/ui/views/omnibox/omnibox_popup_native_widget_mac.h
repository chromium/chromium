// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_NATIVE_WIDGET_MAC_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_NATIVE_WIDGET_MAC_H_

#if defined(__OBJC__)
#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"

// NSWindow for the omnibox popup. Window managers (Moom, Rectangle, ...) act on
// AXFocusedWindow, which is the popup while it is key. This proxies AXPosition
// and AXSize (reads and writes) to the root parent window, so they move the
// browser instead.
@interface OmniboxPopupNSWindow : NativeWidgetMacNSWindow
@end
#endif  // defined(__OBJC__)

namespace views {
class NativeWidget;
class Widget;
}  // namespace views

// Returns a NativeWidget whose NSWindow is an OmniboxPopupNSWindow.
views::NativeWidget* CreateOmniboxPopupNativeWidget(views::Widget* widget);

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_NATIVE_WIDGET_MAC_H_
