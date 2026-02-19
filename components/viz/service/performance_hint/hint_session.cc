// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/performance_hint/hint_session.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)

#include <dlfcn.h>
#include <sys/types.h>

#include "base/android/android_info.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/switches.h"
#include "components/viz/service/performance_hint/boost_manager.h"

static_assert(sizeof(base::PlatformThreadId) == sizeof(int32_t),
              "thread id types incompatible");

extern "C" {

typedef struct APerformanceHintManager APerformanceHintManager;
typedef struct APerformanceHintSession APerformanceHintSession;

using pAPerformanceHint_getManager = APerformanceHintManager* (*)();
using pAPerformanceHint_createSession =
    APerformanceHintSession* (*)(APerformanceHintManager* manager,
                                 const int32_t* threadIds,
                                 size_t size,
                                 int64_t initialTargetWorkDurationNanos);
using pAPerformanceHint_updateTargetWorkDuration =
    int (*)(APerformanceHintSession* session, int64_t targetDurationNanos);
using pAPerformanceHint_reportActualWorkDuration =
    int (*)(APerformanceHintSession* session, int64_t actualDurationNanos);
using pAPerformanceHint_closeSession =
    void (*)(APerformanceHintSession* session);
using pAPerformanceHint_setThreads = int (*)(APerformanceHintSession* session,
                                             const int32_t* threadIds,
                                             size_t size);
using pAPerformanceHint_notifyWorkloadReset =
    int (*)(APerformanceHintSession* session,
            bool cpu,
            bool gpu,
            const char* identifier);
using pAPerformanceHint_notifyWorkloadIncrease =
    int (*)(APerformanceHintSession* session,
            bool cpu,
            bool gpu,
            const char* identifier);
using pAPerformanceHint_setPreferPowerEfficiency =
    int (*)(APerformanceHintSession* session, bool preferPowerEfficiency);
}

