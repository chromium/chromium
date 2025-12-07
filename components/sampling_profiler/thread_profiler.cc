// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sampling_profiler/thread_profiler.h"

#include <string>
#include <utility>
#include <vector>

#include "base/android/library_loader/anchor_functions.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/message_loop/work_id_provider.h"
#include "base/process/process.h"
#include "base/profiler/profiler_buildflags.h"
#include "base/profiler/sample_metadata.h"
#include "base/profiler/sampling_profiler_thread_token.h"
#include "base/rand_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "build/blink_buildflags.h"
#include "build/build_config.h"
#include "components/metrics/call_stacks/call_stack_profile_builder.h"
#include "components/sampling_profiler/process_type.h"
#include "components/sampling_profiler/thread_profiler_client.h"

#if BUILDFLAG(IS_MAC) || (BUILDFLAG(IS_IOS) && BUILDFLAG(USE_BLINK))
#include "base/process/port_provider_mac.h"
#endif  // BUILDFLAG(IS_MAC) || (BUILDFLAG(IS_IOS) && BUILDFLAG(USE_BLINK))

namespace sampling_profiler {
namespace {

// Pointer to the main thread instance, if any. Stored as a global because it's
// created very early in chrome/app - and is thus otherwise inaccessible from
// chrome_dll, by the time we need to register the main thread task runner.
ThreadProfiler* g_main_thread_instance = nullptr;

// Pointer to the embedder-specific client implementation.
// |g_thread_profiler_client| is intentionally leaked on shutdown.
ThreadProfilerClient* g_thread_profiler_client = nullptr;

// The kFractionOfExecutionTimeToSample and SamplingParams settings in
// ThreadProfilerConfiguration::GetSamplingParams() specify fraction = 0.02 and
// sampling period = 1 sample / .1s sampling interval * 300 samples = 30s. The
// period length works out to 30s/0.02 = 1500s = 25m. So every 25 minutes a
// random 30 second continuous interval will be picked to sample.

// Run continuous profiling 2% of the time.
constexpr double kFractionOfExecutionTimeToSample = 0.02;

bool IsCurrentProcessBackgrounded() {
#if BUILDFLAG(IS_MAC) || (BUILDFLAG(IS_IOS) && BUILDFLAG(USE_BLINK))
  base::SelfPortProvider provider;
  return base::Process::Current().GetPriority(&provider) ==
         base::Process::Priority::kBestEffort;
#else   // BUILDFLAG(IS_MAC) || (BUILDFLAG(IS_IOS) && BUILDFLAG(USE_BLINK))
  return base::Process::Current().GetPriority() ==
         base::Process::Priority::kBestEffort;
#endif  // BUILDFLAG(IS_MAC) || (BUILDFLAG(IS_IOS) && BUILDFLAG(USE_BLINK))
}

const base::RepeatingClosure GetApplyPerSampleMetadataCallback(
    ProfilerProcessType process) {
  if (process != ProfilerProcessType::kRenderer) {
    return base::RepeatingClosure();
  }
  static const base::SampleMetadata process_backgrounded(
      "ProcessBackgrounded", base::SampleMetadataScope::kProcess);
  return base::BindRepeating(
      [](base::SampleMetadata process_backgrounded) {
        process_backgrounded.Set(IsCurrentProcessBackgrounded());
      },
      process_backgrounded);
}

}  // namespace

// Records the current unique id for the work item being executed in the target
// thread's message loop.
class ThreadProfiler::WorkIdRecorder : public metrics::WorkIdRecorder {
 public:
  explicit WorkIdRecorder(base::WorkIdProvider* work_id_provider)
      : work_id_provider_(work_id_provider) {}

  // Invoked on the profiler thread while the target thread is suspended.
  unsigned int RecordWorkId() const override {
    return work_id_provider_->GetWorkId();
  }

