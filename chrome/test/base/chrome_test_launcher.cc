// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_test_launcher.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/process_metrics.h"
#include "base/run_loop.h"
#include "base/sampling_heap_profiler/poisson_allocation_sampler.h"
#include "base/strings/string_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "base/test/test_switches.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/metrics/chrome_feature_list_creator.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/profiler/chrome_thread_profiler_client.h"
#include "chrome/common/profiler/main_thread_stack_sampling_profiler.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "chrome/installer/util/taskbar_util.h"
#include "chrome/test/base/chrome_test_suite.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/utility/chrome_content_utility_client.h"
#include "components/crash/core/app/crashpad.h"
#include "components/sampling_profiler/thread_profiler.h"
#include "content/public/app/content_main.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/network_service_test_helper.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/service_factory.h"
#include "services/test/echo/echo_service.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/bundle_locations.h"
#include "chrome/browser/chrome_browser_application_mac.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
#include "chrome/app/chrome_crash_reporter_client.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <Shlobj.h>
#include "base/debug/handle_hooks_win.h"
#include "base/win/registry.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/app/chrome_crash_reporter_client_win.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/firewall_manager_win.h"
#endif

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
#include "chrome/browser/first_run/scoped_relaunch_chrome_browser_override.h"
#include "chrome/browser/upgrade_detector/installed_version_poller.h"
#include "testing/gtest/include/gtest/gtest.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/test_controller_ash.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {
#if BUILDFLAG(IS_CHROMEOS_ASH)
class TestControllerSetupMainExtraParts : public ChromeBrowserMainExtraParts {
 public:
  TestControllerSetupMainExtraParts() = default;

  void PostBrowserStart() override {
    crosapi::CrosapiManager::Get()->crosapi_ash()->SetTestControllerForTesting(
        std::make_unique<crosapi::TestControllerAsh>());
  }

  void PostMainMessageLoopRun() override {
    crosapi::CrosapiManager::Get()->crosapi_ash()->SetTestControllerForTesting(
        nullptr);
  }
};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}  // namespace

// static
int ChromeTestSuiteRunner::RunTestSuiteInternal(ChromeTestSuite* test_suite) {
  // Browser tests are expected not to tear-down various globals.
  test_suite->DisableCheckForLeakedGlobals();
#if BUILDFLAG(IS_ANDROID)
  // Android browser tests run child processes as threads instead.
  content::ContentTestSuiteBase::RegisterInProcessThreads();
#endif
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
  InstalledVersionPoller::ScopedDisableForTesting disable_polling(
      InstalledVersionPoller::MakeScopedDisableForTesting());
#endif
  return test_suite->Run();
}

int ChromeTestSuiteRunner::RunTestSuite(int argc, char** argv) {
  ChromeTestSuite test_suite(argc, argv);
  return RunTestSuiteInternal(&test_suite);
}

#if BUILDFLAG(IS_WIN)

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
  ScopedFirewallRules(const ScopedFirewallRules&) = delete;
  ScopedFirewallRules& operator=(const ScopedFirewallRules&) = delete;

  ~ScopedFirewallRules() {
    if (rules_added_)
      firewall_manager_->RemoveFirewallRules();
  }

 private:
  base::win::ScopedCOMInitializer com_initializer_;
  std::unique_ptr<installer::FirewallManager> firewall_manager_;
  bool rules_added_ = false;
};

#endif  // BUILDFLAG(IS_WIN)

namespace {

auto RunEchoService(mojo::PendingReceiver<echo::mojom::EchoService> receiver) {
  return std::make_unique<echo::EchoService>(std::move(receiver));
}

}  // namespace

ChromeTestLauncherDelegate::ChromeTestLauncherDelegate(
    ChromeTestSuiteRunner* runner)
    : runner_(runner) {}
ChromeTestLauncherDelegate::~ChromeTestLauncherDelegate() {}

int ChromeTestLauncherDelegate::RunTestSuite(int argc, char** argv) {
  return runner_->RunTestSuite(argc, argv);
}

std::string
ChromeTestLauncherDelegate::GetUserDataDirectoryCommandLineSwitch() {
  return switches::kUserDataDir;
}

// A replacement ChromeContentUtilityClient that binds the
// echo::mojom::EchoService within the Utility process. For use with testing
// only.
class BrowserTestChromeContentUtilityClient
    : public ChromeContentUtilityClient {
 public:
  void RegisterIOThreadServices(mojo::ServiceFactory& services) override {
    ChromeContentUtilityClient::RegisterIOThreadServices(services);
    services.Add(RunEchoService);
  }
};

content::ContentUtilityClient*
ChromeTestChromeMainDelegate::CreateContentUtilityClient() {
  chrome_content_utility_client_ =
      std::make_unique<BrowserTestChromeContentUtilityClient>();
  return chrome_content_utility_client_.get();
}

