// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/android/android_browser_test.h"

#include "base/command_line.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/common/extension_features.h"
#endif

namespace {
AndroidBrowserTest* g_current_test = nullptr;
}  // namespace

AndroidBrowserTest::AndroidBrowserTest() {
  CreateTestServer(base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  // Allow unpacked extensions without developer mode for testing.
  feature_list_.InitAndDisableFeature(
      extensions_features::kExtensionDisableUnsupportedDeveloper);
#endif
  g_current_test = this;
}

AndroidBrowserTest::~AndroidBrowserTest() {
  g_current_test = nullptr;
}

AndroidBrowserTest* AndroidBrowserTest::GetCurrent() {
  return g_current_test;
}

void AndroidBrowserTest::SetUp() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  SetUpCommandLine(command_line);
  SetUpDefaultCommandLine(command_line);
  ASSERT_TRUE(test_launcher_utils::CreateUserDataDir(&temp_user_data_dir_));

  InitializeHTTPSTestServer();
  embedded_https_test_server().AddDefaultHandlers(GetChromeTestDataDir());

  ASSERT_TRUE(SetUpUserDataDirectory());

  BrowserTestBase::SetUp();
}

void AndroidBrowserTest::SetUpDefaultCommandLine(
    base::CommandLine* command_line) {
  test_launcher_utils::PrepareBrowserCommandLineForTests(command_line);
  test_launcher_utils::PrepareBrowserCommandLineForBrowserTests(
      command_line, /*open_about_blank_on_launch=*/true);
}

bool AndroidBrowserTest::SetUpUserDataDirectory() {
  return true;
}

void AndroidBrowserTest::PreRunTestOnMainThread() {}

void AndroidBrowserTest::PostRunTestOnMainThread() {
  for (TabModel* model : TabModelList::models()) {
    if (model->GetTabCount()) {
      model->ForceCloseAllTabs();
    }
    ASSERT_EQ(0, model->GetTabCount());
  }

  // Run any shutdown events from closing tabs.
  content::RunAllPendingInMessageLoop();
}

// static
size_t AndroidBrowserTest::GetTestPreCount() {
  constexpr std::string_view kPreTestPrefix = "PRE_";
  std::string_view test_name =
      testing::UnitTest::GetInstance()->current_test_info()->name();
  size_t count = 0;
  while (base::StartsWith(test_name, kPreTestPrefix)) {
    ++count;
    test_name = test_name.substr(kPreTestPrefix.size());
  }
  return count;
}

base::FilePath AndroidBrowserTest::GetChromeTestDataDir() const {
  return chrome_test_utils::GetChromeTestDataDir();
}

Profile* AndroidBrowserTest::GetProfile() const {
  for (TabModel* model : TabModelList::models()) {
    if (model->GetProfile()) {
      return model->GetProfile();
    }
  }
  return nullptr;
}
