// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_H_
#define CHROME_BROWSER_UI_BROWSER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/scoped_observation_traits.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/tab_contents/web_contents_collection.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper_observer.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/chrome_web_modal_dialog_manager_delegate.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/unload_controller.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "components/paint_preview/buildflags/buildflags.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sessions/core/session_id.h"
#include "components/translate/content/browser/content_translate_driver.h"
#include "components/zoom/zoom_observer.h"
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
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/shell_dialogs/select_file_dialog.h"

#if BUILDFLAG(IS_ANDROID)
#error This file should only be included on desktop.
#endif

class BackgroundContents;
class BreadcrumbManagerBrowserAgent;
class BrowserActions;
class BrowserContentSettingBubbleModelDelegate;
class BrowserInstantController;
class BrowserSyncedWindowDelegate;
class BrowserLocationBarModelDelegate;
class BrowserLiveTabContext;
class BrowserView;
class BrowserWindow;
class BrowserWindowFeatures;
class ExclusiveAccessManager;
class FindBarController;
class LocationBarModel;
class OverscrollPrefManager;
class Profile;
class ScopedKeepAlive;
class ScopedProfileKeepAlive;
class StatusBubble;
class TabStripModelDelegate;
class TabMenuModelDelegate;

namespace tabs {
class TabModel;
}

namespace tab_groups {
class DeletionDialogController;
}

namespace blink {
enum class ProtocolHandlerSecurityLevel;
}

namespace chrome {
class BrowserCommandController;
}

namespace content {
class NavigationHandle;
class SessionStorageNamespace;
}

namespace extensions {
class BrowserExtensionWindowController;
class ExtensionBrowserWindowHelper;
}  // namespace extensions

namespace gfx {
class Image;
}

namespace ui {
struct SelectedFileInfo;
}

namespace web_app {
class AppBrowserController;
}

namespace web_modal {
class WebContentsModalDialogHost;
}

namespace views {
class View;
}

// This enum is not a member of `Browser` so that it can be forward
// declared in `unload_controller.h` to avoid circular includes.
enum class BrowserClosingStatus {
  kPermitted,
  kDeniedByUser,
  kDeniedByPolicy,
  kDeniedUnloadHandlersNeedTime
};

