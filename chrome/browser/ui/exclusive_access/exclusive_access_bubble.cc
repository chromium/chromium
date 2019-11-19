// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "extensions/browser/extension_registry.h"
#include "ui/gfx/geometry/rect.h"

// NOTE(koz): Linux doesn't use the thick shadowed border, so we add padding
// here.
#if defined(OS_LINUX)
const int ExclusiveAccessBubble::kPaddingPx = 8;
#else
const int ExclusiveAccessBubble::kPaddingPx = 15;
#endif
const int ExclusiveAccessBubble::kInitialDelayMs = 3800;
const int ExclusiveAccessBubble::kIdleTimeMs = 2300;
const int ExclusiveAccessBubble::kSnoozeNotificationsTimeMs = 900000;  // 15m.
const int ExclusiveAccessBubble::kPositionCheckHz = 10;
const int ExclusiveAccessBubble::kSlideInRegionHeightPx = 4;
const int ExclusiveAccessBubble::kPopupTopPx = 45;
const int ExclusiveAccessBubble::kSimplifiedPopupTopPx = 45;

ExclusiveAccessBubble::ExclusiveAccessBubble(
    ExclusiveAccessManager* manager,
    const GURL& url,
    ExclusiveAccessBubbleType bubble_type)
    : manager_(manager), url_(url), bubble_type_(bubble_type) {
  DCHECK_NE(EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE, bubble_type_);
}

ExclusiveAccessBubble::~ExclusiveAccessBubble() {
}

void ExclusiveAccessBubble::OnUserInput() {
  // We got some user input; reset the idle timer.
  idle_timeout_.Stop();  // If the timer isn't running, this is a no-op.
  idle_timeout_.Start(FROM_HERE,
                      base::TimeDelta::FromMilliseconds(kIdleTimeMs), this,
                      &ExclusiveAccessBubble::CheckMousePosition);

  // If the notification suppression timer has elapsed, re-show it.
  if (!suppress_notify_timeout_.IsRunning()) {
    manager_->RecordBubbleReshownUMA(bubble_type_);

    ShowAndStartTimers();
    return;
  }

  // The timer has not elapsed, but the user provided some input. Reset the
  // timer. (We only want to re-show the message after a period of inactivity.)
  suppress_notify_timeout_.Reset();
}

void ExclusiveAccessBubble::StartWatchingMouse() {
  // Start the initial delay timer and begin watching the mouse.
  ShowAndStartTimers();
  mouse_position_checker_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(1000 / kPositionCheckHz),
      this, &ExclusiveAccessBubble::CheckMousePosition);
}

void ExclusiveAccessBubble::StopWatchingMouse() {
  hide_timeout_.Stop();
  idle_timeout_.Stop();
  mouse_position_checker_.Stop();
}

bool ExclusiveAccessBubble::IsWatchingMouse() const {
  return mouse_position_checker_.IsRunning();
}

void ExclusiveAccessBubble::CheckMousePosition() {
  if (!hide_timeout_.IsRunning())
    Hide();
}

void ExclusiveAccessBubble::ExitExclusiveAccess() {
  manager_->ExitExclusiveAccess();
}

base::string16 ExclusiveAccessBubble::GetCurrentMessageText() const {
  return exclusive_access_bubble::GetLabelTextForType(
      bubble_type_, url_,
      extensions::ExtensionRegistry::Get(manager_->context()->GetProfile()));
}

base::string16 ExclusiveAccessBubble::GetCurrentDenyButtonText() const {
  return exclusive_access_bubble::GetDenyButtonTextForType(bubble_type_);
}

base::string16 ExclusiveAccessBubble::GetCurrentAllowButtonText() const {
  return exclusive_access_bubble::GetAllowButtonTextForType(bubble_type_, url_);
}

base::string16 ExclusiveAccessBubble::GetInstructionText(
    const base::string16& accelerator) const {
  return exclusive_access_bubble::GetInstructionTextForType(bubble_type_,
                                                            accelerator);
}

bool ExclusiveAccessBubble::IsHideTimeoutRunning() const {
  return hide_timeout_.IsRunning();
}

void ExclusiveAccessBubble::ShowAndStartTimers() {
  Show();

  // Do not allow the notification to hide for a few seconds.
  hide_timeout_.Start(FROM_HERE,
                      base::TimeDelta::FromMilliseconds(kInitialDelayMs), this,
                      &ExclusiveAccessBubble::CheckMousePosition);

  // Do not show the notification again until a long time has elapsed.
  suppress_notify_timeout_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(kSnoozeNotificationsTimeMs),
      this, &ExclusiveAccessBubble::CheckMousePosition);
}
