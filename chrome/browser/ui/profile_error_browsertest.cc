// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/simple_message_box_internal.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"

namespace {

class ProfileErrorBrowserTest : public InProcessBrowserTest,
                                public testing::WithParamInterface<bool> {
  // A fixture that allows testing histograms reporting when faced with a
  // corrupted profile. The boolean parameter forces the creation of an empty or
  // corrupted profile, allowing to test both the corruption case and that what
  // it is testing indeed happens differently when not under corruption.
 public:
  ProfileErrorBrowserTest() : do_corrupt_(GetParam()) {}

  bool SetUpUserDataDirectory() override {
    base::FilePath profile_dir;
    if (!base::PathService::Get(chrome::DIR_USER_DATA, &profile_dir)) {
      ADD_FAILURE();
      return false;
    }
    profile_dir = profile_dir.AppendASCII(TestingProfile::kTestUserProfileDir);
    if (!base::CreateDirectory(profile_dir)) {
      ADD_FAILURE();
      return false;
    }
    const base::FilePath pref_file =
        profile_dir.Append(chrome::kPreferencesFilename);

    // Write either an empty or an invalid string to the user profile as
    // determined by the boolean parameter.
    const std::string kUserProfileData(do_corrupt_ ? "invalid json" : "{}");
    if (base::WriteFile(pref_file, kUserProfileData.c_str(),
                        kUserProfileData.size()) !=
        static_cast<int>(kUserProfileData.size())) {
      ADD_FAILURE();
      return false;
    }
    return true;
  }

  void SetUpInProcessBrowserTestFixture() override {
    // Skip showing the error message box in order to avoid freezing the main
    // thread.
    chrome::internal::g_should_skip_message_box_for_test = true;
  }

 protected:
  // Histogram value verifier.
  const base::HistogramTester histogram_tester_;

  // Whether the test fixture and test should set up a corrupted profile and
  // expect a reaction to one.
  const bool do_corrupt_;
};

#if defined(OS_CHROMEOS)
// Disable the test on chromos since kernel controls the user profile thus we
// won't be able to corrupt it.
#define MAYBE_CorruptedProfile DISABLED_CorruptedProfile
#else
// http://crbug.com/527145
#define MAYBE_CorruptedProfile DISABLED_CorruptedProfile
#endif

IN_PROC_BROWSER_TEST_P(ProfileErrorBrowserTest, MAYBE_CorruptedProfile) {
  const char kPaintHistogram[] = "Startup.FirstWebContents.NonEmptyPaint2";

  // Navigate to a URL so the first non-empty paint is registered.
  ui_test_utils::NavigateToURL(browser(), GURL("http://www.example.com/"));

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Wait for the page to produce a frame, the first visually non-empty paint
  // metric is not valid until then.
  bool loaded = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      contents,
      "requestAnimationFrame(function() {"
      "  window.domAutomationController.send(true);"
      "});",
      &loaded));
  ASSERT_TRUE(loaded);

  if (do_corrupt_) {
    histogram_tester_.ExpectTotalCount(kPaintHistogram, 0);
  } else {
    histogram_tester_.ExpectTotalCount(kPaintHistogram, 1);
  }
}

INSTANTIATE_TEST_SUITE_P(ProfileErrorBrowserTestInstance,
                         ProfileErrorBrowserTest,
                         testing::Bool());

}  // namespace
