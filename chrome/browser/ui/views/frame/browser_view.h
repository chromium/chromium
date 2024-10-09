// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_VIEW_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_commands_global_registry.h"
#include "chrome/browser/extensions/extension_keybinding_registry.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/translate/partial_translate_bubble_model.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/exclusive_access_bubble_views_context.h"
#include "chrome/browser/ui/views/extensions/extension_keybinding_registry_views.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view_layout.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/web_contents_close_handler.h"
#include "chrome/browser/ui/views/intent_picker_bubble_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_closer.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"
#include "chrome/browser/ui/webui/tab_search/tab_search.mojom.h"
#include "chrome/common/buildflags.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/infobars/core/infobar_container.h"
#include "components/segmentation_platform/public/result.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "components/user_education/common/feature_promo_handle.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "content/public/browser/page_user_data.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/client_view.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/compositor/throughput_tracker.h"
#endif

// NOTE: For more information about the objects and files in this directory,
// view: http://dev.chromium.org/developers/design-documents/browser-window

class AccessibilityFocusHighlight;
class BookmarkBarView;
class Browser;
class ContentsLayoutManager;
class ExclusiveAccessBubbleViews;
class FullscreenControlHost;
class InfoBarContainerView;
class LocationBarView;
class SidePanel;
class StatusBubbleViews;
class TabSearchBubbleHost;
class TabStrip;
class TabStripRegionView;
class ToolbarButtonProvider;
class ToolbarView;
class TopContainerLoadingBar;
class TopContainerView;
class TopControlsSlideController;
class TopControlsSlideControllerTest;
class WebAppFrameToolbarView;
class WebContentsCloseHandler;
class WebUITabStripContainerView;

namespace ui {
class NativeTheme;
}  // namespace ui

namespace version_info {
enum class Channel;
}

namespace views {
class ExternalFocusTracker;
class WebView;
}  // namespace views

namespace webapps {
enum class InstallableWebAppCheckResult;
struct WebAppBannerData;
}  // namespace webapps

