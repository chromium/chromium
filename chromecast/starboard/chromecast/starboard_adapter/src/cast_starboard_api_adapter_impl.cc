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

}  // namespace

CastStarboardApiAdapter* CastStarboardApiAdapter::GetInstance() {
  std::unique_lock<std::mutex> lock(g_instance_mutex);
  if (!g_instance) {
    // The instance is assigned by the class's constructor.
    new CastStarboardApiAdapterImpl();

    // We use an AtExitManager as a signal that the runtime is shutting down.
    // Note that this callback is not guaranteed to run after all other cast
    // code has been shut down. For example, the media_thread_ in
    // CastContentBrowserClient is sometimes destructed after AtExitManager runs
    // its callbacks (meaning StarboardApiWrapperBase or other media code may
    // still be subscribed).
    //
    // base::Unretained is safe because the AtExitManager is responsible for
    // deleting g_instance.
    base::AtExitManager::RegisterTask(base::BindOnce(
        &CastStarboardApiAdapterImpl::Release, base::Unretained(g_instance)));
  }

  return g_instance;
}

CastStarboardApiAdapterImpl::CastStarboardApiAdapterImpl()
    : initialized_(false) {
  CHECK(!g_instance);
  g_instance = this;
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
      init_p_.set_value(true);
      break;
    case kSbEventTypeStop:
      LOG(INFO) << "Received kSbEventTypeStop event";
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
  LOG(INFO) << "CastStarboardApiAdapterImpl::Initialize";
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
  LOG(INFO) << "CastStarboardApiAdapterImpl::Release";
  {
    std::lock_guard<decltype(lock_)> lock(lock_);
    released_ = true;
    if (!subscribers_.empty()) {
      LOG(WARNING) << "Not stopping Starboard yet, because there are still "
                   << subscribers_.size() << " subscribers.";
      return;
    }
  }

  LOG(INFO) << "Stopping Starboard";
  init_p_ = {};
#if SB_API_VERSION >= 15
  SbSystemRequestStop(0);
#else   // SB_API_VERSION >=15
  CastStarboardApiFinalize();
#endif  // SB_API_VERSION >= 15
  init_f_ = init_p_.get_future();
  initialized_ = init_f_.get();

  std::unique_lock<std::mutex> lock(g_instance_mutex);
  delete g_instance;
  g_instance = nullptr;
}

void CastStarboardApiAdapterImpl::Subscribe(void* context,
                                            CastStarboardApiAdapterImplCB cb) {
  LOG(INFO) << "CastStarboardApiAdapterImpl::Subscribe, context=" << context;

  std::lock_guard<decltype(lock_)> lock(lock_);
  CHECK(!released_)
      << "Subscribe should not be called after the AtExitManager has run";

  if (!initialized_) {
    Initialize();
  }
  subscribers_.insert({context, cb});
}

void CastStarboardApiAdapterImpl::Unsubscribe(void* context) {
  LOG(INFO) << "CastStarboardApiAdapterImpl::Unsubscribe, context=" << context;

  bool do_release = false;
  {
    std::lock_guard<decltype(lock_)> lock(lock_);
    subscribers_.erase(context);
    if (released_ && subscribers_.empty()) {
      do_release = true;
    }
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
  if (!SbWindowIsValid(window_)) {
    window_ = SbWindowCreate(options);
  }

  return window_;
}

}  // namespace chromecast
