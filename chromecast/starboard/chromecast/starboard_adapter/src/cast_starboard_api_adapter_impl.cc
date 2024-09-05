// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast_starboard_api_adapter_impl.h"

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
CastStarboardApiAdapterImpl* GetImpl() {
  static CastStarboardApiAdapterImpl* starboard_adapter =
      new CastStarboardApiAdapterImpl();
  return starboard_adapter;
}
}  // namespace

CastStarboardApiAdapter* CastStarboardApiAdapter::GetInstance() {
  return GetImpl();
}

#if SB_API_VERSION >= 15
CastStarboardApiAdapterImpl::CastStarboardApiAdapterImpl()
    : init_f_(init_p_.get_future()), initialized_(false) {}
#else   // SB_API_VERSION >=15
CastStarboardApiAdapterImpl::CastStarboardApiAdapterImpl()
    : initialized_(false) {}
#endif  // SB_API_VERSION >= 15

CastStarboardApiAdapterImpl::~CastStarboardApiAdapterImpl() {}

SbEglNativeDisplayType CastStarboardApiAdapterImpl::GetEglNativeDisplayType() {
  return SB_EGL_DEFAULT_DISPLAY;
}

// static
void CastStarboardApiAdapterImpl::SbEventHandle(const SbEvent* event) {
  GetImpl()->SbEventHandleInternal(event);
}

#if SB_API_VERSION >= 15
void CastStarboardApiAdapterImpl::SbEventHandleInternal(const SbEvent* event) {
  // If multiple instances of Starboard become supported, |event->window| may
  // need to be checked here for some types before propagating.
  switch (event->type) {
    case kSbEventTypeStart:
      init_p_.set_value(true);
      break;
    default:
      for (const auto p : subscribers_) {
        p.second(p.first, event);
      }
      break;
  }
}
#else   // SB_API_VERSION >=15
void CastStarboardApiAdapterImpl::SbEventHandleInternal(const SbEvent* event) {
  std::lock_guard<decltype(lock_)> lock(lock_);
  for (const auto p : subscribers_) {
    p.second(p.first, event);
  }
}
#endif  // SB_API_VERSION >= 15

#if SB_API_VERSION >= 15
bool CastStarboardApiAdapterImpl::EnsureInitialized() {
  std::lock_guard<decltype(lock_)> lock(lock_);
  if (initialized_) {
    return true;
  }

  sb_main_ = std::make_unique<std::thread>(
      &SbRunStarboardMain, /*argc=*/0, /*argv=*/nullptr,
      &CastStarboardApiAdapterImpl::SbEventHandle);
  sb_main_->detach();
  initialized_ = init_f_.get();
  return initialized_;
}
#else   // SB_API_VERSION >=15
bool CastStarboardApiAdapterImpl::EnsureInitialized() {
  std::lock_guard<decltype(lock_)> lock(lock_);
  if (initialized_) {
    return true;
  }

  CastStarboardApiInitialize(/*argc=*/0, /*argv=*/nullptr,
                             &CastStarboardApiAdapterImpl::SbEventHandle);
  initialized_ = true;
  return true;
}
#endif  // SB_API_VERSION >= 15

void CastStarboardApiAdapterImpl::Subscribe(void* context,
                                            CastStarboardApiAdapterImplCB cb) {
  std::lock_guard<decltype(lock_)> lock(lock_);
  subscribers_.insert({context, cb});
}

void CastStarboardApiAdapterImpl::Unsubscribe(void* context) {
  std::lock_guard<decltype(lock_)> lock(lock_);
  subscribers_.erase(context);
}

SbWindow CastStarboardApiAdapterImpl::GetWindow(
    const SbWindowOptions* options) {
  if (!SbWindowIsValid(window_)) {
    window_ = SbWindowCreate(options);
  }

  return window_;
}

}  // namespace chromecast
