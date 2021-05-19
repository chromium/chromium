// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/test_browser_window.h"

#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/user_education/feature_promo_controller.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "ui/gfx/geometry/rect.h"

// Helpers --------------------------------------------------------------------

std::unique_ptr<Browser> CreateBrowserWithTestWindowForParams(
    Browser::CreateParams params) {
  DCHECK(!params.window);
  TestBrowserWindow* window = new TestBrowserWindow;
  new TestBrowserWindowOwner(window);
  params.window = window;
  return std::unique_ptr<Browser>(Browser::Create(params));
}

// TestBrowserWindow::TestLocationBar -----------------------------------------

GURL TestBrowserWindow::TestLocationBar::GetDestinationURL() const {
  return GURL();
}

WindowOpenDisposition
    TestBrowserWindow::TestLocationBar::GetWindowOpenDisposition() const {
  return WindowOpenDisposition::CURRENT_TAB;
}

ui::PageTransition
    TestBrowserWindow::TestLocationBar::GetPageTransition() const {
  return ui::PAGE_TRANSITION_LINK;
}

base::TimeTicks TestBrowserWindow::TestLocationBar::GetMatchSelectionTimestamp()
    const {
  return base::TimeTicks();
}

const OmniboxView* TestBrowserWindow::TestLocationBar::GetOmniboxView() const {
  return NULL;
}

OmniboxView* TestBrowserWindow::TestLocationBar::GetOmniboxView() {
  return NULL;
}

LocationBarTesting*
    TestBrowserWindow::TestLocationBar::GetLocationBarForTesting() {
  return NULL;
}

bool TestBrowserWindow::TestLocationBar::IsInputTypedUrlWithoutScheme() const {
  return false;
}

// TestBrowserWindow ----------------------------------------------------------

TestBrowserWindow::TestBrowserWindow() {}

TestBrowserWindow::~TestBrowserWindow() {}

void TestBrowserWindow::Close() {
  if (close_callback_)
    std::move(close_callback_).Run();
}

bool TestBrowserWindow::IsActive() const {
  return false;
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

int TestBrowserWindow::GetTopControlsHeight() const {
  return 0;
}

void TestBrowserWindow::SetTopControlsGestureScrollInProgress(
    bool in_progress) {}

StatusBubble* TestBrowserWindow::GetStatusBubble() {
  return NULL;
}

gfx::Rect TestBrowserWindow::GetRestoredBounds() const {
  return gfx::Rect();
}

ui::WindowShowState TestBrowserWindow::GetRestoredState() const {
  return ui::SHOW_STATE_DEFAULT;
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
  return false;
}

bool TestBrowserWindow::ShouldHideUIForFullscreen() const {
  return false;
}

bool TestBrowserWindow::IsFullscreen() const {
  return false;
}

bool TestBrowserWindow::IsFullscreenBubbleVisible() const {
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
    const content::NativeWebKeyboardEvent& event) {
  return content::KeyboardEventProcessingResult::NOT_HANDLED;
}

bool TestBrowserWindow::HandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
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

void TestBrowserWindow::SetIsTabStripEditable(bool is_editable) {
  is_tab_strip_editable_ = is_editable;
}

bool TestBrowserWindow::IsToolbarVisible() const {
  return false;
}

bool TestBrowserWindow::IsToolbarShowing() const {
  return false;
}

ShowTranslateBubbleResult TestBrowserWindow::ShowTranslateBubble(
    content::WebContents* contents,
    translate::TranslateStep step,
    const std::string& source_language,
    const std::string& target_language,
    translate::TranslateErrors::Type error_type,
    bool is_user_gesture) {
  return ShowTranslateBubbleResult::SUCCESS;
}

qrcode_generator::QRCodeGeneratorBubbleView*
TestBrowserWindow::ShowQRCodeGeneratorBubble(
    content::WebContents* contents,
    qrcode_generator::QRCodeGeneratorBubbleController* controller,
    const GURL& url) {
  return nullptr;
}

SharingDialog* TestBrowserWindow::ShowSharingDialog(
    content::WebContents* web_contents,
    SharingDialogData data) {
  return nullptr;
}

send_tab_to_self::SendTabToSelfBubbleView*
TestBrowserWindow::ShowSendTabToSelfBubble(
    content::WebContents* contents,
    send_tab_to_self::SendTabToSelfBubbleController* controller,
    bool is_user_gesture) {
  return nullptr;
}

bool TestBrowserWindow::IsDownloadShelfVisible() const {
  return false;
}

DownloadShelf* TestBrowserWindow::GetDownloadShelf() {
  return &download_shelf_;
}

std::unique_ptr<FindBar> TestBrowserWindow::CreateFindBar() {
  return NULL;
}

web_modal::WebContentsModalDialogHost*
    TestBrowserWindow::GetWebContentsModalDialogHost() {
  return NULL;
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

FeaturePromoController* TestBrowserWindow::GetFeaturePromoController() {
  return feature_promo_controller_.get();
}

FeaturePromoController* TestBrowserWindow::SetFeaturePromoController(
    std::unique_ptr<FeaturePromoController> feature_promo_controller) {
  feature_promo_controller_ = std::move(feature_promo_controller);
  return feature_promo_controller_.get();
}

// TestBrowserWindowOwner -----------------------------------------------------

TestBrowserWindowOwner::TestBrowserWindowOwner(TestBrowserWindow* window)
    : window_(window) {
  BrowserList::AddObserver(this);
}

TestBrowserWindowOwner::~TestBrowserWindowOwner() {
  BrowserList::RemoveObserver(this);
}

void TestBrowserWindowOwner::OnBrowserRemoved(Browser* browser) {
  if (browser->window() == window_.get())
    delete this;
}
