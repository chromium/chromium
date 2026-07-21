// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_web_contents_delegate/browser_web_contents_delegate.h"

#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/desktop_browser_window_capabilities.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/keyboard_lock_controller.h"
#include "chrome/browser/ui/exclusive_access/pointer_lock_controller.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "ui/base/base_window.h"

DEFINE_USER_DATA(BrowserWebContentsDelegate);

BrowserWebContentsDelegate::BrowserWebContentsDelegate(
    BrowserWindowInterface* browser,
    ExclusiveAccessManager& exclusive_access_manager,
    BrowserWindow& window,
    DesktopBrowserWindowCapabilities& capabilities)
    : exclusive_access_manager_(exclusive_access_manager),
      window_(window),
      capabilities_(capabilities),
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