  WorkIdRecorder(const WorkIdRecorder&) = delete;
  WorkIdRecorder& operator=(const WorkIdRecorder&) = delete;

 private:
  const raw_ptr<base::WorkIdProvider> work_id_provider_;
};

ThreadProfiler::~ThreadProfiler() {
  if (g_main_thread_instance == this) {
    g_main_thread_instance = nullptr;
  }
}

// static
std::unique_ptr<ThreadProfiler> ThreadProfiler::CreateAndStartOnMainThread() {
  // If running in single process mode, there may be multiple "main thread"
  // profilers created. In this case, we assume the first created one is the
  // browser one.
  bool is_single_process =
      GetClient()->IsSingleProcess(*base::CommandLine::ForCurrentProcess());
  DCHECK(!g_main_thread_instance || is_single_process);
  auto instance =
      base::WrapUnique(new ThreadProfiler(ProfilerThreadType::kMain));
  if (!g_main_thread_instance) {
    g_main_thread_instance = instance.get();
  }
  return instance;
}

// static
void ThreadProfiler::SetMainThreadTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(g_main_thread_instance);
  g_main_thread_instance->SetMainThreadTaskRunnerImpl(task_runner);
}

void ThreadProfiler::SetAuxUnwinderFactory(
    const base::RepeatingCallback<std::unique_ptr<base::Unwinder>()>& factory) {
  if (!GetClient()->IsProfilerEnabledForCurrentProcessAndThread(thread_)) {
    return;
  }

  aux_unwinder_factory_ = factory;
  startup_profiler_->AddAuxUnwinder(aux_unwinder_factory_.Run());
  if (periodic_profiler_) {
    periodic_profiler_->AddAuxUnwinder(aux_unwinder_factory_.Run());
  }
}

// static
void ThreadProfiler::StartOnChildThread(ProfilerThreadType thread) {
  // The profiler object is stored in a SequenceLocalStorageSlot on child
  // threads to give it the same lifetime as the threads.
  static base::SequenceLocalStorageSlot<std::unique_ptr<ThreadProfiler>>
      child_thread_profiler_sequence_local_storage;

  if (!GetClient()->IsProfilerEnabledForCurrentProcessAndThread(thread)) {
    return;
  }

  child_thread_profiler_sequence_local_storage.emplace(new ThreadProfiler(
      thread, base::SingleThreadTaskRunner::GetCurrentDefault()));
}

// static
void ThreadProfiler::SetClient(std::unique_ptr<ThreadProfilerClient> client) {
  // Generally, the client should only be set once, at process startup. However,
  // some test infrastructure causes initialization to happen more than once.
  delete g_thread_profiler_client;
  g_thread_profiler_client = client.release();
}

// static
ThreadProfilerClient* ThreadProfiler::GetClient() {
  CHECK(g_thread_profiler_client);
  return g_thread_profiler_client;
}

