// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_BROWSER_APP_STATUS_OBSERVER_H_
#define CHROME_BROWSER_UI_ASH_SHELF_BROWSER_APP_STATUS_OBSERVER_H_

#include <string>

class Browser;

namespace content {
class WebContents;
}

typedef uint32_t WebContentsId;

// An instance of a browser-based app. Can represent either of:
// - apps running inside Browser->WebContents,
// - actual browser instances (a single browser window). In this case `contents`
//   will be null and app ID will be set to |extension_misc::kChromeAppId|.
struct BrowserAppInstance {
  BrowserAppInstance(const BrowserAppInstance&) = delete;
  BrowserAppInstance& operator=(const BrowserAppInstance&) = delete;

  std::string app_id;
  Browser* browser;
  content::WebContents* web_contents;
  WebContentsId web_contents_id;
  bool visible;
  bool active;
};

// Observer interface to listen to |BrowserAppsTracker| events.
class BrowserAppStatusObserver {
 public:
  virtual ~BrowserAppStatusObserver() = default;

  // Called when a new app instance is started: a tab or a window is open, a tab
  // navigates to a URL with an app.
  virtual void OnBrowserAppAdded(const BrowserAppInstance& instance);
  // Called when the app's window, app's tab, or a window containing the app's
  // tab changes properties (visibility, active state), or a tab gets moved
  // between browsers.
  virtual void OnBrowserAppUpdated(const BrowserAppInstance& instance);
  // Called when an app instance is stopped: a tab or window is closed, a new
  // windowed app is open, a tab navigates to a URL with an app.
  virtual void OnBrowserAppRemoved(const BrowserAppInstance& instance);
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_BROWSER_APP_STATUS_OBSERVER_H_
