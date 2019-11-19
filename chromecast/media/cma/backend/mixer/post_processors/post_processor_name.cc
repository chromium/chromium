// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/public/chromecast_export.h"
#include "chromecast/public/media/audio_post_processor2_shlib.h"

#define STRINGIFY(x) #x
#define STRINGIFY_WRAPPER(x) STRINGIFY(x)

#include STRINGIFY_WRAPPER(DEFINED_POSTPROCESSOR_HEADER)

extern "C" CHROMECAST_EXPORT chromecast::media::AudioPostProcessor2*
AudioPostProcessor2Shlib_Create(const std::string& config,
                                int num_channels_in) {
  return new DEFINED_POSTPROCESSOR_NAME(config, num_channels_in);
}
