// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_test_launcher.h"

#include <memory>
#include <utility>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/process/process_metrics.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/test_file_util.h"
#include "base/test/test_switches.h"
#include "chrome/app/chrome_main_delegate.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/profiler/main_thread_stack_sampling_profiler.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "chrome/test/base/chrome_test_suite.h"
#include "chrome/utility/chrome_content_utility_client.h"
#include "components/crash/content/app/crashpad.h"
#include "content/public/app/content_main.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/network_service_test_helper.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/test_utils.h"
#include "ui/base/test/ui_controls.h"

#if defined(OS_MACOSX)
#include "base/mac/bundle_locations.h"
#include "chrome/browser/chrome_browser_application_mac.h"
#endif  // defined(OS_MACOSX)

#if defined(USE_AURA)
#include "ui/aura/test/ui_controls_factory_aura.h"
#include "ui/base/test/ui_controls_aura.h"
#endif

#if defined(OS_CHROMEOS)
#include "ash/mojo_interface_factory.h"
#include "ash/mojo_test_interface_factory.h"
#include "ash/public/cpp/manifest.h"
#include "ash/public/cpp/test_manifest.h"
#include "ash/test/ui_controls_factory_ash.h"
#endif

#if defined(OS_LINUX) || defined(OS_ANDROID)
#include "chrome/app/chrome_crash_reporter_client.h"
#endif

#if defined(OS_WIN)
#include "base/win/registry.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/app/chrome_crash_reporter_client_win.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/firewall_manager_win.h"
#endif

#if defined(OS_WIN) || defined(OS_MACOSX) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS))
#include "chrome/browser/first_run/scoped_relaunch_chrome_browser_override.h"
#include "testing/gtest/include/gtest/gtest.h"
#endif

ChromeTestSuiteRunner::ChromeTestSuiteRunner() {}
ChromeTestSuiteRunner::~ChromeTestSuiteRunner() {}

int ChromeTestSuiteRunner::RunTestSuite(int argc, char** argv) {
  ChromeTestSuite test_suite(argc, argv);
  // Browser tests are expected not to tear-down various globals and may
  // complete with the thread priority being above NORMAL.
  test_suite.DisableCheckForLeakedGlobals();
  test_suite.DisableCheckForThreadPriorityAtTestEnd();
#if defined(OS_ANDROID)
  // Android browser tests run child processes as threads instead.
  content::ContentTestSuiteBase::RegisterInProcessThreads();
#endif
  return test_suite.Run();
}

#if defined(OS_WIN)

// A helper class that adds Windows firewall rules for the duration of the test.
class ChromeTestLauncherDelegate::ScopedFirewallRules {
 public:
  ScopedFirewallRules() {
    CHECK(com_initializer_.Succeeded());
    base::FilePath exe_path;
    CHECK(base::PathService::Get(base::FILE_EXE, &exe_path));
    firewall_manager_ = installer::FirewallManager::Create(exe_path);
    CHECK(firewall_manager_);
    rules_added_ = firewall_manager_->AddFirewallRules();
    LOG_IF(WARNING, !rules_added_)
        << "Failed to add Windows firewall rules -- Windows firewall dialogs "
           "may appear.";
  }

  ~ScopedFirewallRules() {
    if (rules_added_)
      firewall_manager_->RemoveFirewallRules();
  }

 private:
  base::win::ScopedCOMInitializer com_initializer_;
  std::unique_ptr<installer::FirewallManager> firewall_manager_;
  bool rules_added_ = false;
  DISALLOW_COPY_AND_ASSIGN(ScopedFirewallRules);
};

#endif  // defined(OS_WIN)

ChromeTestLauncherDelegate::ChromeTestLauncherDelegate(
    ChromeTestSuiteRunner* runner)
    : runner_(runner) {}
ChromeTestLauncherDelegate::~ChromeTestLauncherDelegate() {}

int ChromeTestLauncherDelegate::RunTestSuite(int argc, char** argv) {
  return runner_->RunTestSuite(argc, argv);
}

bool ChromeTestLauncherDelegate::AdjustChildProcessCommandLine(
    base::CommandLine* command_line,
    const base::FilePath& temp_data_dir) {
  base::CommandLine new_command_line(command_line->GetProgram());
  base::CommandLine::SwitchMap switches = command_line->GetSwitches();

  // Strip out user-data-dir if present.  We will add it back in again later.
  switches.erase(switches::kUserDataDir);

  for (base::CommandLine::SwitchMap::const_iterator iter = switches.begin();
       iter != switches.end(); ++iter) {
    new_command_line.AppendSwitchNative((*iter).first, (*iter).second);
  }

  new_command_line.AppendSwitchPath(switches::kUserDataDir, temp_data_dir);

  *command_line = new_command_line;
  return true;
}

#if !defined(OS_ANDROID)
content::ContentMainDelegate*
ChromeTestLauncherDelegate::CreateContentMainDelegate() {
  return new ChromeMainDelegate();
}
#endif

