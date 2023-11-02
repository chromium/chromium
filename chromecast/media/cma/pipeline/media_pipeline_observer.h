// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_PIPELINE_MEDIA_PIPELINE_OBSERVER_H_
#define CHROMECAST_MEDIA_CMA_PIPELINE_MEDIA_PIPELINE_OBSERVER_H_

namespace media {
class AudioDecoderConfig;
}  // namespace media

namespace chromecast {
namespace media {
class MediaPipelineImpl;

// Don't use this
class MediaPipelineObserver {
 private:
  static void NotifyAudioPipelineInitialized(
      MediaPipelineImpl* pipeline,
      const ::media::AudioDecoderConfig& config);
  static void NotifyPipelineDestroyed(MediaPipelineImpl* pipeline);
  friend class MediaPipelineImpl;

 public:
  virtual void OnAudioPipelineInitialized(
      MediaPipelineImpl* pipeline,
      const ::media::AudioDecoderConfig& config) = 0;
  virtual void OnPipelineDestroyed(MediaPipelineImpl* pipeline) = 0;

  static void AddObserver(MediaPipelineObserver* observer);
  static void RemoveObserver(MediaPipelineObserver* observer);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_PIPELINE_MEDIA_PIPELINE_OBSERVER_H_
