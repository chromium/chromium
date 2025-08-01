// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui_browser/webui_browser_window.h"

#include "base/notimplemented.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/webui_browser/webui_location_bar.h"
#include "chrome/common/webui_url_constants.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/sharing_message/sharing_dialog_data.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"

namespace {

// Copied from chrome/browser/ui/views/frame/browser_frame.cc.
bool IsUsingLinuxSystemTheme(Profile* profile) {
#if BUILDFLAG(IS_LINUX)
  return ThemeServiceFactory::GetForProfile(profile)->UsingSystemTheme();
#else
  return false;
#endif
}

}  // namespace

WebUIBrowserWindow::WebUIBrowserWindow(std::unique_ptr<Browser> browser)
    : browser_(std::move(browser)) {
  location_bar_ = std::make_unique<WebUILocationBar>(browser_.get());
  widget_ = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  params.name = "WebUIBrowserWindow";
  params.bounds = gfx::Rect(0, 0, 800, 600);
  widget_->Init(std::move(params));
  auto web_view = std::make_unique<views::WebView>(browser_->profile());

  web_view->LoadInitialURL(GURL(chrome::kChromeUIWebuiBrowserURL));
  // Sets the weview as the content view of the default ClientView.
  // TODO(webium): make a subclass of ClientView so that non-client hit testing
  // can be customized.
  web_view_ = widget_->SetClientContentsView(std::move(web_view));

  widget_->Show();
}

WebUIBrowserWindow::~WebUIBrowserWindow() = default;

