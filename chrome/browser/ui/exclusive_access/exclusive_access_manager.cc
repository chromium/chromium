// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"

#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/exclusive_access/mouse_lock_controller.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/common/input/native_web_keyboard_event.h"
#include "ui/events/keycodes/keyboard_codes.h"

using content::WebContents;

ExclusiveAccessManager::ExclusiveAccessManager(
    ExclusiveAccessContext* exclusive_access_context)
    : exclusive_access_context_(exclusive_access_context),
      fullscreen_controller_(this),
      keyboard_lock_controller_(this),
      mouse_lock_controller_(this) {}

ExclusiveAccessManager::~ExclusiveAccessManager() {
}

ExclusiveAccessBubbleType
ExclusiveAccessManager::GetExclusiveAccessExitBubbleType() const {
  // In kiosk and exclusive app mode we always want to be fullscreen and do not
  // want to show exit instructions for browser mode fullscreen.
  bool app_mode = false;
#if !BUILDFLAG(IS_MAC)  // App mode (kiosk) is not available on Mac yet.
  app_mode = chrome::IsRunningInAppMode();
#endif

  if (fullscreen_controller_.IsWindowFullscreenForTabOrPending()) {
    if (!fullscreen_controller_.IsTabFullscreen())
      return EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION;

    if (mouse_lock_controller_.IsMouseLockedSilently()) {
      return EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE;
    }

    if (keyboard_lock_controller_.RequiresPressAndHoldEscToExit())
      return EXCLUSIVE_ACCESS_BUBBLE_TYPE_KEYBOARD_LOCK_EXIT_INSTRUCTION;

    if (mouse_lock_controller_.IsMouseLocked())
      return EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_MOUSELOCK_EXIT_INSTRUCTION;

    return EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION;
  }

  if (mouse_lock_controller_.IsMouseLockedSilently())
    return EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE;

  if (mouse_lock_controller_.IsMouseLocked())
    return EXCLUSIVE_ACCESS_BUBBLE_TYPE_MOUSELOCK_EXIT_INSTRUCTION;

  if (fullscreen_controller_.IsExtensionFullscreenOrPending())
    return EXCLUSIVE_ACCESS_BUBBLE_TYPE_EXTENSION_FULLSCREEN_EXIT_INSTRUCTION;

  if (fullscreen_controller_.IsControllerInitiatedFullscreen() && !app_mode)
    return EXCLUSIVE_ACCESS_BUBBLE_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION;

  return EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE;
}

void ExclusiveAccessManager::UpdateExclusiveAccessExitBubbleContent(
    ExclusiveAccessBubbleHideCallback bubble_first_hide_callback,
    bool force_update) {
  GURL url = GetExclusiveAccessBubbleURL();
  ExclusiveAccessBubbleType bubble_type = GetExclusiveAccessExitBubbleType();
  exclusive_access_context_->UpdateExclusiveAccessExitBubbleContent(
      url, bubble_type, std::move(bubble_first_hide_callback),
      /*notify_download=*/false, force_update);
}

GURL ExclusiveAccessManager::GetExclusiveAccessBubbleURL() const {
  GURL result = fullscreen_controller_.GetURLForExclusiveAccessBubble();
  if (!result.is_valid())
    result = mouse_lock_controller_.GetURLForExclusiveAccessBubble();
  return result;
}

void ExclusiveAccessManager::OnTabDeactivated(WebContents* web_contents) {
  fullscreen_controller_.OnTabDeactivated(web_contents);
  keyboard_lock_controller_.OnTabDeactivated(web_contents);
  mouse_lock_controller_.OnTabDeactivated(web_contents);
}

void ExclusiveAccessManager::OnTabDetachedFromView(WebContents* web_contents) {
  fullscreen_controller_.OnTabDetachedFromView(web_contents);
  keyboard_lock_controller_.OnTabDetachedFromView(web_contents);
  mouse_lock_controller_.OnTabDetachedFromView(web_contents);
}

void ExclusiveAccessManager::OnTabClosing(WebContents* web_contents) {
  fullscreen_controller_.OnTabClosing(web_contents);
  keyboard_lock_controller_.OnTabClosing(web_contents);
  mouse_lock_controller_.OnTabClosing(web_contents);
}

bool ExclusiveAccessManager::HandleUserKeyEvent(
    const content::NativeWebKeyboardEvent& event) {
  if (event.windows_key_code != ui::VKEY_ESCAPE) {
    OnUserInput();
    return false;
  }

  // Give the |keyboard_lock_controller_| first chance at handling the ESC event
  // as there are specific UX behaviors that occur when that mode is active
  // which are coordinated by that class.  Return false as we don't want to
  // prevent the event from propagating to the webpage.
  if (keyboard_lock_controller_.HandleKeyEvent(event))
    return false;

  bool handled = false;
  handled = fullscreen_controller_.HandleUserPressedEscape();
  handled |= mouse_lock_controller_.HandleUserPressedEscape();
  handled |= keyboard_lock_controller_.HandleUserPressedEscape();
  return handled;
}

void ExclusiveAccessManager::OnUserInput() {
  exclusive_access_context_->OnExclusiveAccessUserInput();
}

void ExclusiveAccessManager::ExitExclusiveAccess() {
  fullscreen_controller_.ExitExclusiveAccessToPreviousState();
  keyboard_lock_controller_.LostKeyboardLock();
  mouse_lock_controller_.LostMouseLock();
}
