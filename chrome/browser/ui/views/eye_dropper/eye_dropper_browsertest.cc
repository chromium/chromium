// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/eye_dropper/eye_dropper.h"

#include <memory>
#include <string>

#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/eye_dropper.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "ui/display/display_switches.h"

// TODO(crbug.com/40269208): enable this test on all supported platforms.
#if BUILDFLAG(IS_WIN)
#include "components/eye_dropper/eye_dropper_view.h"
#endif

class EyeDropperBrowserTest : public UiBrowserTest,
                              public ::testing::WithParamInterface<float> {
 public:
  EyeDropperBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor,
                                    base::NumberToString(GetParam()));
  }

  // UiBrowserTest:
  void ShowUi(const std::string& name) override {
#if BUILDFLAG(IS_WIN)
    content::RenderFrameHost* parent_frame = browser()
                                                 ->tab_strip_model()
                                                 ->GetActiveWebContents()
                                                 ->GetPrimaryMainFrame();
    parent_frame->GetView()->Focus();
    eye_dropper_ = ShowEyeDropper(parent_frame, /*listener=*/nullptr);
#endif
  }

  bool VerifyUi() override {
#if BUILDFLAG(IS_WIN)
    if (!eye_dropper_)
      return false;

    views::Widget* widget =
        static_cast<eye_dropper::EyeDropperView*>(eye_dropper_.get())
            ->GetWidget();
    auto* test_info = testing::UnitTest::GetInstance()->current_test_info();
    const std::string screenshot_name =
        base::StrCat({test_info->test_suite_name(), "_", test_info->name()});
    return VerifyPixelUi(widget, "EyeDropperBrowserTest", screenshot_name) !=
           ui::test::ActionResult::kFailed;
#else
    return true;
#endif
  }

  void WaitForUserDismissal() override {
    // Consider closing the browser to be dismissal.
    ui_test_utils::WaitForBrowserToClose();
  }

  void DismissUi() override { eye_dropper_.reset(); }

 private:
  std::unique_ptr<content::EyeDropper> eye_dropper_;
};

// Invokes the eye dropper.
IN_PROC_BROWSER_TEST_P(EyeDropperBrowserTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(All,
                         EyeDropperBrowserTest,
                         testing::Values(1.0, 1.5, 2.0));
