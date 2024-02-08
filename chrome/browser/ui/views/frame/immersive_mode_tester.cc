// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/immersive_mode_tester.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"

ImmersiveModeTester::ImmersiveModeTester(Browser* browser) : browser_(browser) {
  scoped_observation_.Observe(GetBrowserView()->immersive_mode_controller());
}

ImmersiveModeTester::~ImmersiveModeTester() = default;

BrowserView* ImmersiveModeTester::GetBrowserView() {
  return BrowserView::GetBrowserViewForBrowser(browser_);
}

void ImmersiveModeTester::RunCommand(int command, int expected_index) {
  reveal_started_ = reveal_ended_ = false;
  browser_->command_controller()->ExecuteCommand(command);
  VerifyTabIndexAfterReveal(expected_index);
}

void ImmersiveModeTester::VerifyTabIndexAfterReveal(int expected_index) {
  if (!reveal_ended_) {
    reveal_loop_ = std::make_unique<base::RunLoop>();
    reveal_loop_->Run();
  }
  EXPECT_TRUE(reveal_ended_);
  EXPECT_EQ(expected_index, browser_->tab_strip_model()->active_index());
}

void ImmersiveModeTester::WaitForFullscreenToEnter() {
  if (!GetBrowserView()->immersive_mode_controller()->IsEnabled() ||
      !GetBrowserView()->IsFullscreen()) {
    fullscreen_entering_loop_ = std::make_unique<base::RunLoop>();
    fullscreen_entering_loop_->Run();
  }
  ASSERT_TRUE(GetBrowserView()->immersive_mode_controller()->IsEnabled());
  ASSERT_TRUE(GetBrowserView()->IsFullscreen());
}

void ImmersiveModeTester::WaitForFullscreenToExit() {
  if (GetBrowserView()->immersive_mode_controller()->IsEnabled()) {
    fullscreen_exiting_loop_ = std::make_unique<base::RunLoop>();
    fullscreen_exiting_loop_->Run();
  }
  ASSERT_FALSE(GetBrowserView()->immersive_mode_controller()->IsEnabled());
  ASSERT_FALSE(GetBrowserView()->IsFullscreen());
}

void ImmersiveModeTester::OnImmersiveRevealStarted() {
  EXPECT_FALSE(reveal_started_);
  EXPECT_FALSE(reveal_ended_);
  reveal_started_ = true;
  EXPECT_TRUE(GetBrowserView()->immersive_mode_controller()->IsRevealed());
}

void ImmersiveModeTester::OnImmersiveRevealEnded() {
  EXPECT_TRUE(reveal_started_);
  EXPECT_FALSE(reveal_ended_);
  reveal_started_ = false;
  reveal_ended_ = true;
  EXPECT_FALSE(GetBrowserView()->immersive_mode_controller()->IsRevealed());
  if (reveal_loop_ && reveal_loop_->running())
    reveal_loop_->Quit();
}

void ImmersiveModeTester::OnImmersiveModeControllerDestroyed() {
  DCHECK(scoped_observation_.IsObserving());
  scoped_observation_.Reset();
}

void ImmersiveModeTester::OnImmersiveFullscreenEntered() {
  if (fullscreen_entering_loop_) {
    fullscreen_entering_loop_->Quit();
  }
}

void ImmersiveModeTester::OnImmersiveFullscreenExited() {
  if (fullscreen_exiting_loop_) {
    fullscreen_exiting_loop_->Quit();
  }
}
