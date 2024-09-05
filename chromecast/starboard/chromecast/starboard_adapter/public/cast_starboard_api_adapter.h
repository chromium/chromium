// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROMECAST_STARBOARD_CHROMECAST_STARBOARD_ADAPTER_PUBLIC_CAST_STARBOARD_API_ADAPTER_H_
#define CHROMECAST_STARBOARD_CHROMECAST_STARBOARD_ADAPTER_PUBLIC_CAST_STARBOARD_API_ADAPTER_H_

#include "chromecast/starboard/starboard_buildflags.h"

#if BUILDFLAG(REMOVE_STARBOARD_HEADERS)

using SbEvent = void;
using SbWindowOptions = void;
using SbEglNativeDisplayType = void*;
using SbWindow = void*;

#else
#include <starboard/egl.h>
#include <starboard/event.h>
#include <starboard/export.h>
#include <starboard/input.h>
#endif  // BUILDFLAG(REMOVE_STARBOARD_HEADERS)

namespace chromecast {

// CastStarboardApiAdapter provides a C++ wrapper to the CastStarboardApi,
// which is a C-style interface implemented in an external library. The primary
// purpose of CastStarboardApiAdapter is to managage the lifecycle of the
// external library, which expects to always be initialized before use and only
// have a single instance.
class __attribute__((visibility("default"))) CastStarboardApiAdapter {
 public:
  static CastStarboardApiAdapter* GetInstance();

  virtual ~CastStarboardApiAdapter() = default;

  // Ensures that the CastStarboardApi is initialized. Callers should invoke
  // this before any other functions.
  virtual bool EnsureInitialized() = 0;

  // When Starboard events occur, `callback` will be called with `context`, an
  // integer corresponding to the SbEventType, and a void* containing the data
  // for the event (which may be null, since some events do not contain data).
  // The `callback` must be thread safe, i.e. it will not automatically be
  // posted to the same thread on which Subscribe was called.
  virtual void Subscribe(void* context,
                         void (*callback)(void* context,
                                          const SbEvent* event)) = 0;

  // Removes a `context` which was previously provided to `Subscribe`. The
  // `context` will no longer receive events.
  virtual void Unsubscribe(void* context) = 0;

  // Gets the SbEglDisplay associated with the internally managed context.
  virtual SbEglNativeDisplayType GetEglNativeDisplayType() = 0;

  // Gets the window shared between the graphics and media libraries.
  virtual SbWindow GetWindow(const SbWindowOptions* options) = 0;
};

}  // namespace chromecast

#endif  // CHROMECAST_STARBOARD_CHROMECAST_STARBOARD_ADAPTER_PUBLIC_CAST_STARBOARD_API_ADAPTER_H_
