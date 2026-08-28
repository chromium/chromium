// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/task/thread_pool.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/thread_annotations.h"
#include "base/threading/platform_thread.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/profiler/thread_profiler_configuration.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/metrics/call_stacks/call_stack_profile_metrics_provider.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_test.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace {

// Class that intercepts and stores profiles provided to the
// CallStackProfileMetricsProvider. Intercept() is invoked on the profiler
// thread while FetchProfiles() is invoked on the main thread.
class ProfileInterceptor {
 public:
  using Predicate =
      base::RepeatingCallback<bool(const metrics::SampledProfile&)>;

  // Get the static object instance. This object must leak because there is no
  // synchronization between it and the profiler thread which can invoke
  // Intercept at any time.
  static ProfileInterceptor& GetInstance() {
    static base::NoDestructor<ProfileInterceptor> instance;
    return *instance;
  }

  void SetCallbackAndPredicate(const base::RepeatingClosure& found_closure,
                               const Predicate& predicate) {
    base::AutoLock lock(lock_);
    found_closure_ = found_closure;
    predicate_ = predicate;
    found_profile_ = false;
    CheckPendingProfilesLockRequired();
  }

  bool ProfileWasFound() {
    base::AutoLock lock(lock_);
    return found_profile_;
  }

  void Intercept(metrics::SampledProfile profile) {
    base::AutoLock lock(lock_);
    pending_profiles_.push_back(std::move(profile));
    if (!predicate_.is_null() && !found_closure_.is_null() && !found_profile_) {
      CheckPendingProfilesLockRequired();
    }
  }

 private:
  void CheckPendingProfilesLockRequired() EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    for (auto it = pending_profiles_.begin(); it != pending_profiles_.end();) {
      if (predicate_.Run(*it)) {
        found_profile_ = true;
        it = pending_profiles_.erase(it);
        found_closure_.Run();
        return;
      } else {
        ++it;
      }
    }
  }

  base::Lock lock_;
  base::RepeatingClosure found_closure_ GUARDED_BY(lock_);
  Predicate predicate_ GUARDED_BY(lock_);
  std::vector<metrics::SampledProfile> pending_profiles_ GUARDED_BY(lock_);
  bool found_profile_ GUARDED_BY(lock_) = false;
};

// Returns true if |profile| has the specified properties |trigger_event|,
// |process| and |thread|. Returns false otherwise.
bool MatchesProfile(metrics::SampledProfile::TriggerEvent trigger_event,
                    metrics::Process process,
                    metrics::Thread thread,
                    const metrics::SampledProfile& profile) {
  return profile.trigger_event() == trigger_event &&
         profile.process() == process && profile.thread() == thread;
}

class ThreadProfilerBrowserTest : public PlatformBrowserTest {
 public:
  ThreadProfilerBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(kSamplingProfilerOnWorkerThreads);
  }

  void SetUp() override {
    // Arrange to intercept the CPU profiles at the time they're provided to the
    // metrics component.
    metrics::CallStackProfileMetricsProvider::
        SetCpuInterceptorCallbackForTesting(base::BindRepeating(
            &ProfileInterceptor::Intercept,
            base::Unretained(&ProfileInterceptor::GetInstance())));
    PlatformBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Enable the special browser test mode.
    command_line->AppendSwitchASCII(switches::kStartStackProfiler,
                                    switches::kStartStackProfilerBrowserTest);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Wait for a profile with the specified properties.
bool WaitForProfile(metrics::SampledProfile::TriggerEvent trigger_event,
                    metrics::Process process,
                    metrics::Thread thread) {
  // Profiling is only enabled for trunk builds and non-stable channels.
  // Perform an early return and pass the test for the other channels.
  switch (chrome::GetChannel()) {
    case version_info::Channel::UNKNOWN:
    case version_info::Channel::CANARY:
    case version_info::Channel::DEV:
    case version_info::Channel::BETA:
      break;

    default:
      return true;
  }
  auto predicate =
      base::BindRepeating(&MatchesProfile, trigger_event, process, thread);

  base::RunLoop run_loop;
  ProfileInterceptor::GetInstance().SetCallbackAndPredicate(
      run_loop.QuitClosure(), predicate);

  if (ProfileInterceptor::GetInstance().ProfileWasFound()) {
    return true;
  }

  base::test::ScopedRunLoopTimeout timeout(FROM_HERE, base::Seconds(30));
  run_loop.Run();
  return ProfileInterceptor::GetInstance().ProfileWasFound();
}

}  // namespace