namespace viz {
namespace {

class HintSessionFactoryImpl;

#define LOAD_FUNCTION(lib, func)                                \
  do {                                                          \
    func##Fn = reinterpret_cast<p##func>(                       \
        base::GetFunctionPointerFromNativeLibrary(lib, #func)); \
    if (!func##Fn) {                                            \
      supported = false;                                        \
      LOG(ERROR) << "Unable to load function " << #func;        \
    }                                                           \
  } while (0)

bool ShouldUseWorkloadReset() {
  return android_get_device_api_level() > __ANDROID_API_V__ &&
         base::FeatureList::IsEnabled(features::kEnableADPFWorkloadReset);
}

bool ShouldUseWorkloadIncrease() {
  return android_get_device_api_level() > __ANDROID_API_V__ &&
         base::FeatureList::IsEnabled(
             features::kEnableADPFWorkloadIncreaseOnPageLoad);
}

bool CanUsePowerEfficiencyHint() {
  return android_get_device_api_level() >= __ANDROID_API_V__;
}

struct AdpfMethods {
  static const AdpfMethods& Get() {
    static AdpfMethods instance;
    return instance;
  }

  AdpfMethods() {
    base::NativeLibraryLoadError error;
    base::NativeLibrary main_dl_handle =
        base::LoadNativeLibrary(base::FilePath("libandroid.so"), &error);
    if (!main_dl_handle) {
      LOG(ERROR) << "Couldnt load libandroid.so: " << error.ToString();
      supported = false;
      return;
    }

    LOAD_FUNCTION(main_dl_handle, APerformanceHint_getManager);
    LOAD_FUNCTION(main_dl_handle, APerformanceHint_createSession);
    LOAD_FUNCTION(main_dl_handle, APerformanceHint_updateTargetWorkDuration);
    LOAD_FUNCTION(main_dl_handle, APerformanceHint_reportActualWorkDuration);
    LOAD_FUNCTION(main_dl_handle, APerformanceHint_closeSession);
    if (android_get_device_api_level() >= __ANDROID_API_U__) {
      LOAD_FUNCTION(main_dl_handle, APerformanceHint_setThreads);
    }
    if (ShouldUseWorkloadReset()) {
      LOAD_FUNCTION(main_dl_handle, APerformanceHint_notifyWorkloadReset);
    }
    if (ShouldUseWorkloadIncrease()) {
      LOAD_FUNCTION(main_dl_handle, APerformanceHint_notifyWorkloadIncrease);
    }
    if (CanUsePowerEfficiencyHint()) {
      LOAD_FUNCTION(main_dl_handle, APerformanceHint_setPreferPowerEfficiency);
    }
  }

  ~AdpfMethods() = default;

  bool supported = true;
  pAPerformanceHint_getManager APerformanceHint_getManagerFn;
  pAPerformanceHint_createSession APerformanceHint_createSessionFn;
  pAPerformanceHint_updateTargetWorkDuration
      APerformanceHint_updateTargetWorkDurationFn;
  pAPerformanceHint_reportActualWorkDuration
      APerformanceHint_reportActualWorkDurationFn;
  pAPerformanceHint_closeSession APerformanceHint_closeSessionFn;
  pAPerformanceHint_setThreads APerformanceHint_setThreadsFn;
  pAPerformanceHint_notifyWorkloadReset APerformanceHint_notifyWorkloadResetFn;
  pAPerformanceHint_notifyWorkloadIncrease
      APerformanceHint_notifyWorkloadIncreaseFn;
  pAPerformanceHint_setPreferPowerEfficiency
      APerformanceHint_setPreferPowerEfficiencyFn;
};

class AdpfHintSession : public HintSession {
 public:
  AdpfHintSession(APerformanceHintSession* session,
                  HintSessionFactoryImpl* factory,
                  base::TimeDelta target_duration,
                  SessionType type);
  ~AdpfHintSession() override;

  void UpdateTargetDuration(base::TimeDelta target_duration) override;
  void ReportCpuCompletionTime(base::TimeDelta actual_duration,
                               base::TimeTicks draw_start,
                               BoostType preferable_boost_type) override;
  void SetThreads(
      const base::flat_set<base::PlatformThreadId>& thread_ids) override;
  void NotifyWorkloadReset() override;
  void NotifyWorkloadIncrease() override;
  void SetPreferPowerEfficientScheduling(
      bool prefer_efficient_scheduling) final;

  void WakeUp();

 private:
  bool ShouldScheduleForEfficiency() const;
  void UpdateEfficiencyHintIfNeeded(const bool);
  void UpdateLastFrameReportTime();
  void CloseSessionImpl(base::WaitableEvent* session_closed);
  void SetThreadsImpl(std::vector<int> thread_ids);

  const bool rate_limit_boost_;
  const base::TimeDelta rate_limit_boost_min_wait_;

  base::TimeTicks last_frame_report_time_ = base::TimeTicks();
  const raw_ptr<APerformanceHintSession> hint_session_;
  const raw_ptr<HintSessionFactoryImpl> factory_;
  base::TimeDelta target_duration_;
  const SessionType type_;
  BoostManager boost_manager_;
  // Debounces this session's preference for adaptive mode. We should usually be
  // efficient, so most compositor frame sources should be sending efficiency
  // hints most of the time. However, because we can receive updates out of
  // order and at different frame rates, and ADPF is global, some smoothing is
  // required to avoid rapid strobing. Each time an boost is requested, we hold
  // it for a minimum of 4 frames.
  int8_t will_prefer_efficiency_in_frames_ = 0;
  static constexpr int kMinimumFramesForPerformanceBoost = 4;
  // Stores the most recent efficiency preference sent to ADPF.
  bool prefer_efficiency_applied_ = false;

  // Task runner for making heavier calls to the ADPF API off the Viz thread.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

class HintSessionFactoryImpl : public HintSessionFactory {
 public:
  HintSessionFactoryImpl(
      APerformanceHintManager* manager,
      base::flat_set<base::PlatformThreadId> permanent_thread_ids);
  ~HintSessionFactoryImpl() override;

  std::unique_ptr<HintSession> CreateSession(
      base::flat_set<base::PlatformThreadId> transient_thread_ids,
      base::TimeDelta target_duration,
      HintSession::SessionType type) override;
  void WakeUp() override;
  void NotifyWorkloadIncrease() override;
  base::flat_set<base::PlatformThreadId> GetSessionThreadIds(
      base::flat_set<base::PlatformThreadId> transient_thread_ids,
      HintSession::SessionType type) override;

  void SetPreferPowerEfficientScheduling(bool) override;

 private:
  friend class AdpfHintSession;
  friend class HintSessionFactory;

  const raw_ptr<APerformanceHintManager> manager_;
  const base::flat_set<base::PlatformThreadId> permanent_thread_ids_;
  base::flat_set<raw_ptr<AdpfHintSession, CtnExperimental>> hint_sessions_;
  THREAD_CHECKER(thread_checker_);
};

bool IsAsyncSetThreadsEnabled() {
  // Earlier Android versions don't support SetThreads so it's not used at all,
  // sync or async.
  return android_get_device_api_level() >= __ANDROID_API_U__ &&
         base::FeatureList::IsEnabled(features::kEnableADPFAsyncSetThreads);
}

AdpfHintSession::AdpfHintSession(APerformanceHintSession* session,
                                 HintSessionFactoryImpl* factory,
                                 base::TimeDelta target_duration,
                                 SessionType type)
    : rate_limit_boost_(
          base::FeatureList::IsEnabled(features::kEnableADPFBoostRateLimit)),
      rate_limit_boost_min_wait_(features::kAdpfBoostRateLimitMinWait.Get()),
      hint_session_(session),
      factory_(factory),
      target_duration_(target_duration),
      type_(type) {
  DCHECK_CALLED_ON_VALID_THREAD(factory_->thread_checker_);
  factory_->hint_sessions_.insert(this);
  if (IsAsyncSetThreadsEnabled()) {
    // USER_BLOCKING because updating the set of threads in the ADPF session
    // in a timely fashion can affect user-visible behaviors like scroll jank.
    task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_BLOCKING});
  }
}

void AdpfHintSession::CloseSessionImpl(base::WaitableEvent* session_closed) {
  AdpfMethods::Get().APerformanceHint_closeSessionFn(hint_session_);
  session_closed->Signal();
}

AdpfHintSession::~AdpfHintSession() {
  DCHECK_CALLED_ON_VALID_THREAD(factory_->thread_checker_);
  factory_->hint_sessions_.erase(this);
  if (IsAsyncSetThreadsEnabled()) {
    // Run on a sequenced task runner to make sure that we don't close the
    // session before the already posted SetThreads tasks are executed.
    // Otherwise those SetThreads tasks may crash.
    base::WaitableEvent session_closed;
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&AdpfHintSession::CloseSessionImpl,
                                  base::Unretained(this), &session_closed));
    session_closed.Wait();
  } else {
    AdpfMethods::Get().APerformanceHint_closeSessionFn(hint_session_);
  }
}

