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
#include "chrome/browser/ui/browser_window_deleter.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_change_type.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/unload_controller.h"
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

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/platform_session_manager.h"
#endif

class BackgroundContents;
class BrowserInitState;
class BrowserView;
class BrowserWindow;
class BrowserWindowFeatures;
class FindBarController;
class Profile;
class ScopedKeepAlive;
class ScopedProfileKeepAlive;
class StatusBubble;
class TabStripModelDelegate;

namespace tabs {
class TabInterface;
}

namespace blink {
enum class ProtocolHandlerSecurityLevel;
}

namespace content {
class NavigationHandle;
class SessionStorageNamespace;
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
  // Represents the source of a browser creation request.
  enum class CreationSource {
    kUnknown,
    kSessionRestore,
    kStartupCreator,
    kLastAndUrlsStartupPref,
    kDeskTemplate,
  };

  // Represents whether a value was known to be explicitly specified.
  enum class ValueSpecified { kUnknown, kSpecified, kUnspecified };

  // The default value for a browser's `restore_id` param.
  static constexpr int kDefaultRestoreId = 0;

  struct CreateParams {
    explicit CreateParams(Profile* profile, bool user_gesture);
    CreateParams(Type type, Profile* profile, bool user_gesture);
    CreateParams(const CreateParams& other);
    CreateParams& operator=(const CreateParams& other);
    ~CreateParams();

    static CreateParams CreateForApp(const std::string& app_name,
                                     bool trusted_source,
                                     const gfx::Rect& window_bounds,
                                     Profile* profile,
                                     bool user_gesture);

    static CreateParams CreateForAppPopup(const std::string& app_name,
                                          bool trusted_source,
                                          const gfx::Rect& window_bounds,
                                          Profile* profile,
                                          bool user_gesture);

    static CreateParams CreateForPictureInPicture(const std::string& app_name,
                                                  bool trusted_source,
                                                  Profile* profile,
                                                  bool user_gesture);

    static CreateParams CreateForDevTools(Profile* profile);

    // The browser type.
    Type type;

    // The associated profile.
    raw_ptr<Profile, AcrossTasksDanglingUntriaged> profile;

    // Specifies the WindowFeatureController `is_trusted_source_` value.
    bool trusted_source = false;

    // Specifies the browser `omit_from_session_restore_` value, whether the new
    // Browser should be omitted from being saved/restored by session restore.
    bool omit_from_session_restore = false;

    // Specifies the browser `should_trigger_session_restore` value. If true, a
    // new window opening should be treated like the start of a session (with
    // potential session restore, startup URLs, etc.). Otherwise, don't restore
    // the session.
    bool should_trigger_session_restore = true;

    // The bounds of the window to open.
    gfx::Rect initial_bounds;
    // Whether `initial_bounds.origin()` was explicitly specified, if known.
    // Used to disambiguate coordinate (0,0) from an unspecified location when
    // parameters originate from the JS Window.open() window features string,
    // e.g. window.open(... 'left=0,top=0,...') vs window.open(... 'popup,...').
    ValueSpecified initial_origin_specified = ValueSpecified::kUnknown;

    // The workspace the window should open in, if the platform supports it.
    std::string initial_workspace;

    // Whether the window is visible on all workspaces initially, if the
    // platform supports it.
    bool initial_visible_on_all_workspaces_state = false;

    ui::mojom::WindowShowState initial_show_state =
        ui::mojom::WindowShowState::kDefault;

    CreationSource creation_source = CreationSource::kUnknown;

#if BUILDFLAG(IS_CHROMEOS)
    // If set, the browser should be created on the display given by
    // `display_id`.
    std::optional<int64_t> display_id;
#endif

#if BUILDFLAG(IS_LINUX)
    // When the browser window is shown, the desktop environment is notified
    // using this ID.  In response, the desktop will stop playing the "waiting
    // for startup" animation (if any).
    std::string startup_id;
#endif

#if BUILDFLAG(IS_OZONE)
    // Some platforms support session management assisted by the windowing
    // system, such as:
    // -ChromeOS, where this id is retrieved from the session backing
    // storage and used by Ash to restore the browser window state.
    // - Ozone/Wayland, with xdg-session-management protocol extension, in
    // which case, this id is sent to the Wayland compositor, so it can also
    // restore the window state when the window is initialized. Se
    // ui/ozone/public/platfrom_session_manager.h for more details.
    int32_t restore_id = kDefaultRestoreId;
#endif

    // Whether this browser was created by a user gesture. We track this
    // specifically for the multi-user case in chromeos where we can place
    // windows generated by user gestures differently from ones
    // programmatically created.
    bool user_gesture;

