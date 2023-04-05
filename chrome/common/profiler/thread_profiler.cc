// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiler/thread_profiler.h"

#include <string>
#include <utility>
#include <vector>

#include "base/android/library_loader/anchor_functions.h"
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
#include "build/build_config.h"
#include "chrome/common/profiler/process_type.h"
#include "chrome/common/profiler/thread_profiler_configuration.h"
#include "chrome/common/profiler/unwind_util.h"
#include "components/metrics/call_stack_profile_builder.h"
#include "components/metrics/call_stack_profile_metrics_provider.h"
#include "content/public/common/content_switches.h"
#include "sandbox/policy/sandbox.h"

#if BUILDFLAG(IS_ANDROID) && BUILDFLAG(ENABLE_ARM_CFI_TABLE)
#include "chrome/android/modules/stack_unwinder/public/module.h"
#endif  // BUILDFLAG(IS_ANDROID) && BUILDFLAG(ENABLE_ARM_CFI_TABLE)

#if BUILDFLAG(IS_MAC)
#include "base/process/port_provider_mac.h"
#endif  // BUILDFLAG(IS_MAC)

using CallStackProfileBuilder = metrics::CallStackProfileBuilder;
using CallStackProfileParams = metrics::CallStackProfileParams;
using StackSamplingProfiler = base::StackSamplingProfiler;

namespace {

// Pointer to the main thread instance, if any. Stored as a global because it's
// created very early in chrome/app - and is thus otherwise inaccessible from
// chrome_dll, by the time we need to register the main thread task runner.
ThreadProfiler* g_main_thread_instance = nullptr;

// Run continuous profiling 2% of the time.
constexpr double kFractionOfExecutionTimeToSample = 0.02;

bool IsCurrentProcessBackgrounded() {
#if BUILDFLAG(IS_MAC)
  base::SelfPortProvider provider;
  return base::Process::Current().IsProcessBackgrounded(&provider);
#else   // BUILDFLAG(IS_MAC)
  return base::Process::Current().IsProcessBackgrounded();
#endif  // BUILDFLAG(IS_MAC)
}

const base::RepeatingClosure GetApplyPerSampleMetadataCallback(
    CallStackProfileParams::Process process) {
  if (process != CallStackProfileParams::Process::kRenderer)
    return base::RepeatingClosure();
  static const base::SampleMetadata process_backgrounded(
      "ProcessBackgrounded", base::SampleMetadataScope::kProcess);
  return base::BindRepeating(
      [](base::SampleMetadata process_backgrounded) {
        process_backgrounded.Set(IsCurrentProcessBackgrounded());
      },
      process_backgrounded);
}

}  // namespace

// The scheduler works by splitting execution time into repeated periods such
// that the time to take one collection represents
// |fraction_of_execution_time_to_sample| of the period, and the time not spent
// sampling represents 1 - |fraction_of_execution_time_to_sample| of the period.
// The collection start time is chosen randomly within each period such that the
// entire collection is contained within the period.
//
// The kFractionOfExecutionTimeToSample and SamplingParams settings at the top
// of the file specify fraction = 0.02 and sampling period = 1 sample / .1s
// sampling interval * 300 samples = 30s. The period length works out to
// 30s/0.02 = 1500s = 25m. So every 25 minutes a random 30 second continuous
// interval will be picked to sample.
PeriodicSamplingScheduler::PeriodicSamplingScheduler(
    base::TimeDelta sampling_duration,
    double fraction_of_execution_time_to_sample,
    base::TimeTicks start_time)
    : period_duration_(sampling_duration /
                       fraction_of_execution_time_to_sample),
      sampling_duration_(sampling_duration),
      period_start_time_(start_time) {
  DCHECK(sampling_duration_ <= period_duration_);
}

PeriodicSamplingScheduler::~PeriodicSamplingScheduler() = default;

base::TimeDelta PeriodicSamplingScheduler::GetTimeToNextCollection() {
  const base::TimeTicks now = Now();
  // Avoid scheduling in the past in the presence of discontinuous jumps in
  // the current TimeTicks.
  period_start_time_ = std::max(period_start_time_, now);

  const base::TimeDelta sampling_offset =
      (period_duration_ - sampling_duration_) * RandDouble();
  const base::TimeTicks next_collection_time =
      period_start_time_ + sampling_offset;
  period_start_time_ += period_duration_;
  return next_collection_time - now;
}

double PeriodicSamplingScheduler::RandDouble() const {
  return base::RandDouble();
}

base::TimeTicks PeriodicSamplingScheduler::Now() const {
  return base::TimeTicks::Now();
}

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
  if (g_main_thread_instance == this)
    g_main_thread_instance = nullptr;
}

