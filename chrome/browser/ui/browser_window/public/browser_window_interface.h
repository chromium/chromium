// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_WINDOW_INTERFACE_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_WINDOW_INTERFACE_H_

#include "ui/base/window_open_disposition.h"

// This is the public interface for a browser window. Most features in
// //chrome/browser depend on this interface, and thus to prevent circular
// dependencies this interface should not depend on anything else in //chrome.
// Ping erikchen for assistance if this class does not have the functionality
// your feature needs. This comment will be deleted after there are 10+ features
// in BrowserWindowFeatures.

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace views {
class WebView;
class View;
}  // namespace views

namespace web_modal {
class WebContentsModalDialogHost;
}  // namespace web_modal

class BrowserWindowFeatures;
class GURL;
class SessionID;

class BrowserWindowInterface {
 public:
  // The contents of the active tab is rendered in a views::WebView. When the
  // active tab switches, the contents of the views::WebView is modified, but
  // the instance itself remains the same.
  virtual views::WebView* GetWebView() = 0;

  // Opens a URL, with the given disposition.
  virtual void OpenURL(const GURL& gurl, WindowOpenDisposition disposition) = 0;

  // Returns a session-unique ID.
  virtual const SessionID& GetSessionID() = 0;

  // Returns true if the tab strip is currently visible for this browser window.
  // Will return false on browser initialization before the tab strip is
  // initialized.
  virtual bool IsTabStripVisible() = 0;

  // Returns the top container view.
  virtual views::View* TopContainer() = 0;

  // Returns the foreground tab. This can be nullptr very early during
  // BrowserWindow initialization, and very late during BrowserWindow teardown.
  virtual tabs::TabInterface* GetActiveTabInterface() = 0;

  // Returns the feature controllers scoped to this browser window.
  virtual BrowserWindowFeatures& GetFeatures() = 0;

  // Returns the web contents modal dialog host pertaining to this
  // BrowserWindow.
  virtual web_modal::WebContentsModalDialogHost*
  GetWebContentsModalDialogHostForWindow() = 0;
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_WINDOW_INTERFACE_H_