    // Whether this browser was created specifically for dragged tab(s).
    bool in_tab_dragging = false;

    // Supply a custom BrowserWindow implementation, to be used instead of the
    // default. Intended for testing. The resulting Browser takes ownership
    // of `window`.
    // TODO(crbug.com/413168662): CreateParams should be updated to be move-only
    // and this should become a unique_ptr (or removed completely once
    // deprecated Browser unit tests are eliminated).
    raw_ptr<BrowserWindow, DanglingUntriaged> window = nullptr;

    // User-set title of this browser window, if there is one.
    std::string user_title;

    // Only applied when not in forced app mode. True if the browser is
    // resizeable.
    bool can_resize = true;

    // Only applied when not in forced app mode. True if the browser can be
    // maximizable.
    bool can_maximize = true;

    // Only applied when not in forced app mode. True if the browser can enter
    // fullscreen.
    bool can_fullscreen = true;

    // Document Picture in Picture options, specific to TYPE_PICTURE_IN_PICTURE.
    std::optional<blink::mojom::PictureInPictureWindowOptions> pip_options;

    // Specifies the collapsed state for the Vertical Tab Strip. True if the
    // browser is collapsed.
    std::optional<bool> vertical_tab_strip_collapsed;
    // Specifies the width for the uncollapsed Vertical Tab Strip.
    std::optional<int> vertical_tab_strip_uncollapsed_width;

    // The application name that is also the name of the window to the shell.
    // Do not set this value directly, use CreateForApp/CreateForAppPopup.
    // This name will be set for:
    // 1) v1 applications launched via an application shortcut or extension API.
    // 2) undocked devtool windows.
    // 3) popup windows spawned from v1 applications.
    std::string app_name;

    // Specifies the focused tab group ID, if the window should be created in a
    // focused state.
    std::optional<tab_groups::TabGroupId> focused_tab_group_id;

   private:
    friend class Browser;
    friend class WindowSizerChromeOSTest;

    static CreateParams CreateForAppBase(bool is_popup,
                                         const std::string& app_name,
                                         bool trusted_source,
                                         const gfx::Rect& window_bounds,
                                         Profile* profile,
                                         bool user_gesture);
  };

  // Constructors, Creation, Showing //////////////////////////////////////////

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
  static Browser* Create(const CreateParams& params);

  // WARNING: Use of this is DEPRECATED and exists only to support pre-existing
  // browser unittests. Similar to Create() above, however the created browser
  // is owned by the caller.
  // TODO(crbug.com/417766643): Remove this once all use of Browser in unittests
  // has been eliminated.
  static std::unique_ptr<Browser> DeprecatedCreateOwnedForTesting(
      const CreateParams& params);

  Browser(const Browser&) = delete;
  Browser& operator=(const Browser&) = delete;

  ~Browser() override;

  // Sets whether the UI should be immediately updated when scheduled on a
  // test.
  void set_update_ui_immediately_for_testing() {
    update_ui_immediately_for_testing_ = true;
  }

  // Accessors ////////////////////////////////////////////////////////////////

  // In production code, each instance of Browser will always instantiate an
  // instance of BrowserView in the constructor. Some tests instantiate a
  // Browser without a BrowserView: this is an anti-pattern and should be
  // avoided.
  BrowserView& GetBrowserView();

  base::WeakPtr<Browser> AsWeakPtr();
  base::WeakPtr<const Browser> AsWeakPtr() const;

  // State Storage and Retrieval for UI ///////////////////////////////////////

  GURL GetNewTabURL() const;

  // OnBeforeUnload handling //////////////////////////////////////////////////

  // Called when the window closing process has been cancelled.
  void NotifyWindowCloseCancelled(BrowserWindowInterface::ClosingStatus status);

  // Called when the window closing process has been completed and the window
  // can be safely destroyed.
  void OnWindowCloseComplete();

  // In-progress download termination handling /////////////////////////////////


  // External state change handling ////////////////////////////////////////////

  // Invoked at the end of a fullscreen transition.
  void WindowFullscreenStateChanged();

  // Only used on Mac. Called when the top ui style has been changed since this
  // may trigger bookmark bar state change.
  void FullscreenTopUIStateChanged();

  void OnFindBarVisibilityChanged();

  // Called by Navigate() when a navigation has occurred in a tab in
  // this Browser. Updates the UI for the start of this navigation.
  void UpdateUIForNavigationInTab(content::WebContents* contents,
                                  ui::PageTransition transition,
                                  NavigateParams::WindowAction action,
                                  bool user_initiated);

