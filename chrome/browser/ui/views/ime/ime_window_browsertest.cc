// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/input_method/ime_native_window.h"
#include "chrome/browser/ui/input_method/ime_window.h"
#include "chrome/browser/ui/input_method/ime_window_observer.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/views/widget/widget.h"

namespace ui {
namespace test {

class ImeWindowBrowserTest : public InProcessBrowserTest,
                             public ImeWindowObserver {
 public:
  ImeWindowBrowserTest() : ime_window_(nullptr) {}
  ~ImeWindowBrowserTest() override {}

  void TearDownOnMainThread() override {
    if (ime_window_) {
      ime_window_->Close();
      WaitForWindowClosing();
      ime_window_ = nullptr;
    }
  }

 protected:
  // ImeWindowObserver:
  void OnWindowDestroyed(ImeWindow* ime_window) override {
    message_loop_runner_->Quit();
  }

  void WaitForWindowClosing() {
    message_loop_runner_ = std::make_unique<base::RunLoop>();
    message_loop_runner_->Run();
  }

  void CreateImeWindow(const gfx::Rect& bounds, bool follow_cursor) {
    ime_window_ = new ImeWindow(
        browser()->profile(), nullptr, nullptr, "about:blank",
        follow_cursor ? ImeWindow::FOLLOW_CURSOR : ImeWindow::NORMAL, bounds);
    ime_window_->AddObserver(this);
    EXPECT_FALSE(ime_window_->ime_native_window()->IsVisible());
    ime_window_->Show();
    EXPECT_TRUE(ime_window_->ime_native_window()->IsVisible());
  }

  void VerifyImeWindow(const gfx::Rect& expected_bounds) {
    EXPECT_EQ(expected_bounds,
              ime_window_->ime_native_window()->GetBounds());
    EXPECT_GT(ime_window_->GetFrameId(), 0);
  }

  ImeWindow* ime_window_;

  std::unique_ptr<base::RunLoop> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(ImeWindowBrowserTest);
};

// https://crbug.com/919624 : Flaky on Win7 bots
#if defined(OS_WIN)
#define MAYBE_CreateNormalWindow DISABLED_CreateNormalWindow
#define MAYBE_CreateFollowCursorWindow DISABLED_CreateFollowCursorWindow
#else
#define MAYBE_CreateNormalWindow CreateNormalWindow
#define MAYBE_CreateFollowCursorWindow CreateFollowCursorWindow
#endif

IN_PROC_BROWSER_TEST_F(ImeWindowBrowserTest, MAYBE_CreateNormalWindow) {
  gfx::Rect expected_bounds(100, 200, 300, 400);
  CreateImeWindow(expected_bounds, false);
  VerifyImeWindow(expected_bounds);
}

IN_PROC_BROWSER_TEST_F(ImeWindowBrowserTest, MAYBE_CreateFollowCursorWindow) {
  gfx::Rect expected_bounds(100, 200, 300, 400);
  CreateImeWindow(expected_bounds, true);
  VerifyImeWindow(expected_bounds);
}

IN_PROC_BROWSER_TEST_F(ImeWindowBrowserTest, FollowCursor) {
  gfx::Rect expected_bounds(100, 200, 100, 100);
  CreateImeWindow(expected_bounds, true);
  ime_window_->FollowCursor(gfx::Rect(50, 50, 1, 10));
  expected_bounds.set_x(18);  // cursor left - titlebar width(32).
  expected_bounds.set_y(63);  // cursor top + cursor height + margin(3).
  VerifyImeWindow(expected_bounds);
}

}  // namespace test
}  // namespace ui
