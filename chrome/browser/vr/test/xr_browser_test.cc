// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/xr_browser_test.h"

#include <cstring>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#else
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#endif
namespace vr {

constexpr base::TimeDelta XrBrowserTestBase::kPollCheckIntervalShort;
constexpr base::TimeDelta XrBrowserTestBase::kPollCheckIntervalLong;
constexpr base::TimeDelta XrBrowserTestBase::kPollTimeoutShort;
constexpr base::TimeDelta XrBrowserTestBase::kPollTimeoutMedium;
constexpr base::TimeDelta XrBrowserTestBase::kPollTimeoutLong;
constexpr char XrBrowserTestBase::kOpenXrConfigPathEnvVar[];
constexpr char XrBrowserTestBase::kOpenXrConfigPathVal[];
constexpr char XrBrowserTestBase::kTestFileDir[];
constexpr char XrBrowserTestBase::kSwitchIgnoreRuntimeRequirements[];
const std::vector<std::string> XrBrowserTestBase::kRequiredTestSwitches{
    "enable-gpu", "enable-pixel-output-in-tests",
    "run-through-xr-wrapper-script", "enable-unsafe-swiftshader"};
const std::vector<std::pair<std::string, std::string>>
    XrBrowserTestBase::kRequiredTestSwitchesWithValues{
        std::pair<std::string, std::string>("test-launcher-jobs", "1")};

XrBrowserTestBase::XrBrowserTestBase() : env_(base::Environment::Create()) {
  enable_features_.push_back(features::kLogJsConsoleMessages);
}

XrBrowserTestBase::~XrBrowserTestBase() = default;

base::FilePath::StringType UTF8ToWideIfNecessary(std::string input) {
#if BUILDFLAG(IS_WIN)
  return base::UTF8ToWide(input);
#else
  return input;
#endif  // BUILDFLAG(IS_WIN)
}

std::string WideToUTF8IfNecessary(base::FilePath::StringType input) {
#if BUILDFLAG(IS_WIN)
  return base::WideToUTF8(input);
#else
  return input;
#endif  // BUILDFLAG(IS_WIN)
}

// Returns an std::string consisting of the given path relative to the test
// executable's path, e.g. if the executable is in out/Debug and the given path
// is "test", the returned string should be out/Debug/test.
std::string MakeExecutableRelative(const char* path) {
  base::FilePath executable_path;
  EXPECT_TRUE(
      base::PathService::Get(base::BasePathKey::FILE_EXE, &executable_path));
  executable_path = executable_path.DirName();
  // We need an std::string that is an absolute file path, which requires
  // platform-specific logic since Windows uses std::wstring instead of
  // std::string for FilePaths, but SetVar only accepts std::string.
  return WideToUTF8IfNecessary(
      base::MakeAbsoluteFilePath(
          executable_path.Append(base::FilePath(UTF8ToWideIfNecessary(path))))
          .value());
}

void XrBrowserTestBase::SetUp() {
  // Check whether the required flags were passed to the test - without these,
  // we can fail in ways that are non-obvious, so fail more explicitly here if
  // they aren't present.
  auto* cmd_line = base::CommandLine::ForCurrentProcess();
  for (auto req_switch : kRequiredTestSwitches) {
    ASSERT_TRUE(cmd_line->HasSwitch(req_switch))
        << "Missing switch " << req_switch << " required to run tests properly";
  }
  for (auto req_switch_pair : kRequiredTestSwitchesWithValues) {
    ASSERT_TRUE(cmd_line->HasSwitch(req_switch_pair.first))
        << "Missing switch " << req_switch_pair.first
        << " required to run tests properly";
    ASSERT_TRUE(cmd_line->GetSwitchValueASCII(req_switch_pair.first) ==
                req_switch_pair.second)
        << "Have required switch " << req_switch_pair.first
        << ", but not required value " << req_switch_pair.second;
  }

  // Get the set of runtime requirements to ignore.
  if (cmd_line->HasSwitch(kSwitchIgnoreRuntimeRequirements)) {
    auto reqs = cmd_line->GetSwitchValueASCII(kSwitchIgnoreRuntimeRequirements);
    if (reqs != "") {
      for (auto req : base::SplitString(
               reqs, ",", base::WhitespaceHandling::TRIM_WHITESPACE,
               base::SplitResult::SPLIT_WANT_NONEMPTY)) {
        ignored_requirements_.insert(req);
      }
    }
  }

  // Check whether we meet all runtime requirements for this test.
  XR_CONDITIONAL_SKIP_PRETEST(runtime_requirements_, ignored_requirements_,
                              &test_skipped_at_startup_)

  // Set the environment variable to use the mock OpenXR client.
  // If the kOpenXrConfigPathEnvVar environment variable is set, the OpenXR
  // loader will look for the OpenXR runtime specified in that json file. The
  // json file contains the path to the runtime, relative to the json file
  // itself. Otherwise, the OpenXR loader loads the active OpenXR runtime
  // installed on the system, which is specified by a registry key.
  ASSERT_TRUE(env_->SetVar(kOpenXrConfigPathEnvVar,
                           MakeExecutableRelative(kOpenXrConfigPathVal)))
      << "Failed to set OpenXR JSON location environment variable";

  // Set any command line flags that subclasses have set, e.g. enabling features
  // or specific runtimes.
  for (const auto& switch_string : append_switches_) {
    cmd_line->AppendSwitch(switch_string);
  }

  for (const auto& blink_feature : enable_blink_features_) {
    cmd_line->AppendSwitchASCII(switches::kEnableBlinkFeatures, blink_feature);
  }

#if defined(MEMORY_SANITIZER)
  // Surprisingly enough, there is no constant for this.
  // TODO(crbug.com/40564748): Once generic wrapper scripts for tests are
  // supported, move the logic to avoid passing --enable-gpu to GN.
  if (cmd_line->HasSwitch("enable-gpu")) {
    LOG(WARNING) << "Ignoring --enable-gpu switch, which is incompatible with "
                    "MSan builds.";
    cmd_line->RemoveSwitch("enable-gpu");
  }
#endif

  scoped_feature_list_.InitWithFeatures(enable_features_, disable_features_);

  PlatformBrowserTest::SetUp();
}

void XrBrowserTestBase::TearDown() {
  if (test_skipped_at_startup_) {
    // Since we didn't complete startup, no need to do teardown, either. Doing
    // so can result in hitting a DCHECK.
    return;
  }
  PlatformBrowserTest::TearDown();
}

XrBrowserTestBase::RuntimeType XrBrowserTestBase::GetRuntimeType() const {
  return XrBrowserTestBase::RuntimeType::RUNTIME_NONE;
}

GURL XrBrowserTestBase::GetUrlForFile(const std::string& test_name) {
  // GetURL requires that the path start with /.
  return GetEmbeddedServer()->GetURL(std::string("/") + kTestFileDir +
                                     test_name + ".html");
}

net::EmbeddedTestServer* XrBrowserTestBase::GetEmbeddedServer() {
  if (server_ == nullptr) {
    server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::Type::TYPE_HTTPS);
    // We need to serve from the root in order for the inclusion of the
    // test harness from //third_party to work.
    server_->ServeFilesFromSourceDirectory(".");
    EXPECT_TRUE(server_->Start()) << "Failed to start embedded test server";
  }
  return server_.get();
}

