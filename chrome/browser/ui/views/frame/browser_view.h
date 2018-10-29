// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_VIEW_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/extension_commands_global_registry.h"
#include "chrome/browser/extensions/extension_keybinding_registry.h"
#include "chrome/browser/metrics/browser_window_histogram_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/exclusive_access_bubble_views_context.h"
#include "chrome/browser/ui/views/extensions/extension_keybinding_registry_views.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/top_controls_slide_controller.h"
#include "chrome/browser/ui/views/frame/web_contents_close_handler.h"
#include "chrome/browser/ui/views/load_complete_listener.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_renderer_data.h"
#include "chrome/common/buildflags.h"
#include "components/infobars/core/infobar_container.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/client_view.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/views/intent_picker_bubble_view.h"
#endif  // defined(OS_CHROMEOS)

// NOTE: For more information about the objects and files in this directory,
// view: http://dev.chromium.org/developers/design-documents/browser-window

class BookmarkBarView;
class Browser;
class BrowserViewLayout;
class ContentsLayoutManager;
class DownloadShelfView;
class ExclusiveAccessBubbleViews;
class FullscreenControlHost;
class InfoBarContainerView;
class LocationBarView;
class StatusBubbleViews;
class TabStrip;
class ToolbarButtonProvider;
class ToolbarView;
class TopContainerView;
class TopControlsSlideControllerTest;
class WebContentsCloseHandler;

namespace extensions {
class ActiveTabPermissionGranter;
class Command;
class Extension;
}

namespace version_info {
enum class Channel;
}

