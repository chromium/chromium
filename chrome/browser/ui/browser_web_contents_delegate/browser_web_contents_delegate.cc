// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_web_contents_delegate/browser_web_contents_delegate.h"

#include "base/metrics/histogram_macros.h"
#include "base/notimplemented.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/blocked_content/framebust_block_tab_helper.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/desktop_browser_window_capabilities.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/keyboard_lock_controller.h"
#include "chrome/browser/ui/exclusive_access/pointer_lock_controller.h"
#include "components/blocked_content/list_item_position.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/drop_data.h"
#include "third_party/blink/public/common/page/drag_operation.h"
#include "ui/base/base_window.h"

#if defined(USE_AURA)
#include "chrome/browser/ui/overscroll_pref_manager.h"
#endif  // defined(USE_AURA)

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#endif

DEFINE_USER_DATA(BrowserWebContentsDelegate);

BrowserWebContentsDelegate::BrowserWebContentsDelegate(
    BrowserWindowInterface* browser,
    ExclusiveAccessManager& exclusive_access_manager,
    BrowserWindow& window,
    DesktopBrowserWindowCapabilities& capabilities)
    : exclusive_access_manager_(exclusive_access_manager),
      window_(window),
      capabilities_(capabilities),
      browser_(*browser),
      scoped_data_holder_(browser->GetUnownedUserDataHost(), *this) {}

BrowserWebContentsDelegate* BrowserWebContentsDelegate::From(
    BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

BrowserWebContentsDelegate::~BrowserWebContentsDelegate() = default;

content::PictureInPictureResult
BrowserWebContentsDelegate::EnterPictureInPicture(
    content::WebContents* web_contents) {
  return PictureInPictureWindowManager::GetInstance()
      ->EnterVideoPictureInPicture(web_contents);
}

void BrowserWebContentsDelegate::ExitPictureInPicture() {
  PictureInPictureWindowManager::GetInstance()->ExitPictureInPicture();
}

content::KeyboardEventProcessingResult
BrowserWebContentsDelegate::PreHandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  // Forward keyboard events to the manager for fullscreen / mouse lock. This
  // may consume the event (e.g., Esc exits fullscreen mode).
  // TODO(koz): Write a test for this http://crbug.com/40647724.
  if (exclusive_access_manager_->HandleUserKeyEvent(event)) {
    return content::KeyboardEventProcessingResult::HANDLED;
  }

  return window_->PreHandleKeyboardEvent(event);
}

bool BrowserWebContentsDelegate::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  DevToolsWindow* devtools_window =
      DevToolsWindow::GetInstanceForInspectedWebContents(source);
  return (devtools_window && devtools_window->ForwardKeyboardEvent(event)) ||
         window_->HandleKeyboardEvent(event);
}

void BrowserWebContentsDelegate::RequestPointerLock(
    content::WebContents* web_contents,
    bool user_gesture,
    bool last_unlocked_by_target) {
  exclusive_access_manager_->pointer_lock_controller()->RequestToLockPointer(
      web_contents, user_gesture, last_unlocked_by_target);
}

void BrowserWebContentsDelegate::LostPointerLock() {
  exclusive_access_manager_->pointer_lock_controller()
      ->ExitExclusiveAccessToPreviousState();
}

bool BrowserWebContentsDelegate::IsWaitingForPointerLockPrompt(
    content::WebContents* web_contents) {
  return exclusive_access_manager_->pointer_lock_controller()
      ->IsWaitingForPointerLockPrompt(web_contents);
}

bool BrowserWebContentsDelegate::AllowKeyboardLockForInnerContents(
    content::WebContents* web_contents) {
  return capabilities_->AllowKeyboardLockForInnerContents(web_contents);
}

void BrowserWebContentsDelegate::RequestKeyboardLock(
    content::WebContents* web_contents,
    bool esc_key_locked) {
  exclusive_access_manager_->keyboard_lock_controller()->RequestKeyboardLock(
      web_contents, esc_key_locked);
}

void BrowserWebContentsDelegate::CancelKeyboardLockRequest(
    content::WebContents* web_contents) {
  exclusive_access_manager_->keyboard_lock_controller()
      ->CancelKeyboardLockRequest(web_contents);
}

void BrowserWebContentsDelegate::SetTopControlsShownRatio(
    content::WebContents* web_contents,
    float ratio) {
  window_->SetTopControlsShownRatio(web_contents, ratio);
}

int BrowserWebContentsDelegate::GetTopControlsHeight() {
  return window_->GetTopControlsHeight();
}

bool BrowserWebContentsDelegate::DoBrowserControlsShrinkRendererSize(
    content::WebContents* contents) {
  return window_->DoBrowserControlsShrinkRendererSize(contents);
}