// An instance of this class represents a single browser window on Desktop. All
// features that are scoped to a browser window should have lifetime semantics
// scoped to an instance of this class, usually via direct or indirect ownership
// of a std::unique_ptr. See BrowserWindowFeatures and TabFeatures.
class Browser : public TabStripModelObserver,
                public WebContentsCollection::Observer,
                public content::WebContentsDelegate,
                public ChromeWebModalDialogManagerDelegate,
                public base::SupportsUserData,
                public BookmarkTabHelperObserver,
                public zoom::ZoomObserver,
                public ThemeServiceObserver,
                public translate::ContentTranslateDriver::TranslationObserver,
                public ui::SelectFileDialog::Listener,
                public BrowserWindowInterface {
 public:
  // Possible elements of the Browser window.
  enum WindowFeature {
    FEATURE_NONE = 0,
    FEATURE_TITLEBAR = 1 << 0,
    FEATURE_TABSTRIP = 1 << 1,
    FEATURE_TOOLBAR = 1 << 2,
    FEATURE_LOCATIONBAR = 1 << 3,
    FEATURE_BOOKMARKBAR = 1 << 4,
    // TODO(crbug.com/40639933): Add FEATURE_PAGECONTROLS to describe the
    // presence of per-page controls such as Content Settings Icons, which
    // should be decoupled from FEATURE_LOCATIONBAR as they have independent
    // presence in Web App browsers.
  };

  // The context for a download blocked notification from
  // OkToCloseWithInProgressDownloads.
  enum class DownloadCloseType {
    // Browser close is not blocked by download state.
    kOk,

    // The browser is shutting down and there are active downloads
    // that would be cancelled.
    kBrowserShutdown,

    // There are active downloads associated with this incognito profile
    // that would be canceled.
    kLastWindowInIncognitoProfile,

    // There are active downloads associated with this guest session
    // that would be canceled.
    kLastWindowInGuestSession,
  };

  // Represents the result of the user being warned before closing the browser.
  // See WarnBeforeClosingCallback and WarnBeforeClosing() below.
  enum class WarnBeforeClosingResult { kOkToClose, kDoNotClose };

  // Represents the result of a browser creation request.
  enum class CreationStatus {
    kOk,
    kErrorNoProcess,
    kErrorProfileUnsuitable,
    kErrorLoadingKiosk,
  };

  // Represents the source of a browser creation request.
  enum class CreationSource {
    kUnknown,
    kSessionRestore,
    kStartupCreator,
    kLastAndUrlsStartupPref,
    kDeskTemplate,
  };

  // Represents the reasons for force showing bookmark bar.
  enum ForceShowBookmarkBarFlag {
    kNone = 0,
    kTabGroupsTutorialActive = 1 << 0,
    kTabGroupSaved = 1 << 1,
  };

  // Represents whether a value was known to be explicitly specified.
  enum class ValueSpecified { kUnknown, kSpecified, kUnspecified };

  // The default value for a browser's `restore_id` param.
  static constexpr int kDefaultRestoreId = 0;

  // Callback that receives the result of a user being warned about closing a
  // browser window (for example, if closing the window would interrupt a
  // download). The parameter is whether the close should proceed.
  using WarnBeforeClosingCallback =
      base::OnceCallback<void(WarnBeforeClosingResult)>;

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

    // Specifies the browser `is_trusted_source_` value.
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

    // Whether to enable the tab group feature in the tab strip.
    bool are_tab_groups_enabled = true;

    ui::mojom::WindowShowState initial_show_state =
        ui::mojom::WindowShowState::kDefault;

    CreationSource creation_source = CreationSource::kUnknown;

#if BUILDFLAG(IS_CHROMEOS)
    // The id from the restore data to restore the browser window.
    int32_t restore_id = kDefaultRestoreId;

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

    // Whether this browser was created by a user gesture. We track this
    // specifically for the multi-user case in chromeos where we can place
    // windows generated by user gestures differently from ones
    // programmatically created.
    bool user_gesture;

    // Whether this browser was created specifically for dragged tab(s).
    bool in_tab_dragging = false;

    // Supply a custom BrowserWindow implementation, to be used instead of the
    // default. Intended for testing.
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

   private:
    friend class Browser;
    friend class WindowSizerChromeOSTest;

    static CreateParams CreateForAppBase(bool is_popup,
                                         const std::string& app_name,
                                         bool trusted_source,
                                         const gfx::Rect& window_bounds,
                                         Profile* profile,
                                         bool user_gesture);

    // The application name that is also the name of the window to the shell.
    // Do not set this value directly, use CreateForApp/CreateForAppPopup.
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

  // Creates a browser instance with the provided params.
  // Crashes if the requested browser creation is not allowed.
  // For example, browser creation will not be allowed for profiles that
  // disallow browsing (like sign-in profile on Chrome OS).
  //
  // Unless |params->window| is specified, a new BrowserWindow will be created
  // for the browser - the created BrowserWindow will take the ownership of the
  // created Browser instance.
  //
  // If |params.window| or |params.skip_window_init_for_testing| are set, the
  // caller is expected to take the ownership of the created Browser instance.
  static Browser* Create(const CreateParams& params);

  // Returns whether a browser window can be created for the specified profile.
  static CreationStatus GetCreationStatusForProfile(Profile* profile);

  Browser(const Browser&) = delete;
  Browser& operator=(const Browser&) = delete;

  ~Browser() override;

  // Set overrides for the initial window bounds and maximized state.
  void set_override_bounds(const gfx::Rect& bounds) {
    override_bounds_ = bounds;
  }
  ui::mojom::WindowShowState initial_show_state() const {
    return initial_show_state_;
  }
  void set_initial_show_state(ui::mojom::WindowShowState initial_show_state) {
    initial_show_state_ = initial_show_state;
  }
  // Return true if the initial window bounds have been overridden.
  bool bounds_overridden() const { return !override_bounds_.IsEmpty(); }
  // Set indicator that this browser is being created via session restore.
  // This is used on the Mac (only) to determine animation style when the
  // browser window is shown.
  void set_is_session_restore(bool is_session_restore) {
    creation_source_ = CreationSource::kSessionRestore;
  }
  bool is_session_restore() const {
    return creation_source_ == CreationSource::kSessionRestore;
  }

  // Tells the browser whether it should skip showing any dialogs that ask the
  // user to confirm that they want to close the browser when it is being
  // closed.
  void set_force_skip_warning_user_on_close(
      bool force_skip_warning_user_on_close) {
    force_skip_warning_user_on_close_ = force_skip_warning_user_on_close;
  }

  // Sets whether the UI should be immediately updated when scheduled on a
  // test.
  void set_update_ui_immediately_for_testing() {
    update_ui_immediately_for_testing_ = true;
  }

  // Accessors ////////////////////////////////////////////////////////////////

  const CreateParams& create_params() const { return create_params_; }
  Type type() const { return type_; }
  const std::string& app_name() const { return app_name_; }
  const std::string& user_title() const { return user_title_; }
  bool is_trusted_source() const { return is_trusted_source_; }
  Profile* profile() const { return profile_; }
  gfx::Rect override_bounds() const { return override_bounds_; }
  const std::string& initial_workspace() const { return initial_workspace_; }
  bool initial_visible_on_all_workspaces_state() const {
    return initial_visible_on_all_workspaces_state_;
  }
  CreationSource creation_source() const { return creation_source_; }

  // |window()| will return NULL if called before |CreateBrowserWindow()|
  // is done.
  BrowserWindow* window() const { return window_; }

  // In production code, each instance of Browser will always instantiate an
  // instance of BrowserView in the constructor. Some tests instantiate a
  // Browser without a BrowserView: this is an anti-pattern and should be
  // avoided.
  BrowserView& GetBrowserView();
  LocationBarModel* location_bar_model() { return location_bar_model_.get(); }
  const LocationBarModel* location_bar_model() const {
    return location_bar_model_.get();
  }
#if defined(UNIT_TEST)
  void swap_location_bar_models(
      std::unique_ptr<LocationBarModel>* location_bar_model) {
    location_bar_model->swap(location_bar_model_);
  }
#endif

  // Never nullptr.
  //
  // When the last tab is removed, the browser attempts to close, see
  // TabStripEmpty().
  // TODO(https://crbug.com/331031753): Several existing Browser::Types never
  // show a tab strip, yet are forced to work with the tab strip API to deal
  // with the previous condition. This creates confusing control flow both for
  // the tab strip and this class. One or both of the following should happen:
  //  (1) tab_strip_model_ should become an optional member.
  //  (2) Variations of Browser::Type that never show a tab strip should not use
  //      this class.
  TabStripModel* tab_strip_model() const { return tab_strip_model_.get(); }

  // Never nullptr.
  TabStripModelDelegate* tab_strip_model_delegate() const {
    return tab_strip_model_delegate_.get();
  }

  // Never nullptr.
  TabMenuModelDelegate* tab_menu_model_delegate() const {
    return tab_menu_model_delegate_.get();
  }

  BrowserActions* browser_actions() const { return browser_actions_.get(); }

  chrome::BrowserCommandController* command_controller() {
    return command_controller_.get();
  }

  tab_groups::DeletionDialogController* tab_group_deletion_dialog_controller()
      const {
    return tab_group_deletion_dialog_controller_.get();
  }

  SessionID session_id() const { return session_id_; }
  bool omit_from_session_restore() const { return omit_from_session_restore_; }
  bool should_trigger_session_restore() const {
    return should_trigger_session_restore_;
  }
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
  const web_app::AppBrowserController* app_controller() const {
    return app_controller_.get();
  }
  web_app::AppBrowserController* app_controller() {
    return app_controller_.get();
  }
  SigninViewController* signin_view_controller() {
    return &signin_view_controller_;
  }
  BrowserWindowFeatures* browser_window_features() const {
    return features_.get();
  }

  base::WeakPtr<Browser> AsWeakPtr();
  base::WeakPtr<const Browser> AsWeakPtr() const;

  // Get the FindBarController for this browser, creating it if it does not
  // yet exist.
  FindBarController* GetFindBarController();

  // Returns true if a FindBarController exists for this browser.
  bool HasFindBarController() const;

  // Returns the state of the bookmark bar.
  BookmarkBar::State bookmark_bar_state() const { return bookmark_bar_state_; }

  // State Storage and Retrieval for UI ///////////////////////////////////////

  GURL GetNewTabURL() const;

  // Gets the Favicon of the page in the selected tab.
  gfx::Image GetCurrentPageIcon() const;

  // Gets the title of the window based on the selected tab's title.
  // Disables additional formatting when |include_app_name| is false or if the
  // window is an app window.
  std::u16string GetWindowTitleForCurrentTab(bool include_app_name) const;

  // Gets the window title of the tab at |index|.
  std::u16string GetWindowTitleForTab(int index) const;

  // Gets the window title for the current tab, to display in a menu. If the
  // title is too long to fit in the required space, the tab title will be
  // elided. The result title might still be a larger width than specified, as
  // at least a few characters of the title are always shown.
  std::u16string GetWindowTitleForMaxWidth(int max_width) const;

  // Gets the window title from the provided WebContents.
  // Disables additional formatting when |include_app_name| is false or if the
  // window is an app window.
  std::u16string GetWindowTitleFromWebContents(
      bool include_app_name,
      content::WebContents* contents) const;

  // Prepares a title string for display (removes embedded newlines, etc).
  static std::u16string FormatTitleForDisplay(std::u16string title);

  // OnBeforeUnload handling //////////////////////////////////////////////////

  // Displays any necessary warnings to the user on taking an action that might
  // close the browser (for example, warning if there are downloads in progress
  // that would be interrupted).
  //
  // Distinct from HandleBeforeClose() (which calls this method) because
  // this method does not consider beforeunload handler, only things the user
  // should be prompted about.
  //
  // If no warnings are needed, the method returns kOkToClose, indicating that
  // the close can proceed immediately, and the callback is not called. If the
  // method returns kDoNotClose, closing should be handled by |warn_callback|
  // (and then only if the callback receives the kOkToClose value).
  WarnBeforeClosingResult MaybeWarnBeforeClosing(
      WarnBeforeClosingCallback warn_callback);

  // Gives beforeunload handlers the chance to cancel the close. Returns the
  // closing status. Closing can be denied due to different reasons.
  // This function checks if unload handlers are still executing. It further
  // may ask the user for permission to close the browser (e.g. if downloads
  // are ongoing).
  // If this function is called
  // * but the user denied closure after being prompted, it returns
  // `BrowserClosingStatus::kDeniedByUser`.
  // * but the closure is not permitted by policy, it returns
  // `BrowserClosingStatus::kDeniedByPolicy`.
  // * while the process begun by TryToCloseWindow is in progress, it returns
  // `BrowserClosingStatus::kDeniedUnloadHandlersNeedTime`.
  //
  // If you don't care about beforeunload handlers and just want to prompt the
  // user that they might lose an in-progress operation, call
  // MaybeWarnBeforeClosing() instead (HandleBeforeClose() also calls this
  // method).
  BrowserClosingStatus HandleBeforeClose();

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
  bool TryToCloseWindow(
      bool skip_beforeunload,
      const base::RepeatingCallback<void(bool)>& on_close_confirmed);

  // Clears the results of any beforeunload confirmation dialogs triggered by a
  // TryToCloseWindow call.
  void ResetTryToCloseWindow();

  // Figure out if there are tabs that have beforeunload handlers.
  bool TabsNeedBeforeUnloadFired() const;

  // Browser closing consists of the following phases:
  //
  // 1. If the browser has WebContents with before unload handlers, then the
  //    before unload handlers are processed (this is asynchronous). During this
  //    phase IsAttemptingToCloseBrowser() returns true. When processing
  //    completes, the WebContents is removed. Once all WebContents are removed,
  //    the next phase happens. Note that this phase may be aborted.
  // 2. The Browser window is hidden, and a task is posted that results in
  //    deleting the Browser (Views is responsible for posting the task). This
  //    phase can not be stopped. During this phase is_delete_scheduled()
  //    returns true. IsBrowserClosing() is nearly identical to
  //    is_delete_scheduled(), it's set just before removing the tabs.
  //
  // Note that there are other cases that may delay closing, such as downloads,
  // but that is done before any of these steps.
  // TODO(crbug.com/40064092): See about unifying IsBrowserClosing() and
  // is_delete_scheduled().
  bool IsAttemptingToCloseBrowser() const override;
  bool IsBrowserClosing() const;
  bool is_delete_scheduled() const { return is_delete_scheduled_; }

  // Invoked when the window containing us is closing. Performs the necessary
  // cleanup.
  void OnWindowClosing();

  // In-progress download termination handling /////////////////////////////////

  // Indicates whether or not this browser window can be closed, or
  // would be blocked by in-progress downloads.
  // If executing downloads would be cancelled by this window close,
  // then |*num_downloads_blocking| is updated with how many downloads
  // would be canceled if the close continued.
  DownloadCloseType OkToCloseWithInProgressDownloads(
      int* num_downloads_blocking) const;

  // External state change handling ////////////////////////////////////////////

  // Invoked at the end of a fullscreen transition.
  void WindowFullscreenStateChanged();

  // Only used on Mac and Lacros. Called when the top ui style has been changed
  // since this may trigger bookmark bar state change.
  void FullscreenTopUIStateChanged();

  void OnFindBarVisibilityChanged();

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

  // Whether the specified WebContents can be saved.
  // Saving can be disabled e.g. for the DevTools window.
  bool CanSaveContents(content::WebContents* web_contents) const;

  // Returns whether favicon should be shown.
  bool ShouldDisplayFavicon(content::WebContents* web_contents) const;

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

  // Overridden from TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void OnTabGroupChanged(const TabGroupChange& change) override;
  void TabPinnedStateChanged(TabStripModel* tab_strip_model,
                             content::WebContents* contents,
                             int index) override;
  void TabGroupedStateChanged(std::optional<tab_groups::TabGroupId> group,
                              tabs::TabModel* tab,
                              int index) override;
  void TabStripEmpty() override;

  // Overridden from content::WebContentsDelegate:
  void ActivateContents(content::WebContents* contents) override;
  void SetTopControlsShownRatio(content::WebContents* web_contents,
                                float ratio) override;
  int GetTopControlsHeight() override;
  bool DoBrowserControlsShrinkRendererSize(
      content::WebContents* contents) override;
  int GetVirtualKeyboardHeight(content::WebContents* contents) override;
  void SetTopControlsGestureScrollInProgress(bool in_progress) override;
  bool CanOverscrollContent() override;
  bool ShouldPreserveAbortedURLs(content::WebContents* source) override;
  void SetFocusToLocationBar() override;
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const input::NativeWebKeyboardEvent& event) override;
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;
  bool PreHandleGestureEvent(content::WebContents* source,
                             const blink::WebGestureEvent& event) override;
  bool CanDragEnter(content::WebContents* source,
                    const content::DropData& data,
                    blink::DragOperationsMask operations_allowed) override;
  void CreateSmsPrompt(content::RenderFrameHost*,
                       const std::vector<url::Origin>&,
                       const std::string& one_time_code,
                       base::OnceClosure on_confirm,
                       base::OnceClosure on_cancel) override;
  bool ShouldAllowRunningInsecureContent(content::WebContents* web_contents,
                                         bool allowed_per_prefs,
                                         const url::Origin& origin,
                                         const GURL& resource_url) override;
  void OnDidBlockNavigation(
      content::WebContents* web_contents,
      const GURL& blocked_url,
      const GURL& initiator_url,
      blink::mojom::NavigationBlockedReason reason) override;
  content::PictureInPictureResult EnterPictureInPicture(
      content::WebContents* web_contents) override;
  void ExitPictureInPicture() override;
  bool IsBackForwardCacheSupported(content::WebContents& web_contents) override;
  content::PreloadingEligibility IsPrerender2Supported(
      content::WebContents& web_contents) override;
  bool ShouldShowStaleContentOnEviction(content::WebContents* source) override;
  void MediaWatchTimeChanged(
      const content::MediaPlayerWatchTime& watch_time) override;
  std::unique_ptr<content::EyeDropper> OpenEyeDropper(
      content::RenderFrameHost* frame,
      content::EyeDropperListener* listener) override;
  void InitiatePreview(content::WebContents& web_contents,
                       const GURL& url) override;
  bool ShouldUseInstancedSystemMediaControls() const override;
  void DraggableRegionsChanged(
      const std::vector<blink::mojom::DraggableRegionPtr>& regions,
      content::WebContents* contents) override;

  bool is_type_normal() const { return type_ == TYPE_NORMAL; }
  bool is_type_popup() const { return type_ == TYPE_POPUP; }
  bool is_type_app() const { return type_ == TYPE_APP; }
  bool is_type_app_popup() const { return type_ == TYPE_APP_POPUP; }
  bool is_type_devtools() const { return type_ == TYPE_DEVTOOLS; }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool is_type_custom_tab() const { return type_ == TYPE_CUSTOM_TAB; }
