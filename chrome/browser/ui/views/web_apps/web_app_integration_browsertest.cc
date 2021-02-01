// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/checked_ptr.h"
#include "base/path_service.h"
#include "chrome/browser/ui/views/web_apps/web_app_integration_browsertest_base.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "services/network/public/cpp/network_switches.h"

namespace web_app {

namespace {

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
      GetTestFileDir());
}

}  // anonymous namespace

class WebAppIntegrationBrowserTest
    : public InProcessBrowserTest,
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
    command_line->AppendSwitchASCII(
        network::switches::kUnsafelyTreatInsecureOriginAsSecure,
        helper_.GetInstallableAppURL().GetOrigin().spec());
  }

  WebAppIntegrationBrowserTestBase helper_;
};

IN_PROC_BROWSER_TEST_P(WebAppIntegrationBrowserTest, Default) {
  helper_.ParseParams(GetParam());

  for (auto& action : helper_.testing_actions()) {
    helper_.ExecuteAction(action);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebAppIntegrationBrowserTest,
                         testing::ValuesIn(BuildAllPlatformTestCaseSet()));

}  // namespace web_app