  // Interface implementations ////////////////////////////////////////////////

  // Overridden from TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void TabStripEmpty() override;

  // Gets the browser for opening chrome:// pages. This will return the opener
  // browser if the current browser is in picture-in-picture mode, otherwise
  // returns the current browser.
  BrowserWindowInterface* GetBrowserForOpeningWebUi();

  std::vector<StatusBubble*> GetStatusBubblesForTesting();
  UnloadController* GetUnloadControllerForTesting() {
    return UnloadController::From(this);
  }

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

  // Called by BrowserView on active change for the browser.
  void DidBecomeActive();
  void DidBecomeInactive();

  // Synchronously destroys the browser, `this` is no longer valid after the
  // operation completes.
  // WARNING: Clients should generally not use this and instead prefer
  // requesting the browser close via BrowserWindow::Close(), which happens
  // async and allows graceful teardown of the tab strip and associated data.
  void SynchronouslyDestroyBrowser();

#if BUILDFLAG(IS_OZONE)
  const std::optional<ui::PlatformSessionWindowData>& platform_session_data()
      const {
    return platform_session_data_;
  }
#endif

 private:
  friend class BrowserTest;
  friend class BrowserWebContentsDelegate;
  friend class ExclusiveAccessTest;
  friend class FullscreenControllerInteractiveTest;
  FRIEND_TEST_ALL_PREFIXES(AppModeTest, EnableAppModeTest);
  FRIEND_TEST_ALL_PREFIXES(BrowserCloseTest, LastIncognito);
  FRIEND_TEST_ALL_PREFIXES(BrowserCloseTest, LastRegular);
  FRIEND_TEST_ALL_PREFIXES(BrowserTest, OpenAppWindowLikeNtp);
  FRIEND_TEST_ALL_PREFIXES(BrowserTest, AppIdSwitch);
  FRIEND_TEST_ALL_PREFIXES(ExclusiveAccessBubbleWindowControllerTest,
                           DenyExitsFullscreen);
  FRIEND_TEST_ALL_PREFIXES(ExclusiveAccessTest,
                           TabEntersPresentationModeFromWindowed);
  FRIEND_TEST_ALL_PREFIXES(BrowserCloseTest, LastGuest);

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

  explicit Browser(const CreateParams& params);

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

  // UI update coalescing and handling ////////////////////////////////////////

  // Asks the toolbar (and as such the location bar) to update its state to
  // reflect the current tab's current URL, security state, etc.
  // If |should_restore_state| is true, we're switching (back?) to this tab and
  // should restore any previous location bar state (such as user editing) as
  // well.
  void UpdateToolbar(bool should_restore_state);

  // Asks the toolbar to layout and redraw to reflect the current security
  // state.
  void UpdateToolbarSecurityState();

  // Does one or both of the following for each bit in |changed_flags|:
  // . If the update should be processed immediately, it is.
  // . If the update should processed asynchronously (to avoid lots of ui
  //   updates), then scheduled_updates_ is updated for the |source| and update
  //   pair and a task is scheduled (assuming it isn't running already)
  //   that invokes ProcessPendingUIUpdates.
  void ScheduleUIUpdate(content::WebContents* source, unsigned changed_flags);

  // Processes all pending updates to the UI that have been scheduled by
  // ScheduleUIUpdate in scheduled_updates_.
  void ProcessPendingUIUpdates();

  // Removes all entries from scheduled_updates_ whose source is contents.
  void RemoveScheduledUpdatesFor(content::WebContents* contents);

  // Getters for UI ///////////////////////////////////////////////////////////

  // Returns the list of StatusBubbles from the current toolbar. It is possible
  // for this to be empty if called before the toolbar has initialized. In a
  // split view, there will be multiple status bubbles with the active one
  // listed first.
  // TODO(beng): remove this.
  std::vector<StatusBubble*> GetStatusBubbles();



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

  // Updates the loading state for the window and tabstrip.
  void UpdateWindowForLoadingStateChanged(content::WebContents* source,
                                          bool should_show_loading_ui);

  // Shared code between Reload() and ReloadBypassingCache().
  void ReloadInternal(WindowOpenDisposition disposition, bool bypass_cache);

  // Returns true if a BackgroundContents should be created in response to a
  // WebContents::CreateNewWindow() call.
  bool ShouldCreateBackgroundContents(
      content::SiteInstance* source_site_instance,
      const GURL& opener_url,
      const std::string& frame_name);

