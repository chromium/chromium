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

namespace views {
class WebView;
}  // namespace views

class GURL;

class BrowserWindowInterface {
 public:
  // The contents of the active tab is rendered in a views::WebView. When the
  // active tab switches, the contents of the views::WebView is modified, but
  // the instance itself remains the same.
  virtual views::WebView* GetWebView() = 0;

  // Opens a URL, with the given disposition.
  virtual void OpenURL(const GURL& gurl, WindowOpenDisposition disposition) = 0;
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_WINDOW_INTERFACE_H_
