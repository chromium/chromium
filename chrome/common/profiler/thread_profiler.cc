// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiler/thread_profiler.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/work_id_provider.h"
#include "base/no_destructor.h"
#include "base/process/process.h"
#include "base/profiler/profiler_buildflags.h"
#include "base/profiler/sample_metadata.h"
#include "base/profiler/sampling_profiler_thread_token.h"
#include "base/rand_util.h"
#include "base/threading/platform_thread.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/common/profiler/process_type.h"
#include "chrome/common/profiler/thread_profiler_configuration.h"
#include "components/metrics/call_stack_profile_builder.h"
#include "components/metrics/call_stack_profile_metrics_provider.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_names.mojom.h"
#include "sandbox/policy/sandbox.h"

#if defined(OS_ANDROID) && BUILDFLAG(ENABLE_ARM_CFI_TABLE)
#include <sys/types.h>
#include <unistd.h>

#include "base/android/apk_assets.h"
#include "base/files/memory_mapped_file.h"
#include "base/profiler/arm_cfi_table.h"
#include "base/profiler/chrome_unwinder_android.h"
#include "chrome/android/modules/stack_unwinder/public/module.h"

extern "C" {
// The address of |__executable_start| is the base address of the executable or
// shared library.
extern char __executable_start;
}
#endif  // defined(OS_ANDROID)

using CallStackProfileBuilder = metrics::CallStackProfileBuilder;
using CallStackProfileParams = metrics::CallStackProfileParams;
using StackSamplingProfiler = base::StackSamplingProfiler;

namespace {

// Pointer to the main thread instance, if any. Stored as a global because it's
// created very early in chrome/app - and is thus otherwise inaccessible from
// chrome_dll, by the time we need to register the main thread task runner.
ThreadProfiler* g_main_thread_instance = nullptr;

// Run continuous profiling 2% of the time.
constexpr const double kFractionOfExecutionTimeToSample = 0.02;

bool IsCurrentProcessBackgrounded() {
#if defined(OS_MAC)
  // Port provider that returns the calling process's task port, ignoring its
  // argument.
  class SelfPortProvider : public base::PortProvider {
    mach_port_t TaskForPid(base::ProcessHandle process) const override {
      return mach_task_self();
    }
  };
  SelfPortProvider provider;
  return base::Process::Current().IsProcessBackgrounded(&provider);
#else   // defined(OS_MAC)
  return base::Process::Current().IsProcessBackgrounded();
#endif  // defined(OS_MAC)
}

#if defined(OS_ANDROID) && BUILDFLAG(ENABLE_ARM_CFI_TABLE)
// Encapsulates the setup required to create the Chrome unwinder on Android.
class ChromeUnwinderCreator {
 public:
  ChromeUnwinderCreator() {
    constexpr char kCfiFileName[] = "assets/unwind_cfi_32";

    base::MemoryMappedFile::Region cfi_region;
    int fd = base::android::OpenApkAsset(kCfiFileName, &cfi_region);
    DCHECK_GE(fd, 0);
    bool mapped_file_ok =
        chrome_cfi_file_.Initialize(base::File(fd), cfi_region);
    DCHECK(mapped_file_ok);
    chrome_cfi_table_ = base::ArmCFITable::Parse(
        {chrome_cfi_file_.data(), chrome_cfi_file_.length()});
    DCHECK(chrome_cfi_table_);
  }

  ChromeUnwinderCreator(const ChromeUnwinderCreator&) = delete;
  ChromeUnwinderCreator& operator=(const ChromeUnwinderCreator&) = delete;

  std::unique_ptr<base::Unwinder> Create() {
    return std::make_unique<base::ChromeUnwinderAndroid>(
        chrome_cfi_table_.get(),
        reinterpret_cast<uintptr_t>(&__executable_start));
  }

 private:
  base::MemoryMappedFile chrome_cfi_file_;
  std::unique_ptr<base::ArmCFITable> chrome_cfi_table_;
};

// Encapsulates the setup required to create the Android native unwinder.
class NativeUnwinderCreator {
 public:
  NativeUnwinderCreator()
      : module_(stack_unwinder::Module::Load()),
        memory_regions_map_(module_->CreateMemoryRegionsMap()) {}
  NativeUnwinderCreator(const NativeUnwinderCreator&) = delete;
  NativeUnwinderCreator& operator=(const NativeUnwinderCreator&) = delete;

  std::unique_ptr<base::Unwinder> Create() {
    return module_->CreateNativeUnwinder(
        memory_regions_map_.get(),
        reinterpret_cast<uintptr_t>(&__executable_start));
  }

