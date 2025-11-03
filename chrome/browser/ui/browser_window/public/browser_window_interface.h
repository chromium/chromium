// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_WINDOW_INTERFACE_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_WINDOW_INTERFACE_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "content/public/browser/page_navigator.h"

#if !BUILDFLAG(IS_ANDROID)
#include "ui/base/window_open_disposition.h"
#endif

// This is the public interface for a browser window. Most features in
// //chrome/browser depend on this interface, and thus to prevent circular
// dependencies this interface should not depend on anything else in //chrome.
// Ping erikchen for assistance if this class does not have the functionality
// your feature needs. This comment will be deleted after there are 10+ features
// in BrowserWindowFeatures.
//
// This interface is shared between desktop platforms and the experimental
// desktop android platform. As such, the features exposed directly on this
// class should only be those that apply to all these platforms, and should only
// be features that are core to the concept of a browser window. Classes related
// to specific features should likely instead be stored either as an entry in
// the UnownedUserData (via BrowserWindowInterface::GetUnownedUserDataHost())
// or on DesktopBrowserWindowCapabilities.

#if !BUILDFLAG(IS_ANDROID)
namespace base {
class CallbackListSubscription;
}  // namespace base

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace web_modal {
class WebContentsModalDialogHost;
}  // namespace web_modal

class Browser;
class BrowserActions;
class BrowserWindowFeatures;
class DesktopBrowserWindowCapabilities;
class ExclusiveAccessManager;
class GURL;
class TabStripModel;
#endif  // BUILDFLAG(IS_ANDROID)

namespace ui {
class BaseWindow;
class UnownedUserDataHost;
}  // namespace ui

class Profile;
class SessionID;

#if !BUILDFLAG(IS_ANDROID)
// A feature which wants to show window level call to action UI  should call
// BrowserWindowInterface::ShowCallToAction and keep alive the instance of
// ScopedWindowCallToAction for the duration of the window-modal UI.
class ScopedWindowCallToAction {
 public:
  ScopedWindowCallToAction() = default;
  virtual ~ScopedWindowCallToAction() = default;
};
#endif  // !BUILDFLAG(IS_ANDROID)

class BrowserWindowInterface : public content::PageNavigator {
 public:
  // TODO(crbug.com/421758609): Hoist other enums above method declarations.
  enum class ClosingStatus {
    kPermitted,
    kDeniedByUser,
    kDeniedByPolicy,
    kDeniedUnloadHandlersNeedTime
  };

  // Returns the UnownedUserDataHost associated with this browser window. This
  // is used to retrieve arbitrary features from the browser window without
  // requiring BrowserWindowInterface to have knowledge of them.
  virtual ui::UnownedUserDataHost& GetUnownedUserDataHost() = 0;
  virtual const ui::UnownedUserDataHost& GetUnownedUserDataHost() const = 0;

  // Returns the ui::BaseWindow for this browser window. This allows for
  // generic window actions, such as activation, querying minimize/maximized
  // state, etc.
  virtual ui::BaseWindow* GetWindow() = 0;
  virtual const ui::BaseWindow* GetWindow() const = 0;

  // Returns the profile that semantically owns this browser window.
  // On most desktop platforms, there is only one profile per browser window.
  // This will never be null and never changes for the lifetime of a given
  // browser window. All tabs contained in a browser window have the same
  // Profile / BrowserContext as the browser window itself.
  // On mobile platforms, this is not the case -- browser windows may have
  // multiple profiles. Since this is currently not needed on mobile platforms,
  // this is okay.
  // On the experimental desktop android platform, we are adapting the mobile
  // version to have the same guarantees as existing desktop platforms. Thus,
  // when implemented, this will return a single Profile for the given browser
  // window.
  virtual Profile* GetProfile() = 0;
  virtual const Profile* GetProfile() const = 0;

  // Returns a session-unique ID.
  virtual const SessionID& GetSessionID() const = 0;

