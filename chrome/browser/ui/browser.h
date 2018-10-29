// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_H_
#define CHROME_BROWSER_UI_BROWSER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "chrome/browser/devtools/devtools_toggle_action.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper_observer.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/chrome_bubble_manager.h"
#include "chrome/browser/ui/chrome_web_modal_dialog_manager_delegate.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/profile_chooser_constants.h"
#include "chrome/browser/ui/signin_view_controller.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/omnibox/browser/toolbar_model.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sessions/core/session_id.h"
#include "components/translate/content/browser/content_translate_driver.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/page_zoom.h"
#include "extensions/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/ui_base_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/shell_dialogs/select_file_dialog.h"

#if !defined(OS_ANDROID)
#include "components/zoom/zoom_observer.h"
#endif  // !defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry_observer.h"
#endif

class BrowserContentSettingBubbleModelDelegate;
class BrowserInstantController;
class BrowserSyncedWindowDelegate;
class BrowserToolbarModelDelegate;
class BrowserLiveTabContext;
class BrowserWindow;
class FastUnloadController;
class FindBarController;
class Profile;
class ScopedKeepAlive;
class StatusBubble;
class TabStripModel;
class TabStripModelDelegate;
class UnloadController;

namespace chrome {
class BrowserCommandController;
}

namespace content {
class SessionStorageNamespace;
}

namespace extensions {
class BrowserExtensionWindowController;
class HostedAppBrowserController;
class Extension;
class ExtensionRegistry;
}

namespace gfx {
class Image;
}

namespace ui {
struct SelectedFileInfo;
}

namespace web_modal {
class WebContentsModalDialogHost;
}

namespace viz {
class SurfaceId;
}

class Browser : public TabStripModelObserver,
                public content::WebContentsDelegate,
                public CoreTabHelperDelegate,
                public ChromeWebModalDialogManagerDelegate,
                public BookmarkTabHelperObserver,
#if !defined(OS_ANDROID)
                public zoom::ZoomObserver,
#endif  // !defined(OS_ANDROID)
                public content::PageNavigator,
                public content::NotificationObserver,
#if BUILDFLAG(ENABLE_EXTENSIONS)
                public extensions::ExtensionRegistryObserver,
