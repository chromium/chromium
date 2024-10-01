// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/test/launcher/test_launcher.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ssl/https_upgrades_navigation_throttle.h"
#include "chrome/test/base/chrome_test_launcher.h"
#include "chrome/test/base/chrome_test_suite.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "gpu/ipc/service/image_transport_surface.h"
#include "ui/base/interaction/interactive_test_internal.h"
#include "ui/base/test/ui_controls.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/test/ui_controls_ash.h"
#elif BUILDFLAG(IS_WIN)
#include "ui/aura/test/ui_controls_aurawin.h"
#endif

#if defined(USE_AURA) && BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#include "ui/platform_window/common/platform_window_defaults.h"
#endif  // defined(USE_AURA) && BUILDFLAG(IS_OZONE)

#if BUILDFLAG(IS_WIN)
#include "base/win/scoped_com_initializer.h"
#include "base/win/win_util.h"
#include "chrome/test/base/always_on_top_window_killer_win.h"
#endif

class InteractiveUITestSuite : public ChromeTestSuite {
 public:
  InteractiveUITestSuite(int argc, char** argv) : ChromeTestSuite(argc, argv) {}
  ~InteractiveUITestSuite() override = default;

 protected:
  // ChromeTestSuite overrides:
  void Initialize() override {
    ChromeTestSuite::Initialize();

#if BUILDFLAG(IS_CHROMEOS_ASH)
    ash::test::EnableUIControlsAsh();
#elif BUILDFLAG(IS_WIN)
    com_initializer_ = std::make_unique<base::win::ScopedCOMInitializer>();
    aura::test::EnableUIControlsAuraWin();
#elif BUILDFLAG(IS_OZONE)
    // Notifies the platform that test config is needed. For Wayland, for
    // example, makes it possible to use emulated input.
    ui::test::EnableTestConfigForPlatformWindows();

    ui::OzonePlatform::InitParams params;
    params.single_process = true;
    ui::OzonePlatform::InitializeForUI(params);
    ui_controls::EnableUIControls();
#else
    ui_controls::EnableUIControls();
#endif
    // Allow interactive Kombucha test verbs in interactive UI tests.
    ui::test::internal::InteractiveTestPrivate::
        set_interactive_test_verbs_allowed(
            base::PassKey<InteractiveUITestSuite>());

    // TODO(crbug.com/40263135) Investigate why https upgrade causes
    // interactive_ui_tests to run longer.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
    // Force the HTTPS-Upgrades timeout to zero.
    HttpsUpgradesNavigationThrottle::set_timeout_for_testing(base::TimeDelta());
#endif
  }

  void Shutdown() override {
#if BUILDFLAG(IS_WIN)
    com_initializer_.reset();
#endif
  }

 private:
#if BUILDFLAG(IS_WIN)
  std::unique_ptr<base::win::ScopedCOMInitializer> com_initializer_;
#endif
};

class InteractiveUITestLauncherDelegate : public ChromeTestLauncherDelegate {
 public:
  explicit InteractiveUITestLauncherDelegate(ChromeTestSuiteRunner* runner)
      : ChromeTestLauncherDelegate(runner) {}
  InteractiveUITestLauncherDelegate(const InteractiveUITestLauncherDelegate&) =
      delete;
  InteractiveUITestLauncherDelegate& operator=(
      const InteractiveUITestLauncherDelegate&) = delete;

 protected:
  // content::TestLauncherDelegate:
  void PreSharding() override {
    ChromeTestLauncherDelegate::PreSharding();
#if BUILDFLAG(IS_WIN)
    // Check for any always-on-top windows present before any tests are run.
    // Take a snapshot if any are found and attempt to close any that are system
    // dialogs.
    KillAlwaysOnTopWindows(RunType::BEFORE_SHARD);
#endif
  }

  void OnTestTimedOut(const base::CommandLine& command_line) override {
#if BUILDFLAG(IS_WIN)
    // Take a snapshot of the screen and check for any always-on-top windows
    // present before terminating the test. Attempt to close any that are system
    // dialogs.
    KillAlwaysOnTopWindows(RunType::AFTER_TEST_TIMEOUT, &command_line);
#endif
    ChromeTestLauncherDelegate::OnTestTimedOut(command_line);
  }

#if BUILDFLAG(IS_MAC)
  void PreRunTest() override {
    // Clear currently pressed modifier keys (if any) before the test starts.
    ui_test_utils::ClearKeyEventModifiers();
  }

  void PostRunTest(base::TestResult* test_result) override {
    // Clear currently pressed modifier keys (if any) after the test finishes
    // and report an error if there were some.
    bool had_hanging_modifiers = ui_test_utils::ClearKeyEventModifiers();
    if (had_hanging_modifiers &&
        test_result->status == base::TestResult::TEST_SUCCESS) {
      test_result->status = base::TestResult::TEST_FAILURE_ON_EXIT;
    }
    ChromeTestLauncherDelegate::PostRunTest(test_result);
  }
#endif  // BUILDFLAG(IS_MAC)
};

class InteractiveUITestSuiteRunner : public ChromeTestSuiteRunner {
 public:
  int RunTestSuite(int argc, char** argv) override {
    InteractiveUITestSuite test_suite(argc, argv);
    return RunTestSuiteInternal(&test_suite);
  }
};

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);

#if BUILDFLAG(IS_CHROMEOS_ASH) && defined(MEMORY_SANITIZER)
  // Force software-gl. This is necessary for mus tests to avoid an msan warning
  // in gl init.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kOverrideUseSoftwareGLForTests);
#endif

  // Without this it's possible for the first browser to start up in the
  // background, generally because the last test did something that causes the
  // test to run in the background. Most interactive ui tests assume they are in
  // the foreground and fail in weird ways if they aren't (for example, clicks
  // go to the wrong place). This ensures the first browser is always in the
  // foreground.
  InProcessBrowserTest::set_global_browser_set_up_function(
      &ui_test_utils::BringBrowserWindowToFront);

#if BUILDFLAG(IS_WIN)
  base::win::EnableHighDPISupport();
#endif  // BUILDFLAG(IS_WIN)

  // For ash chrome, it's using multiple X11 windows to host the browser.
  // Also, {emulating|injecting} keyboard and mouse events happen at ozone
  // level, not OS level. So it is fine to run tests in parallel.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  size_t parallel_jobs = base::NumParallelJobs(/*cores_per_job=*/2);
  if (parallel_jobs == 0) {
    parallel_jobs = 1;
  }
#else
  // Run interactive_ui_tests serially, they do not support running in parallel.
  size_t parallel_jobs = 1;
#endif

  InteractiveUITestSuiteRunner runner;
  InteractiveUITestLauncherDelegate delegate(&runner);
  return LaunchChromeTests(parallel_jobs, &delegate, argc, argv);
}
