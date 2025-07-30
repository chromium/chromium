// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_BROWSER_WINDOW_H_
#define CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_BROWSER_WINDOW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_window.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_source.h"

namespace views {
class WebView;
class Widget;
}  // namespace views

class WebUILocationBar;
class Browser;

// A BrowserWindow implementation that uses WebUI for its primary UI. It still
// uses views::Widget for windowing management.
class WebUIBrowserWindow : public BrowserWindow,
                           public ui::ColorProviderSource {
 public:
  explicit WebUIBrowserWindow(std::unique_ptr<Browser> browser);
  ~WebUIBrowserWindow() override;

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
  ui::ElementContext GetElementContext() override;
  int GetTopControlsHeight() const override;
  void SetTopControlsGestureScrollInProgress(bool in_progress) override;
  std::vector<StatusBubble*> GetStatusBubbles() override;
  void UpdateTitleBar() override;
  void BookmarkBarStateChanged(
      BookmarkBar::AnimateChangeType change_type) override;
  void TemporarilyShowBookmarkBar(base::TimeDelta duration) override;
  void UpdateDevTools() override;
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
  void SetFocusToLocationBar(bool select_all) override;
  void UpdateReloadStopState(bool is_loading, bool force) override;
  void UpdateToolbar(content::WebContents* contents) override;
  bool UpdateToolbarSecurityState() override;
  void UpdateCustomTabBarVisibility(bool visible, bool animate) override;
  void SetContentScrimVisibility(bool visible) override;
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
  views::View* GetTopContainer() override;
  views::View* GetLensOverlayView() override;
  DownloadBubbleUIController* GetDownloadBubbleUIController() override;
  void ConfirmBrowserCloseWithPendingDownloads(
      int download_count,
      Browser::DownloadCloseType dialog_type,
      base::OnceCallback<void(bool)> callback) override;
  void UserChangedTheme(BrowserThemeChangeType theme_change_type) override;
  void ShowAppMenu() override;
  bool PreHandleMouseEvent(const blink::WebMouseEvent& event) override;
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
  views::WebView* GetContentsWebView() override;
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
  bool IsFullscreen() const override;
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

 protected:
  void DestroyBrowser() override;

 private:
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::WebView> web_view_ = nullptr;
  std::unique_ptr<WebUILocationBar> location_bar_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_BROWSER_WINDOW_H_