#endif
                public translate::ContentTranslateDriver::Observer,
                public ui::SelectFileDialog::Listener {
 public:
  // SessionService::WindowType mirrors these values.  If you add to this
  // enum, look at SessionService::WindowType to see if it needs to be
  // updated.
  enum Type {
    // If you add a new type, consider updating the test
    // BrowserTest.StartMaximized.
    TYPE_TABBED = 1,
    TYPE_POPUP = 2
  };

  // Possible elements of the Browser window.
  enum WindowFeature {
    FEATURE_NONE = 0,
    FEATURE_TITLEBAR = 1,
    FEATURE_TABSTRIP = 2,
    FEATURE_TOOLBAR = 4,
    FEATURE_LOCATIONBAR = 8,
    FEATURE_BOOKMARKBAR = 16,
    FEATURE_INFOBAR = 32,
    FEATURE_DOWNLOADSHELF = 64,
  };

  // The context for a download blocked notification from
  // OkToCloseWithInProgressDownloads.
  enum DownloadClosePreventionType {
    // Browser close is not blocked by download state.
    DOWNLOAD_CLOSE_OK,

    // The browser is shutting down and there are active downloads
    // that would be cancelled.
    DOWNLOAD_CLOSE_BROWSER_SHUTDOWN,

    // There are active downloads associated with this incognito profile
    // that would be canceled.
    DOWNLOAD_CLOSE_LAST_WINDOW_IN_INCOGNITO_PROFILE,
  };

  struct CreateParams {
    explicit CreateParams(Profile* profile, bool user_gesture);
    CreateParams(Type type, Profile* profile, bool user_gesture);
    CreateParams(const CreateParams& other);

    static CreateParams CreateForApp(const std::string& app_name,
                                     bool trusted_source,
                                     const gfx::Rect& window_bounds,
                                     Profile* profile,
                                     bool user_gesture);

    static CreateParams CreateForDevTools(Profile* profile);

    // The browser type.
    Type type;

    // The associated profile.
    Profile* profile;

    // Specifies the browser is_trusted_source_ value.
    bool trusted_source = false;

    // The bounds of the window to open.
    gfx::Rect initial_bounds;

    // The workspace the window should open in, if the platform supports it.
    std::string initial_workspace;

    ui::WindowShowState initial_show_state = ui::SHOW_STATE_DEFAULT;

    bool is_session_restore = false;

    // Whether this browser was created by a user gesture. We track this
    // specifically for the multi-user case in chromeos where we can place
    // windows generated by user gestures differently from ones
    // programmatically created.
    bool user_gesture;

    // Supply a custom BrowserWindow implementation, to be used instead of the
    // default. Intended for testing.
    BrowserWindow* window = nullptr;

   private:
    friend class Browser;
    friend class WindowSizerAshTest;

    // The application name that is also the name of the window to the shell.
    // Do not set this value directly, use CreateForApp.
    // This name will be set for:
    // 1) v1 applications launched via an application shortcut or extension API.
    // 2) undocked devtool windows.
    // 3) popup windows spawned from v1 applications.
    std::string app_name;

    // When set to true, skip initializing |window_| and everything that depends
    // on it.
    bool skip_window_init_for_testing = false;
  };

  // Constructors, Creation, Showing //////////////////////////////////////////

  explicit Browser(const CreateParams& params);
  ~Browser() override;

  // Set overrides for the initial window bounds and maximized state.
  void set_override_bounds(const gfx::Rect& bounds) {
    override_bounds_ = bounds;
  }
  ui::WindowShowState initial_show_state() const { return initial_show_state_; }
  void set_initial_show_state(ui::WindowShowState initial_show_state) {
    initial_show_state_ = initial_show_state;
  }
  // Return true if the initial window bounds have been overridden.
  bool bounds_overridden() const {
    return !override_bounds_.IsEmpty();
  }
  // Set indicator that this browser is being created via session restore.
  // This is used on the Mac (only) to determine animation style when the
  // browser window is shown.
  void set_is_session_restore(bool is_session_restore) {
    is_session_restore_ = is_session_restore;
  }
  bool is_session_restore() const {
    return is_session_restore_;
  }

  // Accessors ////////////////////////////////////////////////////////////////

  Type type() const { return type_; }
  const std::string& app_name() const { return app_name_; }
  bool is_trusted_source() const { return is_trusted_source_; }
  Profile* profile() const { return profile_; }
  gfx::Rect override_bounds() const { return override_bounds_; }
  const std::string& initial_workspace() const { return initial_workspace_; }

  // |window()| will return NULL if called before |CreateBrowserWindow()|
  // is done.
  BrowserWindow* window() const { return window_; }
  ToolbarModel* toolbar_model() { return toolbar_model_.get(); }
  const ToolbarModel* toolbar_model() const { return toolbar_model_.get(); }
#if defined(UNIT_TEST)
  void swap_toolbar_models(std::unique_ptr<ToolbarModel>* toolbar_model) {
    toolbar_model->swap(toolbar_model_);
  }
#endif
  TabStripModel* tab_strip_model() const { return tab_strip_model_.get(); }
  chrome::BrowserCommandController* command_controller() {
    return command_controller_.get();
  }
  const SessionID& session_id() const { return session_id_; }
  BrowserContentSettingBubbleModelDelegate*
      content_setting_bubble_model_delegate() {
    return content_setting_bubble_model_delegate_.get();
  }
  BrowserLiveTabContext* live_tab_context() { return live_tab_context_.get(); }
  BrowserSyncedWindowDelegate* synced_window_delegate() {
    return synced_window_delegate_.get();
  }
  BrowserInstantController* instant_controller() {
    return instant_controller_.get();
  }
  const extensions::HostedAppBrowserController* hosted_app_controller() const {
    return hosted_app_controller_.get();
  }
  extensions::HostedAppBrowserController* hosted_app_controller() {
    return hosted_app_controller_.get();
  }

  SigninViewController* signin_view_controller() {
    return &signin_view_controller_;
  }

  // Will lazy create the bubble manager.
  ChromeBubbleManager* GetBubbleManager();

  // Get the FindBarController for this browser, creating it if it does not
  // yet exist.
  FindBarController* GetFindBarController();

  // Returns true if a FindBarController exists for this browser.
  bool HasFindBarController() const;

  // Returns the state of the bookmark bar.
  BookmarkBar::State bookmark_bar_state() const { return bookmark_bar_state_; }

  // State Storage and Retrieval for UI ///////////////////////////////////////

  // Gets the Favicon of the page in the selected tab.
  gfx::Image GetCurrentPageIcon() const;

  // Gets the title of the window based on the selected tab's title.
  // Disables additional formatting when |include_app_name| is false or if the
  // window is an app window.
  base::string16 GetWindowTitleForCurrentTab(bool include_app_name) const;

  // Gets the window title of the tab at |index|.
  // Disables additional formatting when |include_app_name| is false or if the
  // window is an app window.
  base::string16 GetWindowTitleForTab(bool include_app_name, int index) const;

  // Gets the window title from the provided WebContents.
  // Disables additional formatting when |include_app_name| is false or if the
  // window is an app window.
  base::string16 GetWindowTitleFromWebContents(
      bool include_app_name,
      content::WebContents* contents) const;

  // Prepares a title string for display (removes embedded newlines, etc).
  static void FormatTitleForDisplay(base::string16* title);

  // OnBeforeUnload handling //////////////////////////////////////////////////

  // Gives beforeunload handlers the chance to cancel the close. Returns whether
  // to proceed with the close. If called while the process begun by
  // TryToCloseWindow is in progress, returns false without taking action.
  bool ShouldCloseWindow();

  // Begins the process of confirming whether the associated browser can be
  // closed. If there are no tabs with beforeunload handlers it will immediately
  // return false. If |skip_beforeunload| is true, all beforeunload
  // handlers will be skipped and window closing will be confirmed without
  // showing the prompt, the function will return false as well.
  // Otherwise, it starts prompting the user, returns true and will call
  // |on_close_confirmed| with the result of the user's decision.
  // After calling this function, if the window will not be closed, call
  // ResetBeforeUnloadHandlers() to reset all beforeunload handlers; calling
  // this function multiple times without an intervening call to
  // ResetTryToCloseWindow() will run only the beforeunload handlers
  // registered since the previous call.
  // Note that if the browser window has been used before, users should always
  // have a chance to save their work before the window is closed without
  // triggering beforeunload event.
  bool TryToCloseWindow(bool skip_beforeunload,
                        const base::Callback<void(bool)>& on_close_confirmed);

  // Clears the results of any beforeunload confirmation dialogs triggered by a
  // TryToCloseWindow call.
  void ResetTryToCloseWindow();

  // Figure out if there are tabs that have beforeunload handlers.
  // It starts beforeunload/unload processing as a side-effect.
  bool TabsNeedBeforeUnloadFired();

  // Returns true if all tabs' beforeunload/unload events have fired.
  bool HasCompletedUnloadProcessing() const;

  bool IsAttemptingToCloseBrowser() const;

  // Invoked when the window containing us is closing. Performs the necessary
  // cleanup.
  void OnWindowClosing();

  // In-progress download termination handling /////////////////////////////////

  // Called when the user has decided whether to proceed or not with the browser
  // closure.  |cancel_downloads| is true if the downloads should be canceled
  // and the browser closed, false if the browser should stay open and the
  // downloads running.
  void InProgressDownloadResponse(bool cancel_downloads);

  // Indicates whether or not this browser window can be closed, or
  // would be blocked by in-progress downloads.
  // If executing downloads would be cancelled by this window close,
  // then |*num_downloads_blocking| is updated with how many downloads
  // would be canceled if the close continued.
  DownloadClosePreventionType OkToCloseWithInProgressDownloads(
      int* num_downloads_blocking) const;

  // External state change handling ////////////////////////////////////////////

  // WindowFullscreenStateWillChange is invoked at the beginning of a fullscreen
  // transition, and WindowFullscreenStateChanged is at the end.
  void WindowFullscreenStateWillChange();
  void WindowFullscreenStateChanged();
  // Only used on Mac. Called when the top ui style has been changed since this
  // may trigger bookmark bar state change.
  void FullscreenTopUIStateChanged();

  // Assorted browser commands ////////////////////////////////////////////////

  // NOTE: Within each of the following sections, the IDs are ordered roughly by
  // how they appear in the GUI/menus (left to right, top to bottom, etc.).

  // See the description of
  // FullscreenController::ToggleFullscreenModeWithExtension.
  void ToggleFullscreenModeWithExtension(const GURL& extension_url);

  // Returns true if the Browser supports the specified feature. The value of
  // this varies during the lifetime of the browser. For example, if the window
  // is fullscreen this may return a different value. If you only care about
  // whether or not it's possible for the browser to support a particular
  // feature use |CanSupportWindowFeature|.
  bool SupportsWindowFeature(WindowFeature feature) const;

  // Returns true if the Browser can support the specified feature. See comment
  // in |SupportsWindowFeature| for details on this.
  bool CanSupportWindowFeature(WindowFeature feature) const;

  // Show various bits of UI
  void OpenFile();

  void UpdateDownloadShelfVisibility(bool visible);

  /////////////////////////////////////////////////////////////////////////////

  // Called by Navigate() when a navigation has occurred in a tab in
  // this Browser. Updates the UI for the start of this navigation.
  void UpdateUIForNavigationInTab(content::WebContents* contents,
                                  ui::PageTransition transition,
                                  NavigateParams::WindowAction action,
                                  bool user_initiated);

  // Used to register a KeepAlive to affect the Chrome lifetime. The KeepAlive
  // is registered when the browser is added to the browser list, and unregisted
  // when it is removed from it.
  void RegisterKeepAlive();
  void UnregisterKeepAlive();

  // Interface implementations ////////////////////////////////////////////////

  // Overridden from content::PageNavigator:
  content::WebContents* OpenURL(const content::OpenURLParams& params) override;

  // Overridden from TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void TabPinnedStateChanged(TabStripModel* tab_strip_model,
                             content::WebContents* contents,
                             int index) override;
  void TabStripEmpty() override;

  // Overridden from content::WebContentsDelegate:
  void SetTopControlsShownRatio(content::WebContents* web_contents,
                                float ratio) override;
  int GetTopControlsHeight() const override;
  bool DoBrowserControlsShrinkRendererSize(
      const content::WebContents* contents) const override;
  void SetTopControlsGestureScrollInProgress(bool in_progress) override;
  bool CanOverscrollContent() const override;
  bool ShouldPreserveAbortedURLs(content::WebContents* source) override;
  void SetFocusToLocationBar(bool select_all) override;
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  bool PreHandleGestureEvent(content::WebContents* source,
                             const blink::WebGestureEvent& event) override;
  bool CanDragEnter(content::WebContents* source,
                    const content::DropData& data,
                    blink::WebDragOperationsMask operations_allowed) override;
  blink::WebSecurityStyle GetSecurityStyle(
      content::WebContents* web_contents,
      content::SecurityStyleExplanations* security_style_explanations) override;
  std::unique_ptr<content::BluetoothChooser> RunBluetoothChooser(
      content::RenderFrameHost* frame,
      const content::BluetoothChooser::EventHandler& event_handler) override;
  void RequestAppBannerFromDevTools(
      content::WebContents* web_contents) override;
  void PassiveInsecureContentFound(const GURL& resource_url) override;
  bool ShouldAllowRunningInsecureContent(content::WebContents* web_contents,
                                         bool allowed_per_prefs,
                                         const url::Origin& origin,
                                         const GURL& resource_url) override;
  void OnDidBlockFramebust(content::WebContents* web_contents,
                           const GURL& url) override;
  gfx::Size EnterPictureInPicture(const viz::SurfaceId&,
                                  const gfx::Size&) override;
  void ExitPictureInPicture() override;
  std::unique_ptr<content::WebContents> SwapWebContents(
      content::WebContents* old_contents,
      std::unique_ptr<content::WebContents> new_contents,
      bool did_start_load,
      bool did_finish_load) override;

  bool is_type_tabbed() const { return type_ == TYPE_TABBED; }
  bool is_type_popup() const { return type_ == TYPE_POPUP; }

  bool is_app() const;
  bool is_devtools() const;

  // True when the mouse cursor is locked.
  bool IsMouseLocked() const;

  // Called each time the browser window is shown.
  void OnWindowDidShow();

  ExclusiveAccessManager* exclusive_access_manager() {
    return exclusive_access_manager_.get();
  }

  extensions::BrowserExtensionWindowController* extension_window_controller()
      const {
    return extension_window_controller_.get();
  }

  bool ShouldRunUnloadListenerBeforeClosing(content::WebContents* web_contents);
  bool RunUnloadListenerBeforeClosing(content::WebContents* web_contents);

 private:
  friend class BrowserTest;
  friend class FullscreenControllerInteractiveTest;
  friend class FullscreenControllerTest;
  FRIEND_TEST_ALL_PREFIXES(AppModeTest, EnableAppModeTest);
  FRIEND_TEST_ALL_PREFIXES(BrowserCommandControllerTest,
                           IsReservedCommandOrKeyIsApp);
  FRIEND_TEST_ALL_PREFIXES(BrowserCommandControllerTest, AppFullScreen);
  FRIEND_TEST_ALL_PREFIXES(BrowserTest, OpenAppWindowLikeNtp);
  FRIEND_TEST_ALL_PREFIXES(BrowserTest, AppIdSwitch);
  FRIEND_TEST_ALL_PREFIXES(ExclusiveAccessBubbleWindowControllerTest,
                           DenyExitsFullscreen);
  FRIEND_TEST_ALL_PREFIXES(FullscreenControllerTest,
                           TabEntersPresentationModeFromWindowed);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorTest, OpenAppShortcutNoPref);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorTest,
                           OpenAppShortcutWindowPref);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorTest, OpenAppShortcutTabPref);

  class InterstitialObserver;

  // Used to describe why a tab is being detached. This is used by
  // TabDetachedAtImpl.
  enum DetachType {
    // Result of TabDetachedAt.
    DETACH_TYPE_DETACH,

    // Result of TabReplacedAt.
    DETACH_TYPE_REPLACE,

    // Result of the tab strip not having any significant tabs.
    DETACH_TYPE_EMPTY
  };

  // Describes where the bookmark bar state change originated from.
  enum BookmarkBarStateChangeReason {
    // From the constructor.
    BOOKMARK_BAR_STATE_CHANGE_INIT,

    // Change is the result of the active tab changing.
    BOOKMARK_BAR_STATE_CHANGE_TAB_SWITCH,

    // Change is the result of the bookmark bar pref changing.
    BOOKMARK_BAR_STATE_CHANGE_PREF_CHANGE,

    // Change is the result of a state change in the active tab.
    BOOKMARK_BAR_STATE_CHANGE_TAB_STATE,

    // Change is the result of window toggling in/out of fullscreen mode.
    BOOKMARK_BAR_STATE_CHANGE_TOGGLE_FULLSCREEN,

    // Change is the result of switching the option of showing toolbar in full
    // screen. Only used on Mac.
    BOOKMARK_BAR_STATE_CHANGE_TOOLBAR_OPTION_CHANGE,
  };

  // Overridden from content::WebContentsDelegate:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  void NavigationStateChanged(content::WebContents* source,
                              content::InvalidateTypes changed_flags) override;
  void VisibleSecurityStateChanged(content::WebContents* source) override;
  void AddNewContents(content::WebContents* source,
                      std::unique_ptr<content::WebContents> new_contents,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_rect,
                      bool user_gesture,
                      bool* was_blocked) override;
  void ActivateContents(content::WebContents* contents) override;
  void LoadingStateChanged(content::WebContents* source,
                           bool to_different_document) override;
  void CloseContents(content::WebContents* source) override;
  void SetContentsBounds(content::WebContents* source,
                         const gfx::Rect& bounds) override;
  void UpdateTargetURL(content::WebContents* source, const GURL& url) override;
  void ContentsMouseEvent(content::WebContents* source,
                          bool motion,
                          bool exited) override;
  void ContentsZoomChange(bool zoom_in) override;
  bool TakeFocus(content::WebContents* source, bool reverse) override;
  void BeforeUnloadFired(content::WebContents* source,
                         bool proceed,
                         bool* proceed_to_fire_unload) override;
  bool ShouldFocusLocationBarByDefault(content::WebContents* source) override;
  void ShowRepostFormWarningDialog(content::WebContents* source) override;
  bool ShouldCreateWebContents(
      content::WebContents* web_contents,
      content::RenderFrameHost* opener,
      content::SiteInstance* source_site_instance,
      int32_t route_id,
      int32_t main_frame_route_id,
      int32_t main_frame_widget_route_id,
      content::mojom::WindowContainerType window_container_type,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url,
      const std::string& partition_id,
      content::SessionStorageNamespace* session_storage_namespace) override;
  void WebContentsCreated(content::WebContents* source_contents,
                          int opener_render_process_id,
                          int opener_render_frame_id,
                          const std::string& frame_name,
                          const GURL& target_url,
                          content::WebContents* new_contents) override;
  void PortalWebContentsCreated(
      content::WebContents* portal_web_contents) override;
  void RendererUnresponsive(
      content::WebContents* source,
      content::RenderWidgetHost* render_widget_host,
      base::RepeatingClosure hang_monitor_restarter) override;
  void RendererResponsive(
      content::WebContents* source,
      content::RenderWidgetHost* render_widget_host) override;
  void DidNavigateMainFramePostCommit(
      content::WebContents* web_contents) override;
  content::JavaScriptDialogManager* GetJavaScriptDialogManager(
      content::WebContents* source) override;
  content::ColorChooser* OpenColorChooser(
      content::WebContents* web_contents,
      SkColor color,
      const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions)
      override;
  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      std::unique_ptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override;
  void EnumerateDirectory(content::WebContents* web_contents,
                          std::unique_ptr<content::FileSelectListener> listener,
                          const base::FilePath& path) override;
  bool EmbedsFullscreenWidget() const override;
  void EnterFullscreenModeForTab(
      content::WebContents* web_contents,
      const GURL& origin,
      const blink::WebFullscreenOptions& options) override;
  void ExitFullscreenModeForTab(content::WebContents* web_contents) override;
  bool IsFullscreenForTabOrPending(
      const content::WebContents* web_contents) const override;
  blink::WebDisplayMode GetDisplayMode(
      const content::WebContents* web_contents) const override;
  void RegisterProtocolHandler(content::WebContents* web_contents,
                               const std::string& protocol,
                               const GURL& url,
                               bool user_gesture) override;
  void UnregisterProtocolHandler(content::WebContents* web_contents,
                                 const std::string& protocol,
                                 const GURL& url,
                                 bool user_gesture) override;
  void FindReply(content::WebContents* web_contents,
                 int request_id,
                 int number_of_matches,
                 const gfx::Rect& selection_rect,
                 int active_match_ordinal,
                 bool final_update) override;
  void RequestToLockMouse(content::WebContents* web_contents,
                          bool user_gesture,
                          bool last_unlocked_by_target) override;
  void LostMouseLock() override;
  void RequestKeyboardLock(content::WebContents* web_contents,
                           bool esc_key_locked) override;
  void CancelKeyboardLockRequest(content::WebContents* web_contents) override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const GURL& security_origin,
                                  content::MediaStreamType type) override;
  std::string GetDefaultMediaDeviceID(content::WebContents* web_contents,
                                      content::MediaStreamType type) override;
  bool RequestPpapiBrokerPermission(
      content::WebContents* web_contents,
      const GURL& url,
      const base::FilePath& plugin_path,
      const base::Callback<void(bool)>& callback) override;
  gfx::Size GetSizeForNewRenderView(
      content::WebContents* web_contents) const override;