#if !BUILDFLAG(IS_ANDROID)
std::optional<int> ChromeTestChromeMainDelegate::PostEarlyInitialization(
    InvokedIn invoked_in) {
  auto result = ChromeMainDelegate::PostEarlyInitialization(invoked_in);
  if (absl::get_if<InvokedInBrowserProcess>(&invoked_in)) {
    // If servicing an `InProcessBrowserTest`, give the test an opportunity to
    // prepopulate Local State with preferences.
    ChromeFeatureListCreator* chrome_feature_list_creator =
        chrome_content_browser_client_->startup_data()
            ->chrome_feature_list_creator();
    PrefService* const local_state = chrome_feature_list_creator->local_state();
    if (auto* test_instance = InProcessBrowserTest::GetCurrent()) {
      test_instance->SetUpLocalStatePrefService(local_state);
    }
  }
  return result;
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
bool ChromeTestChromeMainDelegate::ShouldHandleConsoleControlEvents() {
  // Allow Ctrl-C and friends to terminate the test processes forthwith.
  return false;
}
#endif

void ChromeTestChromeMainDelegate::CreateThreadPool(std::string_view name) {
  base::test::TaskEnvironment::CreateThreadPool();

  // The ThreadProfiler client must be set before main thread profiling is
  // started (below).
  sampling_profiler::ThreadProfiler::SetClient(
      std::make_unique<ChromeThreadProfilerClient>());

// `ChromeMainDelegateAndroid::PreSandboxStartup` creates the profiler a little
// later.
#if !BUILDFLAG(IS_ANDROID)
  // Start the sampling profiler as early as possible - namely, once the thread
  // pool has been created.
  sampling_profiler_ = std::make_unique<MainThreadStackSamplingProfiler>();
#endif
}

#if !BUILDFLAG(IS_ANDROID)
content::ContentMainDelegate*
ChromeTestLauncherDelegate::CreateContentMainDelegate() {
  return new ChromeTestChromeMainDelegate();
}
#endif

void ChromeTestLauncherDelegate::CreatedBrowserMainParts(
    content::BrowserMainParts* browser_main_parts) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  static_cast<ChromeBrowserMainParts*>(browser_main_parts)
      ->AddParts(std::make_unique<TestControllerSetupMainExtraParts>());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void ChromeTestLauncherDelegate::PreSharding() {
#if BUILDFLAG(IS_WIN)
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
  // firewall dialog during the test run. Silently do nothing if not running as
  // an admin, to avoid error messages.
  if (IsUserAnAdmin())
    firewall_rules_ = std::make_unique<ScopedFirewallRules>();
#endif
}

void ChromeTestLauncherDelegate::OnDoneRunningTests() {
#if BUILDFLAG(IS_WIN)
  firewall_rules_.reset();
#endif
}

int LaunchChromeTests(size_t parallel_jobs,
                      content::TestLauncherDelegate* delegate,
                      int argc,
                      char** argv) {
#if BUILDFLAG(IS_MAC)
  // Set up the path to the framework so resources can be loaded. This is also
  // performed in ChromeTestSuite, but in browser tests that only affects the
  // browser process. Child processes need access to the Framework bundle too.
  base::FilePath path;
  CHECK(base::PathService::Get(base::DIR_EXE, &path));
  path = path.Append(chrome::kFrameworkName);
  base::apple::SetOverrideFrameworkBundlePath(path);
#endif

#if BUILDFLAG(IS_WIN)
  // Create a primordial InstallDetails instance for the test.
  install_static::ScopedInstallDetails install_details;

  // Install handle hooks for tests only.
  base::debug::HandleHooks::PatchLoadedModules();
#endif  // BUILDFLAG(IS_WIN)

  // PoissonAllocationSampler's TLS slots need to be set up before
  // MainThreadStackSamplingProfiler, which can allocate TLS slots of its own.
  // On some platforms pthreads can malloc internally to access higher-numbered
  // TLS slots, which can cause reentry in the heap profiler. (See the comment
  // on ReentryGuard::InitTLSSlot().)
  // TODO(crbug.com/40062835): Clean up other paths that call this Init()
  // function, which are now redundant.
  base::PoissonAllocationSampler::Init();

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  ChromeCrashReporterClient::Create();
#elif BUILDFLAG(IS_WIN)
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
      network_service_test_helper = content::NetworkServiceTestHelper::Create();

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
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

#if BUILDFLAG(IS_WIN)
  SetCanPinToTaskbarDelegate(([]() {
    ADD_FAILURE()
        << "Attempting to pint shortcut to taskbar in test."
        << " Use web_app::OsIntegrationManager::ScopedSuppressForTesting or "
        << "other mechanism to not pin to taskbar.";
    return false;
  }));
#endif  // BUILDFLAG(IS_WIN)

  return content::LaunchTests(delegate, parallel_jobs, argc, argv);
}
