// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ui_controls_factory_ash.h"
#include "base/command_line.h"
#include "base/test/launcher/test_launcher.h"
#include "chrome/test/base/chrome_test_launcher.h"
#include "chrome/test/base/chrome_test_suite.h"
#include "content/public/common/content_switches.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "ui/base/test/ui_controls.h"

// This class is introduced to provide ui_controls since some test cases use
// it. Ideally such tests should be moved into interactive_ui_tests.
// TODO(mukai): remove this after moving such tests.
class BrowserTestSuiteChromeOS : public ChromeTestSuite {
 public:
  BrowserTestSuiteChromeOS(int argc, char** argv)
      : ChromeTestSuite(argc, argv) {}
  ~BrowserTestSuiteChromeOS() override = default;

 private:
  // ChromeTestSuite overrides:
  void Initialize() override {
    ChromeTestSuite::Initialize();
    ui_controls::InstallUIControlsAura(ash::test::CreateAshUIControls());
  }
};

class BrowserTestSuiteRunnerChromeOS : public ChromeTestSuiteRunner {
 public:
  int RunTestSuite(int argc, char** argv) override {
    BrowserTestSuiteChromeOS test_suite(argc, argv);
    // Browser tests are expected not to tear-down various globals.
    test_suite.DisableCheckForLeakedGlobals();
    return test_suite.Run();
  }
};

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  size_t parallel_jobs = base::NumParallelJobs(/*cores_per_job=*/2);
  if (parallel_jobs == 0U)
    return 1;

  BrowserTestSuiteRunnerChromeOS runner;
  ChromeTestLauncherDelegate delegate(&runner);

  // Disable system tracing for browser tests by default. This prevents breakage
  // of tests that spin the run loop until idle on platforms with system tracing
  // (e.g. Chrome OS). Browser tests exercising this feature re-enable it with a
  // custom system tracing service.
  tracing::PerfettoTracedProcess::SetSystemProducerEnabledForTesting(false);

  // Temporarily force the CPU backend to use AAA. (https://crbug.com/1421297)
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kForceSkiaAnalyticAntialiasing);

  return LaunchChromeTests(parallel_jobs, &delegate, argc, argv);
}