// static
std::unique_ptr<ThreadProfiler> ThreadProfiler::CreateAndStartOnMainThread() {
  // If running in single process mode, there may be multiple "main thread"
  // profilers created. In this case, we assume the first created one is the
  // browser one.
  auto* command_line = base::CommandLine::ForCurrentProcess();
  bool is_single_process = command_line->HasSwitch(switches::kSingleProcess) ||
                           command_line->HasSwitch(switches::kInProcessGPU);
  DCHECK(!g_main_thread_instance || is_single_process);
  auto instance = base::WrapUnique(
      new ThreadProfiler(CallStackProfileParams::Thread::kMain));
  if (!g_main_thread_instance)
    g_main_thread_instance = instance.get();
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
  if (!ThreadProfilerConfiguration::Get()
           ->IsProfilerEnabledForCurrentProcessAndThread(thread_)) {
    return;
  }

  aux_unwinder_factory_ = factory;
  if (startup_profiler_) {
    startup_profiler_->AddAuxUnwinder(aux_unwinder_factory_.Run());
  }
  if (periodic_profiler_)
    periodic_profiler_->AddAuxUnwinder(aux_unwinder_factory_.Run());
}

// static
void ThreadProfiler::StartOnChildThread(CallStackProfileParams::Thread thread) {
  // The profiler object is stored in a SequenceLocalStorageSlot on child
  // threads to give it the same lifetime as the threads.
  static base::SequenceLocalStorageSlot<std::unique_ptr<ThreadProfiler>>
      child_thread_profiler_sequence_local_storage;

  if (!ThreadProfilerConfiguration::Get()
           ->IsProfilerEnabledForCurrentProcessAndThread(thread)) {
    return;
  }

  child_thread_profiler_sequence_local_storage.emplace(new ThreadProfiler(
      thread, base::SingleThreadTaskRunner::GetCurrentDefault()));
}

// static
bool ThreadProfiler::ShouldCollectProfilesForChildProcess() {
  CallStackProfileParams::Process process =
      GetProfileParamsProcess(*base::CommandLine::ForCurrentProcess());
  DCHECK_NE(CallStackProfileParams::Process::kBrowser, process);
  return ThreadProfilerConfiguration::Get()
      ->IsProfilerEnabledForCurrentProcess();
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
    CallStackProfileParams::Thread thread,
    scoped_refptr<base::SingleThreadTaskRunner> owning_thread_task_runner)
    : process_(
          GetProfileParamsProcess(*base::CommandLine::ForCurrentProcess())),
      thread_(thread),
      owning_thread_task_runner_(owning_thread_task_runner),
      work_id_recorder_(std::make_unique<WorkIdRecorder>(
          base::WorkIdProvider::GetForCurrentThread())) {
  if (!ThreadProfilerConfiguration::Get()
           ->IsProfilerEnabledForCurrentProcessAndThread(thread_)) {
    return;
  }

  const base::StackSamplingProfiler::SamplingParams sampling_params =
      ThreadProfilerConfiguration::Get()->GetSamplingParams();

  if (ThreadProfilerConfiguration::Get()->IsStartupProfilingEnabled()) {
    startup_profiler_ = std::make_unique<StackSamplingProfiler>(
        base::GetSamplingProfilerCurrentThreadToken(), sampling_params,
        std::make_unique<CallStackProfileBuilder>(
            CallStackProfileParams(
                process_, thread,
                CallStackProfileParams::Trigger::kProcessStartup),
            work_id_recorder_.get()),
        CreateCoreUnwindersFactory(),
        GetApplyPerSampleMetadataCallback(process_));

    startup_profiler_->Start();
  }

  // Estimated time at which the startup profiling will be completed. It's OK if
  // this doesn't exactly coincide with the end of the startup profiling, since
  // there's no harm in having a brief overlap of startup and periodic
  // profiling.
  base::TimeTicks startup_profiling_completion_time =
      base::TimeTicks::Now() +
      sampling_params.samples_per_profile * sampling_params.sampling_interval;

  periodic_sampling_scheduler_ = std::make_unique<PeriodicSamplingScheduler>(
      sampling_params.samples_per_profile * sampling_params.sampling_interval,
      kFractionOfExecutionTimeToSample, startup_profiling_completion_time);

  if (owning_thread_task_runner_)
    ScheduleNextPeriodicCollection();
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
  if (!ThreadProfilerConfiguration::Get()
           ->IsProfilerEnabledForCurrentProcessAndThread(thread_)) {
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
  periodic_profiler_ = std::make_unique<StackSamplingProfiler>(
      base::GetSamplingProfilerCurrentThreadToken(),
      ThreadProfilerConfiguration::Get()->GetSamplingParams(),
      std::make_unique<CallStackProfileBuilder>(
          CallStackProfileParams(
              process_, thread_,
              CallStackProfileParams::Trigger::kPeriodicCollection),
          work_id_recorder_.get(),
          base::BindOnce(&ThreadProfiler::OnPeriodicCollectionCompleted,
                         owning_thread_task_runner_,
                         weak_factory_.GetWeakPtr())),
      CreateCoreUnwindersFactory(),
      GetApplyPerSampleMetadataCallback(process_));
  if (aux_unwinder_factory_)
    periodic_profiler_->AddAuxUnwinder(aux_unwinder_factory_.Run());

  periodic_profiler_->Start();
}
