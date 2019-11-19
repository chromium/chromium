// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/threading/platform_thread.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/metrics/call_stack_profile_metrics_provider.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace {

// Class that intercepts and stores profiles provided to the
// CallStackProfileMetricsProvider. Intercept() is invoked on the profiler
// thread while FetchProfiles() is invoked on the main thread.
class ProfileInterceptor {
 public:
  // Get the static object instance. This object must leak because there is no
  // synchronization between it and the profiler thread which can invoke
  // Intercept at any time.
  static ProfileInterceptor& GetInstance() {
    static base::NoDestructor<ProfileInterceptor> instance;
    return *instance;
  }

  void Intercept(metrics::SampledProfile profile) {
    base::AutoLock lock(lock_);
    profiles_.push_back(std::move(profile));
  }

  std::vector<metrics::SampledProfile> FetchProfiles() {
    base::AutoLock lock(lock_);
    std::vector<metrics::SampledProfile> profiles;
    profiles.swap(profiles_);
    return profiles;
  }

 private:
  base::Lock lock_;
  std::vector<metrics::SampledProfile> profiles_ GUARDED_BY(lock_);
};

class StackSamplingBrowserTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    // Arrange to intercept the CPU profiles at the time they're provided to the
    // metrics component.
    metrics::CallStackProfileMetricsProvider::
        SetCpuInterceptorCallbackForTesting(base::BindRepeating(
            &ProfileInterceptor::Intercept,
            base::Unretained(&ProfileInterceptor::GetInstance())));
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Enable the special browser test mode.
    command_line->AppendSwitchASCII(switches::kStartStackProfiler,
                                    switches::kStartStackProfilerBrowserTest);
  }
};

// Wait for a profile with the specified properties. Checks once per second
// until the profile is seen or we time out.
bool WaitForProfile(metrics::SampledProfile::TriggerEvent trigger_event,
                    metrics::Process process,
                    metrics::Thread thread) {
  // The profiling duration is one second when enabling browser test mode via
  // the kStartStackProfilerBrowserTest switch argument. We expect to see the
  // profiles shortly thereafter, but wait up to 30 seconds to give ample time
  // to avoid flaky failures.
  int seconds_to_wait = 30;
  do {
    std::vector<metrics::SampledProfile> profiles =
        ProfileInterceptor::GetInstance().FetchProfiles();
    const bool was_received =
        std::find_if(profiles.begin(), profiles.end(),
                     [&](const metrics::SampledProfile& profile) {
                       return profile.trigger_event() == trigger_event &&
                              profile.process() == process &&
                              profile.thread() == thread;
                     }) != profiles.end();
    if (was_received)
      return true;
    base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(1));
    // Manually spinning message loop is fine here because the main thread
    // message loop will not be continuously busy at Chrome startup, and we will
    // spin it enough over 30 seconds to ensure that any necessary processing is
    // done.
    base::RunLoop().RunUntilIdle();
  } while (--seconds_to_wait > 0);

  return false;
}

}  // namespace

// Check that we receive startup profiles in the browser process for profiled
// processes/threads. We've seen multiple breakages previously where profiles
// were dropped as a result of bugs introduced by mojo refactorings.

IN_PROC_BROWSER_TEST_F(StackSamplingBrowserTest, BrowserProcessMainThread) {
  EXPECT_TRUE(WaitForProfile(metrics::SampledProfile::PROCESS_STARTUP,
                             metrics::BROWSER_PROCESS, metrics::MAIN_THREAD));
}

IN_PROC_BROWSER_TEST_F(StackSamplingBrowserTest, BrowserProcessIOThread) {
  EXPECT_TRUE(WaitForProfile(metrics::SampledProfile::PROCESS_STARTUP,
                             metrics::BROWSER_PROCESS, metrics::IO_THREAD));
}

IN_PROC_BROWSER_TEST_F(StackSamplingBrowserTest, GpuProcessMainThread) {
  EXPECT_TRUE(WaitForProfile(metrics::SampledProfile::PROCESS_STARTUP,
                             metrics::GPU_PROCESS, metrics::MAIN_THREAD));
}

IN_PROC_BROWSER_TEST_F(StackSamplingBrowserTest, GpuProcessIOThread) {
  EXPECT_TRUE(WaitForProfile(metrics::SampledProfile::PROCESS_STARTUP,
                             metrics::GPU_PROCESS, metrics::IO_THREAD));
}

IN_PROC_BROWSER_TEST_F(StackSamplingBrowserTest, GpuProcessCompositorThread) {
  EXPECT_TRUE(WaitForProfile(metrics::SampledProfile::PROCESS_STARTUP,
                             metrics::GPU_PROCESS, metrics::COMPOSITOR_THREAD));
}

IN_PROC_BROWSER_TEST_F(StackSamplingBrowserTest, RendererProcessMainThread) {
  EXPECT_TRUE(WaitForProfile(metrics::SampledProfile::PROCESS_STARTUP,
                             metrics::RENDERER_PROCESS, metrics::MAIN_THREAD));
}

IN_PROC_BROWSER_TEST_F(StackSamplingBrowserTest, RendererProcessIOThread) {
  EXPECT_TRUE(WaitForProfile(metrics::SampledProfile::PROCESS_STARTUP,
                             metrics::RENDERER_PROCESS, metrics::IO_THREAD));
}

IN_PROC_BROWSER_TEST_F(StackSamplingBrowserTest,
                       RendererProcessCompositorThread) {
  EXPECT_TRUE(WaitForProfile(metrics::SampledProfile::PROCESS_STARTUP,
                             metrics::RENDERER_PROCESS,
                             metrics::COMPOSITOR_THREAD));
}
