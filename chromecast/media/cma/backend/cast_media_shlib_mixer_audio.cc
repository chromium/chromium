// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Provides CastMediaShlib functions common to all devices using StreamMixer.

#include "chromecast/public/cast_media_shlib.h"

#include <string>
#include <utility>

#include "chromecast/media/cma/backend/stream_mixer.h"

namespace chromecast {
namespace media {

void CastMediaShlib::AddLoopbackAudioObserver(LoopbackAudioObserver* observer) {
  StreamMixer::Get()->AddLoopbackAudioObserver(observer);
}

void CastMediaShlib::RemoveLoopbackAudioObserver(
    LoopbackAudioObserver* observer) {
  StreamMixer::Get()->RemoveLoopbackAudioObserver(observer);
}

void CastMediaShlib::ResetPostProcessors(CastMediaShlib::ResultCallback cb) {
  StreamMixer::Get()->ResetPostProcessors(std::move(cb));
}

void CastMediaShlib::SetPostProcessorConfig(const std::string& name,
                                            const std::string& config) {
  StreamMixer::Get()->SetPostProcessorConfig(name, config);
}

}  // namespace media
}  // namespace chromecast
