// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller_chromeos.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller_test_api.h"
#include "content/public/test/browser_test.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

using ChromeViewsDelegateLacrosBrowsertest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(ChromeViewsDelegateLacrosBrowsertest,
                       DefaultNonClientFrameViewEntersImmersiveMode) {
  views::UniqueWidgetPtr widget(std::make_unique<views::Widget>());
  widget->Init(
      views::Widget::InitParams(views::Widget::InitParams::TYPE_WINDOW));
  widget->Show();

  auto* immersive_fullscreen_controller =
      chromeos::ImmersiveFullscreenController::Get(widget.get());
  chromeos::ImmersiveFullscreenControllerTestApi(
      immersive_fullscreen_controller)
      .SetupForTest();

  EXPECT_FALSE(immersive_fullscreen_controller->IsEnabled());

  // Fullscreen the framed widget, it should enter immersive mode.
  widget->SetFullscreen(true);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return immersive_fullscreen_controller->IsEnabled(); }));

  // Exiting fullscreen should also exit immersive mode.
  widget->SetFullscreen(false);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !immersive_fullscreen_controller->IsEnabled(); }));
}
