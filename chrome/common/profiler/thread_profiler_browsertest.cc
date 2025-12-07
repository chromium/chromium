// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/thread_annotations.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
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

  void SetFoundClosure(const base::RepeatingClosure& found_closure) {
    base::AutoLock lock(lock_);
    found_closure_ = found_closure;
  }

  void SetPredicate(const Predicate& predicate) {
    base::AutoLock lock(lock_);
    predicate_ = predicate;
  }

  bool ProfileWasFound() {
    base::AutoLock lock(lock_);
    return found_profile_;
  }

  void Intercept(metrics::SampledProfile profile) {
    base::AutoLock lock(lock_);
    if (predicate_.is_null()) {
      pending_profiles_.push_back(profile);
    } else {
      CHECK(!found_closure_.is_null());
      if (predicate_.Run(profile)) {
        OnProfileFound();
        return;
      }
      for (const auto& pending_profile : pending_profiles_) {
        if (predicate_.Run(pending_profile)) {
          OnProfileFound();
          break;
        }
      }
      pending_profiles_.clear();
    }
  }

 private:
  void OnProfileFound() EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    found_profile_ = true;
    found_closure_.Run();
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
  ProfileInterceptor::GetInstance().SetFoundClosure(run_loop.QuitClosure());
  ProfileInterceptor::GetInstance().SetPredicate(predicate);

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
