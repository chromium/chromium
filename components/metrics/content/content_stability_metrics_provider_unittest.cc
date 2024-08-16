// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/content/content_stability_metrics_provider.h"

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "components/metrics/content/extensions_helper.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/hashing.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/process_type.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace metrics {

namespace {

const char kTestUtilityProcessName[] = "test_utility_process";

class MockExtensionsHelper : public ExtensionsHelper {
 public:
  MockExtensionsHelper() = default;
  MockExtensionsHelper(const MockExtensionsHelper&) = delete;
  MockExtensionsHelper& operator=(const MockExtensionsHelper&) = delete;
  ~MockExtensionsHelper() override = default;

  void set_extension_host(content::RenderProcessHost* host) { host_ = host; }
  // ExtensionsHelper:
  bool IsExtensionProcess(
      content::RenderProcessHost* render_process_host) override {
    return render_process_host == host_;
  }

 private:
  raw_ptr<content::RenderProcessHost> host_ = nullptr;
};

}  // namespace

class ContentStabilityMetricsProviderTest : public testing::Test {
 protected:
  ContentStabilityMetricsProviderTest()
      : prefs_(std::make_unique<TestingPrefServiceSimple>()) {
    metrics::StabilityMetricsHelper::RegisterPrefs(prefs()->registry());
  }
  ContentStabilityMetricsProviderTest(
      const ContentStabilityMetricsProviderTest&) = delete;
  ContentStabilityMetricsProviderTest& operator=(
      const ContentStabilityMetricsProviderTest&) = delete;
  ~ContentStabilityMetricsProviderTest() override = default;

  TestingPrefServiceSimple* prefs() { return prefs_.get(); }

