// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_controller.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_menubar_tracker.h"
#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_animation_controller.h"
#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_mouse_tracker.h"
#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_visibility_lock_controller.h"
#import "chrome/browser/ui/cocoa/fullscreen/immersive_fullscreen_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"

@implementation FullscreenToolbarController

- (id)initWithDelegate:(id<FullscreenToolbarContextDelegate>)delegate {
  if ((self = [super init])) {
    animationController_ =
        std::make_unique<FullscreenToolbarAnimationController>(self);
    visibilityLockController_.reset(
        [[FullscreenToolbarVisibilityLockController alloc]
            initWithFullscreenToolbarController:self
                            animationController:animationController_.get()]);
  }

  delegate_ = delegate;
  return self;
}

- (void)dealloc {
  DCHECK(!inFullscreenMode_);
  [super dealloc];
}

- (void)enterFullscreenMode {
  DCHECK(!inFullscreenMode_);
  inFullscreenMode_ = YES;

  if ([delegate_ isInImmersiveFullscreen]) {
    immersiveFullscreenController_.reset(
        [[ImmersiveFullscreenController alloc] initWithDelegate:delegate_]);
    [immersiveFullscreenController_ updateMenuBarAndDockVisibility];
  } else {
    menubarTracker_.reset([[FullscreenMenubarTracker alloc]
        initWithFullscreenToolbarController:self]);
    mouseTracker_.reset([[FullscreenToolbarMouseTracker alloc]
        initWithFullscreenToolbarController:self]);
  }
}

- (void)exitFullscreenMode {
  DCHECK(inFullscreenMode_);
  inFullscreenMode_ = NO;

  animationController_->StopAnimationAndTimer();
  [[NSNotificationCenter defaultCenter] removeObserver:self];

  menubarTracker_.reset();
  mouseTracker_.reset();
  immersiveFullscreenController_.reset();
}

- (void)revealToolbarForWebContents:(content::WebContents*)contents
                       inForeground:(BOOL)inForeground {
  animationController_->AnimateToolbarForTabstripChanges(contents,
                                                         inForeground);
}

- (CGFloat)toolbarFraction {
  // Visibility fractions for the menubar and toolbar.
  constexpr CGFloat kHideFraction = 0.0;
  constexpr CGFloat kShowFraction = 1.0;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kKioskMode))
    return kHideFraction;

  switch (toolbarStyle_) {
    case FullscreenToolbarStyle::TOOLBAR_PRESENT:
      return kShowFraction;
    case FullscreenToolbarStyle::TOOLBAR_NONE:
      return kHideFraction;
    case FullscreenToolbarStyle::TOOLBAR_HIDDEN:
      if (animationController_->IsAnimationRunning())
        return animationController_->GetToolbarFractionFromProgress();

      if ([self mustShowFullscreenToolbar])
        return kShowFraction;

      return [menubarTracker_ menubarFraction];
  }
}

- (FullscreenToolbarStyle)toolbarStyle {
  return toolbarStyle_;
}

- (BOOL)mustShowFullscreenToolbar {
  if (!inFullscreenMode_)
    return NO;

  if (toolbarStyle_ == FullscreenToolbarStyle::TOOLBAR_PRESENT)
    return YES;

  if (toolbarStyle_ == FullscreenToolbarStyle::TOOLBAR_NONE)
    return NO;

  FullscreenMenubarState menubarState = [menubarTracker_ state];
  return menubarState == FullscreenMenubarState::SHOWN ||
         [visibilityLockController_ isToolbarVisibilityLocked];
}

- (void)updateToolbarFrame:(NSRect)frame {
  if (mouseTracker_.get())
    [mouseTracker_ updateToolbarFrame:frame];
}

- (void)layoutToolbar {
  animationController_->ToolbarDidUpdate();
  [mouseTracker_ updateTrackingArea];
}

- (BOOL)isInFullscreen {
  return inFullscreenMode_;
}

- (FullscreenMenubarTracker*)menubarTracker {
  return menubarTracker_.get();
}

- (FullscreenToolbarVisibilityLockController*)visibilityLockController {
  return visibilityLockController_.get();
}

- (ImmersiveFullscreenController*)immersiveFullscreenController {
  return immersiveFullscreenController_.get();
}

- (void)setToolbarStyle:(FullscreenToolbarStyle)style {
  toolbarStyle_ = style;
}

- (id<FullscreenToolbarContextDelegate>)delegate {
  return delegate_;
}

@end

@implementation FullscreenToolbarController (ExposedForTesting)

- (FullscreenToolbarAnimationController*)animationController {
  return animationController_.get();
}

- (void)setMenubarTracker:(FullscreenMenubarTracker*)tracker {
  menubarTracker_.reset([tracker retain]);
}

- (void)setMouseTracker:(FullscreenToolbarMouseTracker*)tracker {
  mouseTracker_.reset([tracker retain]);
}

- (void)setTestFullscreenMode:(BOOL)isInFullscreen {
  inFullscreenMode_ = isInFullscreen;
}

@end
