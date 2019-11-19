// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_animation_controller.h"

#include "base/bind.h"
#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_controller.h"
#include "content/public/browser/web_contents.h"

namespace {

// If the fullscreen toolbar is hidden, it is difficult for the user to see
// changes in the tabstrip. As a result, if a tab is inserted or the current
// tab switched to a new one, the toolbar must animate in and out to display
// the tabstrip changes to the user. The animation drops down the toolbar and
// then wait for 0.75 seconds before it hides the toolbar.
const NSTimeInterval kTabStripChangesDelay = 750;

}  // end namespace

//////////////////////////////////////////////////////////////////
// FullscreenToolbarAnimationController, public:

FullscreenToolbarAnimationController::FullscreenToolbarAnimationController(
    FullscreenToolbarController* owner)
    : WebContentsObserver(nullptr),
      owner_(owner),
      animation_(this),
      hide_toolbar_timer_(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kTabStripChangesDelay),
          base::Bind(&FullscreenToolbarAnimationController::
                         AnimateToolbarOutIfPossible,
                     base::Unretained(this))),
      animation_start_value_(0),
      should_hide_toolbar_after_delay_(false) {
  animation_.SetSlideDuration(base::TimeDelta::FromMilliseconds(200));
  animation_.SetTweenType(gfx::Tween::EASE_OUT);
}

FullscreenToolbarAnimationController::~FullscreenToolbarAnimationController() {}

void FullscreenToolbarAnimationController::ToolbarDidUpdate() {
  animation_start_value_ = [owner_ toolbarFraction];
}

void FullscreenToolbarAnimationController::StopAnimationAndTimer() {
  animation_.Stop();
  hide_toolbar_timer_.Stop();
}

void FullscreenToolbarAnimationController::AnimateToolbarForTabstripChanges(
    content::WebContents* contents,
    bool in_foreground) {
  // Don't kickstart the animation if the toolbar is already displayed.
  if ([owner_ mustShowFullscreenToolbar])
    return;

  if (animation_.IsShowing()) {
    hide_toolbar_timer_.Reset();
    Observe(nullptr);
    return;
  }

  should_hide_toolbar_after_delay_ = true;
  if (in_foreground && contents &&
      !contents->CompletedFirstVisuallyNonEmptyPaint()) {
    Observe(contents);
  }

  AnimateToolbarIn();
}

void FullscreenToolbarAnimationController::AnimateToolbarIn() {
  if (![owner_ isInFullscreen])
    return;

  animation_.Reset(animation_start_value_);
  animation_.Show();
}

void FullscreenToolbarAnimationController::AnimateToolbarOutIfPossible() {
  if (![owner_ isInFullscreen] || [owner_ mustShowFullscreenToolbar])
    return;

  if (animation_.IsClosing())
    return;

  animation_.Stop();
  animation_.Hide();
}

CGFloat FullscreenToolbarAnimationController::GetToolbarFractionFromProgress()
    const {
  return animation_.GetCurrentValue();
}

bool FullscreenToolbarAnimationController::IsAnimationRunning() const {
  return animation_.is_animating();
}

//////////////////////////////////////////////////////////////////
// FullscreenToolbarAnimationController::WebContentsObserver:

void FullscreenToolbarAnimationController::DidFirstVisuallyNonEmptyPaint() {
  StartHideTimerIfPossible();
  Observe(nullptr);
}

//////////////////////////////////////////////////////////////////
// FullscreenToolbarAnimationController::AnimationDelegate:

void FullscreenToolbarAnimationController::AnimationProgressed(
    const gfx::Animation* animation) {
  [owner_ layoutToolbar];
}

void FullscreenToolbarAnimationController::AnimationEnded(
    const gfx::Animation* animation) {
  if (!web_contents() && animation_.IsShowing())
    StartHideTimerIfPossible();
}

//////////////////////////////////////////////////////////////////
// FullscreenToolbarAnimationController, private:

void FullscreenToolbarAnimationController::StartHideTimerIfPossible() {
  DCHECK(animation_.IsShowing());
  if (should_hide_toolbar_after_delay_) {
    hide_toolbar_timer_.Reset();
    should_hide_toolbar_after_delay_ = false;
  }
}