 private:
  const std::unique_ptr<stack_unwinder::Module> module_;
  const std::unique_ptr<stack_unwinder::MemoryRegionsMap> memory_regions_map_;
};

// This function must only be called via the closure created in
// CreateCoreUnwindersFactory(), which is passed into StackSamplingProfiler.
// This ensures the expensive work involved in constructing the unwinder
// creators is done on the profiler thread rather than the profiled thread.
// These take 50+ ms to execute due to the creation of the memory regions map
// and lookup of the modules in the process.
std::vector<std::unique_ptr<base::Unwinder>> CreateCoreUnwinders() {
  // This function is expected to be run on the profiler thread which is never
  // the main thread in the process.
  DCHECK_NE(getpid(), gettid());

  static base::NoDestructor<NativeUnwinderCreator> native_unwinder_creator;
  static base::NoDestructor<ChromeUnwinderCreator> chrome_unwinder_creator;

  // Note order matters: the more general unwinder must appear first in the
  // vector.
  std::vector<std::unique_ptr<base::Unwinder>> unwinders;
  unwinders.push_back(native_unwinder_creator->Create());
  unwinders.push_back(chrome_unwinder_creator->Create());
  return unwinders;
}
#endif  // defined(OS_ANDROID) && BUILDFLAG(ENABLE_ARM_CFI_TABLE)

base::StackSamplingProfiler::UnwindersFactory CreateCoreUnwindersFactory() {
#if defined(OS_ANDROID) && BUILDFLAG(ENABLE_ARM_CFI_TABLE)
  // The module is loadable if the profiler is enabled for the current
  // process.
  CHECK(
      ThreadProfilerConfiguration::Get()->IsProfilerEnabledForCurrentProcess());

  return base::BindOnce(&CreateCoreUnwinders);
#else
  return base::StackSamplingProfiler::UnwindersFactory();
#endif
}

const base::RepeatingClosure GetApplyPerSampleMetadataCallback(
    CallStackProfileParams::Process process) {
  if (process != CallStackProfileParams::RENDERER_PROCESS)
    return base::RepeatingClosure();
  static const base::SampleMetadata process_backgrounded("ProcessBackgrounded");
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
  base::WorkIdProvider* const work_id_provider_;
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
  auto instance =
      base::WrapUnique(new ThreadProfiler(CallStackProfileParams::MAIN_THREAD));
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
  startup_profiler_->AddAuxUnwinder(aux_unwinder_factory_.Run());
  if (periodic_profiler_)
    periodic_profiler_->AddAuxUnwinder(aux_unwinder_factory_.Run());
}

// static
void ThreadProfiler::StartOnChildThread(CallStackProfileParams::Thread thread) {
  // The profiler object is stored in a SequenceLocalStorageSlot on child
  // threads to give it the same lifetime as the threads.
  static base::NoDestructor<
      base::SequenceLocalStorageSlot<std::unique_ptr<ThreadProfiler>>>
      child_thread_profiler_sequence_local_storage;

  if (!ThreadProfilerConfiguration::Get()
           ->IsProfilerEnabledForCurrentProcessAndThread(thread)) {
    return;
  }

  child_thread_profiler_sequence_local_storage->emplace(
      new ThreadProfiler(thread, base::ThreadTaskRunnerHandle::Get()));
}

// static
void ThreadProfiler::SetBrowserProcessReceiverCallback(
    const base::RepeatingCallback<void(base::TimeTicks,
                                       metrics::SampledProfile)>& callback) {
  CallStackProfileBuilder::SetBrowserProcessReceiverCallback(callback);
}

// static
void ThreadProfiler::SetCollectorForChildProcess(
    mojo::PendingRemote<metrics::mojom::CallStackProfileCollector> collector) {
  if (!ThreadProfilerConfiguration::Get()->IsProfilerEnabledForCurrentProcess())
    return;

  DCHECK_NE(CallStackProfileParams::BROWSER_PROCESS,
            GetProfileParamsProcess(*base::CommandLine::ForCurrentProcess()));
  CallStackProfileBuilder::SetParentProfileCollectorForChildProcess(
      std::move(collector));
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

  startup_profiler_ = std::make_unique<StackSamplingProfiler>(
      base::GetSamplingProfilerCurrentThreadToken(), sampling_params,
      std::make_unique<CallStackProfileBuilder>(
          CallStackProfileParams(process_, thread,
                                 CallStackProfileParams::PROCESS_STARTUP),
          work_id_recorder_.get()),
      CreateCoreUnwindersFactory(),
      GetApplyPerSampleMetadataCallback(process_));

  startup_profiler_->Start();

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
          CallStackProfileParams(process_, thread_,
                                 CallStackProfileParams::PERIODIC_COLLECTION),
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