#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_ARMEL)
// Android doesn't have a network service process.
#define MAYBE_NetworkServiceProcessIOThread \
  DISABLED_NetworkServiceProcessIOThread
#else
#define MAYBE_NetworkServiceProcessIOThread NetworkServiceProcessIOThread
#endif

// Check that we receive startup profiles in the browser process for profiled
// processes/threads. We've seen multiple breakages previously where profiles
// were dropped as a result of bugs introduced by mojo refactorings.

IN_PROC_BROWSER_TEST_F(ThreadProfilerBrowserTest, BrowserProcessMainThread) {
  EXPECT_TRUE(WaitForProfile(metrics::SampledProfile::PROCESS_STARTUP,
                             metrics::BROWSER_PROCESS, metrics::MAIN_THREAD));
}

IN_PROC_BROWSER_TEST_F(ThreadProfilerBrowserTest, BrowserProcessIOThread) {
  EXPECT_TRUE(WaitForProfile(metrics::SampledProfile::PROCESS_STARTUP,
                             metrics::BROWSER_PROCESS, metrics::IO_THREAD));
}

IN_PROC_BROWSER_TEST_F(ThreadProfilerBrowserTest, GpuProcessMainThread) {
  EXPECT_TRUE(WaitForProfile(metrics::SampledProfile::PROCESS_STARTUP,
                             metrics::GPU_PROCESS, metrics::MAIN_THREAD));
}

IN_PROC_BROWSER_TEST_F(ThreadProfilerBrowserTest, GpuProcessIOThread) {
  EXPECT_TRUE(WaitForProfile(metrics::SampledProfile::PROCESS_STARTUP,
                             metrics::GPU_PROCESS, metrics::IO_THREAD));
}

IN_PROC_BROWSER_TEST_F(ThreadProfilerBrowserTest, GpuProcessCompositorThread) {
  EXPECT_TRUE(WaitForProfile(metrics::SampledProfile::PROCESS_STARTUP,
                             metrics::GPU_PROCESS, metrics::COMPOSITOR_THREAD));
}

IN_PROC_BROWSER_TEST_F(ThreadProfilerBrowserTest, RendererProcessMainThread) {
  EXPECT_TRUE(WaitForProfile(metrics::SampledProfile::PROCESS_STARTUP,
                             metrics::RENDERER_PROCESS, metrics::MAIN_THREAD));
}

IN_PROC_BROWSER_TEST_F(ThreadProfilerBrowserTest, RendererProcessIOThread) {
  EXPECT_TRUE(WaitForProfile(metrics::SampledProfile::PROCESS_STARTUP,
                             metrics::RENDERER_PROCESS, metrics::IO_THREAD));
}

IN_PROC_BROWSER_TEST_F(ThreadProfilerBrowserTest,
                       RendererProcessCompositorThread) {
  EXPECT_TRUE(WaitForProfile(metrics::SampledProfile::PROCESS_STARTUP,
                             metrics::RENDERER_PROCESS,
                             metrics::COMPOSITOR_THREAD));
}

IN_PROC_BROWSER_TEST_F(ThreadProfilerBrowserTest,
                       MAYBE_NetworkServiceProcessIOThread) {
  EXPECT_TRUE(WaitForProfile(metrics::SampledProfile::PROCESS_STARTUP,
                             metrics::NETWORK_SERVICE_PROCESS,
                             metrics::IO_THREAD));
}

IN_PROC_BROWSER_TEST_F(ThreadProfilerBrowserTest, BrowserProcessThreadPool) {
  base::RepeatingTimer timer;
  timer.Start(FROM_HERE, base::Milliseconds(10), base::BindRepeating([] {
                base::ThreadPool::PostTask(FROM_HERE, base::DoNothing());
              }));
  EXPECT_TRUE(WaitForProfile(metrics::SampledProfile::PERIODIC_COLLECTION,
                             metrics::BROWSER_PROCESS,
                             metrics::THREAD_POOL_THREAD));
}