content::WebContents* XrBrowserTestBase::GetCurrentWebContents() {
#if !BUILDFLAG(IS_ANDROID)
  // `chrome_test_utils::GetActiveWebContents()` doesn't properly account for
  // the presence of an incognito browser, and only looks in the browser
  // returned by the base class, which doesn't get overridden by the incognito
  // browser.
  if (incognito_) {
    Browser* incognito_browser = chrome::FindTabbedBrowser(
        browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/false),
        /*match_original_profiles=*/false);
    return incognito_browser->tab_strip_model()->GetActiveWebContents();
  }
#endif
  return chrome_test_utils::GetActiveWebContents(this);
}

void XrBrowserTestBase::SetIncognito() {
  incognito_ = true;
  OpenNewTab(url::kAboutBlankURL);
}

void XrBrowserTestBase::OpenNewTab(const std::string& url) {
  OpenNewTab(url, incognito_);
}

void XrBrowserTestBase::OpenNewTab(const std::string& url, bool incognito) {
#if BUILDFLAG(IS_ANDROID)
  NOTREACHED();
#else
  if (incognito) {
    OpenURLOffTheRecord(browser()->profile(), GURL(url));
  } else {
    // -1 is a special index value used to append to the end of the tab list.
    chrome::AddTabAt(browser(), GURL(url), /*index=*/-1, /*foreground=*/true);
  }
#endif
}

