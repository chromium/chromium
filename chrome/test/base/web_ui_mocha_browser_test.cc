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
    : test_loader_host_(chrome::kChromeUIWebUITestHost) {}

WebUIMochaBrowserTest::~WebUIMochaBrowserTest() = default;

void WebUIMochaBrowserTest::set_test_loader_host(const std::string& host) {
  test_loader_host_ = host;
}

void WebUIMochaBrowserTest::SetUpOnMainThread() {
  // Load browser_tests.pak.
  base::FilePath pak_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_ASSETS, &pak_path));
  pak_path = pak_path.AppendASCII("browser_tests.pak");
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      pak_path, ui::kScaleFactorNone);

  // Register the chrome://webui-test data source.
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  webui::CreateAndAddWebUITestDataSource(profile);

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
  RunTest(file, trigger, /*requires_focus=*/false, /*skip_test_loader=*/false);
}

void WebUIMochaBrowserTest::RunTest(const std::string& file,
                                    const std::string& trigger,
                                    const bool& requires_focus,
                                    const bool& skip_test_loader) {
  // Construct URL to load the test module file.
  GURL url(
      skip_test_loader
          ? std::string("chrome://" + test_loader_host_)
          : std::string(
                "chrome://" + test_loader_host_ +
                "/test_loader.html?adapter=mocha_adapter_simple.js&module=") +
                file);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(web_contents);

  if (requires_focus) {
    web_contents->Focus();
  }

  // Check that the navigation does not point to an error page like
  // chrome-error://chromewebdata/.
  bool is_error_page =
      web_contents->GetController().GetLastCommittedEntry()->GetPageType() ==
      content::PageType::PAGE_TYPE_ERROR;
  if (is_error_page) {
    FAIL() << "Navigation to '" << url.spec() << "' failed.";
  }

  if (skip_test_loader) {
    // Perform setup steps normally done by test_loader.html.
    // TODO(dpapad): Figure out why moving this logic in a private
    // SimulateTestloaderSteps() helper method causes ASSERT_TRUE() failures to
    // not propagate to the parent caller. Inlining logic here as a workaround.

    // Step 1: Programmatically loads mocha.js and mocha_adapter_simple.js.
    std::string loadMochaScript(base::StringPrintf(
        R"(
      async function load() {
        await import('chrome://%s/mocha.js');
        await import('chrome://%s/mocha_adapter_simple.js');
      }
      load();)",
        chrome::kChromeUIWebUITestHost, chrome::kChromeUIWebUITestHost));
    ASSERT_TRUE(ExecJs(web_contents->GetPrimaryMainFrame(), loadMochaScript));

    // Step 2: Programmatically loads the Mocha test file.
    std::string loadTestModuleScript(
        base::StringPrintf("import('chrome://%s/%s');",
                           chrome::kChromeUIWebUITestHost, file.c_str()));
    ASSERT_TRUE(
        ExecJs(web_contents->GetPrimaryMainFrame(), loadTestModuleScript));
  }

  // Trigger the Mocha tests, and wait for completion.
  ASSERT_TRUE(ExecJs(web_contents->GetPrimaryMainFrame(), trigger));
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
    FAIL() << "Mocha test failures detected in file: " << file
           << ", triggered by '" << trigger << "'";
  }
}

void WebUIMochaBrowserTest::RunTestWithoutTestLoader(
    const std::string& file,
    const std::string& trigger) {
  RunTest(file, trigger, /*requires_focus=*/false, /*skip_test_loader=*/true);
}

void WebUIMochaFocusTest::RunTest(const std::string& file,
                                  const std::string& trigger) {
  WebUIMochaBrowserTest::RunTest(file, trigger, /*requires_focus=*/true,
                                 /*skip_test_loader=*/false);
}
