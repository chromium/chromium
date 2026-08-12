// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_H_
#define CHROME_BROWSER_UI_BROWSER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/scoped_observation_traits.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "chrome/browser/tab_contents/web_contents_collection.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar_controller.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/browser_window_deleter.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_change_type.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/window_feature_controller/window_feature_controller.h"
#include "components/paint_preview/buildflags/buildflags.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sessions/core/session_id.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/browser/fullscreen_types.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "extensions/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom-forward.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/ui_base_types.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#error This file should only be included on desktop.
#endif

class BrowserInitState;
class BrowserWindow;
struct BrowserWindowCreateParams;
class BrowserWindowFeatures;
class FindBarController;
class Profile;
class ScopedKeepAlive;
class ScopedProfileKeepAlive;
class TabStripModelDelegate;

namespace tabs {
class TabInterface;
}

namespace blink {
enum class ProtocolHandlerSecurityLevel;
}

namespace content {
class NavigationHandle;
}  // namespace content

namespace web_app {
class AppBrowserController;
}

namespace web_modal {
class WebContentsModalDialogHost;
}

// This enum is not a member of `Browser` so that it can be forward
// declared in `unload_controller.h` to avoid circular includes.
enum class BrowserClosingStatus {
  kPermitted,
  kDeniedByUser,
  kDeniedByPolicy,
  kDeniedUnloadHandlersNeedTime
};

