// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/test_browser_window.h"

#include <utility>

#include "base/feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "components/sharing_message/sharing_dialog_data.h"
#include "components/user_education/common/new_badge/new_badge_controller.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_manager.h"
#include "ui/gfx/geometry/rect.h"

// Helpers --------------------------------------------------------------------

std::unique_ptr<Browser> CreateBrowserWithTestWindowForParams(
    Browser::CreateParams params) {
  DCHECK(!params.window);
  auto window = std::make_unique<TestBrowserWindow>();
  window->set_is_minimized(params.initial_show_state ==
                           ui::mojom::WindowShowState::kMinimized);
  // Tests generally expect TestBrowserWindows not to be active.
  window->set_is_active(
      params.initial_show_state != ui::mojom::WindowShowState::kInactive &&
      params.initial_show_state != ui::mojom::WindowShowState::kDefault &&
      params.initial_show_state != ui::mojom::WindowShowState::kMinimized);
  params.window = window.release();

  return Browser::DeprecatedCreateOwnedForTesting(params);
}

// TestBrowserWindow::TestLocationBar -----------------------------------------

OmniboxView* TestBrowserWindow::TestLocationBar::GetOmniboxView() {
  return nullptr;
}

OmniboxController* TestBrowserWindow::TestLocationBar::GetOmniboxController() {
  return nullptr;
}

LocationBarTesting*
    TestBrowserWindow::TestLocationBar::GetLocationBarForTesting() {
  return nullptr;
}

LocationBarModel* TestBrowserWindow::TestLocationBar::GetLocationBarModel() {
  return nullptr;
}

content::WebContents* TestBrowserWindow::TestLocationBar::GetWebContents() {
  return nullptr;
}

std::optional<bubble_anchor_util::AnchorConfiguration>
TestBrowserWindow::TestLocationBar::GetChipAnchor() {
  return {};
}

// TestBrowserWindow ----------------------------------------------------------

TestBrowserWindow::TestBrowserWindow() {
  // TestBrowserWindow will always be instantiated before its Browser.
  // TODO(crbug.com/413168662): This can be removed once Browser is updated to
  // always own its BrowserWindow.
  browser_list_observer_.Observe(BrowserList::GetInstance());
}

TestBrowserWindow::~TestBrowserWindow() {
  if (browser_) {
    // BrowserWindow implementations are expected to call
    // TearDownPreBrowserWindowDestruction() before destruction.
    browser_->GetFeatures().TearDownPreBrowserWindowDestruction();
    browser_ = nullptr;
  }
}

void TestBrowserWindow::Close() {
  if (close_callback_) {
    std::move(close_callback_).Run();
  }
  is_closed_ = true;
}

bool TestBrowserWindow::IsActive() const {
  return is_active_;
}

ui::ZOrderLevel TestBrowserWindow::GetZOrderLevel() const {
  return ui::ZOrderLevel::kNormal;
}

gfx::NativeWindow TestBrowserWindow::GetNativeWindow() const {
  return native_window_;
}

bool TestBrowserWindow::IsOnCurrentWorkspace() const {
  return true;
}

bool TestBrowserWindow::IsVisibleOnScreen() const {
  return true;
}

void TestBrowserWindow::SetTopControlsShownRatio(
    content::WebContents* web_contents,
    float ratio) {}

bool TestBrowserWindow::DoBrowserControlsShrinkRendererSize(
    const content::WebContents* contents) const {
  return false;
}

ui::NativeTheme* TestBrowserWindow::GetNativeTheme() {
  return nullptr;
}

const ui::ThemeProvider* TestBrowserWindow::GetThemeProvider() const {
  return nullptr;
}

const ui::ColorProvider* TestBrowserWindow::GetColorProvider() const {
  return ui::ColorProviderManager::Get().GetColorProviderFor(
      ui::ColorProviderKey());
}

int TestBrowserWindow::GetTopControlsHeight() const {
  return 0;
}

void TestBrowserWindow::SetTopControlsGestureScrollInProgress(
    bool in_progress) {}

std::vector<StatusBubble*> TestBrowserWindow::GetStatusBubbles() {
  return {};
}

bool TestBrowserWindow::CanDockDevTools() const {
  return true;
}

gfx::Rect TestBrowserWindow::GetRestoredBounds() const {
  return gfx::Rect();
}

ui::mojom::WindowShowState TestBrowserWindow::GetRestoredState() const {
  return ui::mojom::WindowShowState::kDefault;
}

gfx::Rect TestBrowserWindow::GetBounds() const {
  return gfx::Rect();
}

gfx::Size TestBrowserWindow::GetContentsSize() const {
  return gfx::Size();
}

void TestBrowserWindow::SetContentsSize(const gfx::Size& size) {}

bool TestBrowserWindow::IsMaximized() const {
  return false;
}

bool TestBrowserWindow::IsMinimized() const {
  return is_minimized_;
}

bool TestBrowserWindow::ShouldHideUIForFullscreen() const {
  return false;
}

bool TestBrowserWindow::GetCanResize() {
  return false;
}

ui::mojom::WindowShowState TestBrowserWindow::GetWindowShowState() const {
  return ui::mojom::WindowShowState::kDefault;
}

bool TestBrowserWindow::IsFullscreen() const {
  return false;
}

bool TestBrowserWindow::IsFullscreenBubbleVisible() const {
  return false;
}

bool TestBrowserWindow::IsForceFullscreen() const {
  return false;
}

bool TestBrowserWindow::UpdateToolbarSecurityState() {
  return false;
}

