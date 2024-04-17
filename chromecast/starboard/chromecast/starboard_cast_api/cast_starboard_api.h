// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_STARBOARD_CHROMECAST_STARBOARD_CAST_API_CAST_STARBOARD_API_H_
#define CHROMECAST_STARBOARD_CHROMECAST_STARBOARD_CAST_API_CAST_STARBOARD_API_H_

#include <starboard/egl.h>
#include <starboard/event.h>
#include <starboard/gles.h>
#include <starboard/media.h>
#include <starboard/player.h>
#include <starboard/window.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*SbEventHandleCB)(const SbEvent*);

// Initializes the Starboard thread and event loop. After this function is
// called, the Starboard APIs included above are expected to be available.
//
// Optional command line arguments are passed through |argc| and |argv|.
// The |callback| is analogous to SbEventHandle and must receive SbEvents.
//
// Must be called prior to the other library functions.
__attribute__((visibility("default"))) int
CastStarboardApiInitialize(int argc, char** argv, SbEventHandleCB callback);

// Finalizes the library in the provided |context|.
//
// Must not be called prior to the other library functions.
__attribute__((visibility("default"))) void CastStarboardApiFinalize();

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // CHROMECAST_STARBOARD_CHROMECAST_STARBOARD_CAST_API_CAST_STARBOARD_API_H_