void WebUIBrowserWindow::Show() {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::ShowInactive() {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::Hide() {
  NOTIMPLEMENTED();
}

bool WebUIBrowserWindow::IsVisible() const {
  NOTIMPLEMENTED();
  return false;
}

void WebUIBrowserWindow::SetBounds(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::Close() {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::Activate() {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::Deactivate() {
  NOTIMPLEMENTED();
}

bool WebUIBrowserWindow::IsActive() const {
  NOTIMPLEMENTED();
  return false;
}

void WebUIBrowserWindow::FlashFrame(bool flash) {
  NOTIMPLEMENTED();
}

ui::ZOrderLevel WebUIBrowserWindow::GetZOrderLevel() const {
  NOTIMPLEMENTED();
  return ui::ZOrderLevel::kNormal;
}

void WebUIBrowserWindow::SetZOrderLevel(ui::ZOrderLevel order) {
  NOTIMPLEMENTED();
}

gfx::NativeWindow WebUIBrowserWindow::GetNativeWindow() const {
  NOTIMPLEMENTED();
  return gfx::NativeWindow();
}

bool WebUIBrowserWindow::IsOnCurrentWorkspace() const {
  NOTIMPLEMENTED();
  return false;
}

bool WebUIBrowserWindow::IsVisibleOnScreen() const {
  NOTIMPLEMENTED();
  return false;
}

void WebUIBrowserWindow::SetTopControlsShownRatio(
    content::WebContents* web_contents,
    float ratio) {
  NOTIMPLEMENTED();
}

bool WebUIBrowserWindow::DoBrowserControlsShrinkRendererSize(
    const content::WebContents* contents) const {
  NOTIMPLEMENTED();
  return false;
}

ui::NativeTheme* WebUIBrowserWindow::GetNativeTheme() {
  NOTIMPLEMENTED();
  return nullptr;
}

const ui::ThemeProvider* WebUIBrowserWindow::GetThemeProvider() const {
  // Copied from BrowserFrame::GetThemeProvider().
  auto* app_controller = browser_->app_controller();
  // Ignore the system theme for web apps with window-controls-overlay as the
  // display_override so the web contents can blend with the overlay by using
  // the developer-provided theme color for a better experience. Context:
  // https://crbug.com/1219073.
  if (app_controller && (!IsUsingLinuxSystemTheme(browser_->profile()) ||
                         app_controller->AppUsesWindowControlsOverlay())) {
    return app_controller->GetThemeProvider();
  }
  return &ThemeService::GetThemeProviderForProfile(browser_->profile());
}

const ui::ColorProvider* WebUIBrowserWindow::GetColorProvider() const {
  return ui::ColorProviderManager::Get().GetColorProviderFor(
      GetColorProviderKey());
}

ui::ColorProviderKey WebUIBrowserWindow::GetColorProviderKey() const {
  return ui::NativeTheme::GetInstanceForNativeUi()->GetColorProviderKey(
      nullptr);
}

ui::RendererColorMap WebUIBrowserWindow::GetRendererColorMap(
    ui::ColorProviderKey::ColorMode color_mode,
    ui::ColorProviderKey::ForcedColors forced_colors) const {
  auto key = GetColorProviderKey();
  key.color_mode = color_mode;
  key.forced_colors = forced_colors;
  ui::ColorProvider* color_provider =
      ui::ColorProviderManager::Get().GetColorProviderFor(key);
  CHECK(color_provider);
  return ui::CreateRendererColorMap(*color_provider);
}

ui::ElementContext WebUIBrowserWindow::GetElementContext() {
  NOTIMPLEMENTED();
  return ui::ElementContext();
}

int WebUIBrowserWindow::GetTopControlsHeight() const {
  NOTIMPLEMENTED();
  return 0;
}

void WebUIBrowserWindow::SetTopControlsGestureScrollInProgress(
    bool in_progress) {
  NOTIMPLEMENTED();
}

std::vector<StatusBubble*> WebUIBrowserWindow::GetStatusBubbles() {
  NOTIMPLEMENTED();
  return {};
}

void WebUIBrowserWindow::UpdateTitleBar() {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::BookmarkBarStateChanged(
    BookmarkBar::AnimateChangeType change_type) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::TemporarilyShowBookmarkBar(base::TimeDelta duration) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::UpdateDevTools() {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::UpdateLoadingAnimations(bool is_visible) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::SetStarredState(bool is_starred) {
  NOTIMPLEMENTED();
}

bool WebUIBrowserWindow::IsTabModalPopupDeprecated() const {
  NOTIMPLEMENTED();
  return false;
}

void WebUIBrowserWindow::SetIsTabModalPopupDeprecated(
    bool is_tab_modal_popup_deprecated) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::OnActiveTabChanged(content::WebContents* old_contents,
                                            content::WebContents* new_contents,
                                            int index,
                                            int reason) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::OnTabDetached(content::WebContents* contents,
                                       bool was_active) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::ZoomChangedForActiveTab(bool can_show_bubble) {
  NOTIMPLEMENTED();
}

bool WebUIBrowserWindow::ShouldHideUIForFullscreen() const {
  NOTIMPLEMENTED();
  return false;
}

bool WebUIBrowserWindow::IsFullscreenBubbleVisible() const {
  NOTIMPLEMENTED();
  return false;
}

bool WebUIBrowserWindow::IsForceFullscreen() const {
  NOTIMPLEMENTED();
  return false;
}

void WebUIBrowserWindow::SetForceFullscreen(bool force_fullscreen) {
  NOTIMPLEMENTED();
}

gfx::Size WebUIBrowserWindow::GetContentsSize() const {
  NOTIMPLEMENTED();
  return gfx::Size();
}

void WebUIBrowserWindow::SetContentsSize(const gfx::Size& size) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::UpdatePageActionIcon(PageActionIconType type) {
  NOTIMPLEMENTED();
}

autofill::AutofillBubbleHandler*
WebUIBrowserWindow::GetAutofillBubbleHandler() {
  NOTIMPLEMENTED();
  return nullptr;
}

void WebUIBrowserWindow::ExecutePageActionIconForTesting(
    PageActionIconType type) {
  NOTIMPLEMENTED();
}

LocationBar* WebUIBrowserWindow::GetLocationBar() const {
  return location_bar_.get();
}

void WebUIBrowserWindow::SetFocusToLocationBar(bool select_all) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::UpdateReloadStopState(bool is_loading, bool force) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::UpdateToolbar(content::WebContents* contents) {
  NOTIMPLEMENTED();
}

bool WebUIBrowserWindow::UpdateToolbarSecurityState() {
  NOTIMPLEMENTED();
  return false;
}

void WebUIBrowserWindow::UpdateCustomTabBarVisibility(bool visible,
                                                      bool animate) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::SetContentScrimVisibility(bool visible) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::SetDevToolsScrimVisibility(bool visible) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::ResetToolbarTabState(content::WebContents* contents) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::FocusToolbar() {
  NOTIMPLEMENTED();
}

ExtensionsContainer* WebUIBrowserWindow::GetExtensionsContainer() {
  NOTIMPLEMENTED();
  return nullptr;
}

void WebUIBrowserWindow::ToolbarSizeChanged(bool is_animating) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::TabDraggingStatusChanged(bool is_dragging) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::LinkOpeningFromGesture(
    WindowOpenDisposition disposition) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::FocusAppMenu() {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::FocusBookmarksToolbar() {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::FocusInactivePopupForAccessibility() {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::RotatePaneFocus(bool forwards) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::FocusWebContentsPane() {
  NOTIMPLEMENTED();
}

bool WebUIBrowserWindow::IsBookmarkBarVisible() const {
  NOTIMPLEMENTED();
  return false;
}

bool WebUIBrowserWindow::IsBookmarkBarAnimating() const {
  NOTIMPLEMENTED();
  return false;
}

bool WebUIBrowserWindow::IsTabStripEditable() const {
  NOTIMPLEMENTED();
  return false;
}

void WebUIBrowserWindow::SetTabStripNotEditableForTesting() {
  NOTIMPLEMENTED();
}

bool WebUIBrowserWindow::IsToolbarVisible() const {
  NOTIMPLEMENTED();
  return false;
}

bool WebUIBrowserWindow::IsToolbarShowing() const {
  NOTIMPLEMENTED();
  return false;
}

bool WebUIBrowserWindow::IsLocationBarVisible() const {
  NOTIMPLEMENTED();
  return false;
}

SharingDialog* WebUIBrowserWindow::ShowSharingDialog(
    content::WebContents* contents,
    SharingDialogData data) {
  NOTIMPLEMENTED();
  return nullptr;
}

void WebUIBrowserWindow::ShowUpdateChromeDialog() {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::ShowIntentPickerBubble(
    std::vector<apps::IntentPickerAppInfo> app_info,
    bool show_stay_in_chrome,
    bool show_remember_selection,
    apps::IntentPickerBubbleType bubble_type,
    const std::optional<url::Origin>& initiating_origin,
    IntentPickerResponse callback) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::ShowBookmarkBubble(const GURL& url,
                                            bool already_bookmarked) {
  NOTIMPLEMENTED();
}

sharing_hub::ScreenshotCapturedBubble*
WebUIBrowserWindow::ShowScreenshotCapturedBubble(content::WebContents* contents,
                                                 const gfx::Image& image) {
  NOTIMPLEMENTED();
  return nullptr;
}

qrcode_generator::QRCodeGeneratorBubbleView*
WebUIBrowserWindow::ShowQRCodeGeneratorBubble(content::WebContents* contents,
                                              const GURL& url,
                                              bool show_back_button) {
  NOTIMPLEMENTED();
  return nullptr;
}

send_tab_to_self::SendTabToSelfBubbleView*
WebUIBrowserWindow::ShowSendTabToSelfDevicePickerBubble(
    content::WebContents* contents) {
  NOTIMPLEMENTED();
  return nullptr;
}

send_tab_to_self::SendTabToSelfBubbleView*
WebUIBrowserWindow::ShowSendTabToSelfPromoBubble(content::WebContents* contents,
                                                 bool show_signin_button) {
  NOTIMPLEMENTED();
  return nullptr;
}

#if BUILDFLAG(IS_CHROMEOS)
views::Button* WebUIBrowserWindow::GetSharingHubIconButton() {
  NOTIMPLEMENTED();
  return nullptr;
}

void WebUIBrowserWindow::ToggleMultitaskMenu() const {
  NOTIMPLEMENTED();
}
#else
sharing_hub::SharingHubBubbleView* WebUIBrowserWindow::ShowSharingHubBubble(
    share::ShareAttempt attempt) {
  NOTIMPLEMENTED();
  return nullptr;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

ShowTranslateBubbleResult WebUIBrowserWindow::ShowTranslateBubble(
    content::WebContents* contents,
    translate::TranslateStep step,
    const std::string& source_language,
    const std::string& target_language,
    translate::TranslateErrors error_type,
    bool is_user_gesture) {
  NOTIMPLEMENTED();
  return ShowTranslateBubbleResult::BROWSER_WINDOW_NOT_VALID;
}

void WebUIBrowserWindow::StartPartialTranslate(
    const std::string& source_language,
    const std::string& target_language,
    const std::u16string& text_selection) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::ShowOneClickSigninConfirmation(
    const std::u16string& email,
    base::OnceCallback<void(bool)> confirmed_callback) {
  NOTIMPLEMENTED();
}

views::View* WebUIBrowserWindow::GetTopContainer() {
  NOTIMPLEMENTED();
  return nullptr;
}

views::View* WebUIBrowserWindow::GetLensOverlayView() {
  NOTIMPLEMENTED();
  return nullptr;
}

DownloadBubbleUIController*
WebUIBrowserWindow::GetDownloadBubbleUIController() {
  NOTIMPLEMENTED();
  return nullptr;
}

void WebUIBrowserWindow::ConfirmBrowserCloseWithPendingDownloads(
    int download_count,
    Browser::DownloadCloseType dialog_type,
    base::OnceCallback<void(bool)> callback) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::UserChangedTheme(
    BrowserThemeChangeType theme_change_type) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::ShowAppMenu() {
  NOTIMPLEMENTED();
}

bool WebUIBrowserWindow::PreHandleMouseEvent(
    const blink::WebMouseEvent& event) {
  NOTIMPLEMENTED();
  return false;
}

void WebUIBrowserWindow::PreHandleDragUpdate(const content::DropData& drop_data,
                                             const gfx::PointF& point) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::PreHandleDragExit() {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::HandleDragEnded() {
  NOTIMPLEMENTED();
}

content::KeyboardEventProcessingResult
WebUIBrowserWindow::PreHandleKeyboardEvent(
    const input::NativeWebKeyboardEvent& event) {
  NOTIMPLEMENTED();
  return content::KeyboardEventProcessingResult::NOT_HANDLED;
}

bool WebUIBrowserWindow::HandleKeyboardEvent(
    const input::NativeWebKeyboardEvent& event) {
  NOTIMPLEMENTED();
  return false;
}

std::unique_ptr<FindBar> WebUIBrowserWindow::CreateFindBar() {
  NOTIMPLEMENTED();
  return nullptr;
}

web_modal::WebContentsModalDialogHost*
WebUIBrowserWindow::GetWebContentsModalDialogHost() {
  NOTIMPLEMENTED();
  return nullptr;
}

void WebUIBrowserWindow::ShowAvatarBubbleFromAvatarButton(
    bool is_source_accelerator) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::MaybeShowProfileSwitchIPH() {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::MaybeShowSupervisedUserProfileSignInIPH() {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::ShowHatsDialog(
    const std::string& site_id,
    const std::optional<std::string>& hats_histogram_name,
    const std::optional<uint64_t> hats_survey_ukm_id,
    base::OnceClosure success_callback,
    base::OnceClosure failure_callback,
    const SurveyBitsData& product_specific_bits_data,
    const SurveyStringData& product_specific_string_data) {
  NOTIMPLEMENTED();
}

ExclusiveAccessContext* WebUIBrowserWindow::GetExclusiveAccessContext() {
  NOTIMPLEMENTED();
  return nullptr;
}

std::string WebUIBrowserWindow::GetWorkspace() const {
  NOTIMPLEMENTED();
  return std::string();
}

bool WebUIBrowserWindow::IsVisibleOnAllWorkspaces() const {
  NOTIMPLEMENTED();
  return false;
}

void WebUIBrowserWindow::ShowEmojiPanel() {
  NOTIMPLEMENTED();
}

std::unique_ptr<content::EyeDropper> WebUIBrowserWindow::OpenEyeDropper(
    content::RenderFrameHost* frame,
    content::EyeDropperListener* listener) {
  NOTIMPLEMENTED();
  return nullptr;
}

void WebUIBrowserWindow::ShowCaretBrowsingDialog() {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::CreateTabSearchBubble(
    tab_search::mojom::TabSearchSection section,
    tab_search::mojom::TabOrganizationFeature organization_feature) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::CloseTabSearchBubble() {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::ShowIncognitoClearBrowsingDataDialog() {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::ShowIncognitoHistoryDisclaimerDialog() {
  NOTIMPLEMENTED();
}

bool WebUIBrowserWindow::IsBorderlessModeEnabled() const {
  NOTIMPLEMENTED();
  return false;
}

void WebUIBrowserWindow::OnWebApiWindowResizableChanged() {
  NOTIMPLEMENTED();
}

bool WebUIBrowserWindow::GetCanResize() {
  NOTIMPLEMENTED();
  return false;
}

ui::mojom::WindowShowState WebUIBrowserWindow::GetWindowShowState() const {
  NOTIMPLEMENTED();
  return ui::mojom::WindowShowState::kDefault;
}

void WebUIBrowserWindow::ShowChromeLabs() {
  NOTIMPLEMENTED();
}

views::WebView* WebUIBrowserWindow::GetContentsWebView() {
  NOTIMPLEMENTED();
  return nullptr;
}

BrowserView* WebUIBrowserWindow::AsBrowserView() {
  NOTIMPLEMENTED();
  return nullptr;
}

gfx::Rect WebUIBrowserWindow::GetBounds() const {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

bool WebUIBrowserWindow::IsMaximized() const {
  NOTIMPLEMENTED();
  return false;
}

bool WebUIBrowserWindow::IsMinimized() const {
  NOTIMPLEMENTED();
  return false;
}

bool WebUIBrowserWindow::IsFullscreen() const {
  NOTIMPLEMENTED();
  return false;
}

gfx::Rect WebUIBrowserWindow::GetRestoredBounds() const {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

ui::mojom::WindowShowState WebUIBrowserWindow::GetRestoredState() const {
  NOTIMPLEMENTED();
  return ui::mojom::WindowShowState::kDefault;
}

void WebUIBrowserWindow::Maximize() {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::Minimize() {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::Restore() {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::DestroyBrowser() {
  NOTIMPLEMENTED();
}