 private:
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(ContentStabilityMetricsProviderTest,
       BrowserChildProcessObserverUtility) {
  base::HistogramTester histogram_tester;
  metrics::ContentStabilityMetricsProvider provider(prefs(), nullptr);

  content::ChildProcessData child_process_data(content::PROCESS_TYPE_UTILITY);
  child_process_data.metrics_name = kTestUtilityProcessName;

  provider.BrowserChildProcessLaunchedAndConnected(child_process_data);
  const int kExitCode = 1;
  content::ChildProcessTerminationInfo abnormal_termination_info;
  abnormal_termination_info.status =
      base::TERMINATION_STATUS_ABNORMAL_TERMINATION;
  abnormal_termination_info.exit_code = kExitCode;
  provider.BrowserChildProcessCrashed(child_process_data,
                                      abnormal_termination_info);
  provider.BrowserChildProcessCrashed(child_process_data,
                                      abnormal_termination_info);

  // Verify metrics.
  histogram_tester.ExpectUniqueSample(
      "ChildProcess.Launched.UtilityProcessHash",
      variations::HashName(kTestUtilityProcessName), 1);
  histogram_tester.ExpectBucketCount("Stability.Counts2",
                                     StabilityEventType::kUtilityLaunch, 1);
  histogram_tester.ExpectUniqueSample(
      "ChildProcess.Crashed.UtilityProcessHash",
      variations::HashName(kTestUtilityProcessName), 2);
  histogram_tester.ExpectUniqueSample(
      "ChildProcess.Crashed.UtilityProcessExitCode", kExitCode, 2);
  histogram_tester.ExpectBucketCount("Stability.Counts2",
                                     StabilityEventType::kUtilityCrash, 2);
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(ContentStabilityMetricsProviderTest, RenderProcessObserver) {
  metrics::ContentStabilityMetricsProvider provider(prefs(), nullptr);
  content::TestBrowserContext browser_context;
  content::MockRenderProcessHostFactory rph_factory;
  scoped_refptr<content::SiteInstance> site_instance(
      content::SiteInstance::Create(&browser_context));

  // Owned by rph_factory.
  content::RenderProcessHost* host(rph_factory.CreateRenderProcessHost(
      &browser_context, site_instance.get()));

  base::HistogramTester histogram_tester;

  // Crash and abnormal termination should increment renderer crash count.
  content::ChildProcessTerminationInfo crash_details;
  crash_details.status = base::TERMINATION_STATUS_PROCESS_CRASHED;
  crash_details.exit_code = 1;
  provider.OnRenderProcessHostCreated(host);
  provider.RenderProcessExited(host, crash_details);

  content::ChildProcessTerminationInfo term_details;
  term_details.status = base::TERMINATION_STATUS_ABNORMAL_TERMINATION;
  term_details.exit_code = 1;
  provider.OnRenderProcessHostCreated(host);
  provider.RenderProcessExited(host, term_details);

  // Kill does not increment renderer crash count.
  content::ChildProcessTerminationInfo kill_details;
  kill_details.status = base::TERMINATION_STATUS_PROCESS_WAS_KILLED;
  kill_details.exit_code = 1;
  provider.OnRenderProcessHostCreated(host);
  provider.RenderProcessExited(host, kill_details);

  // Failed launch increments failed launch count.
  content::ChildProcessTerminationInfo failed_launch_details;
  failed_launch_details.status = base::TERMINATION_STATUS_LAUNCH_FAILED;
  failed_launch_details.exit_code = 1;
  provider.OnRenderProcessHostCreationFailed(host, failed_launch_details);

  // Verify metrics.
  histogram_tester.ExpectBucketCount("Stability.Counts2",
                                     StabilityEventType::kRendererCrash, 2);
  histogram_tester.ExpectBucketCount(
      "Stability.Counts2", StabilityEventType::kRendererFailedLaunch, 1);
  histogram_tester.ExpectBucketCount("Stability.Counts2",
                                     StabilityEventType::kExtensionCrash, 0);
}

TEST_F(ContentStabilityMetricsProviderTest,
       MetricsServicesWebContentsObserver) {
  metrics::ContentStabilityMetricsProvider provider(prefs(), nullptr);
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount("Stability.Counts2",
                                     StabilityEventType::kPageLoad, 0);

  // Simulate page loads.
  const auto expected_page_load_count = 4;
  for (int i = 0; i < expected_page_load_count; i++) {
    provider.OnPageLoadStarted();
  }

  // Verify metrics.
  histogram_tester.ExpectBucketCount("Stability.Counts2",
                                     StabilityEventType::kPageLoad,
                                     expected_page_load_count);
}

#endif  // !BUILDFLAG(IS_ANDROID)

// Assertions for an extension related crash.
// This test only works if extensions are enabled as there is a DCHECK in
// StabilityMetricsHelper that it is only called with a value of true for
// extension process if extensions are enabled.
#if BUILDFLAG(ENABLE_EXTENSIONS)
TEST_F(ContentStabilityMetricsProviderTest, ExtensionsNotificationObserver) {
  content::TestBrowserContext browser_context;
  content::MockRenderProcessHostFactory rph_factory;
  scoped_refptr<content::SiteInstance> site_instance(
      content::SiteInstance::Create(&browser_context));

  // Owned by rph_factory.
  content::RenderProcessHost* extension_host =
      rph_factory.CreateRenderProcessHost(&browser_context,
                                          site_instance.get());
  auto extensions_helper = std::make_unique<MockExtensionsHelper>();
  extensions_helper->set_extension_host(extension_host);
  metrics::ContentStabilityMetricsProvider provider(
      prefs(), std::move(extensions_helper));

  base::HistogramTester histogram_tester;

  // Crash and abnormal termination should increment extension crash count.
  content::ChildProcessTerminationInfo crash_details;
  crash_details.status = base::TERMINATION_STATUS_PROCESS_CRASHED;
  crash_details.exit_code = 1;
  provider.OnRenderProcessHostCreated(extension_host);
  provider.RenderProcessExited(extension_host, crash_details);

  // Failed launch increments failed launch count.
  content::ChildProcessTerminationInfo failed_launch_details;
  failed_launch_details.status = base::TERMINATION_STATUS_LAUNCH_FAILED;
  failed_launch_details.exit_code = 1;
  provider.OnRenderProcessHostCreationFailed(extension_host,
                                             failed_launch_details);

  // Verify metrics.
  histogram_tester.ExpectBucketCount("Stability.Counts2",
                                     StabilityEventType::kRendererCrash, 0);
  histogram_tester.ExpectBucketCount("Stability.Counts2",
                                     StabilityEventType::kExtensionCrash, 1);
  histogram_tester.ExpectBucketCount(
      "Stability.Counts2", StabilityEventType::kExtensionRendererFailedLaunch,
      1);
}
#endif

}  // namespace metrics
