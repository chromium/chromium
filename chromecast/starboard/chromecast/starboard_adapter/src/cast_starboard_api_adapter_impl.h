// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_STARBOARD_CHROMECAST_STARBOARD_ADAPTER_SRC_CAST_STARBOARD_API_ADAPTER_IMPL_H_
#define CHROMECAST_STARBOARD_CHROMECAST_STARBOARD_ADAPTER_SRC_CAST_STARBOARD_API_ADAPTER_IMPL_H_

#include <future>
#include <mutex>
#include <thread>
#include <unordered_map>

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

  CastStarboardApiAdapterImpl();
  ~CastStarboardApiAdapterImpl() override;

 private:
  void SbEventHandleInternal(const SbEvent* event);
  void Initialize();
  void Release();

  // CastStarboardApiAdapter implementation:
  void Subscribe(void* context,
                 CastStarboardApiAdapterImplCB callback) override;
  void Unsubscribe(void* context) override;
  SbEglNativeDisplayType GetEglNativeDisplayType() override;
  SbWindow GetWindow(const SbWindowOptions*) override;

#if SB_API_VERSION >= 15
  std::unique_ptr<std::thread> sb_main_;
#endif  // SB_API_VERSION >= 15
  std::promise<bool> init_p_;
  std::future<bool> init_f_;
  SbWindow window_ = kSbWindowInvalid;
  std::mutex lock_;
  bool initialized_;
  std::unordered_map<void*, CastStarboardApiAdapterImplCB> subscribers_;
};

}  // namespace chromecast

#endif  // CHROMECAST_STARBOARD_CHROMECAST_STARBOARD_ADAPTER_SRC_CAST_STARBOARD_API_ADAPTER_IMPL_H_