namespace enterprise_watermark {
class WatermarkView;
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
                    public content::WebContentsObserver,
                    public views::ClientView,
                    public infobars::InfoBarContainer::Delegate,
                    public ExclusiveAccessContext,
                    public ExclusiveAccessBubbleViewsContext,
                    public extensions::ExtensionKeybindingRegistry::Delegate,
                    public ImmersiveModeController::Observer,
                    public webapps::AppBannerManager::Observer {
  METADATA_HEADER(BrowserView, views::ClientView)

 public:
  explicit BrowserView(std::unique_ptr<Browser> browser);
  BrowserView(const BrowserView&) = delete;
  BrowserView& operator=(const BrowserView&) = delete;
  ~BrowserView() override;

  void set_frame(BrowserFrame* frame) {
    frame_ = frame;
    paint_as_active_subscription_ =
        frame_->RegisterPaintAsActiveChangedCallback(base::BindRepeating(
            &BrowserView::PaintAsActiveChanged, base::Unretained(this)));
  }
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

  // After calling RevealTabStripIfNeeded(), there is normally a delay before
  // the tabstrip is hidden. Tests can use this function to disable that delay
  // (and hide immediately).
  static void SetDisableRevealerDelayForTesting(bool disable);

  bool IsLoadingAnimationRunningForTesting() const;

  // Returns a Browser instance of this view.
  Browser* browser() { return browser_.get(); }
  const Browser* browser() const { return browser_.get(); }

  const TopControlsSlideController* top_controls_slide_controller() const {
    return top_controls_slide_controller_.get();
  }

  void SetDownloadShelfForTest(DownloadShelf* download_shelf);

  // Returns the constraining bounding box that should be used to lay out the
  // FindBar within. This is _not_ the size of the find bar, just the bounding
  // box it should be laid out within. The coordinate system of the returned
  // rect is in the coordinate system of the frame, since the FindBar is a child
  // window.
  gfx::Rect GetFindBarBoundingBox() const;

  // Returns the preferred height of the TabStrip. Used to position the
  // incognito avatar icon.
  int GetTabStripHeight() const;

  // Returns the preferred size of the Web App Frame Toolbar. Used for example
  // to determine the height of the title bar.
  // Returns an empty size if this browser is not for a web app.
  gfx::Size GetWebAppFrameToolbarPreferredSize() const;

  // Container for the tabstrip, toolbar, etc.
  TopContainerView* top_container() { return top_container_; }

#if BUILDFLAG(IS_MAC)
  views::Widget* overlay_widget() { return overlay_widget_.get(); }
  views::View* overlay_view() { return overlay_view_.get(); }
  views::Widget* tab_overlay_widget() { return tab_overlay_widget_.get(); }
  views::View* tab_overlay_view() { return tab_overlay_view_.get(); }

  // Returns if this browser view will use immersive fullscreen mode, based
  // on the state of the two relevant base::Features, as well as the type of
  // browser this is a view for.
  bool UsesImmersiveFullscreenMode() const;

  // Returns if this browser view will use immersive fullscreen tabbed mode.
  // In tabbed mode the tab strip is contained within the window's titlebar. In
  // non-tabbed mode the tab strip is positioned below the titlebar.
  // The return value is determined based on the state of
  // `features::kImmersiveFullscreen` and `features::kImmersiveFullscreenTabs`
  // as well as the type of browser.
  bool UsesImmersiveFullscreenTabbedMode() const;
#endif

  // Container for the web contents.
  views::View* contents_container() { return contents_container_; }

  SidePanel* unified_side_panel() { return unified_side_panel_; }

  void set_contents_border_widget(views::Widget* contents_border_widget) {
    GetBrowserViewLayout()->set_contents_border_widget(contents_border_widget);
  }
  views::Widget* contents_border_widget() {
    return GetBrowserViewLayout()->contents_border_widget();
  }
  void SetContentBorderBounds(
      const std::optional<gfx::Rect>& region_capture_rect) {
    GetBrowserViewLayout()->SetContentBorderBounds(region_capture_rect);
  }

  TabStripRegionView* tab_strip_region_view() const {
    return tab_strip_region_view_;
  }

  // Accessor for the TabStrip.
  TabStrip* tabstrip() { return tabstrip_; }
  const TabStrip* tabstrip() const { return tabstrip_; }

  // Accessor for the WebUI tab strip.
  WebUITabStripContainerView* webui_tab_strip() { return webui_tab_strip_; }

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

  // Accessor for the contents and devtools WebViews.
  ContentsWebView* contents_web_view() { return contents_web_view_; }
  views::WebView* devtools_web_view() { return devtools_web_view_; }

  base::WeakPtr<BrowserView> GetAsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Accessor for the BrowserView's TabSearchBubbleHost instance.
  TabSearchBubbleHost* GetTabSearchBubbleHost();

  // Returns true if the top UI are visible on screen.
  bool GetTabStripVisible() const;

  // Returns true if the top UI should be drawn.
  // On macOS, it is possible that the top UI is drawn but hidden.
  bool ShouldDrawTabStrip() const;

  // Returns true if the profile associated with this Browser window is
  // incognito.
  bool GetIncognito() const;

  // Returns true if the profile associated with this Browser window is
  // a guest session.
  bool GetGuestSession() const;

  // Returns true if the profile associated with this Browser window is
  // not incognito or a guest session.
  bool GetRegularOrGuestSession() const;

  // Provides the containing frame with the accelerator for the specified
  // command id. This can be used to provide menu item shortcut hints etc.
  // Returns true if an accelerator was found for the specified |cmd_id|, false
  // otherwise.
  bool GetAccelerator(int cmd_id, ui::Accelerator* accelerator) const;

  // Returns true if the specificed |accelerator| is registered with this view.
  bool IsAcceleratorRegistered(const ui::Accelerator& accelerator);

  // Returns the active WebContents.
  content::WebContents* GetActiveWebContents();

  // Returns true if the Browser object associated with this BrowserView
  // supports tabs, such as all normal browsers, and tabbed apps like terminal.
  bool GetSupportsTabStrip() const;

  // Returns true if the Browser object associated with this BrowserView is a
  // normal window (i.e. a browser window, not an app or popup).
  bool GetIsNormalType() const;

  // Returns true if the Browser object associated with this BrowserView is a
  // for an installed web app.
  bool GetIsWebAppType() const;

  // Returns true if the Browser object associated with this BrowserView is for
  // a Picture in Picture window.
  bool GetIsPictureInPictureType() const;

  // Returns the document picture in picture options from |browser_|'s
  // CreateParams.
  std::optional<blink::mojom::PictureInPictureWindowOptions>
  GetDocumentPictureInPictureOptions() const;

  // Returns true if the top browser controls (a.k.a. top-chrome UIs) are
  // allowed to slide up and down with the gesture scrolls on the current tab's
  // page.
  bool GetTopControlsSlideBehaviorEnabled() const;

#if BUILDFLAG(IS_WIN)
  // Returns whether the browser can ever display a titlebar. Used in Windows
  // touch mode. Possibly expand to ChromeOS if we add a titlebar back there in
  // touch mode.
  bool GetSupportsTitle() const;

  // Returns whether the browser can ever display a window icon.
  bool GetSupportsIcon() const;
#endif

  // Returns the current shown ratio of the top browser controls.
  float GetTopControlsSlideBehaviorShownRatio() const;

  // Returns the widget for anchoring bubbles and dialogs.
  // This returns BrowserFrame except on fullscreen macOS where the toolbar is
  // hosted in an OverlayWidget.
  views::Widget* GetWidgetForAnchoring();

  // See ImmersiveModeController for description.
  ImmersiveModeController* immersive_mode_controller() const {
    return immersive_mode_controller_.get();
  }

  // Returns true if the view has been initialized.
  bool initialized() const { return initialized_; }

  // Restores the focused view. This is also used to set the initial focus
  // when a new browser window is created.
  void RestoreFocus();

  // Called during the widget's fullscreen state changes without going through
  // FullscreenController. This method does any processing which was skipped.
  void FullscreenStateChanging();

  // Called after the widget's fullscreen state is changed without going through
  // FullscreenController. This method does any processing which was skipped.
  void FullscreenStateChanged();

  // Sets the button provider for this BrowserView. Must be called before
  // InitViews() which sets the ToolbarView as the default button provider.
  void SetToolbarButtonProvider(ToolbarButtonProvider* provider);
  ToolbarButtonProvider* toolbar_button_provider() {
    return toolbar_button_provider_;
  }

  // Callback for listening for link-opening-from-gesture events (i.e. only
  // those resulting from direct user action).
  using OnLinkOpeningFromGestureCallback =
      base::RepeatingCallback<void(WindowOpenDisposition)>;
  using OnLinkOpeningFromGestureCallbackList =
      base::RepeatingCallbackList<OnLinkOpeningFromGestureCallback::RunType>;

  // Listens to the "link opened from gesture" event. Callback will be called
  // when a link is opened from user interaction in the same browser window, but
  // before the tabstrip is actually modified. Useful for doing certain types
  // of animations (e.g. "flying link" animation in tablet mode).
  base::CallbackListSubscription AddOnLinkOpeningFromGestureCallback(
      OnLinkOpeningFromGestureCallback callback);

  // Updates the variable keeping track of the borderless mode visibility, which
  // together with the `window_management_permission_granted_` controls whether
  // the title bar is shown or not.
  void UpdateBorderlessModeEnabled();

  // Returns true when an app's effective display mode is
  // window-controls-overlay.
  bool AppUsesWindowControlsOverlay() const;

  // Returns true when an app's effective display mode is borderless.
  bool AppUsesBorderlessMode() const;

  // Returns whether any of the features enabling draggable regions is enabled.
  bool AreDraggableRegionsEnabled() const;

  // Returns true when the window controls overlay should be displayed instead
  // of a full titlebar. This is only supported for desktop web apps.
  bool IsWindowControlsOverlayEnabled() const;

  // Enable or disable the window controls overlay and notify the browser frame
  // view of the update.
  void ToggleWindowControlsOverlayEnabled(base::OnceClosure done);

  bool WidgetOwnedByAnchorContainsPoint(
      const gfx::Point& point_in_browser_view_coords);

  bool borderless_mode_enabled_for_testing() const {
    return borderless_mode_enabled_;
  }

  bool window_management_permission_granted_for_testing() const {
    return window_management_permission_granted_;
  }

  // Update the side panel's horizontal alignment when
  // prefs::kSidePanelHorizontalAlignment is changed from the appearance
  // settings page.
  void UpdateSidePanelHorizontalAlignment();

  void UpdateWebAppStatusIconsVisiblity();

  // Getter for the `window.setResizable(bool)` state.
  std::optional<bool> GetCanResizeFromWebAPI() const;

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
  ui::ZOrderLevel GetZOrderLevel() const override;
  void SetZOrderLevel(ui::ZOrderLevel order) override;
  gfx::NativeWindow GetNativeWindow() const override;
  bool IsOnCurrentWorkspace() const override;
  void SetTopControlsShownRatio(content::WebContents* web_contents,
                                float ratio) override;
  bool DoBrowserControlsShrinkRendererSize(
      const content::WebContents* contents) const override;
  ui::NativeTheme* GetNativeTheme() override;
  const ui::ThemeProvider* GetThemeProvider() const override;
  const ui::ColorProvider* GetColorProvider() const override;
  ui::ElementContext GetElementContext() override;
  int GetTopControlsHeight() const override;
  void SetTopControlsGestureScrollInProgress(bool in_progress) override;
  StatusBubble* GetStatusBubble() override;
  void UpdateTitleBar() override;
  void BookmarkBarStateChanged(
      BookmarkBar::AnimateChangeType change_type) override;
  void TemporarilyShowBookmarkBar(base::TimeDelta duration) override;
  void UpdateDevTools() override;
  void UpdateLoadingAnimations(bool is_visible) override;
  void SetStarredState(bool is_starred) override;
  void SetTranslateIconToggled(bool is_lit) override;
  void OnActiveTabChanged(content::WebContents* old_contents,
                          content::WebContents* new_contents,
                          int index,
                          int reason) override;
  void OnTabDetached(content::WebContents* contents, bool was_active) override;
  void ZoomChangedForActiveTab(bool can_show_bubble) override;
  gfx::Rect GetRestoredBounds() const override;
  ui::mojom::WindowShowState GetRestoredState() const override;
  gfx::Rect GetBounds() const override;
  gfx::Size GetContentsSize() const override;
  void SetContentsSize(const gfx::Size& size) override;
  bool IsMaximized() const override;
  bool IsMinimized() const override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  void OnCanResizeFromWebAPIChanged() override;
  bool GetCanResize() override;
  ui::mojom::WindowShowState GetWindowShowState() const override;
  void EnterFullscreen(const GURL& url,
                       ExclusiveAccessBubbleType bubble_type,
                       int64_t display_id) override;
  void ExitFullscreen() override;
  void UpdateExclusiveAccessBubble(
      const ExclusiveAccessBubbleParams& params,
      ExclusiveAccessBubbleHideCallback first_hide_callback) override;
  bool IsExclusiveAccessBubbleDisplayed() const override;
  void OnExclusiveAccessUserInput() override;
  bool ShouldHideUIForFullscreen() const override;
  bool IsFullscreen() const override;
  bool IsFullscreenBubbleVisible() const override;
  bool IsForceFullscreen() const override;
  void SetForceFullscreen(bool force_fullscreen) override;
  void UpdatePageActionIcon(PageActionIconType type) override;
  autofill::AutofillBubbleHandler* GetAutofillBubbleHandler() override;
  void ExecutePageActionIconForTesting(PageActionIconType type) override;
  LocationBar* GetLocationBar() const override;
  void SetFocusToLocationBar(bool is_user_initiated) override;
  void UpdateReloadStopState(bool is_loading, bool force) override;
  void UpdateToolbar(content::WebContents* contents) override;
  bool UpdateToolbarSecurityState() override;
  void UpdateCustomTabBarVisibility(bool visible, bool animate) override;
  void ResetToolbarTabState(content::WebContents* contents) override;
  void FocusToolbar() override;
  ExtensionsContainer* GetExtensionsContainer() override;
  void ToolbarSizeChanged(bool is_animating) override;
  void TabDraggingStatusChanged(bool is_dragging) override;
  void LinkOpeningFromGesture(WindowOpenDisposition disposition) override;
  void FocusAppMenu() override;
  void FocusBookmarksToolbar() override;
  void FocusInactivePopupForAccessibility() override;
  void RotatePaneFocus(bool forwards) override;
  void FocusWebContentsPane() override;
  void DestroyBrowser() override;
  bool IsBookmarkBarVisible() const override;
  bool IsBookmarkBarAnimating() const override;
  bool IsTabStripEditable() const override;
  bool IsToolbarVisible() const override;
  bool IsToolbarShowing() const override;
  bool IsLocationBarVisible() const override;
  bool IsBorderlessModeEnabled() const override;
  void ShowChromeLabs() override;
  views::WebView* GetContentsWebView() override;
  BrowserView* AsBrowserView() override;
  SharingDialog* ShowSharingDialog(content::WebContents* contents,
                                   SharingDialogData data) override;
  void ShowUpdateChromeDialog() override;
  void ShowIntentPickerBubble(
      std::vector<IntentPickerBubbleView::AppInfo> app_info,
      bool show_stay_in_chrome,
      bool show_remember_selection,
      apps::IntentPickerBubbleType bubble_type,
      const std::optional<url::Origin>& initiating_origin,
      IntentPickerResponse callback) override;
  void ShowBookmarkBubble(const GURL& url, bool already_bookmarked) override;
  sharing_hub::ScreenshotCapturedBubble* ShowScreenshotCapturedBubble(
      content::WebContents* contents,
      const gfx::Image& image) override;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // TODO(crbug.com/339262105): Clean up the old password promo methods after
  // the generic promo launch.
  void VerifyUserEligibilityIOSPasswordPromoBubble() override;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  qrcode_generator::QRCodeGeneratorBubbleView* ShowQRCodeGeneratorBubble(
      content::WebContents* contents,
      const GURL& url,
      bool show_back_button) override;
  send_tab_to_self::SendTabToSelfBubbleView*
  ShowSendTabToSelfDevicePickerBubble(content::WebContents* contents) override;
  send_tab_to_self::SendTabToSelfBubbleView* ShowSendTabToSelfPromoBubble(
      content::WebContents* contents,
      bool show_signin_button) override;
#if BUILDFLAG(IS_CHROMEOS)
  views::Button* GetSharingHubIconButton() override;
  void ToggleMultitaskMenu() const override;
#else
  sharing_hub::SharingHubBubbleView* ShowSharingHubBubble(
      share::ShareAttempt attempt) override;
#endif  // BUILDFLAG(IS_CHROMEOS)
  ShowTranslateBubbleResult ShowTranslateBubble(
      content::WebContents* contents,
      translate::TranslateStep step,
      const std::string& source_language,
      const std::string& target_language,
      translate::TranslateErrors error_type,
      bool is_user_gesture) override;
  void StartPartialTranslate(const std::string& source_language,
                             const std::string& target_language,
                             const std::u16string& text_selection) override;
  void ShowOneClickSigninConfirmation(
      const std::u16string& email,
      base::OnceCallback<void(bool)> confirmed_callback) override;
  // TODO(beng): Not an override, move somewhere else.
  void SetDownloadShelfVisible(bool visible);
  bool IsDownloadShelfVisible() const override;
  DownloadShelf* GetDownloadShelf() override;
  views::View* GetTopContainer() override;
  DownloadBubbleUIController* GetDownloadBubbleUIController() override;
  void ConfirmBrowserCloseWithPendingDownloads(
      int download_count,
      Browser::DownloadCloseType dialog_type,
      base::OnceCallback<void(bool)> callback) override;
  void UserChangedTheme(BrowserThemeChangeType theme_change_type) override;
  void ShowAppMenu() override;
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      const input::NativeWebKeyboardEvent& event) override;
  bool HandleKeyboardEvent(const input::NativeWebKeyboardEvent& event) override;
  std::unique_ptr<FindBar> CreateFindBar() override;
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;
  void ShowAvatarBubbleFromAvatarButton(bool is_source_accelerator) override;
  void MaybeShowProfileSwitchIPH() override;
  void ShowHatsDialog(
      const std::string& site_id,
      const std::optional<std::string>& hats_histogram_name,
      const std::optional<uint64_t> hats_survey_ukm_id,
      base::OnceClosure success_callback,
      base::OnceClosure failure_callback,
      const SurveyBitsData& product_specific_bits_data,
      const SurveyStringData& product_specific_string_data) override;
  ExclusiveAccessContext* GetExclusiveAccessContext() override;
  std::string GetWorkspace() const override;
  bool IsVisibleOnAllWorkspaces() const override;
  void HideDownloadShelf();
  void UnhideDownloadShelf();

