// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/web_ui_mocha_browser_test.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/web_ui_test_data_source.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/page_type.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace {

bool WaitForTestToFinish(content::WebContents* web_contents) {
  content::DOMMessageQueue message_queue(web_contents);
  std::string message;
  do {
    if (!message_queue.WaitForMessage(&message)) {
      return false;
    }
  } while (message.compare("\"PENDING\"") == 0);

  return message.compare("\"SUCCESS\"") == 0;
}

}  // namespace

WebUIMochaBrowserTest::WebUIMochaBrowserTest()
    : test_loader_host_(chrome::kChromeUIWebUITestHost),
      test_loader_scheme_(content::kChromeUIScheme) {}

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

content::WebContents* WebUIMochaBrowserTest::GetWebContentsForSetup() {
  return chrome_test_utils::GetActiveWebContents(this);
}

void WebUIMochaBrowserTest::SetUpOnMainThread() {
  // Load browser_tests.pak.
  base::FilePath pak_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_ASSETS, &pak_path));
  pak_path = pak_path.AppendASCII("browser_tests.pak");
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      pak_path, ui::kScaleFactorNone);

  // Register the chrome://webui-test data source.
  content::WebContents* web_contents = GetWebContentsForSetup();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (test_loader_scheme_ == content::kChromeUIScheme) {
    webui::CreateAndAddWebUITestDataSource(profile);
  } else {
    // Must be untrusted
    webui::CreateAndAddUntrustedWebUITestDataSource(profile);
  }

  // Necessary setup for reporting code coverage metrics.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDevtoolsCodeCoverage)) {
    base::FilePath devtools_code_coverage_dir =
        command_line->GetSwitchValuePath(switches::kDevtoolsCodeCoverage);
    coverage_handler_ = std::make_unique<DevToolsAgentCoverageObserver>(
        devtools_code_coverage_dir);
  }
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

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
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

  bool success = WaitForTestToFinish(web_contents);

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