#endif
  bool is_type_picture_in_picture() const {
    return type_ == TYPE_PICTURE_IN_PICTURE;
  }

  // True when the mouse cursor is locked.
  bool IsPointerLocked() const;

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

  // Sets the browser's user title. Setting it to an empty string clears it.
  void SetWindowUserTitle(const std::string& user_title);

  // Gets the browser for opening chrome:// pages. This will return the opener
  // browser if the current browser is in picture-in-picture mode, otherwise
  // returns the current browser.
  Browser* GetBrowserForOpeningWebUi();

  StatusBubble* GetStatusBubbleForTesting();

  // Sets or clears the flags to force showing bookmark bar.
  void SetForceShowBookmarkBarFlag(ForceShowBookmarkBarFlag flag);
  void ClearForceShowBookmarkBarFlag(ForceShowBookmarkBarFlag flag);

  // BrowserWindowInterface overrides:
  views::WebView* GetWebView() override;
  Profile* GetProfile() override;
  void OpenGURL(const GURL& gurl, WindowOpenDisposition disposition) override;
  content::WebContents* OpenURL(
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;
  const SessionID& GetSessionID() override;
  TabStripModel* GetTabStripModel() override;
  bool IsTabStripVisible() override;
  bool ShouldHideUIForFullscreen() const override;
  views::View* TopContainer() override;
  base::CallbackListSubscription RegisterActiveTabDidChange(
      ActiveTabChangeCallback callback) override;
  tabs::TabInterface* GetActiveTabInterface() override;
  BrowserWindowFeatures& GetFeatures() override;
  web_modal::WebContentsModalDialogHost*
  GetWebContentsModalDialogHostForWindow() override;
  bool IsActive() override;
  base::CallbackListSubscription RegisterDidBecomeActive(
      DidBecomeActiveCallback callback) override;
  base::CallbackListSubscription RegisterDidBecomeInactive(
      DidBecomeInactiveCallback callback) override;
  ExclusiveAccessManager* GetExclusiveAccessManager() override;
  BrowserActions* GetActions() override;
  Type GetType() const override;
  BrowserUserEducationInterface* GetUserEducationInterface() override;
  web_app::AppBrowserController* GetAppBrowserController() override;

  // Called by BrowserView when on active changes.
  void DidBecomeActive();
  void DidBecomeInactive();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool IsLockedForOnTask();
  void SetLockedForOnTask(bool locked);
#endif

 private:
  friend class BrowserTest;
  friend class ExclusiveAccessTest;
  friend class FullscreenControllerInteractiveTest;
  FRIEND_TEST_ALL_PREFIXES(AppModeTest, EnableAppModeTest);
  FRIEND_TEST_ALL_PREFIXES(BrowserCommandControllerTest,
                           IsReservedCommandOrKeyIsApp);
  FRIEND_TEST_ALL_PREFIXES(BrowserCloseTest, LastIncognito);
  FRIEND_TEST_ALL_PREFIXES(BrowserCloseTest, LastRegular);
  FRIEND_TEST_ALL_PREFIXES(BrowserCommandControllerTest, AppFullScreen);
  FRIEND_TEST_ALL_PREFIXES(BrowserTest, OpenAppWindowLikeNtp);
  FRIEND_TEST_ALL_PREFIXES(BrowserTest, AppIdSwitch);
  FRIEND_TEST_ALL_PREFIXES(ExclusiveAccessBubbleWindowControllerTest,
                           DenyExitsFullscreen);
  FRIEND_TEST_ALL_PREFIXES(ExclusiveAccessTest,
                           TabEntersPresentationModeFromWindowed);
  FRIEND_TEST_ALL_PREFIXES(BrowserCloseTest, LastGuest);

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

    // Change is the result of a force show reason
    BOOKMARK_BAR_STATE_CHANGE_FORCE_SHOW,
  };

  explicit Browser(const CreateParams& params);

  // Overridden from content::WebContentsDelegate:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;
  void NavigationStateChanged(content::WebContents* source,
                              content::InvalidateTypes changed_flags) override;
  void VisibleSecurityStateChanged(content::WebContents* source) override;
  content::WebContents* AddNewContents(
      content::WebContents* source,
      std::unique_ptr<content::WebContents> new_contents,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      bool user_gesture,
      bool* was_blocked) override;
  void LoadingStateChanged(content::WebContents* source,
                           bool should_show_loading_ui) override;
  void CloseContents(content::WebContents* source) override;
  void SetContentsBounds(content::WebContents* source,
                         const gfx::Rect& bounds) override;
  void UpdateTargetURL(content::WebContents* source, const GURL& url) override;
  void ContentsMouseEvent(content::WebContents* source,
                          const ui::Event& event) override;
  void ContentsZoomChange(bool zoom_in) override;
  bool TakeFocus(content::WebContents* source, bool reverse) override;
  void BeforeUnloadFired(content::WebContents* source,
                         bool proceed,
                         bool* proceed_to_fire_unload) override;
  bool ShouldFocusLocationBarByDefault(content::WebContents* source) override;
  bool ShouldFocusPageAfterCrash(content::WebContents* source) override;
  void ShowRepostFormWarningDialog(content::WebContents* source) override;
  bool IsWebContentsCreationOverridden(
      content::SiteInstance* source_site_instance,
      content::mojom::WindowContainerType window_container_type,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url) override;
  content::WebContents* CreateCustomWebContents(
      content::RenderFrameHost* opener,
      content::SiteInstance* source_site_instance,
      bool is_new_browsing_instance,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url,
      const content::StoragePartitionConfig& partition_config,
      content::SessionStorageNamespace* session_storage_namespace) override;
  void WebContentsCreated(content::WebContents* source_contents,
                          int opener_render_process_id,
                          int opener_render_frame_id,
                          const std::string& frame_name,
                          const GURL& target_url,
                          content::WebContents* new_contents) override;
  void RendererUnresponsive(
      content::WebContents* source,
      content::RenderWidgetHost* render_widget_host,
      base::RepeatingClosure hang_monitor_restarter) override;
  void RendererResponsive(
      content::WebContents* source,
      content::RenderWidgetHost* render_widget_host) override;
  content::JavaScriptDialogManager* GetJavaScriptDialogManager(
      content::WebContents* source) override;
  bool GuestSaveFrame(content::WebContents* guest_web_contents) override;
  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override;
  void EnumerateDirectory(content::WebContents* web_contents,
                          scoped_refptr<content::FileSelectListener> listener,
                          const base::FilePath& path) override;
  bool CanUseWindowingControls(
      content::RenderFrameHost* requesting_frame) override;
  void OnCanResizeFromWebAPIChanged() override;
  bool GetCanResize() override;
  void MinimizeFromWebAPI() override;
  void MaximizeFromWebAPI() override;
  void RestoreFromWebAPI() override;
  ui::mojom::WindowShowState GetWindowShowState() const override;
  bool CanEnterFullscreenModeForTab(
      content::RenderFrameHost* requesting_frame) override;
  void EnterFullscreenModeForTab(
      content::RenderFrameHost* requesting_frame,
      const blink::mojom::FullscreenOptions& options) override;
  void ExitFullscreenModeForTab(content::WebContents* web_contents) override;
  bool IsFullscreenForTabOrPending(
      const content::WebContents* web_contents) override;
  content::FullscreenState GetFullscreenState(
      const content::WebContents* web_contents) const override;
  blink::mojom::DisplayMode GetDisplayMode(
      const content::WebContents* web_contents) override;
  blink::ProtocolHandlerSecurityLevel GetProtocolHandlerSecurityLevel(
      content::RenderFrameHost* requesting_frame) override;
  void RegisterProtocolHandler(content::RenderFrameHost* requesting_frame,
                               const std::string& protocol,
                               const GURL& url,
                               bool user_gesture) override;
  void UnregisterProtocolHandler(content::RenderFrameHost* requesting_frame,
                                 const std::string& protocol,
                                 const GURL& url,
                                 bool user_gesture) override;
  void FindReply(content::WebContents* web_contents,
                 int request_id,
                 int number_of_matches,
                 const gfx::Rect& selection_rect,
                 int active_match_ordinal,
                 bool final_update) override;
  void RequestPointerLock(content::WebContents* web_contents,
                          bool user_gesture,
                          bool last_unlocked_by_target) override;
  void LostPointerLock() override;
  bool IsWaitingForPointerLockPrompt(
      content::WebContents* web_contents) override;
  void RequestKeyboardLock(content::WebContents* web_contents,
                           bool esc_key_locked) override;
  void CancelKeyboardLockRequest(content::WebContents* web_contents) override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const url::Origin& security_origin,
                                  blink::mojom::MediaStreamType type) override;
  std::string GetTitleForMediaControls(
      content::WebContents* web_contents) override;