  void ShowEmojiPanel() override;
  void ShowCaretBrowsingDialog() override;

  std::unique_ptr<content::EyeDropper> OpenEyeDropper(
      content::RenderFrameHost* frame,
      content::EyeDropperListener* listener) override;

  BookmarkBarView* GetBookmarkBarView() const;
  LocationBarView* GetLocationBarView() const;

  bool IsFeaturePromoActive(const base::Feature& iph_feature) const override;
  user_education::FeaturePromoResult CanShowFeaturePromo(
      const base::Feature& iph_feature) const override;
  void MaybeShowFeaturePromo(
      user_education::FeaturePromoParams params) override;
  bool MaybeShowStartupFeaturePromo(
      user_education::FeaturePromoParams params) override;
  bool AbortFeaturePromo(const base::Feature& iph_feature) override;
  user_education::FeaturePromoHandle CloseFeaturePromoAndContinue(
      const base::Feature& iph_feature) override;
  bool NotifyFeaturePromoFeatureUsed(
      const base::Feature& feature,
      FeaturePromoFeatureUsedAction action) override;
  void NotifyAdditionalConditionEvent(const char* event_name) override;
  user_education::DisplayNewBadge MaybeShowNewBadgeFor(
      const base::Feature& feature) override;
  void NotifyNewBadgeFeatureUsed(const base::Feature& feature) override;