namespace views {
class ExternalFocusTracker;
class WebView;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView
//
//  A ClientView subclass that provides the contents of a browser window,
//  including the TabStrip, toolbars, download shelves, the content area etc.
//
class BrowserView : public BrowserWindow,
                    public TabStripModelObserver,
                    public ui::AcceleratorProvider,
                    public views::WidgetDelegate,
                    public views::WidgetObserver,
                    public views::ClientView,
                    public infobars::InfoBarContainer::Delegate,
                    public LoadCompleteListener::Delegate,
                    public ExclusiveAccessContext,
                    public ExclusiveAccessBubbleViewsContext,
                    public extensions::ExtensionKeybindingRegistry::Delegate,
                    public ImmersiveModeController::Observer {
 public:
  // The browser view's class name.
  static const char kViewClassName[];

  BrowserView();
  ~BrowserView() override;

  void Init(std::unique_ptr<Browser> browser);

  void set_frame(BrowserFrame* frame) { frame_ = frame; }
  BrowserFrame* frame() const { return frame_; }

  // Returns a pointer to the BrowserView* interface implementation (an
  // instance of this object, typically) for a given native window, or null if
  // there is no such association.
  //
  // Don't use this unless you only have a NativeWindow. In nearly all
  // situations plumb through browser and use it.
  static BrowserView* GetBrowserViewForNativeWindow(gfx::NativeWindow window);

  // Returns the BrowserView used for the specified Browser.
  static BrowserView* GetBrowserViewForBrowser(const Browser* browser);

  // Paints a 1 device-pixel-thick horizontal line (regardless of device scale
  // factor) at either the very bottom or very top of the interior of |bounds|,
  // depending on |at_bottom|.
  static void Paint1pxHorizontalLine(gfx::Canvas* canvas,
                                     SkColor color,
                                     const gfx::Rect& bounds,
                                     bool at_bottom);

  // After calling RevealTabStripIfNeeded(), there is normally a delay before
  // the tabstrip is hidden. Tests can use this function to disable that delay
  // (and hide immediately).
  static void SetDisableRevealerDelayForTesting(bool disable);

  // Returns a Browser instance of this view.
  Browser* browser() { return browser_.get(); }
  const Browser* browser() const { return browser_.get(); }

  const TopControlsSlideController* top_controls_slide_controller() const {
    return top_controls_slide_controller_.get();
  }

  // Initializes (or re-initializes) the status bubble.  We try to only create
  // the bubble once and re-use it for the life of the browser, but certain
  // events (such as changing enabling/disabling Aero on Win) can force a need
  // to change some of the bubble's creation parameters.
  void InitStatusBubble();

  // Returns the constraining bounding box that should be used to lay out the
  // FindBar within. This is _not_ the size of the find bar, just the bounding
  // box it should be laid out within. The coordinate system of the returned
  // rect is in the coordinate system of the frame, since the FindBar is a child
  // window.
  gfx::Rect GetFindBarBoundingBox() const;

  // Returns the preferred height of the TabStrip. Used to position the
  // incognito avatar icon.
  int GetTabStripHeight() const;

  // Takes some view's origin (relative to this BrowserView) and offsets it such
  // that it can be used as the source origin for seamlessly tiling the toolbar
  // background image over that view.
  gfx::Point OffsetPointForToolbarBackgroundImage(
      const gfx::Point& point) const;

  // Container for the tabstrip, toolbar, etc.
  TopContainerView* top_container() { return top_container_; }

  // Container for the web contents.
  views::View* contents_container() { return contents_container_; }

  // Accessor for the TabStrip.
  TabStrip* tabstrip() { return tabstrip_; }

  // Accessor for the Toolbar.
  ToolbarView* toolbar() { return toolbar_; }

  // Bookmark bar may be null, for example for pop-ups.
  BookmarkBarView* bookmark_bar() { return bookmark_bar_view_.get(); }

  // Returns the do-nothing view which controls the z-order of the find bar
  // widget relative to views which paint into layers and views which have an
  // associated NativeView. The presence / visibility of this view is not
  // indicative of the visibility of the find bar widget or even whether
  // FindBarController is initialized.
  View* find_bar_host_view() { return find_bar_host_view_; }

  // Accessor for the InfobarContainer.
  InfoBarContainerView* infobar_container() { return infobar_container_; }

  // Accessor for the FullscreenExitBubbleViews.
  ExclusiveAccessBubbleViews* exclusive_access_bubble() {
    return exclusive_access_bubble_.get();
  }

  // Accessor for the contents WebView.
  views::WebView* contents_web_view() { return contents_web_view_; }

  // Returns true if various window components are visible.
  bool IsTabStripVisible() const;

  bool IsInfoBarVisible() const;

  // Returns true if the profile associated with this Browser window is
  // incognito.
  bool IsIncognito() const;

  // Returns true if the profile associated with this Browser window is
  // a guest session.
  bool IsGuestSession() const;

  // Returns true if the profile associated with this Browser window is
  // not incognito or a guest session.
  bool IsRegularOrGuestSession() const;

  // Provides the containing frame with the accelerator for the specified
  // command id. This can be used to provide menu item shortcut hints etc.
  // Returns true if an accelerator was found for the specified |cmd_id|, false
  // otherwise.
  bool GetAccelerator(int cmd_id, ui::Accelerator* accelerator) const;

  // Returns true if the specificed |accelerator| is registered with this view.
  bool IsAcceleratorRegistered(const ui::Accelerator& accelerator);

  // Returns the active WebContents. Used by our NonClientView's
  // TabIconView::TabContentsProvider implementations.
  // TODO(beng): exposing this here is a bit bogus, since it's only used to
  // determine loading state. It'd be nicer if we could change this to be
  // bool IsSelectedTabLoading() const; or something like that. We could even
  // move it to a WindowDelegate subclass.
  content::WebContents* GetActiveWebContents() const;

  // Returns true if the Browser object associated with this BrowserView is a
  // tabbed-type window (i.e. a browser window, not an app or popup).
  bool IsBrowserTypeNormal() const {
    return browser_->is_type_tabbed();
  }

  // Returns true if the Browser object associated with this BrowserView is a
  // for an installed hosted app.
  bool IsBrowserTypeHostedApp() const;

  // Returns true if the top browser controls (a.k.a. top-chrome UIs) are
  // allowed to slide up and down with the gesture scrolls on the current tab's
  // page.
  bool IsTopControlsSlideBehaviorEnabled() const;

  // Returns the current shown ratio of the top browser controls.
  float GetTopControlsSlideBehaviorShownRatio() const;

  // See ImmersiveModeController for description.
  ImmersiveModeController* immersive_mode_controller() const {
    return immersive_mode_controller_.get();
  }

  // Returns true if the view has been initialized.
  bool initialized() const { return initialized_; }

  // Restores the focused view. This is also used to set the initial focus
  // when a new browser window is created.
  void RestoreFocus();

  // Called after the widget's fullscreen state is changed without going through
  // FullscreenController. This method does any processing which was skipped.
  void FullscreenStateChanged();

  // Sets the button provider for this BrowserView. Must be called before
  // InitViews() which sets the ToolbarView as the default button provider.
  void SetToolbarButtonProvider(ToolbarButtonProvider* provider);
  ToolbarButtonProvider* toolbar_button_provider() {
    return toolbar_button_provider_;
  }

  // BrowserWindow:
  void Show() override;
  void ShowInactive() override;
  void Hide() override;
  bool IsVisible() const override;
  void SetBounds(const gfx::Rect& bounds) override;
  void Close() override;
  void Activate() override;
  void Deactivate() override;
  bool IsActive() const override;
  void FlashFrame(bool flash) override;
  bool IsAlwaysOnTop() const override;
  void SetAlwaysOnTop(bool always_on_top) override;
  gfx::NativeWindow GetNativeWindow() const override;
  void SetTopControlsShownRatio(content::WebContents* web_contents,
                                float ratio) override;
  bool DoBrowserControlsShrinkRendererSize(
      const content::WebContents* contents) const override;
  int GetTopControlsHeight() const override;
  void SetTopControlsGestureScrollInProgress(bool in_progress) override;
  StatusBubble* GetStatusBubble() override;
  void UpdateTitleBar() override;
  void BookmarkBarStateChanged(
      BookmarkBar::AnimateChangeType change_type) override;
  void UpdateDevTools() override;
  void UpdateLoadingAnimations(bool should_animate) override;
  void SetStarredState(bool is_starred) override;
  void SetTranslateIconToggled(bool is_lit) override;
  void OnActiveTabChanged(content::WebContents* old_contents,
                          content::WebContents* new_contents,
                          int index,
                          int reason) override;
  void OnTabDetached(content::WebContents* contents, bool was_active) override;
  void ZoomChangedForActiveTab(bool can_show_bubble) override;
  gfx::Rect GetRestoredBounds() const override;
  ui::WindowShowState GetRestoredState() const override;
  gfx::Rect GetBounds() const override;
  gfx::Size GetContentsSize() const override;
  bool IsMaximized() const override;
  bool IsMinimized() const override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  void EnterFullscreen(const GURL& url,
                       ExclusiveAccessBubbleType bubble_type) override;
  void ExitFullscreen() override;
  void UpdateExclusiveAccessExitBubbleContent(
      const GURL& url,
      ExclusiveAccessBubbleType bubble_type,
      ExclusiveAccessBubbleHideCallback bubble_first_hide_callback,
      bool force_update) override;
  void OnExclusiveAccessUserInput() override;
  bool ShouldHideUIForFullscreen() const override;
  bool IsFullscreen() const override;
  bool IsFullscreenBubbleVisible() const override;
  PageActionIconContainer* GetPageActionIconContainer() override;
  LocationBar* GetLocationBar() const override;
  void SetFocusToLocationBar(bool select_all) override;
  void UpdateReloadStopState(bool is_loading, bool force) override;
  void UpdateToolbar(content::WebContents* contents) override;
  void ResetToolbarTabState(content::WebContents* contents) override;
  void FocusToolbar() override;
  ToolbarActionsBar* GetToolbarActionsBar() override;
  void ToolbarSizeChanged(bool is_animating) override;
  void FocusAppMenu() override;
  void FocusBookmarksToolbar() override;
  void FocusInactivePopupForAccessibility() override;
  void RotatePaneFocus(bool forwards) override;
  void DestroyBrowser() override;
  bool IsBookmarkBarVisible() const override;
  bool IsBookmarkBarAnimating() const override;
  bool IsTabStripEditable() const override;
  bool IsToolbarVisible() const override;
  bool IsToolbarShowing() const override;
  void ShowUpdateChromeDialog() override;
#if defined(OS_CHROMEOS)
  void ShowIntentPickerBubble(
      std::vector<IntentPickerBubbleView::AppInfo> app_info,
      bool disable_stay_in_chrome,
      IntentPickerResponse callback) override;
  void SetIntentPickerViewVisibility(bool visible) override;
#endif  //  defined(OS_CHROMEOS)
  void ShowBookmarkBubble(const GURL& url, bool already_bookmarked) override;
  autofill::SaveCardBubbleView* ShowSaveCreditCardBubble(
      content::WebContents* contents,
      autofill::SaveCardBubbleController* controller,
      bool is_user_gesture) override;
  autofill::LocalCardMigrationBubble* ShowLocalCardMigrationBubble(
      content::WebContents* contents,
      autofill::LocalCardMigrationBubbleController* controller,
      bool is_user_gesture) override;
  ShowTranslateBubbleResult ShowTranslateBubble(
      content::WebContents* contents,
      translate::TranslateStep step,
      translate::TranslateErrors::Type error_type,
      bool is_user_gesture) override;
#if BUILDFLAG(ENABLE_ONE_CLICK_SIGNIN)
  void ShowOneClickSigninConfirmation(
      const base::string16& email,
      const StartSyncCallback& start_sync_callback) override;
#endif
  // TODO(beng): Not an override, move somewhere else.
  void SetDownloadShelfVisible(bool visible);
  bool IsDownloadShelfVisible() const override;
  DownloadShelf* GetDownloadShelf() override;
  void ConfirmBrowserCloseWithPendingDownloads(
      int download_count,
      Browser::DownloadClosePreventionType dialog_type,
      bool app_modal,
      const base::Callback<void(bool)>& callback) override;
  void UserChangedTheme() override;
  void ShowAppMenu() override;
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) override;
  bool HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) override;
  void CutCopyPaste(int command_id) override;
  FindBar* CreateFindBar() override;
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;
  void ShowHatsBubbleFromAppMenuButton() override;
  void ShowAvatarBubbleFromAvatarButton(
      AvatarBubbleMode mode,
      const signin::ManageAccountsParams& manage_accounts_params,
      signin_metrics::AccessPoint access_point,
      bool is_source_keyboard) override;
  int GetRenderViewHeightInsetWithDetachedBookmarkBar() override;
  void ExecuteExtensionCommand(const extensions::Extension* extension,
                               const extensions::Command& command) override;
  ExclusiveAccessContext* GetExclusiveAccessContext() override;
  void ShowImeWarningBubble(
      const extensions::Extension* extension,
      const base::Callback<void(ImeWarningBubblePermissionStatus status)>&
          callback) override;
  std::string GetWorkspace() const override;
  bool IsVisibleOnAllWorkspaces() const override;

  BookmarkBarView* GetBookmarkBarView() const;
  LocationBarView* GetLocationBarView() const;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void TabStripEmpty() override;
  void WillCloseAllTabs(TabStripModel* tab_strip_model) override;
  void CloseAllTabsStopped(TabStripModel* tab_strip_model,
                           CloseAllStoppedReason reason) override;

  // ui::AcceleratorProvider:
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;

  // views::WidgetDelegate:
  bool CanResize() const override;
  bool CanMaximize() const override;
  bool CanMinimize() const override;
  bool CanActivate() const override;
  base::string16 GetWindowTitle() const override;
  base::string16 GetAccessibleWindowTitle() const override;
  views::View* GetInitiallyFocusedView() override;
  bool ShouldShowWindowTitle() const override;
  gfx::ImageSkia GetWindowAppIcon() override;
  gfx::ImageSkia GetWindowIcon() override;
  bool ShouldShowWindowIcon() const override;
  bool ExecuteWindowsCommand(int command_id) override;
  std::string GetWindowName() const override;
  void SaveWindowPlacement(const gfx::Rect& bounds,
                           ui::WindowShowState show_state) override;
  bool GetSavedWindowPlacement(const views::Widget* widget,
                               gfx::Rect* bounds,
                               ui::WindowShowState* show_state) const override;
  views::View* GetContentsView() override;
  views::ClientView* CreateClientView(views::Widget* widget) override;
  views::View* CreateOverlayView() override;
  void OnWindowBeginUserBoundsChange() override;
  void OnWindowEndUserBoundsChange() override;
  void OnWidgetMove() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;
  void GetAccessiblePanes(std::vector<View*>* panes) override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

  // views::ClientView:
  bool CanClose() override;
  int NonClientHitTest(const gfx::Point& point) override;
  gfx::Size GetMinimumSize() const override;

  // infobars::InfoBarContainer::Delegate:
  void InfoBarContainerStateChanged(bool is_animating) override;

  // views::View:
  const char* GetClassName() const override;
  void Layout() override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;
  void PaintChildren(const views::PaintInfo& paint_info) override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void ChildPreferredSizeChanged(View* child) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnThemeChanged() override;

  // ui::AcceleratorTarget:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

  // ExclusiveAccessContext:
  Profile* GetProfile() override;
  void UpdateUIForTabFullscreen(TabFullscreenState state) override;
  content::WebContents* GetActiveWebContents() override;
  void HideDownloadShelf() override;
  void UnhideDownloadShelf() override;
  bool CanUserExitFullscreen() const override;

  // ExclusiveAccessBubbleViewsContext:
  ExclusiveAccessManager* GetExclusiveAccessManager() override;
  views::Widget* GetBubbleAssociatedWidget() override;
  ui::AcceleratorProvider* GetAcceleratorProvider() override;
  gfx::NativeView GetBubbleParentView() const override;
  gfx::Point GetCursorPointInParent() const override;
  gfx::Rect GetClientAreaBoundsInScreen() const override;
  bool IsImmersiveModeEnabled() const override;
  gfx::Rect GetTopContainerBoundsInScreen() override;
  void DestroyAnyExclusiveAccessBubble() override;
  bool CanTriggerOnMouse() const override;

  // extension::ExtensionKeybindingRegistry::Delegate:
  extensions::ActiveTabPermissionGranter* GetActiveTabPermissionGranter()
      override;

  // ImmersiveModeController::Observer:
  void OnImmersiveRevealStarted() override;
  void OnImmersiveRevealEnded() override;
  void OnImmersiveFullscreenExited() override;
  void OnImmersiveModeControllerDestroyed() override;

  // Creates an accessible tab label for screen readers that includes the tab
  // status for the given tab index. This takes the form of
  // "Page title - Tab state".
  base::string16 GetAccessibleTabLabel(bool include_app_name, int index) const;

  // Testing interface:
  views::View* GetContentsContainerForTest() { return contents_container_; }
  views::WebView* GetDevToolsWebViewForTest() { return devtools_web_view_; }
  FullscreenControlHost* fullscreen_control_host_for_test() {
    return fullscreen_control_host_.get();
  }

  // Called by BrowserFrame during theme changes.
  void NativeThemeUpdated(const ui::NativeTheme* theme);

  // Gets the amount to vertically shift the placement of the icons on the
  // bookmark bar so the icons appear centered relative to the views above and
  // below them.
  int GetBookmarkBarContentVerticalOffset() const;

 private:
  // Do not friend BrowserViewLayout. Use the BrowserViewLayoutDelegate
  // interface to keep these two classes decoupled and testable.
  friend class BrowserViewLayoutDelegateImpl;
  friend class TopControlsSlideControllerTest;
  FRIEND_TEST_ALL_PREFIXES(BrowserViewTest, BrowserView);
  FRIEND_TEST_ALL_PREFIXES(BrowserViewTest, AccessibleWindowTitle);

  // If the browser is in immersive full screen mode, it will reveal the
  // tabstrip for a short duration. This is useful for shortcuts that perform
  // tab navigations and need to give users a visual clue as to what tabs are
  // affected.
  void RevealTabStripIfNeeded();

  // Constructs and initializes the child views.
  void InitViews();

  // Callback for the loading animation(s) associated with this view.
  void LoadingAnimationCallback();

  // LoadCompleteListener::Delegate implementation. Creates the JumpList after
  // the first page load.
  void OnLoadCompleted() override;

  // Returns the BrowserViewLayout.
  BrowserViewLayout* GetBrowserViewLayout() const;

  // Returns the ContentsLayoutManager.
  ContentsLayoutManager* GetContentsLayoutManager() const;

  // Prepare to show the Bookmark Bar for the specified WebContents.
  // Returns true if the Bookmark Bar can be shown (i.e. it's supported for this
  // Browser type) and there should be a subsequent re-layout to show it.
  // |contents| can be null.
  bool MaybeShowBookmarkBar(content::WebContents* contents);

  // Moves the bookmark bar view to the specified parent, which may be null,
  // |this|, or |top_container_|. Ensures that |top_container_| stays in front
  // of |bookmark_bar_view_|.
  void SetBookmarkBarParent(views::View* new_parent);

  // Prepare to show an Info Bar for the specified WebContents. Returns
  // true if there is an Info Bar to show and one is supported for this Browser
  // type, and there should be a subsequent re-layout to show it.
  // |contents| can be null.
  bool MaybeShowInfoBar(content::WebContents* contents);

  // Updates devtools window for given contents. This method will show docked
  // devtools window for inspected |web_contents| that has docked devtools
  // and hide it for null or not inspected |web_contents|. It will also make
  // sure devtools window size and position are restored for given tab.
  // This method will not update actual DevTools WebContents, if not
  // |update_devtools_web_contents|. In this case, manual update is required.
  void UpdateDevToolsForContents(content::WebContents* web_contents,
                                 bool update_devtools_web_contents);

  // Updates various optional child Views, e.g. Bookmarks Bar, Info Bar or the
  // Download Shelf in response to a change notification from the specified
  // |contents|. |contents| can be null. In this case, all optional UI will be
  // removed.
  void UpdateUIForContents(content::WebContents* contents);

  // Invoked to update the necessary things when our fullscreen state changes
  // to |fullscreen|. On Windows this is invoked immediately when we toggle the
  // full screen node_data. On Linux changing the fullscreen node_data is async,
  // so we
  // ask the window to change its fullscreen node_data, then when we get
  // notification that it succeeded this method is invoked.
  // If |url| is not empty, it is the URL of the page that requested fullscreen
  // (via the fullscreen JS API).
  // |bubble_type| determines what should be shown in the fullscreen exit
  // bubble.
  void ProcessFullscreen(bool fullscreen,
                         const GURL& url,
                         ExclusiveAccessBubbleType bubble_type);

  // Returns whether immmersive fullscreen should replace fullscreen. This
  // should only occur for "browser-fullscreen" for tabbed-typed windows (not
  // for tab-fullscreen and not for app/popup type windows).
  bool ShouldUseImmersiveFullscreenForUrl(const GURL& url) const;

  // Copy the accelerator table from the app resources into something we can
  // use.
  void LoadAccelerators();

  // Retrieves the command id for the specified Windows app command.
  int GetCommandIDForAppCommandID(int app_command_id) const;

  // Possibly records a user metrics action corresponding to the passed-in
  // accelerator.  Only implemented for Chrome OS, where we're interested in
  // learning about how frequently the top-row keys are used.
  void UpdateAcceleratorMetrics(const ui::Accelerator& accelerator,
                                int command_id);

  // Calls |method| which is either WebContents::Cut, ::Copy, or ::Paste on
  // the given WebContents, returning true if it consumed the event.
  bool DoCutCopyPasteForWebContents(
      content::WebContents* contents,
      void (content::WebContents::*method)());

  // Shows the next app-modal dialog box, if there is one to be shown, or moves
  // an existing showing one to the front.
  void ActivateAppModalDialog() const;

  // Retrieves the chrome command id associated with |accelerator|. The function
  // returns false if |accelerator| is unknown. Otherwise |command_id| will be
  // set to the chrome command id defined in //chrome/app/chrome_command_ids.h.
  bool FindCommandIdForAccelerator(const ui::Accelerator& accelerator,
                                   int* command_id) const;

  // Called by GetAccessibleWindowTitle, split out to make it testable.
  base::string16 GetAccessibleWindowTitleForChannelAndProfile(
      version_info::Channel,
      Profile* profile) const;

  // Returns the amount of space between the bottom of the location bar to the
  // bottom of the toolbar. This does not include the part of the toolbar that
  // overlaps with the bookmark bar.
  int GetBottomInsetOfLocationBarWithinToolbar() const;

  // Reparents |top_container_| to be a child of |this| instead of
  // |overlay_view_|.
  void ReparentTopContainerForEndOfImmersive();

  // The BrowserFrame that hosts this view.
  BrowserFrame* frame_ = nullptr;

  // The Browser object we are associated with.
  std::unique_ptr<Browser> browser_;

  // BrowserView layout (LTR one is pictured here).
  //
  // --------------------------------------------------------------------
  // | TopContainerView (top_container_)                                |
  // |  --------------------------------------------------------------  |
  // |  | Tabs (tabstrip_)                                           |  |
  // |  |------------------------------------------------------------|  |
  // |  | Navigation buttons, address bar, menu (toolbar_)           |  |
  // |  --------------------------------------------------------------  |
  // |------------------------------------------------------------------|
  // | All infobars (infobar_container_) [1]                            |
  // |------------------------------------------------------------------|
  // | Bookmarks (bookmark_bar_view_) [1]                               |
  // |------------------------------------------------------------------|
  // | Contents container (contents_container_)                         |
  // |  --------------------------------------------------------------  |
  // |  |  devtools_web_view_                                        |  |
  // |  |------------------------------------------------------------|  |
  // |  |  contents_web_view_                                        |  |
  // |  --------------------------------------------------------------  |
  // |------------------------------------------------------------------|
  // | Active downloads (download_shelf_)                               |
  // --------------------------------------------------------------------
  //
  // [1] The bookmark bar and info bar are swapped when on the new tab page.
  //     Additionally when the bookmark bar is detached, contents_container_ is
  //     positioned on top of the bar while the tab's contents are placed below
  //     the bar.  This allows the find bar to always align with the top of
  //     contents_container_ regardless if there's bookmark or info bars.

  // The view that manages the tab strip, toolbar, and sometimes the bookmark
  // bar. Stacked top in the view hiearachy so it can be used to slide out
  // the top views in immersive fullscreen.
  TopContainerView* top_container_ = nullptr;

  // The TabStrip.
  TabStrip* tabstrip_ = nullptr;

  // The Toolbar containing the navigation buttons, menus and the address bar.
  ToolbarView* toolbar_ = nullptr;

  // The OverlayView for the widget, which is used to host |top_container_|
  // during immersive reveal.
  std::unique_ptr<views::ViewTargeterDelegate> overlay_view_targeter_;
  views::View* overlay_view_ = nullptr;

  // The Bookmark Bar View for this window. Lazily created. May be null for
  // non-tabbed browsers like popups. May not be visible.
  std::unique_ptr<BookmarkBarView> bookmark_bar_view_;

  // The do-nothing view which controls the z-order of the find bar widget
  // relative to views which paint into layers and views with an associated
  // NativeView.
  View* find_bar_host_view_ = nullptr;

  // The download shelf view (view at the bottom of the page).
  std::unique_ptr<DownloadShelfView> download_shelf_;

  // The InfoBarContainerView that contains InfoBars for the current tab.
  InfoBarContainerView* infobar_container_ = nullptr;

  // The view that contains the selected WebContents.
  ContentsWebView* contents_web_view_ = nullptr;

  // The view that contains devtools window for the selected WebContents.
  views::WebView* devtools_web_view_ = nullptr;

  // The view managing the devtools and contents positions.
  // Handled by ContentsLayoutManager.
  views::View* contents_container_ = nullptr;

  // Provides access to the toolbar buttons this browser view uses. Buttons may
  // appear in a hosted app frame or in a tabbed UI toolbar.
  ToolbarButtonProvider* toolbar_button_provider_ = nullptr;

  // Tracks and stores the last focused view which is not the
  // devtools_web_view_ or any of its children. Used to restore focus once
  // the devtools_web_view_ is hidden.
  std::unique_ptr<views::ExternalFocusTracker> devtools_focus_tracker_;

  // The Status information bubble that appears at the bottom of the window.
  std::unique_ptr<StatusBubbleViews> status_bubble_;

  // A mapping between accelerators and command IDs.
  std::map<ui::Accelerator, int> accelerator_table_;

  // True if we have already been initialized.
  bool initialized_ = false;

  // True if we're currently handling a theme change (i.e. inside
  // OnThemeChanged()).
  bool handling_theme_changed_ = false;

  // True when in ProcessFullscreen(). The flag is used to avoid reentrance and
  // to ignore requests to layout while in ProcessFullscreen() to reduce
  // jankiness.
  bool in_process_fullscreen_ = false;

  std::unique_ptr<ExclusiveAccessBubbleViews> exclusive_access_bubble_;