#if BUILDFLAG(ENABLE_PRINTING)
  void PrintCrossProcessSubframe(
      content::WebContents* web_contents,
      const gfx::Rect& rect,
      int document_cookie,
      content::RenderFrameHost* subframe_host) const override;
#endif

#if BUILDFLAG(ENABLE_PAINT_PREVIEW)
  void CapturePaintPreviewOfSubframe(
      content::WebContents* web_contents,
      const gfx::Rect& rect,
      const base::UnguessableToken& guid,
      content::RenderFrameHost* render_frame_host) override;
#endif

  // WebContentsCollection::Observer:
  void DidFinishNavigation(
      content::WebContents* web_contents,
      content::NavigationHandle* navigation_handle) override;

  // Overridden from WebContentsModalDialogManagerDelegate:
  void SetWebContentsBlocked(content::WebContents* web_contents,
                             bool blocked) override;
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;

  // Overridden from BookmarkTabHelperObserver:
  void URLStarredChanged(content::WebContents* web_contents,
                         bool starred) override;

  // Overridden from ZoomObserver:
  void OnZoomControllerDestroyed(
      zoom::ZoomController* zoom_controller) override;
  void OnZoomChanged(
      const zoom::ZoomController::ZoomChangedEventData& data) override;

  // Overridden from SelectFileDialog::Listener:
  void FileSelected(const ui::SelectedFileInfo& file_info, int index) override;
  void FileSelectionCanceled() override;

  // Overridden from ThemeServiceObserver:
  void OnThemeChanged() override;

  // Overridden from translate::ContentTranslateDriver::TranslationObserver:
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Handle `on_task_locked_` state changes.
  void OnLockedForOnTaskUpdated();