  void ShowIncognitoClearBrowsingDataDialog() override;

  void ShowIncognitoHistoryDisclaimerDialog() override;

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
  bool CanFullscreen() const override;
  bool CanMaximize() const override;
  bool CanActivate() const override;
  std::u16string GetWindowTitle() const override;
  std::u16string GetAccessibleWindowTitle() const override;
  views::View* GetInitiallyFocusedView() override;
  bool ShouldShowWindowTitle() const override;
  bool ShouldShowWindowIcon() const override;
  ui::ImageModel GetWindowAppIcon() override;
  ui::ImageModel GetWindowIcon() override;
  bool ExecuteWindowsCommand(int command_id) override;
  std::string GetWindowName() const override;
  bool ShouldSaveWindowPlacement() const override;
  void SaveWindowPlacement(const gfx::Rect& bounds,
                           ui::mojom::WindowShowState show_state) override;
  bool GetSavedWindowPlacement(
      const views::Widget* widget,
      gfx::Rect* bounds,
      ui::mojom::WindowShowState* show_state) const override;
  views::View* GetContentsView() override;
  views::ClientView* CreateClientView(views::Widget* widget) override;
  views::View* CreateOverlayView() override;
#if BUILDFLAG(IS_MAC)
  views::View* CreateMacOverlayView();
#endif
  void OnWindowBeginUserBoundsChange() override;
  void OnWindowEndUserBoundsChange() override;
  void OnWidgetMove() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;
  void GetAccessiblePanes(std::vector<View*>* panes) override;
  bool ShouldDescendIntoChildForEventHandling(
      gfx::NativeView child,
      const gfx::Point& location) override;
  bool RotatePaneFocusFromView(views::View* focused_view,
                               bool forward,
                               bool enable_wrapping) override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;
  void OnWidgetShowStateChanged(views::Widget* widget) override;

