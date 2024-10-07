// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/performance_hint/hint_session.h"

#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_split.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)

#include <dlfcn.h>
#include <sys/types.h>

#include "base/android/build_info.h"
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
};

class AdpfHintSession : public HintSession {
 public:
  AdpfHintSession(APerformanceHintSession* session,
                  HintSessionFactoryImpl* factory,
                  base::TimeDelta target_duration);
  ~AdpfHintSession() override;

  void UpdateTargetDuration(base::TimeDelta target_duration) override;
  void ReportCpuCompletionTime(base::TimeDelta actual_duration,
                               base::TimeTicks draw_start,
                               BoostType preferable_boost_type) override;

  void WakeUp();

 private:
  const raw_ptr<APerformanceHintSession> hint_session_;
  const raw_ptr<HintSessionFactoryImpl> factory_;
  base::TimeDelta target_duration_;
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
      base::TimeDelta target_duration) override;
  void WakeUp() override;

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
                                 base::TimeDelta target_duration)
    : hint_session_(session),
      factory_(factory),
      target_duration_(target_duration) {
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
                      "target_duration_ms", target_duration.InMillisecondsF());
  AdpfMethods::Get().APerformanceHint_updateTargetWorkDurationFn(
      hint_session_, target_duration.InNanoseconds());
}

void AdpfHintSession::ReportCpuCompletionTime(base::TimeDelta actual_duration,
                                              base::TimeTicks draw_start,
                                              BoostType preferable_boost_type) {
  DCHECK_CALLED_ON_VALID_THREAD(factory_->thread_checker_);

  base::TimeDelta frame_duration =
      boost_manager_.GetFrameDurationAndMaybeUpdateBoostType(
          target_duration_, actual_duration, draw_start, preferable_boost_type);
  TRACE_EVENT_INSTANT("android.adpf", "ReportCpuCompletionTime",
                      "frame_duration_ms", frame_duration.InMillisecondsF(),
                      "target_duration_ms", target_duration_.InMillisecondsF());
  AdpfMethods::Get().APerformanceHint_reportActualWorkDurationFn(
      hint_session_, frame_duration.InNanoseconds());
}

void AdpfHintSession::WakeUp() {
  DCHECK_CALLED_ON_VALID_THREAD(factory_->thread_checker_);
  ReportCpuCompletionTime(target_duration_, base::TimeTicks::Now(),
                          BoostType::kWakeUpBoost);
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
    base::TimeDelta target_duration) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  transient_thread_ids.insert(permanent_thread_ids_.cbegin(),
                              permanent_thread_ids_.cend());
  std::vector<int32_t> thread_ids(transient_thread_ids.begin(),
                                  transient_thread_ids.end());
  APerformanceHintSession* hint_session =
      AdpfMethods::Get().APerformanceHint_createSessionFn(
          manager_, thread_ids.data(), thread_ids.size(),
          target_duration.InNanoseconds());
  if (!hint_session) {
    TRACE_EVENT_INSTANT("android.adpf", "FailedToCreateSession");
    return nullptr;
  }
  TRACE_EVENT_INSTANT("android.adpf", "CreateSession", "thread_ids", thread_ids,
                      "target_duration_ms", target_duration.InMillisecondsF());
  return std::make_unique<AdpfHintSession>(hint_session, this, target_duration);
}

void HintSessionFactoryImpl::WakeUp() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (hint_sessions_.empty())
    return;
  (*hint_sessions_.begin())->WakeUp();
}

// Returns true if Chrome should use ADPF.
bool IsAdpfEnabled() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableAdpf)) {
    return false;
  }
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_S) {
    return false;
  }
  if (!AdpfMethods::Get().supported) {
    return false;
  }
  if (!base::FeatureList::IsEnabled(features::kAdpf)) {
    return false;
  }

  std::string allowlist_param = features::kADPFSocManufacturerAllowlist.Get();
  std::vector<std::string_view> allowlist = base::SplitStringPiece(
      allowlist_param, "|", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::string blocklist_param = features::kADPFSocManufacturerBlocklist.Get();
  std::vector<std::string_view> blocklist = base::SplitStringPiece(
      blocklist_param, "|", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::string soc = base::SysInfo::SocManufacturer();
  // If there's no allowlist, soc must be absent from the blocklist.
  if (allowlist.empty()) {
    return !base::Contains(blocklist, soc);
  }
  // If there's an allowlist, soc must be in the allowlist.
  // Blocklist is ignored in this case.
  return base::Contains(allowlist, soc);
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
    auto session = factory->CreateSession({}, base::Milliseconds(10));
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