  // Creates a BackgroundContents. This should only be called when
  // ShouldCreateBackgroundContents() is true.
  BackgroundContents* CreateBackgroundContents(
      content::SiteInstance* source_site_instance,
      content::RenderFrameHost* opener,
      const GURL& opener_url,
      bool is_new_browsing_instance,
      const std::string& frame_name,
      const GURL& target_url,
      const content::StoragePartitionConfig& partition_config,
      content::SessionStorageNamespace* session_storage_namespace);



  // Create `FindBarController` if it does not exist.
  // TODO(crbug.com/423956131): Convert to `GetFindBarController` which returns
  // existing `FindBarController`.
  FindBarController* CreateOrGetFindBarController();

  // Returns true if a `FindBarController` exists for this browser.
  // TODO(crbug.com/423956131): Remove this function.
  bool HasFindBarController();

  // Notifies the tab UI that it should update when the browser schedule or
  // process UI updates.
  void NotifyTabUIChanged(int tab_index, TabChangeType change_type);

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

  // The active state of this browser.
  bool is_active_ = false;

  std::unique_ptr<TabStripModelDelegate> const tab_strip_model_delegate_;
  std::unique_ptr<TabStripModel> const tab_strip_model_;

  // Unique identifier of this browser for session restore. This id is only
  // unique within the current session, and is not guaranteed to be unique
  // across sessions.
  const SessionID session_id_;

  // UI update coalescing and handling ////////////////////////////////////////

  typedef std::map<tabs::TabInterface*, int> UpdateMap;

  // Maps from TabInterface to pending UI updates that need to be processed.
  // We don't update things like the URL or tab title right away to avoid
  // flickering and extra painting.
  // See ScheduleUIUpdate and ProcessPendingUIUpdates.
  UpdateMap scheduled_updates_;

  // In-progress download termination handling /////////////////////////////////

  /////////////////////////////////////////////////////////////////////////////

  std::unique_ptr<ScopedKeepAlive> keep_alive_;

  // If true, immediately updates the UI when scheduled.
  bool update_ui_immediately_for_testing_ = false;

  // The opener browser of the document picture-in-picture browser. Null if the
  // current browser is a regular browser.
  raw_ptr<BrowserWindowInterface> opener_browser_ = nullptr;

  WebContentsCollection web_contents_collection_{this};

  // If true, the Browser window has been closed and this will be deleted
  // shortly (after a PostTask).
  bool is_delete_scheduled_ = false;

  // If true, the browser window was created as a tab modal pop-up.
  bool is_tab_modal_popup_ = false;

  using BrowserDidCloseCallbackList =
      base::RepeatingCallbackList<void(BrowserWindowInterface*)>;
  BrowserDidCloseCallbackList browser_did_close_callback_list_;

  using BrowserCloseCancelledCallbackList =
      base::RepeatingCallbackList<void(BrowserWindowInterface*,
                                       BrowserWindowInterface::ClosingStatus)>;
  BrowserCloseCancelledCallbackList browser_close_cancelled_callback_list_;

  using DidActiveTabChangeCallbackList =
      base::RepeatingCallbackList<void(BrowserWindowInterface*)>;
  DidActiveTabChangeCallbackList did_active_tab_change_callback_list_;

  using DidBecomeActiveCallbackList =
      base::RepeatingCallbackList<void(BrowserWindowInterface*)>;
  DidBecomeActiveCallbackList did_become_active_callback_list_;

  using DidBecomeInactiveCallbackList =
      base::RepeatingCallbackList<void(BrowserWindowInterface*)>;
  DidBecomeInactiveCallbackList did_become_inactive_callback_list_;

  ui::UnownedUserDataHost unowned_user_data_host_;

  // Creation and initial parameters of this browser window. Constructed early
  // (before `features_` and the window) so downstream features and window setup
  // can query it. Declared after `unowned_user_data_host_` so it is destroyed
  // before the host it registers with.
  std::unique_ptr<BrowserInitState> init_state_;

  std::unique_ptr<BrowserWindowFeatures> features_;

#if BUILDFLAG(IS_OZONE)
  // If supported by the platform, this stores stores data related to the
  // windowing system level session. E.g: session and window IDs. See
  // ui/ozone/public/platform_session_manager.h for more details.
  std::optional<ui::PlatformSessionWindowData> platform_session_data_ =
      std::nullopt;
#endif

  // Tracks whether the browser object is fully initialized.
  bool is_initialized_ = false;

  // The following factory is used for chrome update coalescing.
  base::WeakPtrFactory<Browser> chrome_updater_factory_{this};

  // The following factory is used to close the frame at a later time.
  base::WeakPtrFactory<Browser> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_BROWSER_H_