int BrowserWebContentsDelegate::GetVirtualKeyboardHeight(
    content::WebContents* contents) {
  // This API is currently only used by View Transitions when the virtual
  // keyboard resizes content.  On desktop platforms, the virtual keyboard can
  // only inset the visual viewport so it shouldn't ever be called.
  NOTIMPLEMENTED();
  return 0;
}

void BrowserWebContentsDelegate::SetTopControlsGestureScrollInProgress(
    bool in_progress) {
  window_->SetTopControlsGestureScrollInProgress(in_progress);
}

bool BrowserWebContentsDelegate::CanOverscrollContent() {
#if defined(USE_AURA)
  return browser_->GetFeatures()
      .overscroll_pref_manager()
      ->CanOverscrollContent();
#else
  return false;
#endif
}

bool BrowserWebContentsDelegate::ShouldPreserveAbortedURLs(
    content::WebContents* source) {
  // Allow failed URLs to stick around in the omnibox on the NTP, but not when
  // other pages have committed.
  Profile* profile = Profile::FromBrowserContext(source->GetBrowserContext());
  if (!profile || !source->GetController().GetLastCommittedEntry()) {
    return false;
  }
  GURL committed_url(source->GetController().GetLastCommittedEntry()->GetURL());
  return search::IsNTPOrRelatedURL(committed_url, profile);
}

void BrowserWebContentsDelegate::SetFocusToLocationBar() {
  // Two differences between this and FocusLocationBar():
  // (1) This doesn't get recorded in user metrics, since it's called
  //     internally.
  // (2) This is called with |is_user_initiated| == false, because this is a
  //     renderer initiated focus (this method is a WebContentsDelegate
  //     override).
  window_->SetFocusToLocationBar(false);
}

void BrowserWebContentsDelegate::PreHandleDragUpdate(
    const content::DropData& drop_data,
    const gfx::PointF& client_pt) {
  window_->PreHandleDragUpdate(drop_data, client_pt);
}

void BrowserWebContentsDelegate::PreHandleDragExit() {
  window_->PreHandleDragExit();
}

void BrowserWebContentsDelegate::HandleDragEnded() {
  window_->HandleDragEnded();
}

bool BrowserWebContentsDelegate::CanDragEnter(
    content::WebContents* source,
    const content::DropData& data,
    blink::DragOperationsMask operations_allowed) {
#if BUILDFLAG(IS_CHROMEOS)
  // Disallow drag-and-drop navigation for Settings windows which do not support
  // external navigation.
  if ((operations_allowed & blink::kDragOperationLink) &&
      chrome::SettingsWindowManager::GetInstance()->IsSettingsBrowser(
          browser_->GetBrowserForMigrationOnly())) {
    return false;
  }
#endif
  return true;
}

void BrowserWebContentsDelegate::CreateSmsPrompt(
    content::RenderFrameHost*,
    const std::vector<url::Origin>&,
    const std::string& one_time_code,
    base::OnceClosure on_confirm,
    base::OnceClosure on_cancel) {
  // TODO(crbug.com/40103792): implementation left pending deliberately.
  std::move(on_confirm).Run();
}

bool BrowserWebContentsDelegate::ShouldAllowRunningInsecureContent(
    content::WebContents* web_contents,
    bool allowed_per_prefs,
    const url::Origin& origin,
    const GURL& resource_url) {
  // Note: this implementation is a mirror of
  // ContentSettingsObserver::allowRunningInsecureContent.
  if (allowed_per_prefs) {
    return true;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(profile);
  return content_settings->GetContentSetting(
             web_contents->GetLastCommittedURL(), GURL(),
             ContentSettingsType::MIXEDSCRIPT) == CONTENT_SETTING_ALLOW;
}

void BrowserWebContentsDelegate::OnDidBlockNavigation(
    content::WebContents* web_contents,
    const GURL& blocked_url,
    const GURL& initiator_url,
    const url::Origin& initiator_origin,
    blink::mojom::NavigationBlockedReason reason) {
  if (reason ==
      blink::mojom::NavigationBlockedReason::kRedirectWithNoUserGesture) {
    if (auto* framebust_helper =
            FramebustBlockTabHelper::FromWebContents(web_contents)) {
      auto on_click = [](const GURL& url, size_t index, size_t total_elements) {
        UMA_HISTOGRAM_ENUMERATION(
            "WebCore.Framebust.ClickThroughPosition",
            blocked_content::GetListItemPositionFromDistance(index,
                                                             total_elements));
      };
      framebust_helper->AddBlockedUrl(blocked_url, initiator_origin,
                                      base::BindOnce(on_click));
    }
  }
}

bool BrowserWebContentsDelegate::IsBackForwardCacheSupported(
    content::WebContents& web_contents) {
  return true;
}