void AdpfHintSession::UpdateTargetDuration(base::TimeDelta target_duration) {
  DCHECK_CALLED_ON_VALID_THREAD(factory_->thread_checker_);
  if (target_duration_ == target_duration)
    return;
  target_duration_ = target_duration;
  TRACE_EVENT_INSTANT("android.adpf", "UpdateTargetDuration",
                      "target_duration_ms", target_duration.InMillisecondsF(),
                      "type", type_);
  AdpfMethods::Get().APerformanceHint_updateTargetWorkDurationFn(
      hint_session_, target_duration.InNanoseconds());
}

bool AdpfHintSession::ShouldScheduleForEfficiency() const {
  switch (features::kAdpfEfficiencyModeParam.Get()) {
    case features::AdpfEfficiencyMode::kNever:
      [[likely]] return false;
    case features::AdpfEfficiencyMode::kAdaptive: {
      return will_prefer_efficiency_in_frames_ <= 0;
    }
    default:
      return true;
  }
}

void AdpfHintSession::UpdateLastFrameReportTime() {
  last_frame_report_time_ = base::TimeTicks::Now();
}

void AdpfHintSession::UpdateEfficiencyHintIfNeeded(
    const bool prefer_efficient_scheduling) {
  if (prefer_efficiency_applied_ == prefer_efficient_scheduling ||
      !CanUsePowerEfficiencyHint()) [[likely]] {
    return;
  }
  const int result =
      AdpfMethods::Get().APerformanceHint_setPreferPowerEfficiencyFn(
          hint_session_, prefer_efficient_scheduling);
  if (result == 0) [[likely]] {
    prefer_efficiency_applied_ = prefer_efficient_scheduling;
    if (!prefer_efficient_scheduling) {
      TRACE_EVENT_BEGIN("android.adpf", "AdpfHintSession::Boost",
                        perfetto::Track::FromPointer(this));
    } else {
      TRACE_EVENT_END("android.adpf", perfetto::Track::FromPointer(this));
    }
  } else {
    LOG(ERROR) << "setPreferPowerEfficiency (service failure). Returned: "
               << std::strerror(result);
  }
}

void AdpfHintSession::ReportCpuCompletionTime(base::TimeDelta actual_duration,
                                              base::TimeTicks draw_start,
                                              BoostType preferable_boost_type) {
  DCHECK_CALLED_ON_VALID_THREAD(factory_->thread_checker_);

  UpdateEfficiencyHintIfNeeded(ShouldScheduleForEfficiency());
  will_prefer_efficiency_in_frames_ =
      std::max(0, will_prefer_efficiency_in_frames_ - 1);

  // At the moment, we don't have a good way to distinguish repeating animation
  // work from other workloads on CrRendererMain, so we don't report any timing
  // durations.
  if (type_ == SessionType::kRendererMain) {
    return;
  }

  base::TimeDelta frame_duration = boost_manager_.GetFrameDuration(
      target_duration_, actual_duration, draw_start, preferable_boost_type);
  TRACE_EVENT_INSTANT("android.adpf", "ReportCpuCompletionTime",
                      "frame_duration_ms", frame_duration.InMillisecondsF(),
                      "target_duration_ms", target_duration_.InMillisecondsF());
  AdpfMethods::Get().APerformanceHint_reportActualWorkDurationFn(
      hint_session_, frame_duration.InNanoseconds());
  UpdateLastFrameReportTime();
}

