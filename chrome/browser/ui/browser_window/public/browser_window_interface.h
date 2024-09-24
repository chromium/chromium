// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_WINDOW_INTERFACE_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_WINDOW_INTERFACE_H_

#include "base/callback_list.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/page_navigator.h"
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

namespace web_app {
class AppBrowserController;
}  // namespace web_app

namespace web_modal {
class WebContentsModalDialogHost;
}  // namespace web_modal

class BrowserActions;
class BrowserUserEducationInterface;
class BrowserWindowFeatures;
class ExclusiveAccessManager;
class GURL;
class Profile;
class SessionID;
class TabStripModel;

class BrowserWindowInterface : public content::PageNavigator {
 public:
  // The contents of the active tab is rendered in a views::WebView. When the
  // active tab switches, the contents of the views::WebView is modified, but
  // the instance itself remains the same.
  virtual views::WebView* GetWebView() = 0;

  // Returns the profile that semantically owns this browser window. This value
  // is never null, and never changes for the lifetime of a given browser
  // window. All tabs contained in a browser window have the same
  // profile/BrowserContext as the browser window itself.
  virtual Profile* GetProfile() = 0;

  // Opens a URL, with the given disposition. This is a convenience wrapper
  // around OpenURL from content::PageNavigator.
  virtual void OpenGURL(const GURL& gurl,
                        WindowOpenDisposition disposition) = 0;

  // Returns a session-unique ID.
  virtual const SessionID& GetSessionID() = 0;

  virtual TabStripModel* GetTabStripModel() = 0;

  // Returns true if the tab strip is currently visible for this browser window.
  // Will return false on browser initialization before the tab strip is
  // initialized.
  virtual bool IsTabStripVisible() = 0;

  // Returns true if the browser controls are hidden due to being in fullscreen.
  virtual bool ShouldHideUIForFullscreen() const = 0;

  // See Browser::IsAttemptingToCloseBrowser() for more details.
  virtual bool IsAttemptingToCloseBrowser() const = 0;

  // Returns the top container view.
  virtual views::View* TopContainer() = 0;

  using ActiveTabChangeCallback =
      base::RepeatingCallback<void(BrowserWindowInterface*)>;
  virtual base::CallbackListSubscription RegisterActiveTabDidChange(
      ActiveTabChangeCallback callback) = 0;

  // Returns the foreground tab. This can be nullptr very early during
  // BrowserWindow initialization, and very late during BrowserWindow teardown.
  virtual tabs::TabInterface* GetActiveTabInterface() = 0;

  // Returns the feature controllers scoped to this browser window.
  // BrowserWindowFeatures that depend on other BrowserWindowFeatures should not
  // use this method. Instead they should use use dependency injection to pass
  // dependencies at construction or initialization. This method exists for
  // three purposes:
  //   (1) TabFeatures often depend on state of BrowserWindowFeatures for the
  //   attached window, which can change. TabFeatures need a way to dynamically
  //   fetch BrowserWindowFeatures.
  //   (2) To expose BrowserWindowFeatures for tests.
  //   (3) It is not possible to perform dependency injection for legacy code
  //   that is conceptually a BrowserWindowFeature and needs access to other
  //   BrowserWindowFeature.
  virtual BrowserWindowFeatures& GetFeatures() = 0;

  // Returns the web contents modal dialog host pertaining to this
  // BrowserWindow.
  virtual web_modal::WebContentsModalDialogHost*
  GetWebContentsModalDialogHostForWindow() = 0;

  // Whether the window is active.
  // This definition needs to be more precise, as "active" has different
  // semantics and nuance on each platform.
  virtual bool IsActive() = 0;

  // Register for these two callbacks to detect changes to IsActive().
  using DidBecomeActiveCallback =
      base::RepeatingCallback<void(BrowserWindowInterface*)>;
  virtual base::CallbackListSubscription RegisterDidBecomeActive(
      DidBecomeActiveCallback callback) = 0;
  using DidBecomeInactiveCallback =
      base::RepeatingCallback<void(BrowserWindowInterface*)>;
  virtual base::CallbackListSubscription RegisterDidBecomeInactive(
      DidBecomeInactiveCallback callback) = 0;

  // This class is responsible for controlling fullscreen and pointer lock.
  virtual ExclusiveAccessManager* GetExclusiveAccessManager() = 0;

  // This class manages actions that a user can take that are scoped to a
  // browser window (e.g. most of the 3-dot menu actions).
  virtual BrowserActions* GetActions() = 0;

  // SessionService::WindowType mirrors these values.  If you add to this
  // enum, look at SessionService::WindowType to see if it needs to be
  // updated.
  // TODO(https://crbug.com/331031753): Several of these existing Window Types
  // likely should not have been using Browser as a base to begin with and
  // should be migrated. Please refrain from adding new types.
  enum Type {
    // Normal tabbed non-app browser (previously TYPE_TABBED).
    TYPE_NORMAL,
    // Popup browser.
    TYPE_POPUP,
    // App browser. Specifically, one of these:
    // * Web app; comes in different flavors but is backed by the same code:
    //   - Progressive Web App (PWA)
    //   - Shortcut app (from 3-dot menu > More tools > Create shortcut)
    //   - System web app (Chrome OS only)
    // * Legacy packaged app ("v1 packaged app")
    // * Hosted app (e.g. the Web Store "app" preinstalled on Chromebooks)
    TYPE_APP,
    // Devtools browser.
    TYPE_DEVTOOLS,
    // App popup browser. It behaves like an app browser (e.g. it should have an
    // AppBrowserController) but looks like a popup (e.g. it never has a tab
    // strip).
    TYPE_APP_POPUP,
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Browser for ARC++ Chrome custom tabs.
    // It's an enhanced version of TYPE_POPUP, and is used to show the Chrome
    // Custom Tab toolbar for ARC++ apps. It has UI customizations like using
    // the Android app's theme color, and the three dot menu in
    // CustomTabToolbarview.
    TYPE_CUSTOM_TAB,
#endif
    // Document picture-in-picture browser.  It's mostly the same as a
    // TYPE_POPUP, except that it floats above other windows.  It also has some
    // additional restrictions, like it cannot navigated, to prevent misuse.
    TYPE_PICTURE_IN_PICTURE,
    // If you add a new type, consider updating the test
    // BrowserTest.StartMaximized.
  };
  virtual Type GetType() const = 0;

  // Gets an object that provides common per-browser-window functionality for
  // user education. The remainder of functionality is provided directly by the
  // UserEducationService, which can be retrieved directly from the profile.
  virtual BrowserUserEducationInterface* GetUserEducationInterface() = 0;

  virtual web_app::AppBrowserController* GetAppBrowserController() = 0;
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_WINDOW_INTERFACE_H_
