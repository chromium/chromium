// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_MOCK_IMMERSIVE_MODE_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_MOCK_IMMERSIVE_MODE_CONTROLLER_H_

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockImmersiveRevealedLock : public ImmersiveRevealedLock {
 public:
  MockImmersiveRevealedLock() = default;
  ~MockImmersiveRevealedLock() override = default;
};

class MockImmersiveModeController : public ImmersiveModeController {
 public:
  explicit MockImmersiveModeController(BrowserWindowInterface* browser);
  ~MockImmersiveModeController() override;

  MOCK_METHOD(void, Init, (BrowserView * browser_view), (override));
  MOCK_METHOD(void, SetEnabled, (bool enabled), (override));
  MOCK_METHOD(bool, IsEnabled, (), (const, override));
  MOCK_METHOD(bool, ShouldHideTopViews, (), (const, override));
  MOCK_METHOD(bool, IsRevealed, (), (const, override));
  MOCK_METHOD(int,
              GetTopContainerVerticalOffset,
              (const gfx::Size& top_container_size),
              (const, override));
  MOCK_METHOD(std::unique_ptr<ImmersiveRevealedLock>,
              GetRevealedLock,
              (AnimateReveal animate_reveal),
              (override));
  MOCK_METHOD(void,
              OnFindBarVisibleBoundsChanged,
              (const gfx::Rect& new_visible_bounds_in_screen),
              (override));
  MOCK_METHOD(bool, ShouldStayImmersiveAfterExitingFullscreen, (), (override));
  MOCK_METHOD(int, GetMinimumContentOffset, (), (const, override));
  MOCK_METHOD(int, GetExtraInfobarOffset, (), (const, override));
  MOCK_METHOD(void,
              OnContentFullscreenChanged,
              (bool is_content_fullscreen),
              (override));
  MOCK_METHOD(void, AddObserver, (Observer * observer), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer * observer), (override));
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_MOCK_IMMERSIVE_MODE_CONTROLLER_H_
