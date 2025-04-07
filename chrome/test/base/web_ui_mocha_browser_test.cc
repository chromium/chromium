// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/web_ui_mocha_browser_test.h"

#include <optional>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/gtest_sub_test_results.h"
#include "base/test/gtest_tags.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/web_ui_test_data_source.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/page_type.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/android/android_ui_test_utils.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#endif

SubTestResult::SubTestResult() = default;
SubTestResult::SubTestResult(const SubTestResult& other) = default;
SubTestResult::SubTestResult(SubTestResult&& other)
    : name(std::move(other.name)),
      duration(other.duration),
      failure_reason(std::move(other.failure_reason)) {}
SubTestResult& SubTestResult::operator=(SubTestResult&& other) {
  name = std::move(other.name);
  duration = other.duration;
  failure_reason = std::move(other.failure_reason);
  return *this;
}
SubTestResult::~SubTestResult() = default;

namespace webui {

void CanonicalizeTestName(std::string* test_name) {
  if (!test_name) {
    return;
  }
  std::replace_if(
      test_name->begin(), test_name->end(),
      [](char c) {
        return !(base::IsAsciiAlpha(c) || base::IsAsciiDigit(c) || c == '_');
      },
      '_');
}

std::tuple<bool, std::vector<SubTestResult>> ProcessMessagesFromJsTest(
    content::WebContents* web_contents) {
  content::DOMMessageQueue message_queue(web_contents);
  std::vector<SubTestResult> results;
  std::string message;
  while (message_queue.WaitForMessage(&message)) {
    if (message == "\"SUCCESS\"") {
      return std::make_tuple(true, results);
    } else if (message == "\"FAILURE\"") {
      return std::make_tuple(false, results);
    }

    std::optional<base::Value> msg = base::JSONReader::Read(message);

    SubTestResult sub_test_result;
    std::string* test_name = msg->GetDict().FindString("fullTitle");
    CHECK(test_name);
    sub_test_result.name = *test_name;
    CanonicalizeTestName(&sub_test_result.name);

    std::optional<int> duration = msg->GetDict().FindInt("duration");
    CHECK(duration);
    sub_test_result.duration = *duration;

    std::string* failure_reason = msg->GetDict().FindString("failureReason");
    if (failure_reason) {
      sub_test_result.failure_reason.emplace(*failure_reason);
    }

    results.push_back(std::move(sub_test_result));
  }
  NOTREACHED();
}

}  // namespace webui

WebUIMochaBrowserTest::WebUIMochaBrowserTest()
    : test_loader_host_(chrome::kChromeUIWebUITestHost),
      test_loader_scheme_(content::kChromeUIScheme),
// XmlUnitTestResultPrinter is not supported on Android.
#if BUILDFLAG(IS_ANDROID)
      sub_test_reporter_(nullptr)
#else
      sub_test_reporter_(std::make_unique<SubTestReporter>())
#endif
{
}

WebUIMochaBrowserTest::~WebUIMochaBrowserTest() = default;

void WebUIMochaBrowserTest::set_test_loader_host(const std::string& host) {
  test_loader_host_ = host;
}

void WebUIMochaBrowserTest::set_test_loader_scheme(const std::string& scheme) {
  // Only chrome:// and chrome-untrusted:// are supported.
  CHECK(scheme == content::kChromeUIScheme ||
        scheme == content::kChromeUIUntrustedScheme);
  test_loader_scheme_ = scheme;
}

Profile* WebUIMochaBrowserTest::GetProfileForSetup() {
  return chrome_test_utils::GetProfile(this);
}

