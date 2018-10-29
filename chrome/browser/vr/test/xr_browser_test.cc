// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/vr/test/xr_browser_test.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "url/gurl.h"

namespace vr {

constexpr base::TimeDelta XrBrowserTestBase::kPollCheckIntervalShort;
constexpr base::TimeDelta XrBrowserTestBase::kPollCheckIntervalLong;
constexpr base::TimeDelta XrBrowserTestBase::kPollTimeoutShort;
constexpr base::TimeDelta XrBrowserTestBase::kPollTimeoutMedium;
constexpr base::TimeDelta XrBrowserTestBase::kPollTimeoutLong;
constexpr char XrBrowserTestBase::kVrOverrideEnvVar[];
constexpr char XrBrowserTestBase::kVrOverrideVal[];
constexpr char XrBrowserTestBase::kVrConfigPathEnvVar[];
constexpr char XrBrowserTestBase::kVrConfigPathVal[];
constexpr char XrBrowserTestBase::kVrLogPathEnvVar[];
constexpr char XrBrowserTestBase::kVrLogPathVal[];

XrBrowserTestBase::XrBrowserTestBase() : env_(base::Environment::Create()) {}

XrBrowserTestBase::~XrBrowserTestBase() = default;

// We need an std::string that is an absolute file path, which requires
// platform-specific logic since Windows uses std::wstring instead of
// std::string for FilePaths, but SetVar only accepts std::string.
#ifdef OS_WIN
#define MAKE_ABSOLUTE(x) \
  base::WideToUTF8(      \
      base::MakeAbsoluteFilePath(base::FilePath(base::UTF8ToWide(x))).value())
#else
#define MAKE_ABSOLUTE(x) base::MakeAbsoluteFilePath(base::FilePath(x)).value()
#endif

void XrBrowserTestBase::SetUp() {
  // Set the environment variable to use the mock OpenVR client.
  EXPECT_TRUE(env_->SetVar(kVrOverrideEnvVar, MAKE_ABSOLUTE(kVrOverrideVal)))
      << "Failed to set OpenVR mock client location environment variable";
  EXPECT_TRUE(
      env_->SetVar(kVrConfigPathEnvVar, MAKE_ABSOLUTE(kVrConfigPathVal)))
      << "Failed to set OpenVR config location environment variable";
  EXPECT_TRUE(env_->SetVar(kVrLogPathEnvVar, MAKE_ABSOLUTE(kVrLogPathVal)))
      << "Failed to set OpenVR log location environment variable";

  // Set any command line flags that subclasses have set, e.g. enabling WebVR
  // and OpenVR support.
  for (const auto& switch_string : append_switches_) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switch_string);
  }
  scoped_feature_list_.InitWithFeatures(enable_features_, {});

  InProcessBrowserTest::SetUp();
}

GURL XrBrowserTestBase::GetHtmlTestFile(const std::string& test_name) {
  return ui_test_utils::GetTestUrl(
      base::FilePath(FILE_PATH_LITERAL("xr/e2e_test_files/html")),
#ifdef OS_WIN
      base::FilePath(base::UTF8ToWide(test_name + ".html"))
#else
      base::FilePath(test_name + ".html")
#endif
          );
}

content::WebContents* XrBrowserTestBase::GetFirstTabWebContents() {
  return browser()->tab_strip_model()->GetWebContentsAt(0);
}

void XrBrowserTestBase::LoadUrlAndAwaitInitialization(const GURL& url) {
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(PollJavaScriptBoolean(
      "isInitializationComplete()", kPollTimeoutMedium,
      browser()->tab_strip_model()->GetActiveWebContents()))
      << "Timed out waiting for JavaScript test initialization.";
}

void XrBrowserTestBase::RunJavaScriptOrFail(
    const std::string& js_expression,
    content::WebContents* web_contents) {
  EXPECT_TRUE(content::ExecuteScript(web_contents, js_expression))
      << "Failed to run given JavaScript: " << js_expression;
}

