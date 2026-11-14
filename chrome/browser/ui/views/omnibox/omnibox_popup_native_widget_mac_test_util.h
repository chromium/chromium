// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_NATIVE_WIDGET_MAC_TEST_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_NATIVE_WIDGET_MAC_TEST_UTIL_H_

#include <memory>

namespace views {
class Widget;
}  // namespace views

// While in scope, makes `browser_widget`'s NSWindow, and the NSWindows attached
// to it, report that they aren't on the active Space, as when a new window has
// entered fullscreen but macOS hasn't switched to its Space yet. This makes
// NativeWidgetNSWindowBridge::OrderChildren() defer attaching child windows,
// such as the omnibox popup. Destroying it restores the real Space state and
// posts NSWorkspaceActiveSpaceDidChangeNotification, which attaches them.
class ScopedSimulateInactiveSpace {
 public:
  explicit ScopedSimulateInactiveSpace(views::Widget* browser_widget);
  ScopedSimulateInactiveSpace(const ScopedSimulateInactiveSpace&) = delete;
  ScopedSimulateInactiveSpace& operator=(const ScopedSimulateInactiveSpace&) =
      delete;
  ~ScopedSimulateInactiveSpace();

 private:
  struct ObjCStorage;
  std::unique_ptr<ObjCStorage> storage_;
};

// Returns true if `popup_widget`'s NSWindow is attached to a parent NSWindow.
bool IsOmniboxPopupAttachedForTesting(const views::Widget* popup_widget);

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_NATIVE_WIDGET_MAC_TEST_UTIL_H_