#endif

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

  // Called when the user has decided whether to proceed or not with the browser
  // closure.  |cancel_downloads| is true if the downloads should be canceled
  // and the browser closed, false if the browser should stay open and the
  // downloads running.
  void InProgressDownloadResponse(bool cancel_downloads);

  // Called when the user has decided whether to proceed or not with the browser
  // closure, in case the cookie migration notice was shown. |proceed_closing|
  // is true if the browser can  be closed.
  void CookieMigrationNoticeResponse(bool proceed_closing);

  // Called when all warnings have completed when attempting to close the
  // browser directly (e.g. via hotkey, close button, terminate signal, etc.)
  // Used as a WarnBeforeClosingCallback by HandleBeforeClose().
  void FinishWarnBeforeClosing(WarnBeforeClosingResult result);

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

  // See comment on SupportsWindowFeatureImpl for info on `check_can_support`.
  bool NormalBrowserSupportsWindowFeature(WindowFeature feature,
                                          bool check_can_support) const;

  // See comment on SupportsWindowFeatureImpl for info on `check_can_support`.
  bool PopupBrowserSupportsWindowFeature(WindowFeature feature,
                                         bool check_can_support) const;

  // See comment on SupportsWindowFeatureImpl for info on `check_can_support`.
  bool AppPopupBrowserSupportsWindowFeature(WindowFeature feature,
                                            bool check_can_support) const;

  // See comment on SupportsWindowFeatureImpl for info on `check_can_support`.
  bool AppBrowserSupportsWindowFeature(WindowFeature feature,
                                       bool check_can_support) const;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // See comment on SupportsWindowFeatureImpl for info on `check_can_support`.
  bool CustomTabBrowserSupportsWindowFeature(WindowFeature feature) const;
