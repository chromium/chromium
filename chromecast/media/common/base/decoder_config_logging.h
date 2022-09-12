// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_COMMON_BASE_DECODER_CONFIG_LOGGING_H_
#define CHROMECAST_MEDIA_COMMON_BASE_DECODER_CONFIG_LOGGING_H_

#include <ostream>

#include "chromecast/public/media/decoder_config.h"

std::ostream& operator<<(std::ostream& stream,
                         ::chromecast::media::AudioCodec codec);
std::ostream& operator<<(std::ostream& stream,
                         ::chromecast::media::SampleFormat format);

#endif  // CHROMECAST_MEDIA_COMMON_BASE_DECODER_CONFIG_LOGGING_H_