bool TestBrowserWindow::IsVisible() const {
  return true;
}

LocationBar* TestBrowserWindow::GetLocationBar() const {
  return const_cast<TestLocationBar*>(&location_bar_);
}

autofill::AutofillBubbleHandler* TestBrowserWindow::GetAutofillBubbleHandler() {
  return &autofill_bubble_handler_;
}

ExtensionsContainer* TestBrowserWindow::GetExtensionsContainer() {
  return nullptr;
}

content::KeyboardEventProcessingResult
TestBrowserWindow::PreHandleKeyboardEvent(
    const input::NativeWebKeyboardEvent& event) {
  return content::KeyboardEventProcessingResult::NOT_HANDLED;
}

bool TestBrowserWindow::HandleKeyboardEvent(
    const input::NativeWebKeyboardEvent& event) {
  return false;
}

bool TestBrowserWindow::IsBookmarkBarVisible() const {
  return false;
}

bool TestBrowserWindow::IsBookmarkBarAnimating() const {
  return false;
}

bool TestBrowserWindow::IsTabStripEditable() const {
  return is_tab_strip_editable_;
}

void TestBrowserWindow::SetTabStripNotEditableForTesting() {
  is_tab_strip_editable_ = false;
}

bool TestBrowserWindow::IsToolbarVisible() const {
  return false;
}

bool TestBrowserWindow::IsToolbarShowing() const {
  return false;
}

bool TestBrowserWindow::IsLocationBarVisible() const {
  return false;
}

bool TestBrowserWindow::IsBorderlessModeEnabled() const {
  return false;
}

BrowserView* TestBrowserWindow::AsBrowserView() {
  return nullptr;
}

void TestBrowserWindow::DeleteBrowserWindow() {
  delete this;
}

ShowTranslateBubbleResult TestBrowserWindow::ShowTranslateBubble(
    content::WebContents* contents,
    translate::TranslateStep step,
    const std::string& source_language,
    const std::string& target_language,
    translate::TranslateErrors error_type,
    bool is_user_gesture) {
  return ShowTranslateBubbleResult::kSuccess;
}

void TestBrowserWindow::StartPartialTranslate(
    const std::string& source_language,
    const std::string& target_language,
    const std::u16string& text_selection) {}

qrcode_generator::QRCodeGeneratorBubbleView*
TestBrowserWindow::ShowQRCodeGeneratorBubble(content::WebContents* contents,
                                             const GURL& url,
                                             bool show_back_button) {
  return nullptr;
}

SharingDialog* TestBrowserWindow::ShowSharingDialog(
    content::WebContents* web_contents,
    SharingDialogData data) {
  return nullptr;
}

#if !BUILDFLAG(IS_ANDROID)
sharing_hub::ScreenshotCapturedBubble*
TestBrowserWindow::ShowScreenshotCapturedBubble(content::WebContents* contents,
                                                const gfx::Image& image) {
  return nullptr;
}
#endif

send_tab_to_self::SendTabToSelfBubbleView*
TestBrowserWindow::ShowSendTabToSelfDevicePickerBubble(
    content::WebContents* contents) {
  return nullptr;
}

send_tab_to_self::SendTabToSelfBubbleView*
TestBrowserWindow::ShowSendTabToSelfPromoBubble(content::WebContents* contents,
                                                bool show_signin_button) {
  return nullptr;
}

#if BUILDFLAG(IS_CHROMEOS)
void TestBrowserWindow::ToggleMultitaskMenu() {
  return;
}
#else
sharing_hub::SharingHubBubbleView* TestBrowserWindow::ShowSharingHubBubble(
    share::ShareAttempt attempt) {
  return nullptr;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

DownloadBubbleUIController* TestBrowserWindow::GetDownloadBubbleUIController() {
  return nullptr;
}

std::unique_ptr<FindBar> TestBrowserWindow::CreateFindBar() {
  return nullptr;
}

web_modal::WebContentsModalDialogHost*
    TestBrowserWindow::GetWebContentsModalDialogHost() {
  return nullptr;
}

web_modal::WebContentsModalDialogHost*
TestBrowserWindow::GetWebContentsModalDialogHostFor(
    content::WebContents* web_contents) {
  return nullptr;
}

ExclusiveAccessContext* TestBrowserWindow::GetExclusiveAccessContext() {
  return nullptr;
}

std::string TestBrowserWindow::GetWorkspace() const {
  return workspace_;
}

bool TestBrowserWindow::IsVisibleOnAllWorkspaces() const {
  return visible_on_all_workspaces_;
}

std::unique_ptr<content::EyeDropper> TestBrowserWindow::OpenEyeDropper(
    content::RenderFrameHost* frame,
    content::EyeDropperListener* listener) {
  return nullptr;
}

void TestBrowserWindow::SetNativeWindow(gfx::NativeWindow window) {
  native_window_ = window;
}

void TestBrowserWindow::SetCloseCallback(base::OnceClosure close_callback) {
  close_callback_ = std::move(close_callback);
}

bool TestBrowserWindow::IsTabModalPopupDeprecated() const {
  return is_tab_modal_popup_deprecated_;
}

void TestBrowserWindow::SetIsTabModalPopupDeprecated(
    bool is_tab_modal_popup_deprecated) {
  is_tab_modal_popup_deprecated_ = is_tab_modal_popup_deprecated;
}

void TestBrowserWindow::OnBrowserAdded(Browser* browser) {
  if (browser->create_params().window == this) {
    browser_ = browser;
    browser_list_observer_.Reset();
  }
}
