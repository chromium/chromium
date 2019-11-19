// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_CONVERSIONS_H_
#define CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_CONVERSIONS_H_

#include "chromecast/media/audio/mixer_service/mixer_service.pb.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/volume_control.h"

namespace chromecast {
namespace media {
namespace mixer_service {

media::SampleFormat ConvertSampleFormat(SampleFormat format);
SampleFormat ConvertSampleFormat(media::SampleFormat format);
int GetSampleSizeBytes(SampleFormat format);

ContentType ConvertContentType(media::AudioContentType content_type);
media::AudioContentType ConvertContentType(ContentType type);

}  // namespace mixer_service
}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_CONVERSIONS_H_