bool XrBrowserTestBase::RunJavaScriptAndExtractBoolOrFail(
    const std::string& js_expression,
    content::WebContents* web_contents) {
  bool result;
  DLOG(ERROR) << "Run JavaScript: " << js_expression;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "window.domAutomationController.send(" + js_expression + ")", &result))
      << "Failed to run given JavaScript for bool: " << js_expression;
  return result;
}

std::string XrBrowserTestBase::RunJavaScriptAndExtractStringOrFail(
    const std::string& js_expression,
    content::WebContents* web_contents) {
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents,
      "window.domAutomationController.send(" + js_expression + ")", &result))
      << "Failed to run given JavaScript for string: " << js_expression;
  return result;
}

bool XrBrowserTestBase::PollJavaScriptBoolean(
    const std::string& bool_expression,
    const base::TimeDelta& timeout,
    content::WebContents* web_contents) {
  // Lambda used because otherwise BindRepeating gets confused about which
  // version of RunJavaScriptAndExtractBoolOrFail to use.
  return BlockOnConditionUnsafe(
      base::BindRepeating(
          [](XrBrowserTestBase* base, std::string expression,
             content::WebContents* contents) {
            return base->RunJavaScriptAndExtractBoolOrFail(expression,
                                                           contents);
          },
          this, bool_expression, web_contents),
      timeout);
}

void XrBrowserTestBase::PollJavaScriptBooleanOrFail(
    const std::string& bool_expression,
    const base::TimeDelta& timeout,
    content::WebContents* web_contents) {
  EXPECT_TRUE(PollJavaScriptBoolean(bool_expression, timeout, web_contents))
      << "Timed out polling JavaScript boolean expression: " << bool_expression;
}

bool XrBrowserTestBase::BlockOnConditionUnsafe(
    base::RepeatingCallback<bool()> condition,
    const base::TimeDelta& timeout,
    const base::TimeDelta& period) {
  base::Time start = base::Time::Now();
  bool successful = false;

  // Poll until the timeout has elapsed, or never if a debugger is attached
  // because that allows code to be slowly stepped through without breaking
  // tests.
  while (base::Time::Now() - start < timeout || base::debug::BeingDebugged()) {
    successful = condition.Run();
    if (successful) {
      break;
    }
    base::PlatformThread::Sleep(period);
  }
  return successful;
}

void XrBrowserTestBase::WaitOnJavaScriptStep(
    content::WebContents* web_contents) {
  // Make sure we aren't trying to wait on a JavaScript test step without the
  // code to do so.
  bool code_available = RunJavaScriptAndExtractBoolOrFail(
      "typeof javascriptDone !== 'undefined'", web_contents);
  EXPECT_TRUE(code_available) << "Attempted to wait on a JavaScript test step "
                              << "without the code to do so. You either forgot "
                              << "to import webxr_e2e.js or "
                              << "are incorrectly using a C++ function.";

  // Actually wait for the step to finish.
  bool success =
      PollJavaScriptBoolean("javascriptDone", kPollTimeoutLong, web_contents);

  // Check what state we're in to make sure javascriptDone wasn't called
  // because the test failed.
  XrBrowserTestBase::TestStatus test_status = CheckTestStatus(web_contents);
  if (!success || test_status == XrBrowserTestBase::TestStatus::STATUS_FAILED) {
    // Failure states: Either polling failed or polling succeeded, but because
    // the test failed.
    std::string reason;
    if (!success) {
      reason = "Timed out waiting for JavaScript step to finish.";
    } else {
      reason =
          "JavaScript testharness reported failure while waiting for "
          "JavaScript step to finish";
    }

    std::string result_string =
        RunJavaScriptAndExtractStringOrFail("resultString", web_contents);
    if (result_string == "") {
      reason +=
          " Did not obtain specific failure reason from JavaScript "
          "testharness.";
    } else {
      reason +=
          " JavaScript testharness reported failure reason: " + result_string;
    }
    FAIL() << reason;
  }

  // Reset the synchronization boolean.
  RunJavaScriptOrFail("javascriptDone = false", web_contents);
}

