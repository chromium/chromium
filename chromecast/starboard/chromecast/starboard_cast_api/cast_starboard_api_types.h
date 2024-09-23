// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_STARBOARD_CHROMECAST_STARBOARD_CAST_API_CAST_STARBOARD_API_TYPES_H_
#define CHROMECAST_STARBOARD_CHROMECAST_STARBOARD_CAST_API_CAST_STARBOARD_API_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

// TODO(b/334907387): this enum can likely be removed in favor of a simpler
// approach to output formats. Either way, this file should be moved out of the
// starboard_cast_api dir, unless we make it part of a starboard API.
//
// Represents a sample format for PCM data to be resampled to. All formats are
// little endian and interleaved.
enum StarboardPcmSampleFormat {
  kStarboardPcmSampleFormatS16,
  kStarboardPcmSampleFormatS32,
  kStarboardPcmSampleFormatF32,
};

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // CHROMECAST_STARBOARD_CHROMECAST_STARBOARD_CAST_API_CAST_STARBOARD_API_TYPES_H_
