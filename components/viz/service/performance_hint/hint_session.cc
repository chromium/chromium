// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/performance_hint/hint_session.h"

#include <utility>
#include <vector>

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)

#include <dlfcn.h>
#include <sys/types.h>

#include "base/android/build_info.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/trace_event/trace_event.h"

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
using pAPerformanceHint_reportActualWorkDuration =
    int (*)(APerformanceHintSession* session, int64_t actualDurationNanos);
using pAPerformanceHint_closeSession =
    void (*)(APerformanceHintSession* session);
}

namespace viz {
namespace {

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
    LOAD_FUNCTION(main_dl_handle, APerformanceHint_reportActualWorkDuration);
    LOAD_FUNCTION(main_dl_handle, APerformanceHint_closeSession);
  }

  ~AdpfMethods() = default;

  bool supported = true;
  pAPerformanceHint_getManager APerformanceHint_getManagerFn;
  pAPerformanceHint_createSession APerformanceHint_createSessionFn;
  pAPerformanceHint_reportActualWorkDuration
      APerformanceHint_reportActualWorkDurationFn;
  pAPerformanceHint_closeSession APerformanceHint_closeSessionFn;
};

class AdpfHintSession : public HintSession {
 public:
  explicit AdpfHintSession(APerformanceHintSession* session)
      : hint_session_(session) {}

  ~AdpfHintSession() override {
    AdpfMethods::Get().APerformanceHint_closeSessionFn(hint_session_);
  }

  void ReportCpuCompletionTime(base::TimeDelta actual_duration) override {
    AdpfMethods::Get().APerformanceHint_reportActualWorkDurationFn(
        hint_session_, actual_duration.InNanoseconds());
  }

 private:
  APerformanceHintSession* const hint_session_ = nullptr;
};

class HintSessionFactoryImpl : public HintSessionFactory {
 public:
  HintSessionFactoryImpl(
      APerformanceHintManager* manager,
      base::flat_set<base::PlatformThreadId> permanent_thread_ids)
      : manager_(manager),
        permanent_thread_ids_(std::move(permanent_thread_ids)) {}

  std::unique_ptr<HintSession> CreateSession(
      base::flat_set<base::PlatformThreadId> transient_thread_ids,
      base::TimeDelta target_duration) override;

 private:
  APerformanceHintManager* const manager_;
  const base::flat_set<base::PlatformThreadId> permanent_thread_ids_;
};

std::unique_ptr<HintSession> HintSessionFactoryImpl::CreateSession(
    base::flat_set<base::PlatformThreadId> transient_thread_ids,
    base::TimeDelta target_duration) {
  transient_thread_ids.insert(permanent_thread_ids_.cbegin(),
                              permanent_thread_ids_.cend());
  std::vector<int32_t> thread_ids(transient_thread_ids.begin(),
                                  transient_thread_ids.end());
  APerformanceHintSession* hint_session =
      AdpfMethods::Get().APerformanceHint_createSessionFn(
          manager_, thread_ids.data(), thread_ids.size(),
          target_duration.InNanoseconds());
  if (!hint_session)
    return nullptr;
  return std::make_unique<AdpfHintSession>(hint_session);
}

}  // namespace

// static
std::unique_ptr<HintSessionFactory> HintSessionFactory::Create(
    base::flat_set<base::PlatformThreadId> permanent_thread_ids) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_S)
    return nullptr;
  if (!AdpfMethods::Get().supported)
    return nullptr;

  APerformanceHintManager* manager =
      AdpfMethods::Get().APerformanceHint_getManagerFn();
  if (!manager)
    return nullptr;
  auto factory = std::make_unique<HintSessionFactoryImpl>(
      manager, std::move(permanent_thread_ids));

  // CreateSession is allowed to return null on unsupported device. Detect this
  // at run time to avoid polluting any experiments with unsupported devices.
  auto session = factory->CreateSession({}, base::Milliseconds(10));
  if (!session)
    return nullptr;
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
