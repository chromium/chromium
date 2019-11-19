// Copyright 2018 The Chromium Authors. All rights reserved.
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
#define NOTREACHED() abort()
#else
#define NOTREACHED() static_cast<void>(0)
#endif

namespace chromecast {
namespace media {

bool ExternalAudioPipelineShlib::IsSupported() {
  return false;
}

void ExternalAudioPipelineShlib::AddExternalMediaVolumeChangeRequestObserver(
    ExternalMediaVolumeChangeRequestObserver* observer) {
  NOTREACHED();
}

void ExternalAudioPipelineShlib::RemoveExternalMediaVolumeChangeRequestObserver(
    ExternalMediaVolumeChangeRequestObserver* observer) {
  NOTREACHED();
}

void ExternalAudioPipelineShlib::SetExternalMediaVolume(float level) {
  NOTREACHED();
}

void ExternalAudioPipelineShlib::SetExternalMediaMuted(bool muted) {
  NOTREACHED();
}

void ExternalAudioPipelineShlib::AddExternalLoopbackAudioObserver(
    LoopbackAudioObserver* observer) {
  NOTREACHED();
}

void ExternalAudioPipelineShlib::RemoveExternalLoopbackAudioObserver(
    LoopbackAudioObserver* observer) {
  NOTREACHED();
}

void ExternalAudioPipelineShlib::AddExternalMediaMetadataChangeObserver(
    ExternalMediaMetadataChangeObserver* observer) {
  NOTREACHED();
}

void ExternalAudioPipelineShlib::RemoveExternalMediaMetadataChangeObserver(
    ExternalMediaMetadataChangeObserver* observer) {
  NOTREACHED();
}

std::unique_ptr<MixerOutputStream>
ExternalAudioPipelineShlib::CreateMixerOutputStream() {
  NOTREACHED();
  return nullptr;
}

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_INTERNAL_MEDIA_AUDIO_EXTERNAL_AUDIO_PIPELINE_DUMMY_H_
