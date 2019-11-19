// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/screen_capture_notification_ui.h"

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/test/browser_test.h"
#include "ui/gfx/native_widget_types.h"

class ScreenCaptureNotificationUiBrowserTest : public DialogBrowserTest {
 public:
  ScreenCaptureNotificationUiBrowserTest() = default;
  ~ScreenCaptureNotificationUiBrowserTest() override = default;

  // TestBrowserUi:
  void ShowUi(const std::string& name) override {
    screen_capture_notification_ui_ =
        ScreenCaptureNotificationUI::Create(base::string16(
            base::ASCIIToUTF16("ScreenCaptureNotificationUI Browser Test")));
    on_started_result_ = screen_capture_notification_ui_->OnStarted(
        base::BindRepeating(
            [](ScreenCaptureNotificationUiBrowserTest* test) {
              if (test->run_loop_)
                test->run_loop_->QuitWhenIdle();
            },
            base::Unretained(this)),
        content::MediaStreamUI::SourceCallback());
  }

  bool VerifyUi() override {
    // on_started_result_ == 0 is a loose signal that indicates
    // DialogBrowserTest::VerifyUi() should be called instead of checking the
    // value of |on_started_result_|.
    // on_started_result_ will occur under the following circumstances:
    //   * Views ScreenCaptureNotificationUI except for Windows.
    //   * ChromeOS (Currently unsupported and not built for this test as the
    //     CrOS system tray is used).
    // TODO(robliao): Remove this override once Views is the only toolkit.
    return (on_started_result_ != 0) || DialogBrowserTest::VerifyUi();
  }

  void DismissUi() override { screen_capture_notification_ui_.reset(); }

  void WaitForUserDismissal() override {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_.reset();
    screen_capture_notification_ui_.reset();
  }

  std::string GetNonDialogName() override {
    // This class tests a non-dialog widget with the following name.
    return "ScreenCaptureNotificationUIViews";
  }

 private:
  std::unique_ptr<ScreenCaptureNotificationUI> screen_capture_notification_ui_;
  gfx::NativeViewId on_started_result_;
  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(ScreenCaptureNotificationUiBrowserTest);
};

IN_PROC_BROWSER_TEST_F(ScreenCaptureNotificationUiBrowserTest, InvokeUi) {
  ShowAndVerifyUi();
}
