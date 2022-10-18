// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SPEECH_AUDIO_SOURCE_CONSUMER_H_
#define CHROME_SERVICES_SPEECH_AUDIO_SOURCE_CONSUMER_H_

#include "media/mojo/mojom/audio_data.mojom.h"

namespace speech {

// An abstract interface class for consuming audio from
// AudioSourceFetcherImpl.
class AudioSourceConsumer {
 public:
  virtual ~AudioSourceConsumer() {}

  // Passes the captured audio to consumer.
  virtual void AddAudio(media::mojom::AudioDataS16Ptr buffer) = 0;

  // Called when audio capture has been stopped.
  virtual void OnAudioCaptureEnd() = 0;

  // Called when an error occurs during audio capture.
  virtual void OnAudioCaptureError() = 0;
};

}  // namespace speech

#endif  // CHROME_SERVICES_SPEECH_AUDIO_SOURCE_CONSUMER_H_
