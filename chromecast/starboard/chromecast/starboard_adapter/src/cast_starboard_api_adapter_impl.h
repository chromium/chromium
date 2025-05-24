// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_STARBOARD_CHROMECAST_STARBOARD_ADAPTER_SRC_CAST_STARBOARD_API_ADAPTER_IMPL_H_
#define CHROMECAST_STARBOARD_CHROMECAST_STARBOARD_ADAPTER_SRC_CAST_STARBOARD_API_ADAPTER_IMPL_H_

#include <thread>
#include <unordered_map>

#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/thread_annotations.h"
#include "chromecast/starboard/chromecast/starboard_adapter/public/cast_starboard_api_adapter.h"

namespace chromecast {

using CastStarboardApiAdapterImplCB = void (*)(void* context,
                                               const SbEvent* event);

// Utility class for wrapping CastStarboardApi which allows delivery of events
// to multiple subscribers.
class CastStarboardApiAdapterImpl : public CastStarboardApiAdapter {
 public:
  // SbEventHandle is provided when calling CastStarboardApiInitialize and is
  // expected to be used as the event callback for Starboard events. It routes
  // events to the singleton instance via SbEventHandleInternal.
  static void SbEventHandle(const SbEvent* event);

 private:
  // CastStarboardApiAdapter needs to construct and delete instances of this
  // class.
  friend CastStarboardApiAdapter;

  CastStarboardApiAdapterImpl();
  ~CastStarboardApiAdapterImpl() override;

  void SbEventHandleInternal(const SbEvent* event);

  // Initializes starboard if necessary, and blocks until starboard has started.
  void EnsureInitialized() LOCKS_EXCLUDED(lock_);

  // Signals that the runtime is shutting down, and that this object should be
  // destructed if there are no remaining subscribers.
  //
  // If there are remaining subscribers, the object will be destructed once the
  // last subscriber unsubscribes.
  void Release() LOCKS_EXCLUDED(lock_);

  // CastStarboardApiAdapter implementation:
  void Subscribe(void* context,
                 CastStarboardApiAdapterImplCB callback) override;
  void Unsubscribe(void* context) override;
  SbEglNativeDisplayType GetEglNativeDisplayType() override;
  SbWindow GetWindow(const SbWindowOptions*) override;

#if SB_API_VERSION >= 15
  std::unique_ptr<std::thread> sb_main_;
#endif  // SB_API_VERSION >= 15
  base::WaitableEvent starboard_started_;
  base::WaitableEvent starboard_stopped_;

  base::Lock lock_;
  SbWindow window_ GUARDED_BY(lock_) = kSbWindowInvalid;
  bool initialized_ GUARDED_BY(lock_) = false;
  std::unordered_map<void*, CastStarboardApiAdapterImplCB> subscribers_
      GUARDED_BY(lock_);

  // Tracks whether Release() has been called (meaning the runtime is shutting
  // down).
  bool released_ GUARDED_BY(lock_) = false;
};

}  // namespace chromecast

#endif  // CHROMECAST_STARBOARD_CHROMECAST_STARBOARD_ADAPTER_SRC_CAST_STARBOARD_API_ADAPTER_IMPL_H_
