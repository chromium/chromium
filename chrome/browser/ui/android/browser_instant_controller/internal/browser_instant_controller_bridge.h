// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_BROWSER_INSTANT_CONTROLLER_INTERNAL_BROWSER_INSTANT_CONTROLLER_BRIDGE_H_
#define CHROME_BROWSER_UI_ANDROID_BROWSER_INSTANT_CONTROLLER_INTERNAL_BROWSER_INSTANT_CONTROLLER_BRIDGE_H_

#include <memory>

class BrowserInstantController;
class BrowserWindowInterface;

// Native class for the Java `BrowserInstantControllerBridge`.
//
// The primary purpose of this class is to own a cross-platform
// `BrowserInstantController` for a browser window and manage its lifecycle
// with the window, which is what `BrowserWindowFeatures` does on desktop.
class BrowserInstantControllerBridge final {
 public:
  // `browser_window` must be non-null and must outlive this bridge, which may
  // retain it through the `BrowserInstantController` it owns.
  explicit BrowserInstantControllerBridge(
      BrowserWindowInterface* browser_window);
  BrowserInstantControllerBridge(const BrowserInstantControllerBridge&) =
      delete;
  BrowserInstantControllerBridge& operator=(
      const BrowserInstantControllerBridge&) = delete;
  ~BrowserInstantControllerBridge();

 private:
  // Null when NTP tabs of the window's profile don't need to be reloaded on
  // default search engine changes (e.g. incognito windows, or when the WebUI
  // NTP for third-party search engines isn't enabled).
  std::unique_ptr<BrowserInstantController> instant_controller_;
};

#endif  // CHROME_BROWSER_UI_ANDROID_BROWSER_INSTANT_CONTROLLER_INTERNAL_BROWSER_INSTANT_CONTROLLER_BRIDGE_H_
