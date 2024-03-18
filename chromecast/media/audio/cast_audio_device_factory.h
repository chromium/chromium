// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_DEVICE_FACTORY_H_
#define CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_DEVICE_FACTORY_H_

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "media/audio/audio_sink_parameters.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/platform/audio/web_audio_device_source_type.h"
#include "third_party/blink/public/web/modules/media/audio/audio_device_factory.h"

namespace media {
class SwitchableAudioRendererSink;
}  // namespace media

namespace chromecast {
namespace media {

class CastAudioDeviceFactory final : public blink::AudioDeviceFactory {
 public:
  CastAudioDeviceFactory();
  ~CastAudioDeviceFactory() override;

  scoped_refptr<::media::SwitchableAudioRendererSink> NewMixableSink(
      blink::WebAudioDeviceSourceType source_type,
      const blink::LocalFrameToken& frame_token,
      const blink::FrameToken& main_frame_token,
      const ::media::AudioSinkParameters& params) override;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_DEVICE_FACTORY_H_