#endif

  // See comment on SupportsWindowFeatureImpl for info on `check_can_support`.
  bool PictureInPictureBrowserSupportsWindowFeature(
      WindowFeature feature,
      bool check_can_support) const;

  // Implementation of SupportsWindowFeature and CanSupportWindowFeature. If
  // `check_can_support` is true, this method returns true if this type of
  // browser can ever support `feature`, under any conditions; if
  // `check_can_support` is false, it returns true if the browser *in its
  // current state* (e.g. whether or not it is currently fullscreen) supports
  // `feature`.
  bool SupportsWindowFeatureImpl(WindowFeature feature,
                                 bool check_can_support) const;

  // Resets |bookmark_bar_state_| based on the active tab. Notifies the
  // BrowserWindow if necessary.
  void UpdateBookmarkBarState(BookmarkBarStateChangeReason reason);

  bool ShouldShowBookmarkBar() const;

  // Returns true if we can start the shutdown sequence for the browser, i.e.
  // the last browser window is being closed.
  bool ShouldStartShutdown() const;

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

  // Data members /////////////////////////////////////////////////////////////

  PrefChangeRegistrar profile_pref_registrar_;

  // This Browser's create params.
  const CreateParams create_params_;

  // This Browser's type.
  const Type type_;

  // This Browser's profile.
  const raw_ptr<Profile, AcrossTasksDanglingUntriaged> profile_;

  // Prevent Profile deletion until this browser window is closed.
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;

  // This Browser's window.
  raw_ptr<BrowserWindow, DanglingUntriaged> window_;

  std::unique_ptr<TabStripModelDelegate> const tab_strip_model_delegate_;
  std::unique_ptr<TabStripModel> const tab_strip_model_;

  std::unique_ptr<TabMenuModelDelegate> const tab_menu_model_delegate_;

  // The application name that is also the name of the window to the shell.
  // This name should be set when:
  // 1) we launch an application via an application shortcut or extension API.
  // 2) we launch an undocked devtool window.
  const std::string app_name_;

  // True if the source is trusted (i.e. we do not need to show the URL in a
  // a popup window). Also used to determine which app windows to save and
  // restore on Chrome OS.
  bool is_trusted_source_;

  // Unique identifier of this browser for session restore. This id is only
  // unique within the current session, and is not guaranteed to be unique
  // across sessions.
  const SessionID session_id_;

  // Whether this Browser should be omitted from being saved/restored by session
  // restore.
  bool omit_from_session_restore_ = false;

  // If true, a new window opening should be treated like the start of a session
  // (with potential session restore, startup URLs, etc.). Otherwise, don't
  // restore the session.
  const bool should_trigger_session_restore_;

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
  ui::mojom::WindowShowState initial_show_state_;
  const std::string initial_workspace_;
  bool initial_visible_on_all_workspaces_state_;

  CreationSource creation_source_ = CreationSource::kUnknown;

  UnloadController unload_controller_;

  // The Find Bar. This may be NULL if there is no Find Bar, and if it is
  // non-NULL, it may or may not be visible.
  std::unique_ptr<FindBarController> find_bar_controller_;

  // Dialog box used for opening and saving files.
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  // Helper which implements the ContentSettingBubbleModel interface.
  std::unique_ptr<BrowserContentSettingBubbleModelDelegate>
      content_setting_bubble_model_delegate_;

  // Helper which implements the LocationBarModelDelegate interface.
  std::unique_ptr<BrowserLocationBarModelDelegate> location_bar_model_delegate_;

  // The model for the toolbar view.
  std::unique_ptr<LocationBarModel> location_bar_model_;

  // Helper which implements the LiveTabContext interface.
  std::unique_ptr<BrowserLiveTabContext> live_tab_context_;

  // Helper which implements the SyncedWindowDelegate interface.
  std::unique_ptr<BrowserSyncedWindowDelegate> synced_window_delegate_;

  std::unique_ptr<BrowserInstantController> instant_controller_;

  // Helper which handles bookmark app specific browser configuration.
  // This must be initialized before |command_controller_| to ensure the correct
  // set of commands are enabled.
  std::unique_ptr<web_app::AppBrowserController> app_controller_;

  BookmarkBar::State bookmark_bar_state_;

  std::unique_ptr<ExclusiveAccessManager> exclusive_access_manager_;

  std::unique_ptr<extensions::BrowserExtensionWindowController>
      extension_window_controller_;

  std::unique_ptr<BrowserActions> browser_actions_;

  std::unique_ptr<chrome::BrowserCommandController> command_controller_;

  // Dialog controller that handles the showing of the deletion dialog.
  // TODO (https://crbug.com/372011320) Move this to be a browser window
  // feature.
  std::unique_ptr<tab_groups::DeletionDialogController>
      tab_group_deletion_dialog_controller_;

  // True if the browser window has been shown at least once.
  bool window_has_shown_;

  std::string user_title_;

  // Controls both signin and sync consent.
  SigninViewController signin_view_controller_;

  // Listens for browser-related breadcrumb events to be added to crash reports.
  std::unique_ptr<BreadcrumbManagerBrowserAgent>
      breadcrumb_manager_browser_agent_;

  std::unique_ptr<ScopedKeepAlive> keep_alive_;

  WarnBeforeClosingCallback warn_before_closing_callback_;

  // Tells if the browser should skip warning the user when closing the window.
  bool force_skip_warning_user_on_close_ = false;

  // If true, immediately updates the UI when scheduled.
  bool update_ui_immediately_for_testing_ = false;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // OnTask is a ChromeOS feature that is not related to web browsers, but
  // happens to be implemented using code in //chrome/browser. The feature,
  // when enabled, disables certain functionality that a web browser would
  // never typically disable.
  bool on_task_locked_ = false;
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  std::unique_ptr<extensions::ExtensionBrowserWindowHelper>
      extension_browser_window_helper_;
