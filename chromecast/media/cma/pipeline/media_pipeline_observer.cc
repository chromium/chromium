// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/pipeline/media_pipeline_observer.h"

#include <memory>

#include "base/observer_list.h"

namespace {
base::ObserverList<chromecast::media::MediaPipelineObserver>::Unchecked
    g_observers;
}  // namespace

namespace chromecast {
namespace media {

// static
void MediaPipelineObserver::NotifyAudioPipelineInitialized(
    MediaPipelineImpl* pipeline,
    const ::media::AudioDecoderConfig& config) {
  for (auto& observer : g_observers) {
    observer.OnAudioPipelineInitialized(pipeline, config);
  }
}

// static
void MediaPipelineObserver::NotifyPipelineDestroyed(
    MediaPipelineImpl* pipeline) {
  for (auto& observer : g_observers) {
    observer.OnPipelineDestroyed(pipeline);
  }
}

// static
void MediaPipelineObserver::AddObserver(MediaPipelineObserver* observer) {
  g_observers.AddObserver(observer);
}

// static
void MediaPipelineObserver::RemoveObserver(MediaPipelineObserver* observer) {
  g_observers.RemoveObserver(observer);
}

}  // namespace media
}  // namespace chromecast