void AdpfHintSession::SetPreferPowerEfficientScheduling(
    bool prefer_efficient_scheduling) {
  DCHECK_CALLED_ON_VALID_THREAD(factory_->thread_checker_);
  if (!prefer_efficient_scheduling) [[unlikely]] {
    // prefer_efficient_scheduling is derived from MainThreadSchedulerImpl's
    // code, which typically holds prefer_efficient_scheduling = false for
    // hundreds of milliseconds. We de-bounce this side because we may be
    // receiving different efficiency hints from different
    // MainThreadSchedulerImpl's, running at different frame rates.
    will_prefer_efficiency_in_frames_ = kMinimumFramesForPerformanceBoost;
  }
}

void AdpfHintSession::SetThreadsImpl(std::vector<int> thread_ids) {
  int retval = AdpfMethods::Get().APerformanceHint_setThreadsFn(
      hint_session_, thread_ids.data(), thread_ids.size());
  TRACE_EVENT_INSTANT("android.adpf", "SetThreads", "thread_ids", thread_ids,
                      "success", retval == 0);
}

void AdpfHintSession::SetThreads(
    const base::flat_set<base::PlatformThreadId>& thread_ids) {
  DCHECK_CALLED_ON_VALID_THREAD(factory_->thread_checker_);
  // Passing an empty list of threads to the underlying API can cause a process
  // crash. So we have to return early.
  if (thread_ids.empty()) {
    TRACE_EVENT_INSTANT("android.adpf", "SkipSetThreadsNoThreads", "type",
                        type_);
    return;
  }
  std::vector<int32_t> tids;
  tids.reserve(thread_ids.size());
  std::transform(thread_ids.begin(), thread_ids.end(), std::back_inserter(tids),
                 [](const base::PlatformThreadId& tid) { return tid.raw(); });
  if (IsAsyncSetThreadsEnabled()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&AdpfHintSession::SetThreadsImpl,
                                  base::Unretained(this), std::move(tids)));
  } else {
    SetThreadsImpl(std::move(tids));
  }
}

void AdpfHintSession::NotifyWorkloadReset() {
  DCHECK_CALLED_ON_VALID_THREAD(factory_->thread_checker_);
  int retval = AdpfMethods::Get().APerformanceHint_notifyWorkloadResetFn(
      hint_session_, /*cpu=*/true, /*gpu=*/false, /*identifier=*/"viz-wakeup");
  TRACE_EVENT_INSTANT("android.adpf", "NotifyWorkloadReset", "retval", retval);
}

void AdpfHintSession::NotifyWorkloadIncrease() {
  DCHECK_CALLED_ON_VALID_THREAD(factory_->thread_checker_);
  if (ShouldUseWorkloadIncrease()) {
    int retval = AdpfMethods::Get().APerformanceHint_notifyWorkloadIncreaseFn(
        hint_session_, /*cpu=*/true, /*gpu=*/false,
        /*identifier=*/"page-load");
    TRACE_EVENT_INSTANT("android.adpf", "NotifyWorkloadIncrease", "retval",
                        retval);
  }
}

void AdpfHintSession::WakeUp() {
  DCHECK_CALLED_ON_VALID_THREAD(factory_->thread_checker_);
  if (rate_limit_boost_ &&
      base::TimeTicks::Now() <=
          last_frame_report_time_ + rate_limit_boost_min_wait_) {
    TRACE_EVENT_INSTANT("android.adpf", "Skip WakeUp");
    return;
  }
  if (ShouldUseWorkloadReset()) {
    NotifyWorkloadReset();
  } else {
    ReportCpuCompletionTime(target_duration_, base::TimeTicks::Now(),
                            BoostType::kWakeUpBoost);
  }
}

HintSessionFactoryImpl::HintSessionFactoryImpl(
    APerformanceHintManager* manager,
    base::flat_set<base::PlatformThreadId> permanent_thread_ids)
    : manager_(manager),
      permanent_thread_ids_(std::move(permanent_thread_ids)) {
  // Can be created on any thread.
  DETACH_FROM_THREAD(thread_checker_);
}

