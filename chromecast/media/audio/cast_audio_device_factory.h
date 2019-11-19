// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_DEVICE_FACTORY_H_
#define CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_DEVICE_FACTORY_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/renderer/media/audio/audio_device_factory.h"
#include "media/audio/audio_sink_parameters.h"
#include "third_party/blink/public/platform/audio/web_audio_device_source_type.h"

namespace media {
class AudioCapturerSource;
class AudioRendererSink;
class SwitchableAudioRendererSink;
}  // namespace media

namespace chromecast {
namespace media {

class CastAudioDeviceFactory : public content::AudioDeviceFactory {
 public:
  CastAudioDeviceFactory();
  ~CastAudioDeviceFactory() final;

  scoped_refptr<::media::AudioRendererSink> CreateFinalAudioRendererSink(
      int render_frame_id,
      const ::media::AudioSinkParameters& params,
      base::TimeDelta auth_timeout) override;

  scoped_refptr<::media::AudioRendererSink> CreateAudioRendererSink(
      blink::WebAudioDeviceSourceType source_type,
      int render_frame_id,
      const ::media::AudioSinkParameters& params) override;

  scoped_refptr<::media::SwitchableAudioRendererSink>
  CreateSwitchableAudioRendererSink(
      blink::WebAudioDeviceSourceType source_type,
      int render_frame_id,
      const ::media::AudioSinkParameters& params) override;

  scoped_refptr<::media::AudioCapturerSource> CreateAudioCapturerSource(
      int render_frame_id,
      const ::media::AudioSourceParameters& params) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CastAudioDeviceFactory);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_DEVICE_FACTORY_H_