#endif

  const base::ElapsedTimer creation_timer_;

  // The opener browser of the document picture-in-picture browser. Null if the
  // current browser is a regular browser.
  raw_ptr<Browser> opener_browser_ = nullptr;

  WebContentsCollection web_contents_collection_{this};

  // If true, the Browser window has been closed and this will be deleted
  // shortly (after a PostTask).
  bool is_delete_scheduled_ = false;

#if defined(USE_AURA)
  std::unique_ptr<OverscrollPrefManager> overscroll_pref_manager_;
#endif

  int force_show_bookmark_bar_flags_ = ForceShowBookmarkBarFlag::kNone;

  using DidActiveTabChangeCallbackList =
      base::RepeatingCallbackList<void(BrowserWindowInterface*)>;
  DidActiveTabChangeCallbackList did_active_tab_change_callback_list_;

  using DidBecomeActiveCallbackList =
      base::RepeatingCallbackList<void(BrowserWindowInterface*)>;
  DidBecomeActiveCallbackList did_become_active_callback_list_;

  using DidBecomeInactiveCallbackList =
      base::RepeatingCallbackList<void(BrowserWindowInterface*)>;
  DidBecomeInactiveCallbackList did_become_inactive_callback_list_;

  std::unique_ptr<BrowserWindowFeatures> features_;

  // The following factory is used for chrome update coalescing.
  base::WeakPtrFactory<Browser> chrome_updater_factory_{this};

  // The following factory is used to close the frame at a later time.
  base::WeakPtrFactory<Browser> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_BROWSER_H_
