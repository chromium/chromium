// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/content/content_stability_metrics_provider.h"

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
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
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

const char kTestGpuProcessName[] = "content_gpu";
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
  content::RenderProcessHost* host_ = nullptr;
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

TEST_F(ContentStabilityMetricsProviderTest, BrowserChildProcessObserverGpu) {
  base::HistogramTester histogram_tester;
  metrics::ContentStabilityMetricsProvider provider(prefs(), nullptr);

  content::ChildProcessData child_process_data(content::PROCESS_TYPE_GPU);
  child_process_data.metrics_name = kTestGpuProcessName;

  provider.BrowserChildProcessLaunchedAndConnected(child_process_data);
  content::ChildProcessTerminationInfo abnormal_termination_info;
  abnormal_termination_info.status =
      base::TERMINATION_STATUS_ABNORMAL_TERMINATION;
  abnormal_termination_info.exit_code = 1;
  provider.BrowserChildProcessCrashed(child_process_data,
                                      abnormal_termination_info);
  provider.BrowserChildProcessCrashed(child_process_data,
                                      abnormal_termination_info);

  // Call ProvideStabilityMetrics to check that it will force pending tasks to
  // be executed immediately.
  metrics::SystemProfileProto system_profile;

  provider.ProvideStabilityMetrics(&system_profile);

  // Check current number of instances created.
  const metrics::SystemProfileProto_Stability& stability =
      system_profile.stability();

  EXPECT_EQ(2, stability.child_process_crash_count());
  EXPECT_TRUE(
      histogram_tester.GetTotalCountsForPrefix("ChildProcess.").empty());
}

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

  // Call ProvideStabilityMetrics to check that it will force pending tasks to
  // be executed immediately.
  metrics::SystemProfileProto system_profile;

  provider.ProvideStabilityMetrics(&system_profile);

  // Check current number of instances created.
  const metrics::SystemProfileProto_Stability& stability =
      system_profile.stability();

  EXPECT_EQ(2, stability.child_process_crash_count());

  // Utility processes also log an entries for the hashed name of the process
  // for launches and crashes.
  histogram_tester.ExpectUniqueSample(
      "ChildProcess.Launched.UtilityProcessHash",
      variations::HashName(kTestUtilityProcessName), 1);
  histogram_tester.ExpectUniqueSample(
      "ChildProcess.Crashed.UtilityProcessHash",
      variations::HashName(kTestUtilityProcessName), 2);
  histogram_tester.ExpectUniqueSample(
      "ChildProcess.Crashed.UtilityProcessExitCode", kExitCode, 2);
}

TEST_F(ContentStabilityMetricsProviderTest, NotificationObserver) {
  metrics::ContentStabilityMetricsProvider provider(prefs(), nullptr);
  content::TestBrowserContext browser_context;
  content::MockRenderProcessHostFactory rph_factory;
  scoped_refptr<content::SiteInstance> site_instance(
      content::SiteInstance::Create(&browser_context));

  // Owned by rph_factory.
  content::RenderProcessHost* host(rph_factory.CreateRenderProcessHost(
      &browser_context, site_instance.get()));

  // Crash and abnormal termination should increment renderer crash count.
  content::ChildProcessTerminationInfo crash_details;
  crash_details.status = base::TERMINATION_STATUS_PROCESS_CRASHED;
  crash_details.exit_code = 1;
  provider.Observe(
      content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
      content::Source<content::RenderProcessHost>(host),
      content::Details<content::ChildProcessTerminationInfo>(&crash_details));

  content::ChildProcessTerminationInfo term_details;
  term_details.status = base::TERMINATION_STATUS_ABNORMAL_TERMINATION;
  term_details.exit_code = 1;
  provider.Observe(
      content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
      content::Source<content::RenderProcessHost>(host),
      content::Details<content::ChildProcessTerminationInfo>(&term_details));

  // Kill does not increment renderer crash count.
  content::ChildProcessTerminationInfo kill_details;
  kill_details.status = base::TERMINATION_STATUS_PROCESS_WAS_KILLED;
  kill_details.exit_code = 1;
  provider.Observe(
      content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
      content::Source<content::RenderProcessHost>(host),
      content::Details<content::ChildProcessTerminationInfo>(&kill_details));

  // Failed launch increments failed launch count.
  content::ChildProcessTerminationInfo failed_launch_details;
  failed_launch_details.status = base::TERMINATION_STATUS_LAUNCH_FAILED;
  failed_launch_details.exit_code = 1;
  provider.Observe(content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                   content::Source<content::RenderProcessHost>(host),
                   content::Details<content::ChildProcessTerminationInfo>(
                       &failed_launch_details));

  metrics::SystemProfileProto system_profile;

  // Call ProvideStabilityMetrics to check that it will force pending tasks to
  // be executed immediately.
  provider.ProvideStabilityMetrics(&system_profile);

  EXPECT_EQ(2, system_profile.stability().renderer_crash_count());
  EXPECT_EQ(1, system_profile.stability().renderer_failed_launch_count());
  EXPECT_EQ(0, system_profile.stability().extension_renderer_crash_count());
}

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

  // Crash and abnormal termination should increment extension crash count.
  content::ChildProcessTerminationInfo crash_details;
  crash_details.status = base::TERMINATION_STATUS_PROCESS_CRASHED;
  crash_details.exit_code = 1;
  provider.Observe(
      content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
      content::Source<content::RenderProcessHost>(extension_host),
      content::Details<content::ChildProcessTerminationInfo>(&crash_details));

  // Failed launch increments failed launch count.
  content::ChildProcessTerminationInfo failed_launch_details;
  failed_launch_details.status = base::TERMINATION_STATUS_LAUNCH_FAILED;
  failed_launch_details.exit_code = 1;
  provider.Observe(content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                   content::Source<content::RenderProcessHost>(extension_host),
                   content::Details<content::ChildProcessTerminationInfo>(
                       &failed_launch_details));

  metrics::SystemProfileProto system_profile;
  provider.ProvideStabilityMetrics(&system_profile);

  EXPECT_EQ(0, system_profile.stability().renderer_crash_count());
  EXPECT_EQ(1, system_profile.stability().extension_renderer_crash_count());
  EXPECT_EQ(
      1, system_profile.stability().extension_renderer_failed_launch_count());
}
#endif

}  // namespace metrics