  // SessionService::WindowType mirrors these values.  If you add to this
  // enum, look at SessionService::WindowType to see if it needs to be
  // updated.
  //
  // TODO(https://crbug.com/331031753): Several of these existing Window Types
  // likely should not have been using Browser as a base to begin with and
  // should be migrated. Other types are not available on all platforms.
  // Please refrain from adding new types.
  //
  // GENERATED_JAVA_ENUM_PACKAGE: (
  //   org.chromium.chrome.browser.ui.browser_window)
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: BrowserWindowType
  // GENERATED_JAVA_PREFIX_TO_STRIP: TYPE_
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
#if !BUILDFLAG(IS_ANDROID)
    // Devtools browser.
    TYPE_DEVTOOLS,
#endif
    // App popup browser. It behaves like an app browser (e.g. it should have an
    // AppBrowserController) but looks like a popup (e.g. it never has a tab
    // strip).
    TYPE_APP_POPUP,
#if BUILDFLAG(IS_CHROMEOS)
    // Browser for ARC++ Chrome custom tabs.
    // It's an enhanced version of TYPE_POPUP, and is used to show the Chrome
    // Custom Tab toolbar for ARC++ apps. It has UI customizations like using
    // the Android app's theme color, and the three dot menu in
    // CustomTabToolbarview.
    TYPE_CUSTOM_TAB,
#endif
#if !BUILDFLAG(IS_ANDROID)
    // Document picture-in-picture browser.  It's mostly the same as a
    // TYPE_POPUP, except that it floats above other windows.  It also has some
    // additional restrictions, like it cannot navigated, to prevent misuse.
    TYPE_PICTURE_IN_PICTURE,
#endif
    // If you add a new type, consider updating the test
    // BrowserTest.StartMaximized.
  };
  virtual Type GetType() const = 0;

  // Represents the result of a check for whether a new browser window can be
  // created. See also CreateBrowserWindow().
  // TODO(devlin): The naming here implies that this is the *result* of a
  // creation request, but this is only used to indicate *whether* a new request
  // is allowed. Tweak to "CreationAllowed" or similar?
  enum class CreationStatus {
    // A new browser window can be created.
    kOk,

    // Indicates that the browser is shutting down.
    kErrorShuttingDown,

    // Indicates the profile is unsuitable for a new window. This can happen for
    // profiles that don't allow new windows, like certain incognito profiles or
    // other special profiles (signin screen, etc) or if the profile is shutting
    // down.
    kErrorProfileUnsuitable,

#if BUILDFLAG(IS_CHROMEOS)
    // Indicates the profile is currently loading kiosk mode, so no new windows
    // should be allowed.
    kErrorLoadingKiosk,
#endif
  };

  // S T O P
  // Please do not add new features here without consulting desktop leads
  // (erikchen@) and Clank leads (twellington@, dtrainor@). See comment at the
  // top of this file.
  // The following methods will be removed in the future.

