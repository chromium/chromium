// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_DESKTOP_BROWSER_WINDOW_CAPABILITIES_DELEGATE_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_DESKTOP_BROWSER_WINDOW_CAPABILITIES_DELEGATE_H_

namespace content {
class WebContents;
}

class DesktopBrowserWindowCapabilitiesDelegate {
 public:
  // These mirror the DesktopBrowserWindowCapabilities functions of the same
  // name.
  virtual bool IsAttemptingToCloseBrowser() const = 0;
  virtual void SetWebContentsBlocked(content::WebContents* web_contents,
                                     bool blocked) = 0;
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_DESKTOP_BROWSER_WINDOW_CAPABILITIES_DELEGATE_H_
