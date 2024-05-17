// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_INTERNAL_MEDIA_AUDIO_EXTERNAL_AUDIO_PIPELINE_DUMMY_H_
#define CHROMECAST_INTERNAL_MEDIA_AUDIO_EXTERNAL_AUDIO_PIPELINE_DUMMY_H_

#include "chromecast/public/media/external_audio_pipeline_shlib.h"

// We redefine what is already available in chromium's base/logging.h because
// we want to avoid having that dependency in dummy implementation. When OEMs
// swap in their implementation it complains about missing symbols.
#if !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)
#include <stdlib.h>  // abort()
#define NOTREACHED_IN_MIGRATION() abort()
#else
#define NOTREACHED_IN_MIGRATION() static_cast<void>(0)
#endif

namespace chromecast {
namespace media {

bool ExternalAudioPipelineShlib::IsSupported() {
  return false;
}

void ExternalAudioPipelineShlib::AddExternalMediaVolumeChangeRequestObserver(
    ExternalMediaVolumeChangeRequestObserver* observer) {
  NOTREACHED_IN_MIGRATION();
}

void ExternalAudioPipelineShlib::RemoveExternalMediaVolumeChangeRequestObserver(
    ExternalMediaVolumeChangeRequestObserver* observer) {
  NOTREACHED_IN_MIGRATION();
}

void ExternalAudioPipelineShlib::SetExternalMediaVolume(float level) {
  NOTREACHED_IN_MIGRATION();
}

void ExternalAudioPipelineShlib::SetExternalMediaMuted(bool muted) {
  NOTREACHED_IN_MIGRATION();
}

void ExternalAudioPipelineShlib::AddExternalLoopbackAudioObserver(
    LoopbackAudioObserver* observer) {
  NOTREACHED_IN_MIGRATION();
}

void ExternalAudioPipelineShlib::RemoveExternalLoopbackAudioObserver(
    LoopbackAudioObserver* observer) {
  NOTREACHED_IN_MIGRATION();
}

void ExternalAudioPipelineShlib::AddExternalMediaMetadataChangeObserver(
    ExternalMediaMetadataChangeObserver* observer) {
  NOTREACHED_IN_MIGRATION();
}

void ExternalAudioPipelineShlib::RemoveExternalMediaMetadataChangeObserver(
    ExternalMediaMetadataChangeObserver* observer) {
  NOTREACHED_IN_MIGRATION();
}

std::unique_ptr<MixerOutputStream>
ExternalAudioPipelineShlib::CreateMixerOutputStream() {
  return nullptr;
}

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_INTERNAL_MEDIA_AUDIO_EXTERNAL_AUDIO_PIPELINE_DUMMY_H_