  // content::WebContentsObserver:
  void DidFirstVisuallyNonEmptyPaint() override;

  // views::ClientView:
  views::CloseRequestResult OnWindowCloseRequested() override;
  int NonClientHitTest(const gfx::Point& point) override;
  gfx::Size GetMinimumSize() const override;

  // infobars::InfoBarContainer::Delegate:
  void InfoBarContainerStateChanged(bool is_animating) override;

  // views::View:
  void Layout(PassKey) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override;
  void AddedToWidget() override;
  void PaintChildren(const views::PaintInfo& paint_info) override;
  void OnThemeChanged() override;
  bool GetDropFormats(int* formats,
                      std::set<ui::ClipboardFormatType>* format_types) override;
  bool AreDropTypesRequired() override;
  bool CanDrop(const ui::OSExchangeData& data) override;
  void OnDragEntered(const ui::DropTargetEvent& event) override;

  // ui::AcceleratorTarget:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

  // ExclusiveAccessContext:
  Profile* GetProfile() override;
  void UpdateUIForTabFullscreen() override;
  content::WebContents* GetWebContentsForExclusiveAccess() override;
  bool CanUserExitFullscreen() const override;

  // ExclusiveAccessBubbleViewsContext:
  ExclusiveAccessManager* GetExclusiveAccessManager() override;
  ui::AcceleratorProvider* GetAcceleratorProvider() override;
  gfx::NativeView GetBubbleParentView() const override;
  gfx::Rect GetClientAreaBoundsInScreen() const override;
  bool IsImmersiveModeEnabled() const override;
  gfx::Rect GetTopContainerBoundsInScreen() override;
  void DestroyAnyExclusiveAccessBubble() override;

  // extension::ExtensionKeybindingRegistry::Delegate:
  content::WebContents* GetWebContentsForExtension() override;

  // ImmersiveModeController::Observer:
  void OnImmersiveRevealStarted() override;
  void OnImmersiveRevealEnded() override;
  void OnImmersiveFullscreenExited() override;
  void OnImmersiveModeControllerDestroyed() override;

  // webapps::AppBannerManager::Observer:
  void OnInstallableWebAppStatusUpdated(
      webapps::InstallableWebAppCheckResult result,
      const std::optional<webapps::WebAppBannerData>& data) override;

  // Creates an accessible tab label for screen readers that includes the tab
  // status for the given tab index. This takes the form of
  // "Page title - Tab state". The optional parameter `is_for_tab` can be set
  // when getting the label for a tab (instead of a window). Titles for the
  // window don't include less important messages like memory usage.
  std::u16string GetAccessibleTabLabel(int index,
                                       bool is_for_tab = false) const;

  // Testing interface:
  views::View* GetContentsContainerForTest() { return contents_container_; }
  views::WebView* GetDevToolsWebViewForTest() { return devtools_web_view_; }
  FullscreenControlHost* fullscreen_control_host_for_test() {
    return fullscreen_control_host_.get();
  }
  views::View* GetSidePanelRoundedCornerForTesting() {
    return side_panel_rounded_corner_;
  }

  // Returns all the NativeViewHosts attached to this BrowserView which should
  // be transformed as part of the TopControlsSlide behavior with touch scroll
  // gestures. These NativeViewHosts include the one hosting the active tab's\
  // WebContents, and the one hosting the webui tabstrip contents (if the
  // feature is enabled).
  std::vector<views::NativeViewHost*> GetNativeViewHostsForTopControlsSlide()
      const;

  void CreateTabSearchBubble(
      int tab_index = -1,
      tab_search::mojom::TabOrganizationFeature organization_feature =
          tab_search::mojom::TabOrganizationFeature::kNone) override;
  // Closes the tab search bubble if open for the given browser instance.
  void CloseTabSearchBubble() override;

  AccessibilityFocusHighlight* GetAccessibilityFocusHighlightForTesting() {
    return accessibility_focus_highlight_.get();
  }

  bool should_show_window_controls_overlay_toggle() const {
    return should_show_window_controls_overlay_toggle_;
  }

  void SetLoadingAnimationStateChangeClosureForTesting(
      base::OnceClosure closure);

  WebAppFrameToolbarView* web_app_frame_toolbar_for_testing() {
    return web_app_frame_toolbar();
  }

  enterprise_watermark::WatermarkView* get_watermark_view_for_testing() {
    return watermark_view_;
  }

  // This value is used in a common calculation in NonClientFrameView
  // subclasses. This must be added to the origin of the first painted pixel of
  // NonClientFrameView to get the correct offset. See
  // TopContainerBackground::PaintThemeCustomImage for details.
  gfx::Point GetThemeOffsetFromBrowserView() const;

  void ApplyWatermarkSettings(const std::string& watermark_text);

#if BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)
  void ApplyScreenshotSettings(bool allow);
#endif

  // Clipboard commands.
  void Cut();
  void Copy();
  void Paste();

 protected:
  // Enumerates where the devtools are docked relative to the browser's main
  // web contents.
  enum class DevToolsDockedPlacement {
    kLeft,
    kRight,
    kBottom,
    // Devtools are not docked.
    kNone,
    kUnknown
  };

  DevToolsDockedPlacement devtools_docked_placement() const {
    return current_devtools_docked_placement_;
  }

  // Return the DevTools docked placement. It infers the docked placement from
  // the bounds of contents_webview relative to the local bounds of the
  // container that holds both contents_webview and devtools_webview.
  static DevToolsDockedPlacement GetDevToolsDockedPlacement(
      const gfx::Rect& contents_webview_bounds,
      const gfx::Rect& local_webview_container_bounds);

 private:
  // Do not friend BrowserViewLayout. Use the BrowserViewLayoutDelegate
  // interface to keep these two classes decoupled and testable.
  friend class BrowserViewLayoutDelegateImpl;
  friend class TopControlsSlideControllerTest;
  FRIEND_TEST_ALL_PREFIXES(BrowserViewTest, BrowserView);
  FRIEND_TEST_ALL_PREFIXES(BrowserViewTest, AccessibleWindowTitle);
  class AccessibilityModeObserver;