#if BUILDFLAG(ENABLE_PRINTING)
  void PrintCrossProcessSubframe(
      content::WebContents* web_contents,
      const gfx::Rect& rect,
      int document_cookie,
      content::RenderFrameHost* subframe_host) const override;
#endif

  // Overridden from CoreTabHelperDelegate:
  bool CanReloadContents(content::WebContents* web_contents) const override;
  bool CanSaveContents(content::WebContents* web_contents) const override;

  // Overridden from WebContentsModalDialogManagerDelegate:
  void SetWebContentsBlocked(content::WebContents* web_contents,
                             bool blocked) override;
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;

  // Overridden from BookmarkTabHelperObserver:
  void URLStarredChanged(content::WebContents* web_contents,
                         bool starred) override;

#if !defined(OS_ANDROID)
  // Overridden from ZoomObserver:
  void OnZoomChanged(
      const zoom::ZoomController::ZoomChangedEventData& data) override;
#endif  // !defined(OS_ANDROID)

  // Overridden from SelectFileDialog::Listener:
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;
  void FileSelectedWithExtraInfo(const ui::SelectedFileInfo& file_info,
                                 int index,
                                 void* params) override;

  // Overridden from content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Overridden from extensions::ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;
#endif

  // Overridden from translate::ContentTranslateDriver::Observer:
  void OnIsPageTranslatedChanged(content::WebContents* source) override;
  void OnTranslateEnabledChanged(content::WebContents* source) override;

  // Command and state updating ///////////////////////////////////////////////

  // Handle changes to tab strip model.
  void OnTabInsertedAt(content::WebContents* contents, int index);
  void OnTabClosing(content::WebContents* contents);
  void OnTabDetached(content::WebContents* contents, bool was_active);
  void OnTabDeactivated(content::WebContents* contents);
  void OnActiveTabChanged(content::WebContents* old_contents,
                          content::WebContents* new_contents,
                          int index,
                          int reason);
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

  // Does one or both of the following for each bit in |changed_flags|:
  // . If the update should be processed immediately, it is.
  // . If the update should processed asynchronously (to avoid lots of ui
  //   updates), then scheduled_updates_ is updated for the |source| and update
  //   pair and a task is scheduled (assuming it isn't running already)
  //   that invokes ProcessPendingUIUpdates.
  void ScheduleUIUpdate(content::WebContents* source,
                        unsigned changed_flags);

  // Processes all pending updates to the UI that have been scheduled by
  // ScheduleUIUpdate in scheduled_updates_.
  void ProcessPendingUIUpdates();

  // Removes all entries from scheduled_updates_ whose source is contents.
  void RemoveScheduledUpdatesFor(content::WebContents* contents);

  // Getters for UI ///////////////////////////////////////////////////////////

  // TODO(beng): remove, and provide AutomationProvider a better way to access
  //             the LocationBarView's edit.
  friend class AutomationProvider;
  friend class BrowserProxy;

  // Returns the StatusBubble from the current toolbar. It is possible for
  // this to return NULL if called before the toolbar has initialized.
  // TODO(beng): remove this.
  StatusBubble* GetStatusBubble();

  // Session restore functions ////////////////////////////////////////////////

  // Notifies the history database of the index for all tabs whose index is
  // >= index.
  void SyncHistoryWithTabs(int index);

  // In-progress download termination handling /////////////////////////////////

  // Called when the window is closing to check if potential in-progress
  // downloads should prevent it from closing.
  // Returns true if the window can close, false otherwise.
  bool CanCloseWithInProgressDownloads();

  // Assorted utility functions ///////////////////////////////////////////////

  // Sets the specified browser as the delegate of the WebContents and all the
  // associated tab helpers that are needed. If |set_delegate| is true, this
  // browser object is set as a delegate for |web_contents| components, else
  // is is removed as a delegate.
  void SetAsDelegate(content::WebContents* web_contents, bool set_delegate);

  // Shows the Find Bar, optionally selecting the next entry that matches the
  // existing search string for that Tab. |forward_direction| controls the
  // search direction.
  void FindInPage(bool find_next, bool forward_direction);

  // Closes the frame.
  // TODO(beng): figure out if we need this now that the frame itself closes
  //             after a return to the message loop.
  void CloseFrame();

  void TabDetachedAtImpl(content::WebContents* contents,
                         bool was_active,
                         DetachType type);

  // Updates the loading state for the window and tabstrip.
  void UpdateWindowForLoadingStateChanged(content::WebContents* source,
                                          bool to_different_document);

  // Shared code between Reload() and ReloadBypassingCache().
  void ReloadInternal(WindowOpenDisposition disposition, bool bypass_cache);

  // Returns true if the Browser window supports a location bar. Having support
  // for the location bar does not mean it will be visible.
  bool SupportsLocationBar() const;

  // Implementation of SupportsWindowFeature and CanSupportWindowFeature. If
  // |check_fullscreen| is true, the set of features reflect the actual state of
  // the browser, otherwise the set of features reflect the possible state of
  // the browser.
  bool SupportsWindowFeatureImpl(WindowFeature feature,
                                 bool check_fullscreen) const;

  // Resets |bookmark_bar_state_| based on the active tab. Notifies the
  // BrowserWindow if necessary.
  void UpdateBookmarkBarState(BookmarkBarStateChangeReason reason);

  bool ShouldHideUIForFullscreen() const;

  // Indicates if we have called BrowserList::NotifyBrowserCloseStarted for the
  // browser.
  bool IsBrowserClosing() const;

  // Returns true if we can start the shutdown sequence for the browser, i.e.
  // the last browser window is being closed.
  bool ShouldStartShutdown() const;

  // Creates a BackgroundContents if appropriate; return true if one was
  // created.
  bool MaybeCreateBackgroundContents(
      content::SiteInstance* source_site_instance,
      content::RenderFrameHost* opener,
      const GURL& opener_url,
      int32_t route_id,
      int32_t main_frame_route_id,
      int32_t main_frame_widget_route_id,
      const std::string& frame_name,
      const GURL& target_url,
      const std::string& partition_id,
      content::SessionStorageNamespace* session_storage_namespace);

  // Data members /////////////////////////////////////////////////////////////

  std::vector<InterstitialObserver*> interstitial_observers_;

  content::NotificationRegistrar registrar_;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      extension_registry_observer_;
