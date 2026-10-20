// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/chromecast/starboard_adapter/src/cast_starboard_api_adapter_impl.h"

#include <dlfcn.h>
#include <starboard/event.h>
#include <starboard/system.h>

#include <thread>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/threading/thread_restrictions.h"
#include "chromecast/common/timing_tracker.h"

namespace chromecast {

namespace {

CastStarboardApiAdapterImpl* g_instance = nullptr;

base::Lock& GetGInstanceLock() {
  static base::NoDestructor<base::Lock> lock;
  return *lock;
}

}  // namespace

// static
CastStarboardApiAdapterImpl* CastStarboardApiAdapterImpl::GetInstance() {
  base::AutoLock lock(GetGInstanceLock());
  if (!g_instance) {
    // The instance is assigned by the class's constructor and kept alive for
    // the lifetime of the process so that late teardown callers (e.g.,
    // media_thread_ destructing after AtExitManager) never dereference a
    // dangling pointer.
    //
    // No base::AtExitManager task is registered here. This code is linked into
    // libcast_starboard_adapter.so.$version, which carries its own copy of
    // //base whose AtExitManager is never instantiated, so registering from
    // here would hit NOTREACHED. Teardown is instead registered by
    // StarboardAdapterLoader in the executable, which calls the exported
    // ReleaseCastStarboardApiAdapter() -> ReleaseForAtExit().
    new CastStarboardApiAdapterImpl();
  }

  return g_instance;
}

CastStarboardApiAdapter* CastStarboardApiAdapter::GetInstance() {
  return CastStarboardApiAdapterImpl::GetInstance();
}

CastStarboardApiAdapterImpl::CastStarboardApiAdapterImpl()
    : starboard_started_(base::WaitableEvent::ResetPolicy::MANUAL,
                         base::WaitableEvent::InitialState::NOT_SIGNALED),
      starboard_stopped_(base::WaitableEvent::ResetPolicy::MANUAL,
                         base::WaitableEvent::InitialState::NOT_SIGNALED) {
  CHECK(!g_instance);
  g_instance = this;
  LOG(INFO) << "Initialized CastStarboardApiAdapterImpl for SB_API_VERSION "
            << SB_API_VERSION;
}

CastStarboardApiAdapterImpl::~CastStarboardApiAdapterImpl() = default;

SbEglNativeDisplayType CastStarboardApiAdapterImpl::GetEglNativeDisplayType() {
  return SB_EGL_DEFAULT_DISPLAY;
}

// static
void CastStarboardApiAdapterImpl::SbEventHandle(const SbEvent* event) {
  g_instance->SbEventHandleInternal(event);
}

void CastStarboardApiAdapterImpl::SbEventHandleInternal(const SbEvent* event) {
  // If multiple instances of Starboard become supported, |event->window| may
  // need to be checked here for some types before propagating.
  switch (event->type) {
    case kSbEventTypeStart:
      LOG(INFO) << "Received kSbEventTypeStart event";
      starboard_started_.Signal();
      break;
    case kSbEventTypeStop:
      LOG(INFO) << "Received kSbEventTypeStop event";
      starboard_stopped_.Signal();
      break;
    default: {
      base::AutoLock autolock(lock_);
      for (const auto p : subscribers_) {
        if (p.second) {
          p.second(p.first, event);
        }
      }
      break;
    }
  }
}

void CastStarboardApiAdapterImpl::EnsureInitialized() {
  bool already_initialized = false;
  {
    base::AutoLock autolock(lock_);
    if (initialized_) {
      already_initialized = true;
    } else {
      CHECK(!at_exit_ran_)
          << "Starboard should not be re-initialized after the AtExitManager "
             "has run.";
      initialized_ = true;
    }
  }

  if (already_initialized) {
    // If starboard was started earlier, this will return immediately. If
    // another thread started starboard but starboard has not yet sent the
    // kSbEventTypeStart event, we still need to wait for it outside lock_.
    starboard_started_.Wait();
    return;
  }

  LOG(INFO) << "Starting starboard";
  CHROMECAST_TIMING_TRACKER;

#if SB_API_VERSION >= 15
  using SbRunStarboardMainFn = int (*)(int, char**, void (*)(const SbEvent*));
  auto run_main = reinterpret_cast<SbRunStarboardMainFn>(
      dlsym(RTLD_DEFAULT, "SbRunStarboardMain"));
  if (run_main) {
    LOG(INFO) << "Initializing Starboard via SbRunStarboardMain (SB15+).";
    // SbRunStarboardMain only returns once Starboard has stopped, so the
    // thread is detached and never joined.
    std::thread(run_main, /*argc=*/0, /*argv=*/nullptr,
                &CastStarboardApiAdapterImpl::SbEventHandle)
        .detach();
  } else {
    LOG(FATAL) << "Failed to find SbRunStarboardMain for Starboard 15+";
  }
#else
  using CastStarboardApiInitializeFn =
      int (*)(int, char**, void (*)(const SbEvent*));
  auto init_fn = reinterpret_cast<CastStarboardApiInitializeFn>(
      dlsym(RTLD_DEFAULT, "CastStarboardApiInitialize"));
  if (init_fn) {
    LOG(INFO)
        << "Initializing Starboard via CastStarboardApiInitialize (Legacy).";
    init_fn(/*argc=*/0, /*argv=*/nullptr,
            &CastStarboardApiAdapterImpl::SbEventHandle);
  } else {
    LOG(FATAL)
        << "Failed to find CastStarboardApiInitialize for Legacy Starboard";
  }
#endif

  starboard_started_.Wait();
}

void CastStarboardApiAdapterImpl::Release() {
  CHROMECAST_TIMING_TRACKER;
  LOG(INFO) << "CastStarboardApiAdapterImpl::Release";
  bool need_to_stop_starboard = false;
  {
    base::AutoLock autolock(lock_);
    if (released_ && subscribers_.empty() && !initialized_) {
      return;
    }
    released_ = true;

    if (!subscribers_.empty()) {
      LOG(WARNING) << "Not stopping Starboard yet, because there are still "
                   << subscribers_.size() << " subscribers.";
      return;
    }

    if (initialized_) {
      need_to_stop_starboard = true;
    }
    initialized_ = false;
    window_ = kSbWindowInvalid;
  }

  if (need_to_stop_starboard) {
    if (!starboard_stopped_.IsSignaled()) {
      LOG(INFO) << "Stopping Starboard";
#if SB_API_VERSION >= 15
      using SbSystemRequestStopFn = void (*)(int);
      auto stop_fn = reinterpret_cast<SbSystemRequestStopFn>(
          dlsym(RTLD_DEFAULT, "SbSystemRequestStop"));
      if (stop_fn) {
        stop_fn(0);
      } else {
        LOG(ERROR) << "Failed to find SbSystemRequestStop for Starboard 15+";
      }
#else
      using CastStarboardApiFinalizeFn = void (*)();
      auto finalize_fn = reinterpret_cast<CastStarboardApiFinalizeFn>(
          dlsym(RTLD_DEFAULT, "CastStarboardApiFinalize"));
      if (finalize_fn) {
        finalize_fn();
      } else {
        LOG(ERROR)
            << "Failed to find CastStarboardApiFinalize for Legacy Starboard";
      }
#endif
    }
    LOG(INFO) << "Waiting for starboard to stop.";
    starboard_stopped_.Wait();
    LOG(INFO) << "Done waiting for starboard to stop.";
  } else {
    LOG(WARNING)
        << "CastStarboardApiAdapterImpl was not initialized before being "
           "released.";
  }

  // INVARIANT: starboard_started_ and starboard_stopped_ are never Reset.
  //
  // Resetting them here would race EnsureInitialized() callers that have
  // already left the critical section but not yet reached
  // starboard_started_.Wait(), wedging them forever because Starboard emits
  // kSbEventTypeStart exactly once.
  //
  // Leaving them signaled is only safe because Starboard is never restarted:
  // the sole production caller of Release() is ReleaseForAtExit(), which
  // latches at_exit_ran_, and EnsureInitialized() CHECKs on it. Release() is
  // deliberately absent from the public CastStarboardApiAdapter interface to
  // keep that true.
  //
  // If you add a Release() caller that does not set at_exit_ran_, you break
  // this: a later EnsureInitialized() would return on generation N's stale
  // start signal before generation N+1 is up, and a later Release() would skip
  // stop_fn() because starboard_stopped_ is still signaled. Reset both events
  // under lock_ on the need_to_start_starboard path instead.
}

void CastStarboardApiAdapterImpl::ReleaseForAtExit() {
  {
    base::AutoLock autolock(lock_);
    at_exit_ran_ = true;
  }
  Release();
}

void CastStarboardApiAdapterImpl::Subscribe(void* context,
                                            CastStarboardApiAdapterImplCB cb) {
  CHROMECAST_TIMING_TRACKER;
  LOG(INFO) << "CastStarboardApiAdapterImpl::Subscribe, context=" << context;
  EnsureInitialized();

  base::AutoLock autolock(lock_);
  subscribers_.insert({context, cb});
}

void CastStarboardApiAdapterImpl::Unsubscribe(void* context) {
  CHROMECAST_TIMING_TRACKER;
  LOG(INFO) << "CastStarboardApiAdapterImpl::Unsubscribe, context=" << context;

  bool do_release = false;
  {
    base::AutoLock autolock(lock_);
    subscribers_.erase(context);
    if (released_ && subscribers_.empty()) {
      do_release = true;
    }

    LOG(INFO) << "After Unsubscribe, there are " << subscribers_.size()
              << " remaining subscribers";
  }

  // Release() must be called while lock_ is not held.
  if (do_release) {
    LOG(INFO) << "The last subscriber unsubscribed after Release(). Retrying "
                 "Release().";
    Release();
  }
}

SbWindow CastStarboardApiAdapterImpl::GetWindow(
    const SbWindowOptions* options) {
  EnsureInitialized();

  base::AutoLock autolock(lock_);
  if (!::SbWindowIsValid(window_)) {
    window_ = ::SbWindowCreate(options);
  }

  return window_;
}

bool CastStarboardApiAdapterImpl::SbWindowIsValid(SbWindow window) {
  return ::SbWindowIsValid(window);
}

void* CastStarboardApiAdapterImpl::SbWindowGetPlatformHandle(SbWindow window) {
  return ::SbWindowGetPlatformHandle(window);
}

const void* CastStarboardApiAdapterImpl::GetExtension(const char* name) {
  EnsureInitialized();
  using SbSystemGetExtensionFn = const void* (*)(const char*);
  static auto get_extension_fn = reinterpret_cast<SbSystemGetExtensionFn>(
      dlsym(RTLD_DEFAULT, "SbSystemGetExtension"));
  if (get_extension_fn) {
    return get_extension_fn(name);
  }
  return nullptr;
}

}  // namespace chromecast