void WebUIMochaBrowserTest::SetUpOnMainThread() {
  // Load browser_tests.pak.
  base::FilePath pak_path;
#if BUILDFLAG(IS_ANDROID)
  // on Android all pak files are inside the paks folder.
  ASSERT_TRUE(base::PathService::Get(base::DIR_ANDROID_APP_DATA, &pak_path));
  pak_path = pak_path.Append(FILE_PATH_LITERAL("paks"));
#else
  ASSERT_TRUE(base::PathService::Get(base::DIR_ASSETS, &pak_path));
#endif  // BUILDFLAG(IS_ANDROID)
  pak_path = pak_path.AppendASCII("browser_tests.pak");
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      pak_path, ui::kScaleFactorNone);

  // Register the chrome://webui-test data source.
  Profile* profile = GetProfileForSetup();
  if (test_loader_scheme_ == content::kChromeUIScheme) {
    webui::CreateAndAddWebUITestDataSource(profile);
  } else {
    // Must be untrusted
    webui::CreateAndAddUntrustedWebUITestDataSource(profile);
  }

#if !BUILDFLAG(IS_ANDROID)
  // Necessary setup for reporting code coverage metrics.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDevtoolsCodeCoverage)) {
    base::FilePath devtools_code_coverage_dir =
        command_line->GetSwitchValuePath(switches::kDevtoolsCodeCoverage);
    coverage_handler_ = std::make_unique<DevToolsAgentCoverageObserver>(
        devtools_code_coverage_dir);
  }
#endif
}

void WebUIMochaBrowserTest::RunTest(const std::string& file,
                                    const std::string& trigger) {
  RunTest(file, trigger, /*skip_test_loader=*/false);
}

void WebUIMochaBrowserTest::OnWebContentsAvailable(
    content::WebContents* web_contents) {
  // Nothing to do here. Should be overridden by any subclasses if additional
  // setup steps are needed.
}

void WebUIMochaBrowserTest::RunTest(const std::string& file,
                                    const std::string& trigger,
                                    const bool& skip_test_loader) {
  // Construct URL to load the test module file.
  GURL url(
      skip_test_loader
          ? std::string(test_loader_scheme_ + "://" + test_loader_host_)
          : std::string(
                test_loader_scheme_ + "://" + test_loader_host_ +
                "/test_loader.html?adapter=mocha_adapter_simple.js&module=") +
                file);

#if BUILDFLAG(IS_ANDROID)
  android_ui_test_utils::OpenUrlInNewTab(
      chrome_test_utils::GetProfile(this),
      chrome_test_utils::GetActiveWebContents(this), url);
#else
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
#endif
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(web_contents);

  // Check that the navigation does not point to an error page like
  // chrome-error://chromewebdata/.
  bool is_error_page =
      web_contents->GetController().GetLastCommittedEntry()->GetPageType() ==
      content::PageType::PAGE_TYPE_ERROR;
  if (is_error_page) {
    FAIL() << "Navigation to '" << url.spec() << "' failed.";
  }

  // Hook for subclasses that need access to the WebContents before the Mocha
  // test runs.
  OnWebContentsAvailable(web_contents);

  ASSERT_TRUE(
      RunTestOnWebContents(web_contents, file, trigger, skip_test_loader));
}