HintSessionFactoryImpl::~HintSessionFactoryImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(hint_sessions_.empty());
}

std::unique_ptr<HintSession> HintSessionFactoryImpl::CreateSession(
    base::flat_set<base::PlatformThreadId> transient_thread_ids,
    base::TimeDelta target_duration,
    HintSession::SessionType type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  const auto combined_thread_ids =
      GetSessionThreadIds(transient_thread_ids, type);
  std::vector<int32_t> thread_ids;
  thread_ids.reserve(combined_thread_ids.size());
  std::transform(combined_thread_ids.begin(), combined_thread_ids.end(),
                 std::back_inserter(thread_ids),
                 [](const base::PlatformThreadId& tid) { return tid.raw(); });
  // Passing an empty list of threads to the underlying API can cause a process
  // crash. So we have to return early.
  if (thread_ids.empty()) {
    TRACE_EVENT_INSTANT("android.adpf", "SkipCreateSessionNoThreads", "type",
                        type);
    return nullptr;
  }
  APerformanceHintSession* hint_session =
      AdpfMethods::Get().APerformanceHint_createSessionFn(
          manager_, thread_ids.data(), thread_ids.size(),
          target_duration.InNanoseconds());
  if (!hint_session) {
    TRACE_EVENT_INSTANT("android.adpf", "FailedToCreateSession", "type", type);
    return nullptr;
  }
  TRACE_EVENT_INSTANT("android.adpf", "CreateSession", "thread_ids", thread_ids,
                      "target_duration_ms", target_duration.InMillisecondsF(),
                      "type", type);
  return std::make_unique<AdpfHintSession>(hint_session, this, target_duration,
                                           type);
}

void HintSessionFactoryImpl::SetPreferPowerEfficientScheduling(
    bool prefer_efficient_scheduling) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (auto& session : hint_sessions_) {
    session->SetPreferPowerEfficientScheduling(prefer_efficient_scheduling);
  }
}

void HintSessionFactoryImpl::WakeUp() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (auto& session : hint_sessions_) {
    session->WakeUp();
  }
}

void HintSessionFactoryImpl::NotifyWorkloadIncrease() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (auto& session : hint_sessions_) {
    session->NotifyWorkloadIncrease();
  }
}

base::flat_set<base::PlatformThreadId>
HintSessionFactoryImpl::GetSessionThreadIds(
    base::flat_set<base::PlatformThreadId> transient_thread_ids,
    HintSession::SessionType type) {
  if (type == HintSession::SessionType::kAnimation) {
    transient_thread_ids.insert(permanent_thread_ids_.cbegin(),
                                permanent_thread_ids_.cend());
  }
  return transient_thread_ids;
}

// Returns true if Chrome should use ADPF.
bool IsAdpfEnabled() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableAdpf)) {
    return false;
  }
  if (base::android::android_info::sdk_int() <
      base::android::android_info::SDK_VERSION_S) {
    return false;
  }
  if (!AdpfMethods::Get().supported) {
    return false;
  }
  if (!base::FeatureList::IsEnabled(features::kAdpf)) {
    return false;
  }

  std::string soc_allowlist = features::kADPFSocManufacturerAllowlist.Get();
  std::string soc = base::SysInfo::SocManufacturer();
  return features::ShouldUseAdpfForSoc(soc_allowlist, soc);
}

}  // namespace

// static
std::unique_ptr<HintSessionFactory> HintSessionFactory::Create(
    base::flat_set<base::PlatformThreadId> permanent_thread_ids) {
  if (!IsAdpfEnabled()) {
    return nullptr;
  }

  APerformanceHintManager* manager =
      AdpfMethods::Get().APerformanceHint_getManagerFn();
  if (!manager)
    return nullptr;
  auto factory = std::make_unique<HintSessionFactoryImpl>(
      manager, std::move(permanent_thread_ids));

  // CreateSession is allowed to return null on unsupported device. Detect this
  // at run time to avoid polluting any experiments with unsupported devices.
  {
    auto session = factory->CreateSession({}, base::Milliseconds(10),
                                          HintSession::SessionType::kAnimation);
    if (!session)
      return nullptr;
  }
  DETACH_FROM_THREAD(factory->thread_checker_);
  return factory;
}

}  // namespace viz

#else  // BUILDFLAG(IS_ANDROID)

namespace viz {
std::unique_ptr<HintSessionFactory> HintSessionFactory::Create(
    base::flat_set<base::PlatformThreadId> permanent_thread_ids) {
  return nullptr;
}
}  // namespace viz

#endif  // BUILDFLAG(IS_ANDROID)