#endif

  PrefChangeRegistrar profile_pref_registrar_;

  // This Browser's type.
  const Type type_;

  // This Browser's profile.
  Profile* const profile_;

  // This Browser's window.
  BrowserWindow* window_;

  std::unique_ptr<TabStripModelDelegate> tab_strip_model_delegate_;
  std::unique_ptr<TabStripModel> tab_strip_model_;

  // The application name that is also the name of the window to the shell.
  // This name should be set when:
  // 1) we launch an application via an application shortcut or extension API.
  // 2) we launch an undocked devtool window.
  std::string app_name_;

  // True if the source is trusted (i.e. we do not need to show the URL in a
  // a popup window). Also used to determine which app windows to save and
  // restore on Chrome OS.
  bool is_trusted_source_;

  // Unique identifier of this browser for session restore. This id is only
  // unique within the current session, and is not guaranteed to be unique
  // across sessions.
  const SessionID session_id_;

  // The model for the toolbar view.
  std::unique_ptr<ToolbarModel> toolbar_model_;

  // UI update coalescing and handling ////////////////////////////////////////

  typedef std::map<const content::WebContents*, int> UpdateMap;

  // Maps from WebContents to pending UI updates that need to be processed.
  // We don't update things like the URL or tab title right away to avoid
  // flickering and extra painting.
  // See ScheduleUIUpdate and ProcessPendingUIUpdates.
  UpdateMap scheduled_updates_;

  // In-progress download termination handling /////////////////////////////////

  enum CancelDownloadConfirmationState {
    NOT_PROMPTED,          // We have not asked the user.
    WAITING_FOR_RESPONSE,  // We have asked the user and have not received a
                           // reponse yet.
    RESPONSE_RECEIVED      // The user was prompted and made a decision already.
  };

  // State used to figure-out whether we should prompt the user for confirmation
  // when the browser is closed with in-progress downloads.
  CancelDownloadConfirmationState cancel_download_confirmation_state_;

  /////////////////////////////////////////////////////////////////////////////

  // Override values for the bounds of the window and its maximized or minimized
  // state.
  // These are supplied by callers that don't want to use the default values.
  // The default values are typically loaded from local state (last session),
  // obtained from the last window of the same type, or obtained from the
  // shell shortcut's startup info.
  gfx::Rect override_bounds_;
  ui::WindowShowState initial_show_state_;
  const std::string initial_workspace_;

  // Tracks when this browser is being created by session restore.
  bool is_session_restore_;

  std::unique_ptr<UnloadController> unload_controller_;
  std::unique_ptr<FastUnloadController> fast_unload_controller_;

  std::unique_ptr<ChromeBubbleManager> bubble_manager_;

  // The Find Bar. This may be NULL if there is no Find Bar, and if it is
  // non-NULL, it may or may not be visible.
  std::unique_ptr<FindBarController> find_bar_controller_;

  // Dialog box used for opening and saving files.
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  // Helper which implements the ContentSettingBubbleModel interface.
  std::unique_ptr<BrowserContentSettingBubbleModelDelegate>
      content_setting_bubble_model_delegate_;

  // Helper which implements the ToolbarModelDelegate interface.
  std::unique_ptr<BrowserToolbarModelDelegate> toolbar_model_delegate_;

  // Helper which implements the LiveTabContext interface.
  std::unique_ptr<BrowserLiveTabContext> live_tab_context_;

  // Helper which implements the SyncedWindowDelegate interface.
  std::unique_ptr<BrowserSyncedWindowDelegate> synced_window_delegate_;

  std::unique_ptr<BrowserInstantController> instant_controller_;

  // Helper which handles bookmark app specific browser configuration.
  // This must be initialized before |command_controller_| to ensure the correct
  // set of commands are enabled.
  std::unique_ptr<extensions::HostedAppBrowserController>
      hosted_app_controller_;

  BookmarkBar::State bookmark_bar_state_;

  std::unique_ptr<ExclusiveAccessManager> exclusive_access_manager_;

  std::unique_ptr<extensions::BrowserExtensionWindowController>
      extension_window_controller_;

  std::unique_ptr<chrome::BrowserCommandController> command_controller_;

  // True if the browser window has been shown at least once.
  bool window_has_shown_;

  SigninViewController signin_view_controller_;

  std::unique_ptr<ScopedKeepAlive> keep_alive_;

  // The following factory is used for chrome update coalescing.
  base::WeakPtrFactory<Browser> chrome_updater_factory_;

  // The following factory is used to close the frame at a later time.
  base::WeakPtrFactory<Browser> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Browser);
};

#endif  // CHROME_BROWSER_UI_BROWSER_H_