  // BrowserUserEducationInterface private methods:
  BrowserFeaturePromoController* GetFeaturePromoControllerImpl() override;

  // Shared implementation by cut, copy and paste.
  void CutCopyPaste(int command_id);

  // Toggles the look and feel of the browser. If we are in the standard view
  // and this function is called we will swap the layout to the compact version.
  // Vice versa if we start in compact mode.
  void ToggleCompactModeUI();

  // If the browser is in immersive full screen mode, it will reveal the
  // tabstrip for a short duration. This is useful for shortcuts that perform
  // tab navigations and need to give users a visual clue as to what tabs are
  // affected.
  void RevealTabStripIfNeeded();

  // Make sure the WebUI tab strip exists if it should.
  void MaybeInitializeWebUITabStrip();

  // Callback for the loading animation(s) associated with this view.
  void LoadingAnimationCallback();

#if BUILDFLAG(IS_WIN)
  // Creates the JumpList.
  void CreateJumpList();
#endif

  // Returns the BrowserViewLayout.
  BrowserViewLayout* GetBrowserViewLayout() const;

  // Returns the ContentsLayoutManager.
  ContentsLayoutManager* GetContentsLayoutManager() const;

  // Prepare to show the Bookmark Bar for the specified WebContents.
  // Returns true if the Bookmark Bar can be shown (i.e. it's supported for this
  // Browser type) and there should be a subsequent re-layout to show it.
  // |contents| can be null.
  bool MaybeShowBookmarkBar(content::WebContents* contents);

  // Prepare to show an Info Bar for the specified WebContents. Returns
  // true if there is an Info Bar to show and one is supported for this Browser
  // type, and there should be a subsequent re-layout to show it.
  // |contents| can be null.
  bool MaybeShowInfoBar(content::WebContents* contents);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // TODO(crbug.com/339262105): Clean up the old password promo methods after
  // the generic promo launch.
  // Decides whether to show the iOS Password Promo Bubble based on segmentation
  // platform classification results (is passed as a callback to the
  // segmentation API).
  void MaybeShowIOSPasswordPromoBubble(
      const segmentation_platform::ClassificationResult& result);
  // TODO(crbug.com/339262105): Clean up the old password promo methods after
  // the generic promo launch.
  // Shows the iOS Password Promo Bubble.
  void ShowIOSPasswordPromoBubble();
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

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

  // Invoked to prepare the transition of fullscreen state.
  // If features::kAsyncFullscreenWindowState is disabled, this is invoked
  // synchronously when requesting platform window state changes.
  // If features::kAsyncFullscreenWindowState is enabled, this is invoked
  // asynchronously when the platform window state change is observed.
  // TODO(crbug.com/40276379): Deprecate the synchronous codepath.
  void PrepareFullscreen(bool fullscreen);

  // Invoked to update the necessary things when our fullscreen state changes
  // to |fullscreen|. On Windows this is invoked immediately when we toggle the
  // full screen node_data. On Linux changing the fullscreen node_data is async,
  // so we
  // ask the window to change its fullscreen node_data, then when we get
  // notification that it succeeded this method is invoked.
  // If the Window Placement experiment is enabled, fullscreen may be requested
  // on a particular display. In that case, |display_id| is the display's id;
  // otherwise, display::kInvalidDisplayId indicates no display is specified.
  void ProcessFullscreen(bool fullscreen,
                         int64_t display_id);

  // Request the underlying platform to make the window fullscreen.
  void RequestFullscreen(bool fullscreen, int64_t display_id);

  void SynchronizeRenderWidgetHostVisualPropertiesForMainFrame();
  void NotifyWidgetSizeConstraintsChanged();

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

  // Shows the next app-modal dialog box, if there is one to be shown, or moves
  // an existing showing one to the front.
  void ActivateAppModalDialog() const;

  // Retrieves the chrome command id associated with |accelerator|. The function
  // returns false if |accelerator| is unknown. Otherwise |command_id| will be
  // set to the chrome command id defined in //chrome/app/chrome_command_ids.h.
  bool FindCommandIdForAccelerator(const ui::Accelerator& accelerator,
                                   int* command_id) const;

  // Updates AppBannerManager::Observer to observe |new_manager| exclusively.
  void ObserveAppBannerManager(webapps::AppBannerManager* new_manager);

  // Called by GetAccessibleWindowTitle, split out to make it testable.
  std::u16string GetAccessibleWindowTitleForChannelAndProfile(
      version_info::Channel,
      Profile* profile) const;

  // Reparents |top_container_| to be a child of |this| instead of
  // |overlay_view_|.
  void ReparentTopContainerForEndOfImmersive();

  // Ensures that the correct focus order is set for child views, regardless of
  // the actual child order.
  void EnsureFocusOrder();

  // Returns true when the window icon of this browser window can change based
  // on the context. GetWindowIcon() method should return the same image if
  // this returns false.
  bool CanChangeWindowIcon() const;

  // Searches for inactive bubbles anchored to elements in this browser view
  // and activates them. It returns true if it succeeded activating a bubble or
  // false otherwise.
  bool ActivateFirstInactiveBubbleForAccessibility();

  // Notifies that window bounds changed to extensions if needed.
  void TryNotifyWindowBoundsChanged(const gfx::Rect& widget_bounds);

  // Called when ui::TouchUiController changes the current touch mode.
  void TouchModeChanged();

  // Attempts to show in-product help for the WebUI tab strip. Should be
  // called when the IPH backend is initialized or whenever the touch
  // mode changes.
  void MaybeShowWebUITabStripIPH();

  // Attempts to show in-product help for the reading list as moved into the
  // side panel. Should be called when the IPH backend is initialized or
  // whenever the touch mode changes.
  void MaybeShowReadingListInSidePanelIPH();

  // Attempts to show IPH promo for experimental AI.
  void MaybeShowExperimentalAIIPH();

  void UpdateWindowControlsOverlayEnabled();

