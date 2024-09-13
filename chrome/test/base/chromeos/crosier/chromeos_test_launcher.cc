// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chromeos/crosier/chromeos_test_launcher.h"

#include <string_view>

#include "base/test/task_environment.h"
#include "base/types/pass_key.h"
#include "chrome/app/chrome_crash_reporter_client.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/common/profiler/chrome_thread_profiler_client.h"
#include "chrome/common/profiler/main_thread_stack_sampling_profiler.h"
#include "chrome/test/base/chromeos/crosier/chromeos_test_suite.h"
#include "chrome/utility/chrome_content_utility_client.h"
#include "components/sampling_profiler/thread_profiler.h"
#include "content/public/test/network_service_test_helper.h"
#include "mojo/public/cpp/bindings/service_factory.h"
#include "services/test/echo/echo_service.h"
#include "ui/base/interaction/interactive_test_internal.h"

namespace {

auto RunEchoService(mojo::PendingReceiver<echo::mojom::EchoService> receiver) {
  return std::make_unique<echo::EchoService>(std::move(receiver));
}

// Acts like normal ChromeContentBrowserClient but injects a TestTaskTracker to
// watch for long-running tasks and produce a useful timeout message in order to
// find the cause of flaky timeout tests.
class BrowserTestChromeOSContentBrowserClient
    : public ChromeContentBrowserClient {
 public:
};

// A replacement ChromeContentUtilityClient that binds the
// echo::mojom::EchoService within the Utility process. For use with testing
// only.
class BrowserTestChromeOSContentUtilityClient
    : public ChromeContentUtilityClient {
 public:
  void RegisterIOThreadServices(mojo::ServiceFactory& services) override {
    ChromeContentUtilityClient::RegisterIOThreadServices(services);
    services.Add(RunEchoService);
  }
};

}  // namespace

// static
int ChromeOSTestSuiteRunner::RunTestSuiteInternal(
    ChromeOSTestSuite* test_suite) {
  // crosint tests are expected not to tear-down various globals.
  test_suite->DisableCheckForLeakedGlobals();
  return test_suite->Run();
}

int ChromeOSTestSuiteRunner::RunTestSuite(int argc, char** argv) {
  ChromeOSTestSuite test_suite(argc, argv);
  return RunTestSuiteInternal(&test_suite);
}

ChromeOSTestLauncherDelegate::ChromeOSTestLauncherDelegate(
    ChromeOSTestSuiteRunner* runner)
    : runner_(runner) {
  CHECK(runner);

  // Enable interactive testing verbs in Kombucha. OS integration tests in
  // ChromeOS should be safe to do e.g. mouse input and window activation.
  ui::test::internal::InteractiveTestPrivate::
      set_interactive_test_verbs_allowed(
          base::PassKey<ChromeOSTestLauncherDelegate>());
}

ChromeOSTestLauncherDelegate::~ChromeOSTestLauncherDelegate() = default;

int ChromeOSTestLauncherDelegate::RunTestSuite(int argc, char** argv) {
  return runner_->RunTestSuite(argc, argv);
}

content::ContentBrowserClient*
ChromeOSTestChromeMainDelegate::CreateContentBrowserClient() {
  chrome_content_browser_client_ =
      std::make_unique<BrowserTestChromeOSContentBrowserClient>();
  return chrome_content_browser_client_.get();
}

content::ContentUtilityClient*
ChromeOSTestChromeMainDelegate::CreateContentUtilityClient() {
  chrome_content_utility_client_ =
      std::make_unique<BrowserTestChromeOSContentUtilityClient>();
  return chrome_content_utility_client_.get();
}

void ChromeOSTestChromeMainDelegate::CreateThreadPool(std::string_view name) {
  base::test::TaskEnvironment::CreateThreadPool();
  // The ThreadProfiler client must be set before main thread profiling is
  // started (below).
  sampling_profiler::ThreadProfiler::SetClient(
      std::make_unique<ChromeThreadProfilerClient>());
  // Start the sampling profiler as early as possible - namely, once the thread
  // pool has been created.
  sampling_profiler_ = std::make_unique<MainThreadStackSamplingProfiler>();
}

content::ContentMainDelegate*
ChromeOSTestLauncherDelegate::CreateContentMainDelegate() {
  return new ChromeOSTestChromeMainDelegate();
}

void ChromeOSTestLauncherDelegate::PreSharding() {}

void ChromeOSTestLauncherDelegate::OnDoneRunningTests() {}

int LaunchChromeOSTests(content::TestLauncherDelegate* delegate,
                        int argc,
                        char** argv) {
  ChromeCrashReporterClient::Create();
  // Setup a working test environment for the network service in case it's used.
  // Only create this object in the utility process, so that its members don't
  // interfere with other test objects in the browser process.
  std::unique_ptr<content::NetworkServiceTestHelper>
      network_service_test_helper = content::NetworkServiceTestHelper::Create();
  return content::LaunchTests(
      /*launcher_delegate=*/delegate,
      /*parallel_jobs=*/1, argc, argv);
}