void XrBrowserTestBase::LoadFileAndAwaitInitialization(
    const std::string& test_name) {
  GURL url = GetUrlForFile(test_name);
  ASSERT_TRUE(content::NavigateToURL(GetCurrentWebContents(), url));
  ASSERT_TRUE(PollJavaScriptBoolean("isInitializationComplete()",
                                    kPollTimeoutMedium,
                                    GetCurrentWebContents()))
      << "Timed out waiting for JavaScript test initialization.";

#if BUILDFLAG(IS_WIN)
  // Now that the browser is opened and has focus, keep track of this window so
  // that we can restore the proper focus after entering each session. This is
  // required for tests that create multiple sessions to work properly.
  hwnd_ = GetForegroundWindow();
#endif
}

void XrBrowserTestBase::RunJavaScriptOrFail(
    const std::string& js_expression,
    content::WebContents* web_contents) {
  if (javascript_failed_) {
    LogJavaScriptFailure();
    return;
  }

  ASSERT_TRUE(content::ExecJs(web_contents, js_expression))
      << "Failed to run given JavaScript: " << js_expression;
}

bool XrBrowserTestBase::RunJavaScriptAndExtractBoolOrFail(
    const std::string& js_expression,
    content::WebContents* web_contents) {
  if (javascript_failed_) {
    LogJavaScriptFailure();
    return false;
  }

  DLOG(INFO) << "Run JavaScript: " << js_expression;
  return content::EvalJs(web_contents, js_expression).ExtractBool();
}

std::string XrBrowserTestBase::RunJavaScriptAndExtractStringOrFail(
    const std::string& js_expression,
    content::WebContents* web_contents) {
  if (javascript_failed_) {
    LogJavaScriptFailure();
    return "";
  }

  return content::EvalJs(web_contents, js_expression).ExtractString();
}

bool XrBrowserTestBase::PollJavaScriptBoolean(
    const std::string& bool_expression,
    const base::TimeDelta& timeout,
    content::WebContents* web_contents) {
  bool result = false;
  base::RunLoop wait_loop(base::RunLoop::Type::kNestableTasksAllowed);
  // Lambda used because otherwise BindRepeating gets confused about which
  // version of RunJavaScriptAndExtractBoolOrFail to use.
  BlockOnCondition(base::BindRepeating(
                       [](XrBrowserTestBase* base, std::string expression,
                          content::WebContents* contents) {
                         return base->RunJavaScriptAndExtractBoolOrFail(
                             expression, contents);
                       },
                       this, bool_expression, web_contents),
                   &result, &wait_loop, base::Time::Now(), timeout);
  wait_loop.Run();
  return result;
}

void XrBrowserTestBase::PollJavaScriptBooleanOrFail(
    const std::string& bool_expression,
    const base::TimeDelta& timeout,
    content::WebContents* web_contents) {
  ASSERT_TRUE(PollJavaScriptBoolean(bool_expression, timeout, web_contents))
      << "Timed out polling JavaScript boolean expression: " << bool_expression;
}