#if !BUILDFLAG(IS_ANDROID)
  // Returns nullptr if no browser window with the given session ID exists.
  static BrowserWindowInterface* FromSessionID(const SessionID& session_id);

  // Opens a URL, with the given disposition. This is a convenience wrapper
  // around OpenURL from content::PageNavigator.
  virtual void OpenGURL(const GURL& gurl,
                        WindowOpenDisposition disposition) = 0;

  virtual TabStripModel* GetTabStripModel() = 0;
  virtual const TabStripModel* GetTabStripModel() const = 0;

  // Returns true if the tab strip is currently visible for this browser window.
  // Will return false on browser initialization before the tab strip is
  // initialized.
  virtual bool IsTabStripVisible() = 0;

  // Returns true if the browser controls are hidden due to being in fullscreen.
  virtual bool ShouldHideUIForFullscreen() const = 0;

  // Register callbacks invoked when browser has successfully processed its
  // close request and has been scheduled for deletion.
  using BrowserDidCloseCallback =
      base::RepeatingCallback<void(BrowserWindowInterface*)>;
  virtual base::CallbackListSubscription RegisterBrowserDidClose(
      BrowserDidCloseCallback callback) = 0;

  // Register callbacks invoked when browser attempted to close but the close
  // operation was cancelled.
  using BrowserCloseCancelledCallback =
      base::RepeatingCallback<void(BrowserWindowInterface*, ClosingStatus)>;
  virtual base::CallbackListSubscription RegisterBrowserCloseCancelled(
      BrowserCloseCancelledCallback callback) = 0;

  // WARNING: Many uses of base::WeakPtr are inappropriate and lead to bugs.
  // An appropriate use case is as a variable passed to an asynchronously
  // invoked PostTask.
  // An inappropriate use case is to store as a member of an object that can
  // outlive BrowserWindowInterface. This leads to inconsistent state machines.
  // For example (don't do this):
  // class FooOutlivesBrowser {
  //   base::WeakPtr<BrowserWindowInterface> bwi_;
  //   // Conceptually, this member should only be set if bwi_ is set.
  //   std::optional<SkColor> color_of_browser_;
  // };
  // For example (do this):
  // class FooOutlivesBrowser {
  //   // Use RegisterBrowserDidClose() to clear both bwi_ and
  //   // color_of_browser_ prior to bwi_ destruction.
  //   raw_ptr<BrowserWindowInterface> bwi_;
  //   std::optional<SkColor> color_of_browser_;
  // };
  virtual base::WeakPtr<BrowserWindowInterface> GetWeakPtr() = 0;

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
  virtual const BrowserWindowFeatures& GetFeatures() const = 0;

  // Returns the web contents modal dialog host pertaining to this
  // BrowserWindow.
  virtual web_modal::WebContentsModalDialogHost*
  GetWebContentsModalDialogHostForWindow() = 0;

  // Returns the web contents modal dialog host for the `tab_interface`.
  virtual web_modal::WebContentsModalDialogHost*
  GetWebContentsModalDialogHostForTab(tabs::TabInterface* tab_interface) = 0;

  // Whether the window is active.
  // The definition of "active" aligns with the window being painted as active
  // instead of the top level widget having focus.
  // Note that on platforms other than Windows, this might not reflect the
  // actual OS level window activation status, as Chrome internally marks any
  // browser window as "active" as soon as it starts the (asynchronous) process
  // to activate the window. However there is no guarantee that the window will
  // actually be activated on the OS level, so this field can easily get out of
  // sync with reality.
  virtual bool IsActive() const = 0;

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

  // This is used by features that need to operate on most or all tabs in the
  // browser window. Do not use this method to find a specific tab.
  virtual std::vector<tabs::TabInterface*> GetAllTabInterfaces() = 0;

  // Downcasts to a Browser*. The only valid use for this method is when
  // migrating a large chunk of code to BrowserWindowInterface, to allow
  // incremental migration.
  virtual Browser* GetBrowserForMigrationOnly() = 0;
  virtual const Browser* GetBrowserForMigrationOnly() const = 0;

  // Checks if the browser popup is tab modal dialog.
  virtual bool IsTabModalPopupDeprecated() const = 0;

  // Features that want to show a window level call to action UI can be mutually
  // exclusive. Before gating on call to action UI first check
  // `CanShowModCanShowCallToActionalUI`. Then call ShowCallToAction() and keep
  // `ScopedWindowCallToAction` alive to prevent other features from showing
  // window level call to action Uis.
  virtual bool CanShowCallToAction() const = 0;
  virtual std::unique_ptr<ScopedWindowCallToAction> ShowCallToAction() = 0;

  virtual DesktopBrowserWindowCapabilities* capabilities() = 0;
  virtual const DesktopBrowserWindowCapabilities* capabilities() const = 0;
#endif  // !BUILDFLAG(IS_ANDROID)

  // S T O P
  // Please do not add new features here without consulting desktop leads
  // (erikchen@) and Clank leads (twellington@, dtrainor@). See comment at the
  // top of this file.
  // The following methods will be removed in the future.
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_BROWSER_WINDOW_INTERFACE_H_
