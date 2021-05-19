// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/accessibility_handler.h"

#include <memory>

#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_web_ui.h"
#include "ui/accessibility/accessibility_switches.h"

namespace chromeos {
namespace settings {

class TestAccessibilityHandler : public AccessibilityHandler {
 public:
  explicit TestAccessibilityHandler(Profile* profile)
      : AccessibilityHandler(profile) {}
  ~TestAccessibilityHandler() override = default;
};

class AccessibilityHandlerTest : public InProcessBrowserTest {
 public:
  AccessibilityHandlerTest() = default;
  AccessibilityHandlerTest(const AccessibilityHandlerTest&) = delete;
  AccessibilityHandlerTest& operator=(const AccessibilityHandlerTest&) = delete;
  ~AccessibilityHandlerTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        ::switches::kEnableExperimentalAccessibilityDictationOffline);
  }

  void SetUpOnMainThread() override {
    handler_ = std::make_unique<TestAccessibilityHandler>(browser()->profile());
    handler_->set_web_ui(&web_ui_);
    handler_->RegisterMessages();
    handler_->AllowJavascriptForTesting();
    base::RunLoop().RunUntilIdle();
  }

  void TearDownOnMainThread() override { handler_.reset(); }

  void AssertWebUICalls(unsigned int num) {
    ASSERT_EQ(num, web_ui_.call_data().size());
  }

  bool WasWebUIListenerCalledWithStringArgument(
      const std::string& expected_listener,
      const std::string& expected_argument) {
    for (auto it = web_ui_.call_data().rbegin();
         it != web_ui_.call_data().rend(); ++it) {
      const content::TestWebUI::CallData* data = it->get();
      std::string listener;
      std::string listener_argument;
      data->arg1()->GetAsString(&listener);
      if (!data->arg2()->GetAsString(&listener_argument)) {
        // Only look for listeners with a single string argument. Continue
        // silently if we find anything else.
        continue;
      }

      if (data->function_name() == "cr.webUIListenerCallback" &&
          listener == expected_listener &&
          expected_argument == listener_argument) {
        return true;
      }
    }

    return false;
  }

  void AddSodaInstallerObserver() { handler_->MaybeAddSodaInstallerObserver(); }

  void OnSodaInstalled() { handler_->OnSodaInstalled(); }

  void OnSodaProgress(int progress) { handler_->OnSodaProgress(progress); }

  void OnSodaError() { handler_->OnSodaError(); }

 private:
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<TestAccessibilityHandler> handler_;
  content::TestWebUI web_ui_;
};

// A sanity check that ensures that |handler_| can be used to call into
// AccessibilityHandler and produce the expected results.
// This also verifies that the correct string is sent to the JavaScript end
// of the web UI.
IN_PROC_BROWSER_TEST_F(AccessibilityHandlerTest, OnSodaInstalledTestApi) {
  AssertWebUICalls(0);
  OnSodaInstalled();
  AssertWebUICalls(1);
  ASSERT_TRUE(WasWebUIListenerCalledWithStringArgument(
      "dictation-setting-subtitle-changed", "Speech files downloaded"));
}

// Verifies that the correct string is sent to the JavaScript end of the web UI.
IN_PROC_BROWSER_TEST_F(AccessibilityHandlerTest, OnSodaProgressTestApi) {
  AssertWebUICalls(0);
  OnSodaProgress(50);
  AssertWebUICalls(1);
  ASSERT_TRUE(WasWebUIListenerCalledWithStringArgument(
      "dictation-setting-subtitle-changed",
      "Downloading speech recognition files… 50%"));
}

// Verifies that the correct string is sent to the JavaScript end of the web UI.
IN_PROC_BROWSER_TEST_F(AccessibilityHandlerTest, OnSodaErrorTestApi) {
  AssertWebUICalls(0);
  OnSodaError();
  AssertWebUICalls(1);
  ASSERT_TRUE(WasWebUIListenerCalledWithStringArgument(
      "dictation-setting-subtitle-changed",
      "Can't download speech files. Dictation will continue to work by sending "
      "your voice to Google."));
}

// Ensures that AccessibilityHandler listens to SODA download state and fires
// the correct listener when SODA is installed.
IN_PROC_BROWSER_TEST_F(AccessibilityHandlerTest, OnSodaInstalledNotification) {
  AssertWebUICalls(0);
  AddSodaInstallerObserver();
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  AssertWebUICalls(1);
  ASSERT_TRUE(WasWebUIListenerCalledWithStringArgument(
      "dictation-setting-subtitle-changed", "Speech files downloaded"));
}

}  // namespace settings
}  // namespace chromeos
