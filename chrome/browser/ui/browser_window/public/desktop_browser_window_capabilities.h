// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_DESKTOP_BROWSER_WINDOW_CAPABILITIES_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_DESKTOP_BROWSER_WINDOW_CAPABILITIES_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;
class BrowserWindow;
class UnownedUserDataHost;

// A collection of capabilities related to desktop browser windows. Most
// functionality should go on this class, rather than being exposed on
// BrowserWindowInterface.
class DesktopBrowserWindowCapabilities {
 public:
  static const char* kDataKey;

  DesktopBrowserWindowCapabilities(BrowserWindow* browser_window,
                                   UnownedUserDataHost& host);
  ~DesktopBrowserWindowCapabilities();

  static DesktopBrowserWindowCapabilities* From(
      BrowserWindowInterface* browser_window_interface);
  static const DesktopBrowserWindowCapabilities* From(
      const BrowserWindowInterface* browser_window_interface);

  // Returns true if the browser window is visible on the screen.
  bool IsVisibleOnScreen() const;

 private:
  // The corresponding BrowserWindow. This should be valid for the lifetime of
  // this class, since this is constructed by BrowserWindowFeatures after
  // Browser creation and destroyed before Browser teardown.
  raw_ptr<BrowserWindow> browser_window_ = nullptr;

  ScopedUnownedUserData<DesktopBrowserWindowCapabilities> scoped_data_holder_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_DESKTOP_BROWSER_WINDOW_CAPABILITIES_H_
