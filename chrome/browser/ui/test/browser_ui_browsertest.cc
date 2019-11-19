// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/process/launch.h"
#include "base/stl_util.h"
#include "base/test/launcher/test_launcher.h"
#include "base/test/test_switches.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Switch for BrowserUiTest.Invoke to spawn a subprocess testing the provided
// argument under a consistent setup.
constexpr const char kUiSwitch[] = "ui";

// Pattern to search in test names that indicate support for UI testing.
constexpr const char kUiPattern[] = "InvokeUi_";

}  // namespace

// Adds a browser_test entry point into the UI testing framework. Without a
// --ui specified, just lists the available UIs and exits.
TEST(BrowserUiTest, Invoke) {
  const base::CommandLine& invoker = *base::CommandLine::ForCurrentProcess();
  const std::string ui_name = invoker.GetSwitchValueASCII(kUiSwitch);

  std::set<std::string> ui_cases;
  const testing::UnitTest* unit_test = testing::UnitTest::GetInstance();
  for (int i = 0; i < unit_test->total_test_case_count(); ++i) {
    const testing::TestCase* test_case = unit_test->GetTestCase(i);
    for (int j = 0; j < test_case->total_test_count(); ++j) {
      const char* name = test_case->GetTestInfo(j)->name();
      if (strstr(name, kUiPattern))
        ui_cases.insert(test_case->name() + std::string(".") + name);
    }
  }

  if (ui_name.empty()) {
    std::string case_list;
    for (const std::string& name : ui_cases)
      case_list += "\t" + name + "\n";
    VLOG(0) << "\nPass one of the following after --" << kUiSwitch << "=\n"
            << case_list;
    return;
  }

  auto it = ui_cases.find(ui_name);
  ASSERT_NE(it, ui_cases.end()) << "UI '" << ui_name << "' not found.";

  // Don't create test output for the subprocess (the paths will conflict).
  base::CommandLine::StringVector argv = invoker.argv();
  std::string ascii(switches::kTestLauncherOutput);
  base::CommandLine::StringType native_switch(ascii.begin(), ascii.end());
  base::EraseIf(
      argv, [native_switch](const base::CommandLine::StringType& arg) -> bool {
        return arg.find(native_switch) != arg.npos;  // Substring search.
      });
  base::CommandLine command(argv);

  // Replace TestBrowserUi.Invoke with |ui_name|.
  command.AppendSwitchASCII(base::kGTestFilterFlag, ui_name);

  base::LaunchOptions options;

#if defined(OS_WIN)
  // Under Windows, the child process won't launch without the wait option.
  // See http://crbug.com/688534.
  options.wait = true;
#else
  options.wait = !command.HasSwitch(switches::kTestLauncherInteractive);
#endif

  base::LaunchProcess(command, options);
}
