// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_TESTER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_TESTER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/pointer/touch_ui_controller.h"

class BrowserView;
class Browser;

// Template to be used as a base class for touch-optimized UI parameterized test
// fixtures.
template <class BaseTest>
class TopChromeMdParamTest : public BaseTest,
                             public ::testing::WithParamInterface<bool> {
 public:
  TopChromeMdParamTest() : touch_ui_scoper_(GetParam()) {}
  ~TopChromeMdParamTest() override = default;

 private:
  ui::TouchUiController::TouchUiScoperForTesting touch_ui_scoper_;
};

// Template used as a base class for touch-optimized UI test fixtures.
template <class BaseTest>
class TopChromeTouchTest : public BaseTest {
 public:
  TopChromeTouchTest() : touch_ui_scoper_(true) {}
  ~TopChromeTouchTest() override = default;

 private:
  ui::TouchUiController::TouchUiScoperForTesting touch_ui_scoper_;
};

// Template to be used when a test does not work with the webUI tabstrip.
template <bool kEnabled, class BaseTest>
class WebUiTabStripOverrideTest : public BaseTest {
 public:
  WebUiTabStripOverrideTest() {
    if (kEnabled)
      feature_override_.InitAndEnableFeature(features::kWebUITabStrip);
    else
      feature_override_.InitAndDisableFeature(features::kWebUITabStrip);
  }
  ~WebUiTabStripOverrideTest() override = default;

 private:
  base::test::ScopedFeatureList feature_override_;
};

// A helper class for immersive mode tests.
class ImmersiveModeTester : public ImmersiveModeController::Observer {
 public:
  explicit ImmersiveModeTester(Browser* browser);
  ImmersiveModeTester(const ImmersiveModeTester&) = delete;
  ImmersiveModeTester& operator=(const ImmersiveModeTester&) = delete;
  ~ImmersiveModeTester() override;

  BrowserView* GetBrowserView();

  // Runs the given command, verifies that a reveal happens and the expected tab
  // is active.
  void RunCommand(int command, int expected_index);

  // Verifies a reveal has happened and the expected tab is active.
  void VerifyTabIndexAfterReveal(int expected_index);

  // Waits for the immersive fullscreen to start (or returns immediately if
  // immersive fullscreen already started).
  void WaitForFullscreenToEnter();

  // Waits for the immersive fullscreen to end (or returns immediately if
  // immersive fullscreen already ended).
  void WaitForFullscreenToExit();

  // ImmersiveModeController::Observer:
  void OnImmersiveRevealStarted() override;
  void OnImmersiveRevealEnded() override;
  void OnImmersiveModeControllerDestroyed() override;
  void OnImmersiveFullscreenEntered() override;
  void OnImmersiveFullscreenExited() override;

 private:
  const raw_ptr<Browser> browser_;
  base::ScopedObservation<ImmersiveModeController,
                          ImmersiveModeController::Observer>
      scoped_observation_{this};
  bool reveal_started_ = false;
  bool reveal_ended_ = false;
  std::unique_ptr<base::RunLoop> reveal_loop_;
  std::unique_ptr<base::RunLoop> fullscreen_entering_loop_;
  std::unique_ptr<base::RunLoop> fullscreen_exiting_loop_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_TESTER_H_