// ThreadProfiler implementation synopsis:
//
// On creation, the profiler creates and starts the startup
// StackSamplingProfiler, and configures the PeriodicSamplingScheduler such that
// it starts scheduling from the time the startup profiling will be complete.
// When a message loop is available (either in the constructor, or via
// SetMainThreadTaskRunner) a task is posted to start the first periodic
// collection at the initial scheduled collection time.
//
// When the periodic collection task executes, it creates and starts a new
// periodic profiler and configures it to call OnPeriodicCollectionCompleted as
// its completion callback. OnPeriodicCollectionCompleted is called on the
// profiler thread and schedules a task on the original thread to schedule
// another periodic collection. When the task runs, it posts a new task to start
// another periodic collection at the next scheduled collection time.
//
// The process in previous paragraph continues until the ThreadProfiler is
// destroyed prior to thread exit.
ThreadProfiler::ThreadProfiler(
    ProfilerThreadType thread,
    scoped_refptr<base::SingleThreadTaskRunner> owning_thread_task_runner)
    : process_(
          GetClient()->GetProcessType(*base::CommandLine::ForCurrentProcess())),
      thread_(thread),
      owning_thread_task_runner_(owning_thread_task_runner),
      work_id_recorder_(std::make_unique<WorkIdRecorder>(
          base::WorkIdProvider::GetForCurrentThread())) {
  if (!GetClient()->IsProfilerEnabledForCurrentProcessAndThread(thread_)) {
    return;
  }

  const base::StackSamplingProfiler::SamplingParams sampling_params =
      GetClient()->GetSamplingParams();

  startup_profiler_ = CreateSamplingProfiler(
      sampling_params, CallStackProfileParams::Trigger::kProcessStartup,
      /*builder_completed_callback=*/base::OnceClosure());

  startup_profiler_->Start();

  // Estimated time at which the startup profiling will be completed. It's OK if
  // this doesn't exactly coincide with the end of the startup profiling, since
  // there's no harm in having a brief overlap of startup and periodic
  // profiling.
  base::TimeTicks startup_profiling_completion_time =
      base::TimeTicks::Now() +
      sampling_params.samples_per_profile * sampling_params.sampling_interval;

  periodic_sampling_scheduler_ =
      std::make_unique<base::PeriodicSamplingScheduler>(
          sampling_params.samples_per_profile *
              sampling_params.sampling_interval,
          kFractionOfExecutionTimeToSample, startup_profiling_completion_time);

  if (owning_thread_task_runner_) {
    ScheduleNextPeriodicCollection();
  }
}

std::unique_ptr<base::StackSamplingProfiler>
ThreadProfiler::CreateSamplingProfiler(
    base::StackSamplingProfiler::SamplingParams sampling_params,
    CallStackProfileParams::Trigger trigger,
    base::OnceClosure builder_completed_callback) {
  return std::make_unique<base::StackSamplingProfiler>(
      base::GetSamplingProfilerCurrentThreadToken(), sampling_params,
      GetClient()->CreateProfileBuilder(
          CallStackProfileParams(process_, thread_, trigger),
          work_id_recorder_.get(), std::move(builder_completed_callback)),
      GetClient()->GetUnwindersFactory(),
      GetApplyPerSampleMetadataCallback(process_));
}

// static
void ThreadProfiler::OnPeriodicCollectionCompleted(
    scoped_refptr<base::SingleThreadTaskRunner> owning_thread_task_runner,
    base::WeakPtr<ThreadProfiler> thread_profiler) {
  owning_thread_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&ThreadProfiler::ScheduleNextPeriodicCollection,
                                thread_profiler));
}

void ThreadProfiler::SetMainThreadTaskRunnerImpl(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  if (!GetClient()->IsProfilerEnabledForCurrentProcessAndThread(thread_)) {
    return;
  }

  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // This should only be called if the task runner wasn't provided in the
  // constructor.
  DCHECK(!owning_thread_task_runner_);
  owning_thread_task_runner_ = task_runner;
  ScheduleNextPeriodicCollection();
}

void ThreadProfiler::ScheduleNextPeriodicCollection() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  owning_thread_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ThreadProfiler::StartPeriodicSamplingCollection,
                     weak_factory_.GetWeakPtr()),
      periodic_sampling_scheduler_->GetTimeToNextCollection());
}

void ThreadProfiler::StartPeriodicSamplingCollection() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // NB: Destroys the previous profiler as side effect.
  periodic_profiler_ = CreateSamplingProfiler(
      GetClient()->GetSamplingParams(),
      CallStackProfileParams::Trigger::kPeriodicCollection,
      base::BindOnce(&ThreadProfiler::OnPeriodicCollectionCompleted,
                     owning_thread_task_runner_, weak_factory_.GetWeakPtr()));
  if (aux_unwinder_factory_) {
    periodic_profiler_->AddAuxUnwinder(aux_unwinder_factory_.Run());
  }

  periodic_profiler_->Start();
}

}  // namespace sampling_profiler