  // `window.setResizable(bool)` API (part of Additional Windowing Controls)
  // can block the use of APIs resizing the window, such as `resizeTo` and
  // `resizeBy`. window.moveTo | window.moveBy should not be blocked.
  bool CanSetBounds(const gfx::Rect& new_bounds);

  // Updates the visibility of the Window Controls Overlay toggle button.
  void UpdateWindowControlsOverlayToggleVisible();

  // Updates the variable keeping track of the Window Management permission,
  // which together with borderless_mode_enabled_ controls whether the title bar
  // is shown or not.
  void UpdateWindowManagementPermission(blink::mojom::PermissionStatus status);

  // Sets the callback which is called when the status of the Window Management
  // permission changes.
  void SetWindowManagementPermissionSubscriptionForBorderlessMode(
      content::WebContents* web_contents);

  WebAppFrameToolbarView* web_app_frame_toolbar();
  const WebAppFrameToolbarView* web_app_frame_toolbar() const;

  void PaintAsActiveChanged();
  void FrameColorsChanged();

  // |allowed_without_policy| represents whether or not the browser is allowed
  // to enter fullscreen, irrespective of policy. This is is necessary to
  // prevent policy from incorrectly allowing the browser to enter fullscreen
  // when it should not be able to.
  void UpdateFullscreenAllowedFromPolicy(bool allowed_without_policy);

  // The BrowserFrame that hosts this view.
  raw_ptr<BrowserFrame> frame_ = nullptr;

  // The Browser object we are associated with.
  std::unique_ptr<Browser> browser_;

  // BrowserView layout (LTR one is pictured here).
  //
  // --------------------------------------------------------------------
  // | TopContainerView (top_container_)                                |
  // |  --------------------------------------------------------------  |
  // |  | Web App toolbar and title (web_app_frame_toolbar_)         |  |
  // |  |------------------------------------------------------------|  |
  // |  | Tabs (tabstrip_)                                           |  |
  // |  |------------------------------------------------------------|  |
  // |  | Navigation buttons, address bar, menu (toolbar_)           |  |
  // |  --------------------------------------------------------------  |
  // |------------------------------------------------------------------|
  // | Bookmarks (bookmark_bar_view_)                                   |
  // |------------------------------------------------------------------|
  // | All infobars (infobar_container_)                                |
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

  // The view that manages the tab strip, toolbar, and sometimes the bookmark
  // bar. Stacked top in the view hiearachy so it can be used to slide out
  // the top views in immersive fullscreen.
  raw_ptr<TopContainerView> top_container_ = nullptr;

  // Menu button and page status icons. Only used by web-app windows.
  raw_ptr<WebAppFrameToolbarView> web_app_frame_toolbar_ = nullptr;

  // Normally the BrowserNonClientFrameView is responsible for rendering the
  // title of a window when appropriate. However for web applications the title
  // needs to be more integrated with other UI components part of BrowserView,
  // so have a title Label for them here.
  raw_ptr<views::Label> web_app_window_title_ = nullptr;

  // The view that contains the tabstrip, new tab button, and grab handle space.
  raw_ptr<TabStripRegionView> tab_strip_region_view_ = nullptr;

  // The TabStrip.
  raw_ptr<TabStrip> tabstrip_ = nullptr;

  // the webui based tabstrip, when applicable. see https://crbug.com/989131.
  raw_ptr<WebUITabStripContainerView> webui_tab_strip_ = nullptr;

  // Allows us to react to changes in accessibility mode.
  // TODO(dfried): this is only used to disable WebUI tabstrip (see above) while
  // that mode has accessibile mode issues (e.g. crbug.com/1136185,
  // crbug.com/1136236). Having an observer object allows for the browser to
  // change mode if it enters or leaves accessibility mode.
  std::unique_ptr<AccessibilityModeObserver> accessibility_mode_observer_;

  // The Toolbar containing the navigation buttons, menus and the address bar.
  raw_ptr<ToolbarView> toolbar_ = nullptr;

  // The OverlayView for the widget, which is used to host `top_container_`
  // during immersive reveal.
  // On Aura, this view is owned by the browser frame. On mac, this view is
  // owned by `overlay_widget_`.
  raw_ptr<views::View, DanglingUntriaged> overlay_view_ = nullptr;

#if BUILDFLAG(IS_MAC)
  // Used when calling CreateMacOverlayView(). This widget owns `overlay_view_`.
  // Its content NSView will be reparented to a NSToolbarFullScreenWindow
  // during fullscreen.
  raw_ptr<views::Widget, DanglingUntriaged> overlay_widget_ = nullptr;

  // Also used when calling CreateMacOverlayView(). This widget will host the
  // tabstrip contents. Its content NSView will be reparented to a separate
  // section of the NSToolbarFullScreenWindow allowing for the tabs to live in
  // the Titlebar.
  raw_ptr<views::Widget, DanglingUntriaged> tab_overlay_widget_ = nullptr;

  // The hosting view of TabStripRegionView during immersive fullscreen.
  raw_ptr<views::View, DanglingUntriaged> tab_overlay_view_ = nullptr;

#endif

  // The Bookmark Bar View for this window. Lazily created. May be null for
  // non-tabbed browsers like popups. May not be visible.
  std::unique_ptr<BookmarkBarView> bookmark_bar_view_;

  // Separator between top container and contents.
  raw_ptr<views::View> contents_separator_ = nullptr;

  // Loading bar (part of top container for / WebUI tab strip).
  raw_ptr<TopContainerLoadingBar> loading_bar_ = nullptr;

  // The do-nothing view which controls the z-order of the find bar widget
  // relative to views which paint into layers and views with an associated
  // NativeView.
  raw_ptr<View> find_bar_host_view_ = nullptr;

  // The download shelf.
  raw_ptr<DownloadShelf> download_shelf_ = nullptr;

  // The InfoBarContainerView that contains InfoBars for the current tab.
  raw_ptr<InfoBarContainerView> infobar_container_ = nullptr;

  // The view that contains the selected WebContents.
  raw_ptr<ContentsWebView> contents_web_view_ = nullptr;

