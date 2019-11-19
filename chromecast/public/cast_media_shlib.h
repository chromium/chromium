// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_CAST_MEDIA_SHLIB_H_
#define CHROMECAST_PUBLIC_CAST_MEDIA_SHLIB_H_

#include <stdint.h>

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "chromecast_export.h"
#include "volume_control.h"

namespace chromecast {
namespace media {

enum SampleFormat : int;

class MediaPipelineBackend;
struct MediaPipelineDeviceParams;
class VideoPlane;

// Provides access to platform-specific media systems and hardware resources.
// In cast_shell, all usage is from the browser process.  An implementation is
// assumed to be in an uninitialized state initially.  When uninitialized, no
// API calls will be made except for Initialize, which brings the implementation
// into an initialized state.  A call to Finalize returns the implementation to
// its uninitialized state.  The implementation must support multiple
// transitions between these states, to support resource grant/revoke events and
// also to allow multiple unit tests to bring up the media systems in isolation
// from other tests.
class CHROMECAST_EXPORT CastMediaShlib {
 public:
  using ResultCallback =
      std::function<void(bool success, const std::string& message)>;

  // Initializes platform-specific media systems.  Only called when in an
  // uninitialized state.
  static void Initialize(const std::vector<std::string>& argv);

  // Tears down platform-specific media systems and returns to the uninitialized
  // state.  The implementation must release all media-related hardware
  // resources.
  static void Finalize();

  // Gets the VideoPlane instance for managing the hardware video plane.
  // While an implementation is in an initialized state, this function may be
  // called at any time.  The VideoPlane object must be destroyed in Finalize.
  static VideoPlane* GetVideoPlane();

  // Creates a media pipeline backend.  Called in the browser process for each
  // media pipeline and raw audio stream. The caller owns the returned
  // MediaPipelineBackend instance.
  static MediaPipelineBackend* CreateMediaPipelineBackend(
      const MediaPipelineDeviceParams& params);

  // Fetches the renderer clock rate (Hz).
  static double GetMediaClockRate();

  // Fetches the granularity of clock rate adjustments.
  static double MediaClockRatePrecision();

  // Fetches the possible range of clock rate adjustments.
  static void MediaClockRateRange(double* minimum_rate, double* maximum_rate);

  // Sets the renderer clock rate (Hz).
  static bool SetMediaClockRate(double new_rate);

  // Tests if the implementation supports renderer clock rate adjustments.
  static bool SupportsMediaClockRateChange();
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_CAST_MEDIA_SHLIB_H_
