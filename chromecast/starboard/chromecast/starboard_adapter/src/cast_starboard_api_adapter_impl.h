// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_STARBOARD_CHROMECAST_STARBOARD_ADAPTER_SRC_CAST_STARBOARD_API_ADAPTER_IMPL_H_
#define CHROMECAST_STARBOARD_CHROMECAST_STARBOARD_ADAPTER_SRC_CAST_STARBOARD_API_ADAPTER_IMPL_H_

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
  static CastStarboardApiAdapterImpl* GetInstance();

  // SbEventHandle is provided when calling CastStarboardApiInitialize and is
  // expected to be used as the event callback for Starboard events. It routes
  // events to the singleton instance via SbEventHandleInternal.
  static void SbEventHandle(const SbEvent* event);

  CastStarboardApiAdapterImpl();
  // Never invoked: the singleton is intentionally leaked (see Release()). It is
  // only declared because the base class destructor is virtual.
  ~CastStarboardApiAdapterImpl() override;

  void SbEventHandleInternal(const SbEvent* event);

  // Initializes starboard if necessary, and blocks until starboard has started.
  void EnsureInitialized() LOCKS_EXCLUDED(lock_);

  // Signals that the runtime is shutting down and stops Starboard, once there
  // are no remaining subscribers. If subscribers remain, Starboard is stopped
  // instead by the last Unsubscribe().
  //
  // This object is never destructed: late teardown callers (e.g. media_thread_
  // in CastContentBrowserClient) can outlive the AtExitManager, and destroying
  // it here is what previously caused a use-after-free.
  //
  // Production teardown always arrives via ReleaseForAtExit(); see the comment
  // in the .cc on why the waitable events are never Reset.
  void Release() LOCKS_EXCLUDED(lock_);

  // Release() variant invoked from the base::AtExitManager task. It latches
  // `at_exit_ran_` so that Starboard can never be re-initialized afterwards.
  void ReleaseForAtExit() LOCKS_EXCLUDED(lock_);

  // CastStarboardApiAdapter implementation:
  void Subscribe(void* context,
                 CastStarboardApiAdapterImplCB callback) override;
  void Unsubscribe(void* context) override;
  SbEglNativeDisplayType GetEglNativeDisplayType() override;
  SbWindow GetWindow(const SbWindowOptions*) override;
  bool SbWindowIsValid(SbWindow window) override;
  void* SbWindowGetPlatformHandle(SbWindow window) override;
  const void* GetExtension(const char* name) override;

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

  // Tracks whether the base::AtExitManager teardown has run. Once it has,
  // Starboard must never be started again.
  bool at_exit_ran_ GUARDED_BY(lock_) = false;
};

}  // namespace chromecast

#endif  // CHROMECAST_STARBOARD_CHROMECAST_STARBOARD_ADAPTER_SRC_CAST_STARBOARD_API_ADAPTER_IMPL_H_
