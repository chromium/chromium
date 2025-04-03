// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast_starboard_api_adapter_impl.h"

#include "base/at_exit.h"
#include "base/functional/bind.h"
#include "base/logging.h"

// TODO(b/333961720): remove all the macros in this file and split the impl into
// two different classes: one for SB 15+, one for older versions of starboard.
// Only include the relevant one in the BUILD.gn file.
#if SB_API_VERSION >= 15
#include <starboard/event.h>
#else  // SB_API_VERSION >=15
#include <cast_starboard_api.h>
#endif  // SB_API_VERSION >= 15

namespace chromecast {

namespace {

CastStarboardApiAdapterImpl* g_instance = nullptr;
std::mutex g_instance_mutex;

void DeleteInstance() {
  std::unique_lock<std::mutex> lock(g_instance_mutex);
  if (g_instance) {
    delete g_instance;
  }
}

}  // namespace

CastStarboardApiAdapter* CastStarboardApiAdapter::GetInstance() {
  // Perform a lazy check, if this is already initialized we can skip checking
  // inits.
  if (!g_instance) {
    // If we do not have an instance, we must re-validate once we have acquired
    // the mutex that it was not already constructed.
    std::unique_lock<std::mutex> lock(g_instance_mutex);
    // The instance is assigned by the class's constructor.
    if (!g_instance) {
      new CastStarboardApiAdapterImpl();

      // Since OzonePlatform* is always orphaned, a handle to CastStarboardApi
      // is still held by CastEglPlatformStarboard rather than being cleaned up
      // by the last Unsubscribe. Artificially clean it up at process exit.
      base::AtExitManager::RegisterTask(base::BindOnce(DeleteInstance));
    }
  }

  return g_instance;
}

CastStarboardApiAdapterImpl::CastStarboardApiAdapterImpl()
    : initialized_(false) {
  CHECK(!g_instance);
  g_instance = this;
}

CastStarboardApiAdapterImpl::~CastStarboardApiAdapterImpl() {
  if (initialized_) {
    Release();
  }

  g_instance = nullptr;
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
      init_p_.set_value(true);
      break;
    case kSbEventTypeStop:
      init_p_.set_value(false);
      break;
    default:
      for (const auto p : subscribers_) {
        if (p.second) {
          p.second(p.first, event);
        }
      }
      break;
  }
}

void CastStarboardApiAdapterImpl::Initialize() {
  init_p_ = {};

#if SB_API_VERSION >= 15
  sb_main_ = std::make_unique<std::thread>(
      &SbRunStarboardMain, /*argc=*/0, /*argv=*/nullptr,
      &CastStarboardApiAdapterImpl::SbEventHandle);
  sb_main_->detach();
#else   // SB_API_VERSION >=15
  CastStarboardApiInitialize(/*argc=*/0, /*argv=*/nullptr,
                             &CastStarboardApiAdapterImpl::SbEventHandle);
#endif  // SB_API_VERSION >= 15
  init_f_ = init_p_.get_future();
  initialized_ = init_f_.get();
}

void CastStarboardApiAdapterImpl::Release() {
  {
    std::lock_guard<decltype(lock_)> lock(lock_);
    subscribers_.clear();
  }

  init_p_ = {};
#if SB_API_VERSION >= 15
  SbSystemRequestStop(0);
#else   // SB_API_VERSION >=15
  CastStarboardApiFinalize();
#endif  // SB_API_VERSION >= 15
  init_f_ = init_p_.get_future();
  initialized_ = init_f_.get();
}

void CastStarboardApiAdapterImpl::Subscribe(void* context,
                                            CastStarboardApiAdapterImplCB cb) {
  std::lock_guard<decltype(lock_)> lock(lock_);
  if (!initialized_) {
    Initialize();
  }
  subscribers_.insert({context, cb});
}

void CastStarboardApiAdapterImpl::Unsubscribe(void* context) {
  {
    std::lock_guard<decltype(lock_)> lock(lock_);
    subscribers_.erase(context);
  }
  // Defer Release() until the destructor is called.
  // This helps simplify the complexity around Unsubscribe and AtExit calls
  // calling at the same time.
}

SbWindow CastStarboardApiAdapterImpl::GetWindow(
    const SbWindowOptions* options) {
  if (!SbWindowIsValid(window_)) {
    window_ = SbWindowCreate(options);
  }

  return window_;
}

}  // namespace chromecast
