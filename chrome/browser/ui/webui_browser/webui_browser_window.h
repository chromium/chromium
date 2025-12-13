// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_BROWSER_WINDOW_H_
#define CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_BROWSER_WINDOW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_key.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_source.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace ui {
class TrackedElement;
}  // namespace ui

namespace views {
class NativeWidget;
class WebView;
class Widget;
}  // namespace views

class Browser;
class WebUIBrowserExtensionsContainer;
class WebUIBrowserModalDialogHost;
class WebUIBrowserSidePanelUI;
class WebUIBrowserUI;
class WebUIBrowserWebContentsDelegate;
class WebUILocationBar;

// A BrowserWindow implementation that uses WebUI for its primary UI. It still
// uses views::Widget for windowing management.
class WebUIBrowserWindow : public BrowserWindow,
                           public ui::ColorProviderSource,
                           public ui::AcceleratorProvider,
                           public ui::AcceleratorTarget,
                           public views::WidgetObserver {
 public:
  explicit WebUIBrowserWindow(Browser* browser);
  ~WebUIBrowserWindow() override;

  // Returns the containing browser window for a WebContents that hosts
  // WebShell.
  static WebUIBrowserWindow* FromWebShellWebContents(
      content::WebContents* web_contents);

  // Returns the WebUIBrowserWindow for a BrowserWindowInterface. If browser
  // does not use WebUIBrowserWindow, returns nullptr.
  static WebUIBrowserWindow* FromBrowser(BrowserWindowInterface* browser);

  // Returns the WebUIBrowserWindow for the given `window`.
  static WebUIBrowserWindow* FromNativeWindow(gfx::NativeWindow window);

  // BrowserWindow:
  gfx::NativeWindow GetNativeWindow() const override;
  bool IsOnCurrentWorkspace() const override;
  bool IsVisibleOnScreen() const override;
  void SetTopControlsShownRatio(content::WebContents* web_contents,
                                float ratio) override;
  bool DoBrowserControlsShrinkRendererSize(
      const content::WebContents* contents) const override;
  ui::NativeTheme* GetNativeTheme() override;
  const ui::ThemeProvider* GetThemeProvider() const override;
  int GetTopControlsHeight() const override;
  void SetTopControlsGestureScrollInProgress(bool in_progress) override;
  std::vector<StatusBubble*> GetStatusBubbles() override;
  void UpdateTitleBar() override;
  void BookmarkBarStateChanged(
      BookmarkBar::AnimateChangeType change_type) override;
  void TemporarilyShowBookmarkBar(base::TimeDelta duration) override;
  void UpdateDevTools(content::WebContents* inspected_web_contents) override;
  bool CanDockDevTools() const override;
  void UpdateLoadingAnimations(bool is_visible) override;
  void SetStarredState(bool is_starred) override;
  bool IsTabModalPopupDeprecated() const override;
  void SetIsTabModalPopupDeprecated(
      bool is_tab_modal_popup_deprecated) override;
  void OnActiveTabChanged(content::WebContents* old_contents,
                          content::WebContents* new_contents,
                          int index,
                          int reason) override;
  void OnTabDetached(content::WebContents* contents, bool was_active) override;
  void ZoomChangedForActiveTab(bool can_show_bubble) override;
  bool ShouldHideUIForFullscreen() const override;
  bool IsFullscreenBubbleVisible() const override;
  bool IsForceFullscreen() const override;
  void SetForceFullscreen(bool force_fullscreen) override;
  gfx::Size GetContentsSize() const override;
  void SetContentsSize(const gfx::Size& size) override;
  void UpdatePageActionIcon(PageActionIconType type) override;
  autofill::AutofillBubbleHandler* GetAutofillBubbleHandler() override;
  void ExecutePageActionIconForTesting(PageActionIconType type) override;
  LocationBar* GetLocationBar() const override;
  void SetFocusToLocationBar(bool is_user_initiated) override;
  void UpdateReloadStopState(bool is_loading, bool force) override;
  void UpdateToolbar(content::WebContents* contents) override;
  bool UpdateToolbarSecurityState() override;
  void UpdateCustomTabBarVisibility(bool visible, bool animate) override;
  void SetDevToolsScrimVisibility(bool visible) override;
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
  bool IsBookmarkBarVisible() const override;
  bool IsBookmarkBarAnimating() const override;
  bool IsTabStripEditable() const override;
  void SetTabStripNotEditableForTesting() override;
  bool IsToolbarVisible() const override;
  bool IsToolbarShowing() const override;
  bool IsLocationBarVisible() const override;
  SharingDialog* ShowSharingDialog(content::WebContents* contents,
                                   SharingDialogData data) override;
  void ShowUpdateChromeDialog() override;
  void ShowIntentPickerBubble(
      std::vector<apps::IntentPickerAppInfo> app_info,
      bool show_stay_in_chrome,
      bool show_remember_selection,
      apps::IntentPickerBubbleType bubble_type,
      const std::optional<url::Origin>& initiating_origin,
      IntentPickerResponse callback) override;
  void ShowBookmarkBubble(const GURL& url, bool already_bookmarked) override;
  sharing_hub::ScreenshotCapturedBubble* ShowScreenshotCapturedBubble(
      content::WebContents* contents,
      const gfx::Image& image) override;
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
  void ToggleMultitaskMenu() override;
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
  DownloadBubbleUIController* GetDownloadBubbleUIController() override;
  void ConfirmBrowserCloseWithPendingDownloads(
      int download_count,
      Browser::DownloadCloseType dialog_type,
      base::OnceCallback<void(bool)> callback) override;
  void UserChangedTheme(BrowserThemeChangeType theme_change_type) override;
  void ShowAppMenu() override;
  void PreHandleDragUpdate(const content::DropData& drop_data,
                           const gfx::PointF& point) override;
  void PreHandleDragExit() override;
  void HandleDragEnded() override;
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      const input::NativeWebKeyboardEvent& event) override;
  bool HandleKeyboardEvent(const input::NativeWebKeyboardEvent& event) override;
  std::unique_ptr<FindBar> CreateFindBar() override;
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHostFor(
      content::WebContents* web_contents) override;
  void ShowAvatarBubbleFromAvatarButton(bool is_source_accelerator) override;
  void MaybeShowProfileSwitchIPH() override;
  void MaybeShowSupervisedUserProfileSignInIPH() override;
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
  void ShowEmojiPanel() override;
  std::unique_ptr<content::EyeDropper> OpenEyeDropper(
      content::RenderFrameHost* frame,
      content::EyeDropperListener* listener) override;
  void ShowCaretBrowsingDialog() override;
  void CreateTabSearchBubble(
      tab_search::mojom::TabSearchSection section,
      tab_search::mojom::TabOrganizationFeature organization_feature) override;
  void CloseTabSearchBubble() override;
  void ShowIncognitoClearBrowsingDataDialog() override;
  void ShowIncognitoHistoryDisclaimerDialog() override;
  bool IsBorderlessModeEnabled() const override;
  void OnWebApiWindowResizableChanged() override;
  bool GetCanResize() override;
  ui::mojom::WindowShowState GetWindowShowState() const override;
  void ShowChromeLabs() override;
  BrowserView* AsBrowserView() override;

  // ui::BaseWindow:
  void Show() override;
  void ShowInactive() override;
  void Hide() override;
  bool IsVisible() const override;
  void SetBounds(const gfx::Rect& bounds) override;
  void Close() override;
  void Activate() override;
  void Deactivate() override;
  bool IsActive() const override;
  gfx::Rect GetBounds() const override;
  bool IsMaximized() const override;
  bool IsMinimized() const override;
  bool IsFullscreen() const override;  // Also in ExclusiveAccessContext.
  gfx::Rect GetRestoredBounds() const override;
  ui::mojom::WindowShowState GetRestoredState() const override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  void FlashFrame(bool flash) override;
  ui::ZOrderLevel GetZOrderLevel() const override;
  void SetZOrderLevel(ui::ZOrderLevel order) override;

  // ui::ColorProviderSource:
  const ui::ColorProvider* GetColorProvider() const override;
  ui::ColorProviderKey GetColorProviderKey() const override;
  ui::RendererColorMap GetRendererColorMap(
      ui::ColorProviderKey::ColorMode color_mode,
      ui::ColorProviderKey::ForcedColors forced_colors) const override;

  // ui::AcceleratorProvider:
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;

  // views::WidgetObserver:
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;

  void ShowSidePanel(SidePanelEntryKey side_panel_entry_key);
  void CloseSidePanel();

  WebUIBrowserUI* GetWebUIBrowserUI() const;
  WebUIBrowserSidePanelUI* GetWebUIBrowserSidePanelUI();

  Browser* browser() { return browser_.get(); }
  views::Widget* widget() { return widget_.get(); }

  gfx::Rect GetContentsBoundsInScreen() const;
  ui::TrackedElement* GetExtensionsMenuButtonAnchor() const;

 protected:
  // BrowserWindow:
  void DeleteBrowserWindow() final;

 private:
  class WidgetDelegate;
  friend class WebUIBrowserExclusiveAccessContext;

  // Called by ExclusiveAccessContext to enter or exit fullscreen.
  void ProcessFullscreen(bool fullscreen);

  // Creates and returns the native widget.
  // Note that this class uses CLIENT_OWNS_WIDGET ownership model whereby
  // the NativeWidget owns itself (i.e. NativeWidget*::WindowDestroyed() frees
  // itself) so this method returns a pointer rather than a unique_ptr.
  views::NativeWidget* CreateNativeWidget();

  // ui::AcceleratorTarget:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;

  // Retrieves the Chrome command ID associated with |accelerator|. The function
  // returns false if |accelerator| is unknown. Otherwise |command_id| will be
  // set to the Chrome command ID defined in //chrome/app/chrome_command_ids.h.
  bool FindCommandIdForAccelerator(const ui::Accelerator& accelerator,
                                   int* command_id) const;

  // Load accelerators into |accelerator_table_| and |accelerator_manager_|.
  void LoadAccelerators();

  // Returns the appropriate ThemeInitializerSupplier based on the window type.
  ui::ColorProviderKey::ThemeInitializerSupplier* GetThemeInitializerSupplier()
      const;

  void OnWindowCloseRequested(views::Widget::ClosedReason close_reason);

  const raw_ptr<Browser> browser_;
  std::unique_ptr<WebUIBrowserWebContentsDelegate> web_contents_delegate_;
  std::unique_ptr<WidgetDelegate> widget_delegate_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::WebView> web_view_ = nullptr;
  std::unique_ptr<WebUILocationBar> location_bar_;

  // A mapping between accelerators and Chrome command IDs as defined in
  // //chrome/app/chrome_command_ids.h.
  std::map<ui::Accelerator, int> accelerator_table_;
  ui::AcceleratorManager accelerator_manager_;

  std::unique_ptr<WebUIBrowserModalDialogHost> modal_dialog_host_;
  std::unique_ptr<WebUIBrowserExtensionsContainer> extensions_container_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_BROWSER_WINDOW_H_