testing::AssertionResult WebUIMochaBrowserTest::RunTestOnWebContents(
    content::WebContents* web_contents,
    const std::string& file,
    const std::string& trigger,
    const bool& skip_test_loader) {
  testing::AssertionResult result(testing::AssertionFailure());

  if (skip_test_loader) {
    // Perform setup steps normally done by test_loader.html.
    result = SimulateTestLoader(web_contents, file);
    if (!result) {
      return result;
    }
  }

  // Trigger the Mocha tests, and wait for completion.
  result = ExecJs(web_contents->GetPrimaryMainFrame(), trigger);
  if (!result) {
    return result;
  }

  // Receive messages from JS.
  auto [success, sub_test_results] =
      webui::ProcessMessagesFromJsTest(web_contents);

  // Report individual JS test results if reporting is enabled.
  if (sub_test_reporter_) {
    // ResultDB limits test identifiers to 512 bytes. However, GTest code isn't
    // privy to the exact schema used (for that, see TestResultsTracker::
    // SaveSummaryAsJSON). Here, it is simply assumed that test name length can
    // be estimated as a sum of the lengths of the GTest fixture name, GTest
    // test name, and Mocha JS test name, plus a few extra bytes for delimiters.
    // Retrieve GTest fixture name and GTest test name to build an estimate.
    const testing::TestInfo* info =
        testing::UnitTest::GetInstance()->current_test_info();
    CHECK(info);

    for (const auto& sub_test_result : sub_test_results) {
      // Estimate the final test identifier length. Allocate 3 bytes for
      // delimiters.
      size_t estimate = strlen(info->name()) + strlen(info->test_suite_name()) +
                        sub_test_result.name.size() + 3ul;

      if (estimate > 512ul) {
        testing::Message msg;
        msg << "Test name too long. Test identifier size estimate is "
            << estimate << ". ResultDB limits test identifiers to 512 bytes. "
            << "Please reduce total test name length by at least "
            << (estimate - 512ul) << " bytes.  name=\"" << info->name()
            << "\", test_suite_name=\"" << info->test_suite_name()
            << "\", js_test_name=\"" << sub_test_result.name << "\"";
        return testing::AssertionFailure(msg);
      }

      sub_test_reporter_->Report(sub_test_result.name, sub_test_result.duration,
                                 sub_test_result.failure_reason);
    }
  }

#if !BUILDFLAG(IS_ANDROID)
  // Report code coverage metrics.
  if (coverage_handler_ && coverage_handler_->CoverageEnabled()) {
    const std::string& full_test_name = base::StrCat({
        ::testing::UnitTest::GetInstance()
            ->current_test_info()
            ->test_suite_name(),
        ::testing::UnitTest::GetInstance()->current_test_info()->name(),
    });
    coverage_handler_->CollectCoverage(full_test_name);
  }
#endif

  if (!success) {
    testing::Message msg;
    msg << "Mocha test failures detected in file: " << file
        << ", triggered by '" << trigger << "'";
    return testing::AssertionFailure(msg);
  }

  return testing::AssertionSuccess();
}

void WebUIMochaBrowserTest::RunTestWithoutTestLoader(
    const std::string& file,
    const std::string& trigger) {
  RunTest(file, trigger, /*skip_test_loader=*/true);
}

void WebUIMochaBrowserTest::SetSubTestResultReportingEnabled(bool enabled) {
  if (enabled) {
    sub_test_reporter_ = std::make_unique<SubTestReporter>();
  } else {
    sub_test_reporter_.reset();
  }
}

testing::AssertionResult WebUIMochaBrowserTest::SimulateTestLoader(
    content::WebContents* web_contents,
    const std::string& file) {
  // Step 1: Programmatically loads mocha.js and mocha_adapter_simple.js.
  std::string loadMochaScript(base::StringPrintf(
      R"(
async function load() {
  await import('//%s/mocha.js');
  await import('//%s/mocha_adapter_simple.js');
}
load();
)",
      chrome::kChromeUIWebUITestHost, chrome::kChromeUIWebUITestHost));

  testing::AssertionResult result =
      ExecJs(web_contents->GetPrimaryMainFrame(), loadMochaScript);
  if (!result) {
    return result;
  }

  // Step 2: Programmatically loads the Mocha test file.
  std::string loadTestModuleScript(base::StringPrintf(
      "import('//%s/%s');", chrome::kChromeUIWebUITestHost, file.c_str()));
  return ExecJs(web_contents->GetPrimaryMainFrame(), loadTestModuleScript);
}

void WebUIMochaFocusTest::OnWebContentsAvailable(
    content::WebContents* web_contents) {
  // Focus the web contents before running the test, used for tests running as
  // interactive_ui_tests.
  web_contents->Focus();
}

void SubTestReporter::Report(
    std::string_view name,
    testing::TimeInMillis elapsed_time,
    std::optional<std::string_view> failure_message) const {
  base::AddSubTestResult(name, elapsed_time, failure_message);
}
