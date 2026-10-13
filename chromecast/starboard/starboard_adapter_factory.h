// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_STARBOARD_STARBOARD_ADAPTER_FACTORY_H_
#define CHROMECAST_STARBOARD_STARBOARD_ADAPTER_FACTORY_H_

namespace chromecast {
class CastStarboardApiAdapter;

namespace media {
class StarboardApiWrapper;
}  // namespace media
}  // namespace chromecast

#define STARBOARD_ADAPTER_EXPORT __attribute__((visibility("default")))

extern "C" {
// Factory function to create the CastStarboardApiAdapter.
// The returned object is owned by the shared library and should not be deleted
// by the caller.
STARBOARD_ADAPTER_EXPORT chromecast::CastStarboardApiAdapter*
CreateCastStarboardApiAdapter();

// Factory function to create the StarboardApiWrapper.
// Ownership of the returned object is transferred to the caller (the caller
// is responsible for deleting the object, e.g. via std::unique_ptr).
STARBOARD_ADAPTER_EXPORT chromecast::media::StarboardApiWrapper*
CreateStarboardApiWrapper();

// Releases the CastStarboardApiAdapter instance during shutdown.
STARBOARD_ADAPTER_EXPORT void ReleaseCastStarboardApiAdapter();
}

#endif  // CHROMECAST_STARBOARD_STARBOARD_ADAPTER_FACTORY_H_
