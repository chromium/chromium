// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/test/browser_ui_test_base.h"

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/strings/strcat.h"
#include "base/test/launcher/test_launcher.h"
#include "base/test/test_switches.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"

void BrowserUiTestBase::Invoke() {
  // Switch for BrowserUiTest.Invoke to spawn a subprocess testing the provided
  // argument under a consistent setup.
  static constexpr char kUiSwitch[] = "ui";
  // Pattern to search in test names that indicate support for UI testing.
  static constexpr char kUiPattern[] = "InvokeUi_";

  base::CommandLine invoker = *base::CommandLine::ForCurrentProcess();
  // Don't create test output for the subprocess (the paths will conflict).
  invoker.RemoveSwitch(switches::kTestLauncherOutput);
  const std::string ui_name = invoker.GetSwitchValueASCII(kUiSwitch);

  std::set<std::string> ui_cases;
  const testing::UnitTest* unit_test = testing::UnitTest::GetInstance();
  for (int i = 0; i < unit_test->total_test_suite_count(); ++i) {
    const testing::TestSuite* test_suite = unit_test->GetTestSuite(i);
    for (int j = 0; j < test_suite->total_test_count(); ++j) {
      const char* name = test_suite->GetTestInfo(j)->name();
      if (strstr(name, kUiPattern)) {
        ui_cases.insert(base::StrCat({test_suite->name(), ".", name}));
      }
    }
  }

  if (ui_name.empty()) {
    std::string case_list;
    for (const std::string& name : ui_cases) {
      case_list = base::StrCat({case_list, "\t", name, "\n"});
    }
    VLOG(0) << "\nPass one of the following after --" << kUiSwitch << "=\n"
            << case_list;
    return;
  }

  auto it = ui_cases.find(ui_name);
  ASSERT_NE(it, ui_cases.end()) << "UI '" << ui_name << "' not found.";

  // Replace TestBrowserUi.Invoke with |ui_name|.
  invoker.AppendSwitchASCII(base::kGTestFilterFlag, ui_name);

  base::LaunchOptions options;

  // Wait on subprocess. Otherwise the whole process group will be killed on
  // parent process exit. See http://crbug.com/1094369.
  options.wait = true;

  base::LaunchProcess(invoker, options);
}
