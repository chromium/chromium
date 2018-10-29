// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/thread_profiler.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/rand_util.h"
#include "base/threading/platform_thread.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/common/stack_sampling_configuration.h"
#include "components/metrics/call_stack_profile_metrics_provider.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_names.mojom.h"
#include "services/service_manager/embedder/switches.h"
#include "services/service_manager/public/cpp/connector.h"

using CallStackProfileParams = metrics::CallStackProfileParams;
using LegacyCallStackProfileBuilder = metrics::LegacyCallStackProfileBuilder;
using StackSamplingProfiler = base::StackSamplingProfiler;

namespace {

// The profiler object is stored in a SequenceLocalStorageSlot on child threads
// to give it the same lifetime as the threads.
base::LazyInstance<
    base::SequenceLocalStorageSlot<std::unique_ptr<ThreadProfiler>>>::Leaky
    g_child_thread_profiler_sequence_local_storage = LAZY_INSTANCE_INITIALIZER;

// Run continuous profiling 2% of the time.
constexpr const double kFractionOfExecutionTimeToSample = 0.02;

constexpr struct StackSamplingProfiler::SamplingParams kSamplingParams = {
    /* initial_delay= */ base::TimeDelta::FromMilliseconds(0),
    /* samples_per_profile= */ 300,
    /* sampling_interval= */ base::TimeDelta::FromMilliseconds(100),
    /* keep_consistent_sampling_interval= */ true};

CallStackProfileParams::Process GetProcess() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  std::string process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);
  if (process_type.empty())
    return CallStackProfileParams::BROWSER_PROCESS;
  if (process_type == switches::kRendererProcess)
    return CallStackProfileParams::RENDERER_PROCESS;
  if (process_type == switches::kGpuProcess)
    return CallStackProfileParams::GPU_PROCESS;
  if (process_type == switches::kUtilityProcess)
    return CallStackProfileParams::UTILITY_PROCESS;
  if (process_type == service_manager::switches::kZygoteProcess)
    return CallStackProfileParams::ZYGOTE_PROCESS;
  if (process_type == switches::kPpapiPluginProcess)
    return CallStackProfileParams::PPAPI_PLUGIN_PROCESS;
  if (process_type == switches::kPpapiBrokerProcess)
    return CallStackProfileParams::PPAPI_BROKER_PROCESS;
  return CallStackProfileParams::UNKNOWN_PROCESS;
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
    : period_duration_(
          base::TimeDelta::FromSecondsD(sampling_duration.InSecondsF() /
                                        fraction_of_execution_time_to_sample)),
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

  double sampling_offset_seconds =
      (period_duration_ - sampling_duration_).InSecondsF() * RandDouble();
  base::TimeTicks next_collection_time =
      period_start_time_ +
      base::TimeDelta::FromSecondsD(sampling_offset_seconds);
  period_start_time_ += period_duration_;
  return next_collection_time - now;
}

double PeriodicSamplingScheduler::RandDouble() const {
  return base::RandDouble();
}

base::TimeTicks PeriodicSamplingScheduler::Now() const {
  return base::TimeTicks::Now();
}

ThreadProfiler::~ThreadProfiler() {}

// static
std::unique_ptr<ThreadProfiler> ThreadProfiler::CreateAndStartOnMainThread() {
  return std::unique_ptr<ThreadProfiler>(
      new ThreadProfiler(CallStackProfileParams::MAIN_THREAD));
}

void ThreadProfiler::SetMainThreadTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  if (!StackSamplingConfiguration::Get()->IsProfilerEnabledForCurrentProcess())
    return;

  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // This should only be called if the task runner wasn't provided in the
  // constructor.
  DCHECK(!owning_thread_task_runner_);
  owning_thread_task_runner_ = task_runner;
  ScheduleNextPeriodicCollection();
}

// static
void ThreadProfiler::StartOnChildThread(CallStackProfileParams::Thread thread) {
  if (!StackSamplingConfiguration::Get()->IsProfilerEnabledForCurrentProcess())
    return;

  auto profiler = std::unique_ptr<ThreadProfiler>(
      new ThreadProfiler(thread, base::ThreadTaskRunnerHandle::Get()));
  g_child_thread_profiler_sequence_local_storage.Get().Set(std::move(profiler));
}

// static
void ThreadProfiler::SetBrowserProcessReceiverCallback(
    const base::RepeatingCallback<void(base::TimeTicks,
                                       metrics::SampledProfile)>& callback) {
  metrics::LegacyCallStackProfileBuilder::SetBrowserProcessReceiverCallback(
      callback);
}

// static
void ThreadProfiler::SetServiceManagerConnectorForChildProcess(
    service_manager::Connector* connector) {
  if (!StackSamplingConfiguration::Get()->IsProfilerEnabledForCurrentProcess())
    return;

  DCHECK_NE(CallStackProfileParams::BROWSER_PROCESS, GetProcess());

  metrics::mojom::CallStackProfileCollectorPtr browser_interface;
  connector->BindInterface(content::mojom::kBrowserServiceName,
                           &browser_interface);
  LegacyCallStackProfileBuilder::SetParentProfileCollectorForChildProcess(
      std::move(browser_interface));
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
    : owning_thread_task_runner_(owning_thread_task_runner),
      periodic_profile_params_(GetProcess(),
                               thread,
                               CallStackProfileParams::PERIODIC_COLLECTION),
      weak_factory_(this) {
  if (!StackSamplingConfiguration::Get()->IsProfilerEnabledForCurrentProcess())
    return;

  auto profile_builder =
      std::make_unique<LegacyCallStackProfileBuilder>(CallStackProfileParams(
          GetProcess(), thread, CallStackProfileParams::PROCESS_STARTUP));

  startup_profiler_ = std::make_unique<StackSamplingProfiler>(
      base::PlatformThread::CurrentId(), kSamplingParams,
      std::move(profile_builder));

  startup_profiler_->Start();

  // Estimated time at which the startup profiling will be completed. It's OK if
  // this doesn't exactly coincide with the end of the startup profiling, since
  // there's no harm in having a brief overlap of startup and periodic
  // profiling.
  base::TimeTicks startup_profiling_completion_time =
      base::TimeTicks::Now() +
      kSamplingParams.samples_per_profile * kSamplingParams.sampling_interval;

  periodic_sampling_scheduler_ = std::make_unique<PeriodicSamplingScheduler>(
      kSamplingParams.samples_per_profile * kSamplingParams.sampling_interval,
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
  auto profile_builder = std::make_unique<LegacyCallStackProfileBuilder>(
      periodic_profile_params_,
      base::BindOnce(&ThreadProfiler::OnPeriodicCollectionCompleted,
                     owning_thread_task_runner_, weak_factory_.GetWeakPtr()));

  periodic_profiler_ = std::make_unique<StackSamplingProfiler>(
      base::PlatformThread::CurrentId(), kSamplingParams,
      std::move(profile_builder));

  periodic_profiler_->Start();
}
