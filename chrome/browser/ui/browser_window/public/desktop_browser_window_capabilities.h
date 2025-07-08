// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_DESKTOP_BROWSER_WINDOW_CAPABILITIES_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_DESKTOP_BROWSER_WINDOW_CAPABILITIES_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;
class BrowserWindow;
class DesktopBrowserWindowCapabilitiesDelegate;

namespace ui {
class UnownedUserDataHost;
}

namespace content {
class WebContents;
}

// A collection of capabilities related to desktop browser windows. Most
// functionality should go on this class, rather than being exposed on
// BrowserWindowInterface.
class DesktopBrowserWindowCapabilities {
 public:
  DECLARE_USER_DATA(DesktopBrowserWindowCapabilities);

  DesktopBrowserWindowCapabilities(
      DesktopBrowserWindowCapabilitiesDelegate* delegate,
      BrowserWindow* browser_window,
      ui::UnownedUserDataHost& host);
  ~DesktopBrowserWindowCapabilities();

  static DesktopBrowserWindowCapabilities* From(
      BrowserWindowInterface* browser_window_interface);
  static const DesktopBrowserWindowCapabilities* From(
      const BrowserWindowInterface* browser_window_interface);

  // Returns true if the browser window is visible on the screen.
  bool IsVisibleOnScreen() const;

  // See Browser::IsAttemptingToCloseBrowser() for more details.
  bool IsAttemptingToCloseBrowser() const;

  // Changes the blocked state of |web_contents|. WebContentses are considered
  // blocked while displaying a web contents modal dialog. During that time
  // renderer host will ignore any UI interaction within WebContents outside of
  // the currently displaying dialog.
  // Note that this is a duplicate of the same method in
  // WebContentsModalDialogManagerDelegate. This is because there are two ways
  // to open tab-modal dialogs, either via TabDialogManager or via
  // //components/web_modal. See crbug.com/377820808.
  void SetWebContentsBlocked(content::WebContents* web_contents, bool blocked);

 private:
  // The associated delegate. Must outlive this class.
  raw_ptr<DesktopBrowserWindowCapabilitiesDelegate> delegate_ = nullptr;

  // The corresponding BrowserWindow. This should be valid for the lifetime of
  // this class, since this is constructed by BrowserWindowFeatures after
  // Browser creation and destroyed before Browser teardown.
  raw_ptr<BrowserWindow> browser_window_ = nullptr;

  ui::ScopedUnownedUserData<DesktopBrowserWindowCapabilities>
      scoped_data_holder_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_DESKTOP_BROWSER_WINDOW_CAPABILITIES_H_