// An instance of this class represents a single browser window on Desktop.
// Owned by BrowserManagerService.
// All features that are scoped to a browser window should have lifetime scoped
// to an instance of this class, usually via direct or indirect ownership of a
// std::unique_ptr. See BrowserWindowFeatures and TabFeatures.
class Browser : public TabStripModelObserver,
                public WebContentsCollection::Observer,
                public BrowserWindowInterface {
 public:
  // Constructors, Creation, Showing //////////////////////////////////////////

  Browser(const Browser&) = delete;
  Browser& operator=(const Browser&) = delete;

  ~Browser() override;

  // Interface implementations ////////////////////////////////////////////////

  // Overridden from TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void TabStripEmpty() override;
  void OnTabGroupFocusChanged(
      std::optional<tab_groups::TabGroupId> new_focused_group,
      std::optional<tab_groups::TabGroupId> old_focused_group) override;

  // BrowserWindowInterface overrides:
  Profile* GetProfile() override;
  const Profile* GetProfile() const override;
  bool IsDeleteScheduled() const override;
  void OpenGURL(const GURL& gurl, WindowOpenDisposition disposition) override;
  content::WebContents* OpenURL(
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;
  const SessionID& GetSessionID() const override;
  TabStripModel* GetTabStripModel() override;
  const TabStripModel* GetTabStripModel() const override;
  bool IsTabStripVisible() override;
  base::CallbackListSubscription RegisterBrowserDidClose(
      BrowserDidCloseCallback callback) override;
  base::CallbackListSubscription RegisterBrowserCloseCancelled(
      BrowserCloseCancelledCallback callback) override;
  base::WeakPtr<BrowserWindowInterface> GetWeakPtr() override;
  base::CallbackListSubscription RegisterActiveTabDidChange(
      ActiveTabChangeCallback callback) override;
  tabs::TabInterface* GetActiveTabInterface() override;
  BrowserWindowFeatures& GetFeatures() override;
  const BrowserWindowFeatures& GetFeatures() const override;
  ui::UnownedUserDataHost& GetUnownedUserDataHost() override;
  const ui::UnownedUserDataHost& GetUnownedUserDataHost() const override;
  web_modal::WebContentsModalDialogHost*
  GetWebContentsModalDialogHostForWindow() override;
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHostForTab(
      tabs::TabInterface* tab_interface) override;
  bool IsActive() const override;
  base::CallbackListSubscription RegisterDidBecomeActive(
      DidBecomeActiveCallback callback) override;
  base::CallbackListSubscription RegisterDidBecomeInactive(
      DidBecomeInactiveCallback callback) override;
  Type GetType() const override;
  std::vector<tabs::TabInterface*> GetAllTabInterfaces() override;
  Browser* GetBrowserForMigrationOnly() override;
  const Browser* GetBrowserForMigrationOnly() const override;
  bool IsTabModalPopup() const override;
  void SetIsTabModalPopup(
      bool is_tab_modal_popup,
      base::PassKey<internal::ScopedBrowserShower>) override;
  bool CreatedBySessionRestore() const override;
  ui::BaseWindow* GetWindow() override;
  const ui::BaseWindow* GetWindow() const override;
  DesktopBrowserWindowCapabilities* capabilities() override;
  const DesktopBrowserWindowCapabilities* capabilities() const override;

 private:
  friend BrowserWindowInterface* CreateBrowserWindow(
      BrowserWindowCreateParams create_params);
  friend std::unique_ptr<Browser> DeprecatedCreateOwnedBrowserWindowForTesting(
      BrowserWindowCreateParams create_params);

  // Used to describe why a tab is being detached. This is used by
  // TabDetachedAtImpl.
  enum class DetachType {
    // Result of TabDetachedAt.
    kDetach,

    // Result of TabReplacedAt.
    kReplace,

    // Result of the tab strip not having any significant tabs.
    kEmpty
  };

  // Creates a browser instance with the provided params. Returns an unowned
  // pointer to the created browser.
  // Crashes if the requested browser creation is not allowed.
  // For example, browser creation will not be allowed for profiles that
  // disallow browsing (like sign-in profile on Chrome OS).
  //
  // Unless |params->window| is specified, a new BrowserWindow will be created
  // for the browser - the created BrowserWindow will take the ownership of the
  // created Browser instance.
  //
  // If |params.window| is set, the caller is expected to take the ownership
  // of the created Browser instance.
  static Browser* Create(BrowserWindowCreateParams params);

  // WARNING: Use of this is DEPRECATED and exists only to support pre-existing
  // browser unittests.
  // TODO(crbug.com/417766643): Remove this once all use of Browser in unittests
  // has been eliminated.
  static std::unique_ptr<Browser> DeprecatedCreateOwnedForTesting(
      BrowserWindowCreateParams params);

  explicit Browser(BrowserWindowCreateParams params);

  // Command and state updating ///////////////////////////////////////////////

  // Handle changes to tab strip model.
  void OnTabInsertedAt(content::WebContents* contents, int index);
  void OnTabClosing(tabs::TabInterface* tab, bool* had_active_modal_dialog);
  void OnTabDetached(tabs::TabInterface* tab,
                     bool was_active,
                     bool had_active_modal_dialog);
  void RestoreFocusAfterTabModalPopupClose(tabs::TabHandle active_tab_handle);
  void OnTabDeactivated(content::WebContents* contents);
  void OnActiveTabChanged(const TabStripModelChange& change,
                          const TabStripSelectionChange& selection);
  void OnTabMoved(int from_index, int to_index);
  void OnTabReplacedAt(content::WebContents* old_contents,
                       content::WebContents* new_contents,
                       int index);

  // Handle changes to kDevToolsAvailability preference.
  void OnDevToolsAvailabilityChanged();

  // In-progress download termination handling /////////////////////////////////

  // Called when the user has decided whether to proceed or not with the browser
  // closure, in case the cookie migration notice was shown. |proceed_closing|
  // is true if the browser can  be closed.
  void CookieMigrationNoticeResponse(bool proceed_closing);

  // Assorted utility functions ///////////////////////////////////////////////

  // Sets the specified browser as the delegate of the WebContents and all the
  // associated tab helpers that are needed. If |set_delegate| is true, this
  // browser object is set as a delegate for |web_contents| components, else
  // is is removed as a delegate.
  void SetAsDelegate(content::WebContents* web_contents, bool set_delegate);

  void TabDetachedAtImpl(content::WebContents* contents,
                         bool was_active,
                         DetachType type);

  // Shared code between Reload() and ReloadBypassingCache().
  void ReloadInternal(WindowOpenDisposition disposition, bool bypass_cache);

  // Create `FindBarController` if it does not exist.
  // TODO(crbug.com/423956131): Convert to `GetFindBarController` which returns
  // existing `FindBarController`.
  FindBarController* CreateOrGetFindBarController();

  // Returns true if a `FindBarController` exists for this browser.
  // TODO(crbug.com/423956131): Remove this function.
  bool HasFindBarController();

  // Data members /////////////////////////////////////////////////////////////

  PrefChangeRegistrar profile_pref_registrar_;

  // This Browser's type.
  const Type type_;

  // This Browser's profile.
  const raw_ptr<Profile, AcrossTasksDanglingUntriaged> profile_;

  // Prevent Profile deletion until this browser window is closed.
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;

  // This Browser's window.
  std::unique_ptr<BrowserWindow, BrowserWindowDeleter> window_;

  std::unique_ptr<TabStripModelDelegate> const tab_strip_model_delegate_;
  std::unique_ptr<TabStripModel> const tab_strip_model_;

  // Unique identifier of this browser for session restore. This id is only
  // unique within the current session, and is not guaranteed to be unique
  // across sessions.
  const SessionID session_id_;

  std::unique_ptr<ScopedKeepAlive> keep_alive_;

  WebContentsCollection web_contents_collection_{this};

  // If true, the browser window was created as a tab modal pop-up.
  bool is_tab_modal_popup_ = false;

  using DidActiveTabChangeCallbackList =
      base::RepeatingCallbackList<void(BrowserWindowInterface*)>;
  DidActiveTabChangeCallbackList did_active_tab_change_callback_list_;

  ui::UnownedUserDataHost unowned_user_data_host_;

  // Creation and initial parameters of this browser window. Constructed early
  // (before `features_` and the window) so downstream features and window setup
  // can query it. Declared after `unowned_user_data_host_` so it is destroyed
  // before the host it registers with.
  std::unique_ptr<BrowserInitState> init_state_;

  std::unique_ptr<BrowserWindowFeatures> features_;

  // Tracks whether the browser object is fully initialized.
  bool is_initialized_ = false;

  // The following factory is used to close the frame at a later time.
  base::WeakPtrFactory<Browser> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_BROWSER_H_