void XrBrowserTestBase::BlockOnCondition(
    base::RepeatingCallback<bool()> condition,
    bool* result,
    base::RunLoop* wait_loop,
    const base::Time& start_time,
    const base::TimeDelta& timeout,
    const base::TimeDelta& period) {
  if (!*result) {
    *result = condition.Run();
  }

  if (*result) {
    if (wait_loop->running()) {
      wait_loop->Quit();
      return;
    }
    // In the case where the condition is met fast enough that the given
    // RunLoop hasn't started yet, spin until it's available.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&XrBrowserTestBase::BlockOnCondition,
                       base::Unretained(this), std::move(condition),
                       base::Unretained(result), base::Unretained(wait_loop),
                       start_time, timeout, period));
    return;
  }

  if (base::Time::Now() - start_time > timeout &&
      !base::debug::BeingDebugged()) {
    wait_loop->Quit();
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&XrBrowserTestBase::BlockOnCondition,
                     base::Unretained(this), std::move(condition),
                     base::Unretained(result), base::Unretained(wait_loop),
                     start_time, timeout, period),
      period);
}

void XrBrowserTestBase::WaitOnJavaScriptStep(
    content::WebContents* web_contents) {
  // Make sure we aren't trying to wait on a JavaScript test step without the
  // code to do so.
  bool code_available = RunJavaScriptAndExtractBoolOrFail(
      "typeof javascriptDone !== 'undefined'", web_contents);
  ASSERT_TRUE(code_available) << "Attempted to wait on a JavaScript test step "
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
    if (result_string.empty()) {
      reason +=
          " Did not obtain specific failure reason from JavaScript "
          "testharness.";
    } else {
      reason +=
          " JavaScript testharness reported failure reason: " + result_string;
    }
    // Store that we've failed waiting for a JavaScript step so we can abort
    // further attempts to run JavaScript, which has the potential to do weird
    // things and produce non-useful output due to JavaScript code continuing
    // to run when it's in a known bad state.
    // This is a workaround for the fact that FAIL() and other gtest macros that
    // cause test failures only abort the current function. Thus, a failure here
    // will show up as a test failure, but there's nothing that actually stops
    // the test from continuing to run since FAIL() is not being called in the
    // main test body.
    javascript_failed_ = true;
    // Newlines to help the failure reason stick out.
    LOG(ERROR) << "\n\n\nvvvvvvvvvvvvvvvvv Useful Stack vvvvvvvvvvvvvvvvv\n\n";
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
  } else if (!test_passed && result_string.empty()) {
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
    case XrBrowserTestBase::TestStatus::STATUS_RUNNING:
      FAIL() << "Attempted to end test in C++ without finishing in JavaScript.";
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
  RunJavaScriptOrFail(js_expression, GetCurrentWebContents());
}

bool XrBrowserTestBase::RunJavaScriptAndExtractBoolOrFail(
    const std::string& js_expression) {
  return RunJavaScriptAndExtractBoolOrFail(js_expression,
                                           GetCurrentWebContents());
}

std::string XrBrowserTestBase::RunJavaScriptAndExtractStringOrFail(
    const std::string& js_expression) {
  return RunJavaScriptAndExtractStringOrFail(js_expression,
                                             GetCurrentWebContents());
}

bool XrBrowserTestBase::PollJavaScriptBoolean(
    const std::string& bool_expression,
    const base::TimeDelta& timeout) {
  return PollJavaScriptBoolean(bool_expression, timeout,
                               GetCurrentWebContents());
}

void XrBrowserTestBase::PollJavaScriptBooleanOrFail(
    const std::string& bool_expression,
    const base::TimeDelta& timeout) {
  PollJavaScriptBooleanOrFail(bool_expression, timeout,
                              GetCurrentWebContents());
}

void XrBrowserTestBase::WaitOnJavaScriptStep() {
  WaitOnJavaScriptStep(GetCurrentWebContents());
}

void XrBrowserTestBase::ExecuteStepAndWait(const std::string& step_function) {
  ExecuteStepAndWait(step_function, GetCurrentWebContents());
}

void XrBrowserTestBase::EndTest() {
  EndTest(GetCurrentWebContents());
}

void XrBrowserTestBase::AssertNoJavaScriptErrors() {
  AssertNoJavaScriptErrors(GetCurrentWebContents());
}

void XrBrowserTestBase::LogJavaScriptFailure() {
  LOG(ERROR) << "HEY! LISTEN! Not running requested JavaScript due to previous "
                "failure. Failures below this are likely garbage. Look for the "
                "useful stack above.";
}

}  // namespace vr