void XrBrowserTestBase::ExecuteStepAndWait(const std::string& step_function,
                                           content::WebContents* web_contents) {
  RunJavaScriptOrFail(step_function, web_contents);
  WaitOnJavaScriptStep(web_contents);
}

XrBrowserTestBase::TestStatus XrBrowserTestBase::CheckTestStatus(
    content::WebContents* web_contents) {
  std::string result_string =
      RunJavaScriptAndExtractStringOrFail("resultString", web_contents);
  bool test_passed =
      RunJavaScriptAndExtractBoolOrFail("testPassed", web_contents);
  if (test_passed) {
    return XrBrowserTestBase::TestStatus::STATUS_PASSED;
  } else if (!test_passed && result_string == "") {
    return XrBrowserTestBase::TestStatus::STATUS_RUNNING;
  }
  // !test_passed && result_string != ""
  return XrBrowserTestBase::TestStatus::STATUS_FAILED;
}

void XrBrowserTestBase::EndTest(content::WebContents* web_contents) {
  switch (CheckTestStatus(web_contents)) {
    case XrBrowserTestBase::TestStatus::STATUS_PASSED:
      break;
    case XrBrowserTestBase::TestStatus::STATUS_FAILED:
      FAIL() << "JavaScript testharness failed with reason: "
             << RunJavaScriptAndExtractStringOrFail("resultString",
                                                    web_contents);
      break;
    case XrBrowserTestBase::TestStatus::STATUS_RUNNING:
      FAIL() << "Attempted to end test in C++ without finishing in JavaScript.";
      break;
    default:
      FAIL() << "Received unknown test status.";
  }
}

void XrBrowserTestBase::AssertNoJavaScriptErrors(
    content::WebContents* web_contents) {
  if (CheckTestStatus(web_contents) ==
      XrBrowserTestBase::TestStatus::STATUS_FAILED) {
    FAIL() << "JavaScript testharness failed with reason: "
           << RunJavaScriptAndExtractStringOrFail("resultString", web_contents);
  }
}

void XrBrowserTestBase::RunJavaScriptOrFail(const std::string& js_expression) {
  RunJavaScriptOrFail(js_expression, GetFirstTabWebContents());
}

bool XrBrowserTestBase::RunJavaScriptAndExtractBoolOrFail(
    const std::string& js_expression) {
  return RunJavaScriptAndExtractBoolOrFail(js_expression,
                                           GetFirstTabWebContents());
}

std::string XrBrowserTestBase::RunJavaScriptAndExtractStringOrFail(
    const std::string& js_expression) {
  return RunJavaScriptAndExtractStringOrFail(js_expression,
                                             GetFirstTabWebContents());
}

bool XrBrowserTestBase::PollJavaScriptBoolean(
    const std::string& bool_expression,
    const base::TimeDelta& timeout) {
  return PollJavaScriptBoolean(bool_expression, timeout,
                               GetFirstTabWebContents());
}

void XrBrowserTestBase::PollJavaScriptBooleanOrFail(
    const std::string& bool_expression,
    const base::TimeDelta& timeout) {
  PollJavaScriptBooleanOrFail(bool_expression, timeout,
                              GetFirstTabWebContents());
}

void XrBrowserTestBase::WaitOnJavaScriptStep() {
  WaitOnJavaScriptStep(GetFirstTabWebContents());
}

void XrBrowserTestBase::ExecuteStepAndWait(const std::string& step_function) {
  ExecuteStepAndWait(step_function, GetFirstTabWebContents());
}

void XrBrowserTestBase::EndTest() {
  EndTest(GetFirstTabWebContents());
}

void XrBrowserTestBase::AssertNoJavaScriptErrors() {
  AssertNoJavaScriptErrors(GetFirstTabWebContents());
}

}  // namespace vr
