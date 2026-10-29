// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/chromecast/starboard_adapter/src/cast_starboard_api_adapter_impl.h"

#include <dlfcn.h>
#include <starboard/event.h>
#include <starboard/system.h>

#include <thread>

#include "base/compiler_specific.h"
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
    new CastStarboardApiAdapterImpl();
  }

  return g_instance;
}

// Overridden by the strong proxy definition in cast_starboard_api_adapter_proxy
// when starboard_adapter_static is linked, and used directly as a fallback by
// unit tests that link only starboard_adapter_impl.
WEAK_SYMBOL CastStarboardApiAdapter* CastStarboardApiAdapter::GetInstance() {
  return CastStarboardApiAdapterImpl::GetInstance();
}

CastStarboardApiAdapterImpl::CastStarboardApiAdapterImpl()
    : starboard_started_(base::WaitableEvent::ResetPolicy::MANUAL,
                         base::WaitableEvent::InitialState::NOT_SIGNALED),
      starboard_stopped_(base::WaitableEvent::ResetPolicy::MANUAL,
                         base::WaitableEvent::InitialState::NOT_SIGNALED) {
  CHECK(!g_instance);
  g_instance = this;
  LOG(INFO) << "Initialized CastStarboardApiAdapterImpl";
}

CastStarboardApiAdapterImpl::~CastStarboardApiAdapterImpl() = default;

void CastStarboardApiAdapterImpl::SetSbApiVersion(int version) {
  sb_api_version_.store(version, std::memory_order_relaxed);
}

int CastStarboardApiAdapterImpl::sb_api_version() const {
  return sb_api_version_.load(std::memory_order_relaxed);
}

void CastStarboardApiAdapterImpl::ResetForTesting() {
  base::AutoLock autolock(lock_);
  subscribers_.clear();
  window_ = kSbWindowInvalid;
  initialized_ = false;
  released_ = false;
  at_exit_ran_ = false;
  starboard_started_.Reset();
  starboard_stopped_.Reset();
}

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
      const SbEvent* event_to_dispatch = event;
      // Starboard 17+ removed kSbEventTypeUser (10),
      // kSbEventTypeVerticalSync (12),
      // kSbEventTypeAccessibilitySettingsChanged (14),
      // kSbEventTypeOnScreenKeyboard* (17..21), and
      // kSbEventTypeAccessibilityCaptionSettingsChanged (22), and moved
      // kSbEventTypeAccessibilityTextToSpeechSettingsChanged after
      // kSbEventDateTimeConfigurationChanged. Subscribers in core_runtime are
      // compiled once against the base Starboard headers (SB15/16), so map
      // SB17+ ordinals 10..17 back to their base SB15/16 enum values before
      // dispatching.
      SbEvent normalized_event;
      const int raw_type = static_cast<int>(event->type);
      if (sb_api_version_.load(std::memory_order_relaxed) >= 17 &&
          raw_type >= 10) {
        static constexpr auto kSb17EventTypeMap = std::to_array<SbEventType>({
            kSbEventTypeLink,                                      // 10 -> 11
            kSbEventTypeScheduled,                                 // 11 -> 13
            kSbEventTypeLowMemory,                                 // 12 -> 15
            kSbEventTypeWindowSizeChanged,                         // 13 -> 16
            kSbEventTypeOsNetworkDisconnected,                     // 14 -> 24
            kSbEventTypeOsNetworkConnected,                        // 15 -> 25
            kSbEventDateTimeConfigurationChanged,                  // 16 -> 26
            kSbEventTypeAccessibilityTextToSpeechSettingsChanged,  // 17 -> 23
        });
        const size_t idx = static_cast<size_t>(raw_type - 10);
        if (idx < kSb17EventTypeMap.size()) {
          normalized_event = *event;
          normalized_event.type = kSb17EventTypeMap[idx];
          event_to_dispatch = &normalized_event;
        }
      }
      base::AutoLock autolock(lock_);
      for (const auto p : subscribers_) {
        if (p.second) {
          p.second(p.first, event_to_dispatch);
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

  const int version = sb_api_version_.load(std::memory_order_relaxed);
  LOG(INFO) << "Starting starboard (SB_API_VERSION " << version << ")";
  CHROMECAST_TIMING_TRACKER;

  if (version >= 15) {
    using SbRunStarboardMainFn = int (*)(int, char**, void (*)(const SbEvent*));
    auto run_main = reinterpret_cast<SbRunStarboardMainFn>(
        dlsym(RTLD_DEFAULT, "SbRunStarboardMain"));
    if (run_main) {
      LOG(INFO) << "Initializing Starboard via SbRunStarboardMain (SB"
                << version << ").";
      // SbRunStarboardMain only returns once Starboard has stopped, so the
      // thread is detached and never joined.
      std::thread(run_main, /*argc=*/0, /*argv=*/nullptr,
                  &CastStarboardApiAdapterImpl::SbEventHandle)
          .detach();
    } else {
      LOG(FATAL) << "Failed to find SbRunStarboardMain for Starboard "
                 << version;
    }
  } else {
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
  }

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
      const int version = sb_api_version_.load(std::memory_order_relaxed);
      LOG(INFO) << "Stopping Starboard (SB_API_VERSION " << version << ")";
      if (version >= 15) {
        using SbSystemRequestStopFn = void (*)(int);
        auto stop_fn = reinterpret_cast<SbSystemRequestStopFn>(
            dlsym(RTLD_DEFAULT, "SbSystemRequestStop"));
        if (stop_fn) {
          stop_fn(0);
        } else {
          LOG(ERROR) << "Failed to find SbSystemRequestStop for Starboard "
                     << version;
        }
      } else {
        using CastStarboardApiFinalizeFn = void (*)();
        auto finalize_fn = reinterpret_cast<CastStarboardApiFinalizeFn>(
            dlsym(RTLD_DEFAULT, "CastStarboardApiFinalize"));
        if (finalize_fn) {
          finalize_fn();
        } else {
          LOG(ERROR)
              << "Failed to find CastStarboardApiFinalize for Legacy Starboard";
        }
      }
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
    using SbWindowCreateFn = SbWindow (*)(const SbWindowOptions*);
    auto create_fn = reinterpret_cast<SbWindowCreateFn>(
        dlsym(RTLD_DEFAULT, "SbWindowCreate"));
    if (create_fn) {
      window_ = create_fn(options);
    }
  }

  return window_;
}

bool CastStarboardApiAdapterImpl::SbWindowIsValid(SbWindow window) {
  return ::SbWindowIsValid(window);
}

void* CastStarboardApiAdapterImpl::SbWindowGetPlatformHandle(SbWindow window) {
  using SbWindowGetPlatformHandleFn = void* (*)(SbWindow);
  auto get_handle_fn = reinterpret_cast<SbWindowGetPlatformHandleFn>(
      dlsym(RTLD_DEFAULT, "SbWindowGetPlatformHandle"));
  return get_handle_fn ? get_handle_fn(window) : nullptr;
}

const void* CastStarboardApiAdapterImpl::GetExtension(const char* name) {
  EnsureInitialized();
  using SbSystemGetExtensionFn = const void* (*)(const char*);
  auto get_extension_fn = reinterpret_cast<SbSystemGetExtensionFn>(
      dlsym(RTLD_DEFAULT, "SbSystemGetExtension"));
  if (get_extension_fn) {
    return get_extension_fn(name);
  }
  return nullptr;
}

}  // namespace chromecast
