// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "chrome/browser/ui/views/web_apps/web_app_integration_browsertest_base.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "services/network/public/cpp/network_switches.h"

namespace web_app {

namespace {

const std::string kTestCaseFileName =
    "web_app_integration_browsertest_cases.csv";

// Returns the path of the requested file in the test data directory.
base::FilePath GetTestFileDir() {
  base::FilePath file_path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &file_path);
  file_path = file_path.Append(FILE_PATH_LITERAL("chrome"));
  file_path = file_path.Append(FILE_PATH_LITERAL("test"));
  file_path = file_path.Append(FILE_PATH_LITERAL("data"));
  return file_path.Append(FILE_PATH_LITERAL("web_apps"));
}

std::vector<std::string> BuildAllPlatformTestCaseSet() {
  return WebAppIntegrationBrowserTestBase::BuildAllPlatformTestCaseSet(
      GetTestFileDir(), kTestCaseFileName);
}

}  // anonymous namespace

class WebAppIntegrationBrowserTest
    : public InProcessBrowserTest,
      public WebAppIntegrationBrowserTestBase::TestDelegate,
      public testing::WithParamInterface<std::string> {
 public:
  WebAppIntegrationBrowserTest() : helper_(this) {}

  // InProcessBrowserTest
  void SetUp() override {
    helper_.SetUp(GetChromeTestDataDir());
    InProcessBrowserTest::SetUp();
  }

  // BrowserTestBase
  void SetUpOnMainThread() override { helper_.SetUpOnMainThread(); }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(embedded_test_server()->Start());
    command_line->AppendSwitchASCII(
        network::switches::kUnsafelyTreatInsecureOriginAsSecure,
        helper_.GetInstallableAppURL("site_a").GetOrigin().spec());
    command_line->AppendSwitchASCII(
        network::switches::kUnsafelyTreatInsecureOriginAsSecure,
        helper_.GetInstallableAppURL("site_b").GetOrigin().spec());
    command_line->AppendSwitchASCII(
        network::switches::kUnsafelyTreatInsecureOriginAsSecure,
        helper_.GetInstallableAppURL("site_c").GetOrigin().spec());
    command_line->AppendSwitchASCII(
        network::switches::kUnsafelyTreatInsecureOriginAsSecure,
        helper_.GetInstallableAppURL("site_a_foo").GetOrigin().spec());
    command_line->AppendSwitchASCII(
        network::switches::kUnsafelyTreatInsecureOriginAsSecure,
        helper_.GetInstallableAppURL("site_a_bar").GetOrigin().spec());
  }

  // WebAppIntegrationBrowserTestBase::TestDelegate
  Browser* CreateBrowser(Profile* profile) override {
    return InProcessBrowserTest::CreateBrowser(profile);
  }

  void AddBlankTabAndShow(Browser* browser) override {
    InProcessBrowserTest::AddBlankTabAndShow(browser);
  }

  net::EmbeddedTestServer* EmbeddedTestServer() override {
    return embedded_test_server();
  }

  std::vector<Profile*> GetAllProfiles() override {
    return std::vector<Profile*>{browser()->profile()};
  }

  bool IsSyncTest() override { return false; }

  bool UserSigninInternal() override {
    NOTREACHED();
    return false;
  }
  void TurnSyncOff() override { NOTREACHED(); }
  void TurnSyncOn() override { NOTREACHED(); }

  WebAppIntegrationBrowserTestBase helper_;
};

// This test is a part of the web app integration test suite, which is
// documented in //chrome/browser/ui/views/web_apps/README.md. For information
// about diagnosing, debugging and/or disabling tests, please look to the
// README file.
IN_PROC_BROWSER_TEST_P(WebAppIntegrationBrowserTest, Default) {
  helper_.ParseParams(GetParam());
  // Since this test framework differs from traditional browser tests, print
  // some useful information for sheriffs and developers to help identify,
  // diagnose, and disable failing tests.
  LOG(INFO) << helper_.BuildLogForTest(helper_.testing_actions(), IsSyncTest());

  for (auto& action : helper_.testing_actions()) {
    helper_.ExecuteAction(action);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebAppIntegrationBrowserTest,
                         testing::ValuesIn(BuildAllPlatformTestCaseSet()));

}  // namespace web_app