  // The view that contains devtools window for the selected WebContents.
  raw_ptr<views::WebView> devtools_web_view_ = nullptr;

  // The view that overlays a watermark on the contents container.
  raw_ptr<enterprise_watermark::WatermarkView> watermark_view_ = nullptr;

  // The view managing the devtools and contents positions.
  // Handled by ContentsLayoutManager.
  raw_ptr<views::View> contents_container_ = nullptr;

  // The side panel aligned to the left or the right side of the browser window
  // depending on the kSidePanelHorizontalAlignment pref's value.
  // Conceptually this member should exist if and only if the
  // side_panel_coordinator is created.
  raw_ptr<SidePanel> unified_side_panel_ = nullptr;
  raw_ptr<views::View> right_aligned_side_panel_separator_ = nullptr;
  raw_ptr<views::View> left_aligned_side_panel_separator_ = nullptr;
  raw_ptr<views::View> side_panel_rounded_corner_ = nullptr;

  // Provides access to the toolbar buttons this browser view uses. Buttons may
  // appear in a hosted app frame or in a tabbed UI toolbar.
  raw_ptr<ToolbarButtonProvider> toolbar_button_provider_ = nullptr;

  // The handler responsible for showing autofill bubbles.
  std::unique_ptr<autofill::AutofillBubbleHandler> autofill_bubble_handler_;

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

  // True if (as of the last time it was checked) the frame type is native.
  bool using_native_frame_ = true;

  // True when in ProcessFullscreen(). The flag is used to avoid reentrance and
  // to ignore requests to layout while in ProcessFullscreen() to reduce
  // jankiness.
  bool in_process_fullscreen_ = false;

  std::unique_ptr<ExclusiveAccessBubbleViews> exclusive_access_bubble_;
  // Tracks the task to asynchronously destroy the exclusive access bubble.
  base::CancelableTaskTracker exclusive_access_bubble_cancelable_task_tracker_;
  std::optional<base::CancelableTaskTracker::TaskId>
      exclusive_access_bubble_destruction_task_id_;

  // True when we do not want to allow exiting fullscreen, e.g. in Chrome OS
  // Kiosk session.
  bool force_fullscreen_ = false;

  // The timer used to update frames for tab-loading animations.
  base::RepeatingTimer loading_animation_timer_;

  // Closure invoked when the state of the loading animation changes.
  base::OnceClosure loading_animation_state_change_closure_;

  // Start timestamp for all throbbers. Set when |loading_animation_timer_|
  // starts and used for all consecutive tabs (while any are loading) to keep
  // throbbers in sync.
  base::TimeTicks loading_animation_start_;

  base::OneShotTimer temporary_bookmark_bar_timer_;

  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  // Whether OnWidgetActivationChanged should RestoreFocus. If this is set and
  // is true, OnWidgetActivationChanged will call RestoreFocus. This is set
  // to true when not set in Show() so that RestoreFocus on activation only
  // happens for very first Show() calls.
  std::optional<bool> restore_focus_on_activation_;

  // This is non-null on Chrome OS only.
  std::unique_ptr<TopControlsSlideController> top_controls_slide_controller_;

  // Used to allow a single layout operation once the top controls slide
  // behavior starts. This needed since sliding the top controls and the page
  // contents is done using layer transform. A layout operation while sliding is
  // in progress might break the view, and will make sliding feel janky.
  // A single layout is needed right before sliding begins. (See
  // TopControlsSlideControllerChromeOS::OnBeginSliding()).
  bool did_first_layout_while_top_controls_are_sliding_ = false;

  std::unique_ptr<ImmersiveModeController> immersive_mode_controller_;

  base::CallbackListSubscription subscription_ =
      ui::TouchUiController::Get()->RegisterCallback(
          base::BindRepeating(&BrowserView::TouchModeChanged,
                              base::Unretained(this)));

  std::unique_ptr<WebContentsCloseHandler> web_contents_close_handler_;

  // The class that registers for keyboard shortcuts for extension commands.
  std::unique_ptr<ExtensionKeybindingRegistryViews>
      extension_keybinding_registry_;

  std::unique_ptr<FullscreenControlHost> fullscreen_control_host_;

  // If the Window Placement experiment is enabled and fullscreen is requested
  // on a particular display, this closure will be called after fullscreen is
  // exited to restore the original pre-fullscreen bounds of the window.
  base::OnceClosure restore_pre_fullscreen_bounds_callback_;

  base::ScopedObservation<webapps::AppBannerManager,
                          webapps::AppBannerManager::Observer>
      app_banner_manager_observation_{this};

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};

  bool interactive_resize_in_progress_ = false;

  // The last bounds we notified about in TryNotifyWindowBoundsChanged().
  gfx::Rect last_widget_bounds_;

  std::unique_ptr<AccessibilityFocusHighlight> accessibility_focus_highlight_;

  std::unique_ptr<BrowserFeaturePromoController> feature_promo_controller_ =
      nullptr;

  OnLinkOpeningFromGestureCallbackList link_opened_from_gesture_callbacks_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // |loading_animation_tracker_| is used to measure animation smoothness for
  // tab loading animation.
  std::optional<ui::ThroughputTracker> loading_animation_tracker_;
#endif

  bool window_controls_overlay_enabled_ = false;
  bool should_show_window_controls_overlay_toggle_ = false;
  bool borderless_mode_enabled_ = false;
  bool window_management_permission_granted_ = false;
  std::optional<content::PermissionController::SubscriptionId>
      window_management_subscription_id_;

  // Caching the last value of `PageData::can_resize_` that has been notified to
  // the WidgetObservers to avoid notifying them when nothing has changed.
  std::optional<bool> cached_can_resize_from_web_api_;

  base::CallbackListSubscription paint_as_active_subscription_;

  DevToolsDockedPlacement current_devtools_docked_placement_ =
      DevToolsDockedPlacement::kNone;

  PrefChangeRegistrar registrar_;

  ui::OmniboxPopupCloser omnibox_popup_closer_{this};

  mutable base::WeakPtrFactory<BrowserView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_VIEW_H_