void ChromeTestLauncherDelegate::PreSharding() {
#if defined(OS_WIN)
  // Pre-test cleanup for registry state keyed off the profile dir (which can
  // proliferate with the use of uniquely named scoped_dirs):
  // https://crbug.com/721245. This needs to be here in order not to be racy
  // with any tests that will access that state.
  base::win::RegKey distrubution_key;
  LONG result = distrubution_key.Open(HKEY_CURRENT_USER,
                                      install_static::GetRegistryPath().c_str(),
                                      KEY_SET_VALUE);

  if (result != ERROR_SUCCESS) {
    LOG_IF(ERROR, result != ERROR_FILE_NOT_FOUND)
        << "Failed to open distribution key for cleanup: " << result;
    return;
  }

  result = distrubution_key.DeleteKey(L"PreferenceMACs");
  LOG_IF(ERROR, result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND)
      << "Failed to cleanup PreferenceMACs: " << result;

  // Add firewall rules for the test binary so that Windows doesn't show a
  // firewall dialog during the test run.
  firewall_rules_ = std::make_unique<ScopedFirewallRules>();
#endif
}

void ChromeTestLauncherDelegate::OnDoneRunningTests() {
#if defined(OS_WIN)
  firewall_rules_.reset();
#endif
}

int LaunchChromeTests(size_t parallel_jobs,
                      content::TestLauncherDelegate* delegate,
                      int argc,
                      char** argv) {
#if defined(OS_MACOSX)
  // Set up the path to the framework so resources can be loaded. This is also
  // performed in ChromeTestSuite, but in browser tests that only affects the
  // browser process. Child processes need access to the Framework bundle too.
  base::FilePath path;
  CHECK(base::PathService::Get(base::DIR_EXE, &path));
  path = path.Append(chrome::kFrameworkName);
  base::mac::SetOverrideFrameworkBundlePath(path);
#endif

#if defined(OS_WIN)
  // Create a primordial InstallDetails instance for the test.
  install_static::ScopedInstallDetails install_details;
#endif

  const auto& command_line = *base::CommandLine::ForCurrentProcess();

  // Initialize sampling profiler for tests that relaunching a browser. This
  // mimics the behavior in standalone Chrome, where this is done in
  // chrome/app/chrome_main.cc, which does not get called by tests.
  std::unique_ptr<MainThreadStackSamplingProfiler> sampling_profiler;
  if (command_line.HasSwitch(switches::kLaunchAsBrowser))
    sampling_profiler = std::make_unique<MainThreadStackSamplingProfiler>();

#if defined(OS_LINUX) || defined(OS_ANDROID)
  ChromeCrashReporterClient::Create();
#elif defined(OS_WIN)
  // We leak this pointer intentionally. The crash client needs to outlive
  // all other code.
  ChromeCrashReporterClient* crash_client = new ChromeCrashReporterClient();
  ANNOTATE_LEAKING_OBJECT_PTR(crash_client);
  crash_reporter::SetCrashReporterClient(crash_client);
#endif

  // Setup a working test environment for the network service in case it's used.
  // Only create this object in the utility process, so that its members don't
  // interfere with other test objects in the browser process.
  std::unique_ptr<content::NetworkServiceTestHelper>
      network_service_test_helper;
  if (command_line.GetSwitchValueASCII(switches::kProcessType) ==
      switches::kUtilityProcess) {
    network_service_test_helper =
        std::make_unique<content::NetworkServiceTestHelper>();
    ChromeContentUtilityClient::SetNetworkBinderCreationCallback(base::Bind(
        [](content::NetworkServiceTestHelper* helper,
           service_manager::BinderRegistry* registry) {
          helper->RegisterNetworkBinders(registry);
        },
        network_service_test_helper.get()));
  }

#if defined(OS_CHROMEOS)
  // Inject the test interfaces for ash. Use a callback to avoid linking test
  // interface support into production code.
  ash::AmendManifestForTesting(ash::GetManifestOverlayForTesting());
  ash::mojo_interface_factory::SetRegisterInterfacesCallback(
      base::Bind(&ash::mojo_test_interface_factory::RegisterInterfaces));
#endif

#if defined(OS_WIN) || defined(OS_MACOSX) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  // Cause a test failure for any test that triggers an unexpected relaunch.
  // Tests that fail here should likely be restructured to put the "before
  // relaunch" code into a PRE_ test with its own
  // ScopedRelaunchChromeBrowserOverride and the "after relaunch" code into the
  // normal non-PRE_ test.
  upgrade_util::ScopedRelaunchChromeBrowserOverride fail_on_relaunch(
      base::BindRepeating([](const base::CommandLine&) {
        ADD_FAILURE() << "Unexpected call to RelaunchChromeBrowser";
        return false;
      }));
#endif

  return content::LaunchTests(delegate, parallel_jobs, argc, argv);
}