#if defined(OS_WIN)
  // Helper class to listen for completion of first page load.
  std::unique_ptr<LoadCompleteListener> load_complete_listener_;
#endif

  // The timer used to update frames for the Loading Animation.
  base::RepeatingTimer loading_animation_timer_;

  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  // If this flag is set then SetFocusToLocationBar() will set focus to the
  // location bar even if the browser window is not active.
  bool force_location_bar_focus_ = false;

  // This is non-null on Chrome OS only.
  std::unique_ptr<TopControlsSlideController> top_controls_slide_controller_;

  std::unique_ptr<ImmersiveModeController> immersive_mode_controller_;

  std::unique_ptr<WebContentsCloseHandler> web_contents_close_handler_;

  // The class that registers for keyboard shortcuts for extension commands.
  std::unique_ptr<ExtensionKeybindingRegistryViews>
      extension_keybinding_registry_;

  std::unique_ptr<BrowserWindowHistogramHelper> histogram_helper_;

  std::unique_ptr<FullscreenControlHost> fullscreen_control_host_;

  struct ResizeSession {
    // The time when user started resizing the window.
    base::TimeTicks begin_timestamp;
    base::TimeTicks last_resize_timestamp;
    // The number of times the window size is changed from the start (i.e. since
    // begin_timestamp).
    size_t step_count = 0;
  };
  base::Optional<ResizeSession> interactive_resize_;

// Set to true if QuitInstructionBubbleController is added as pre-target
// handler.
#if defined(OS_WIN) || (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  bool added_quit_instructions_ = false;
#endif

  mutable base::WeakPtrFactory<BrowserView> activate_modal_dialog_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(BrowserView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_VIEW_H_
