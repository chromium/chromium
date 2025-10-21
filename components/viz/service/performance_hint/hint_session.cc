// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/performance_hint/hint_session.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
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
  void WakeUp();

 private:
  const raw_ptr<APerformanceHintSession> hint_session_;
  const raw_ptr<HintSessionFactoryImpl> factory_;
  base::TimeDelta target_duration_;
  const SessionType type_;
  BoostManager boost_manager_;
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

 private:
  friend class AdpfHintSession;
  friend class HintSessionFactory;

  const raw_ptr<APerformanceHintManager> manager_;
  const base::flat_set<base::PlatformThreadId> permanent_thread_ids_;
  base::flat_set<raw_ptr<AdpfHintSession, CtnExperimental>> hint_sessions_;
  THREAD_CHECKER(thread_checker_);
};

AdpfHintSession::AdpfHintSession(APerformanceHintSession* session,
                                 HintSessionFactoryImpl* factory,
                                 base::TimeDelta target_duration,
                                 SessionType type)
    : hint_session_(session),
      factory_(factory),
      target_duration_(target_duration),
      type_(type) {
  DCHECK_CALLED_ON_VALID_THREAD(factory_->thread_checker_);
  factory_->hint_sessions_.insert(this);
}

AdpfHintSession::~AdpfHintSession() {
  DCHECK_CALLED_ON_VALID_THREAD(factory_->thread_checker_);
  factory_->hint_sessions_.erase(this);
  AdpfMethods::Get().APerformanceHint_closeSessionFn(hint_session_);
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

void AdpfHintSession::ReportCpuCompletionTime(base::TimeDelta actual_duration,
                                              base::TimeTicks draw_start,
                                              BoostType preferable_boost_type) {
  DCHECK_CALLED_ON_VALID_THREAD(factory_->thread_checker_);
  // At the moment, we don't have a good way to distinguish repeating animation
  // work from other workloads on CrRendererMain, so we don't report any timing
  // durations.
  if (type_ == SessionType::kRendererMain) {
    return;
  }

  base::TimeDelta frame_duration =
      boost_manager_.GetFrameDurationAndMaybeUpdateBoostType(
          target_duration_, actual_duration, draw_start, preferable_boost_type);
  TRACE_EVENT_INSTANT("android.adpf", "ReportCpuCompletionTime",
                      "frame_duration_ms", frame_duration.InMillisecondsF(),
                      "target_duration_ms", target_duration_.InMillisecondsF());
  AdpfMethods::Get().APerformanceHint_reportActualWorkDurationFn(
      hint_session_, frame_duration.InNanoseconds());
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
  int retval = AdpfMethods::Get().APerformanceHint_setThreadsFn(
      hint_session_, tids.data(), tids.size());
  TRACE_EVENT_INSTANT("android.adpf", "SetThreads", "thread_ids", thread_ids,
                      "type", type_, "success", retval == 0);
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
  std::string soc_blocklist = features::kADPFSocManufacturerBlocklist.Get();
  std::string soc = base::SysInfo::SocManufacturer();
  return features::ShouldUseAdpfForSoc(soc_allowlist, soc_blocklist, soc);
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
