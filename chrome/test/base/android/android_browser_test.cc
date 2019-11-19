// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/android/android_browser_test.h"

#include "base/command_line.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "content/public/test/test_utils.h"

AndroidBrowserTest::AndroidBrowserTest() = default;
AndroidBrowserTest::~AndroidBrowserTest() = default;

void AndroidBrowserTest::SetUp() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  SetUpCommandLine(command_line);
  SetUpDefaultCommandLine(command_line);
  ASSERT_TRUE(test_launcher_utils::CreateUserDataDir(&temp_user_data_dir_));

  BrowserTestBase::SetUp();
}

void AndroidBrowserTest::SetUpDefaultCommandLine(
    base::CommandLine* command_line) {
  test_launcher_utils::PrepareBrowserCommandLineForTests(command_line);
  test_launcher_utils::PrepareBrowserCommandLineForBrowserTests(
      command_line, /*open_about_blank_on_launch=*/true);
}

void AndroidBrowserTest::PreRunTestOnMainThread() {
}

void AndroidBrowserTest::PostRunTestOnMainThread() {
  for (size_t i = 0; i < TabModelList::size(); ++i) {
    while (TabModelList::get(i)->GetTabCount())
      TabModelList::get(i)->CloseTabAt(0);
  }

  // Run any shutdown events from closing tabs.
  content::RunAllPendingInMessageLoop();
}
