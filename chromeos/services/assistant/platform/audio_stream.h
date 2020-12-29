// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PLATFORM_AUDIO_STREAM_H_
#define CHROMEOS_SERVICES_ASSISTANT_PLATFORM_AUDIO_STREAM_H_

#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "libassistant/shared/public/platform_audio_buffer.h"
#include "media/base/audio_capturer_source.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/audio/public/cpp/device_factory.h"
#include "services/audio/public/mojom/stream_factory.mojom.h"

namespace chromeos {
namespace assistant {

class AudioStreamFactoryDelegate;

// A single audio stream. All captured packets will be sent to the given
// capture callback.
// The audio stream will be opened as soon as this class is created, and
// will be closed in the destructor.
class AudioStream {
 public:
  AudioStream(AudioStreamFactoryDelegate* delegate,
              const std::string& device_id,
              bool detect_dead_stream,
              assistant_client::BufferFormat buffer_format,
              media::AudioCapturerSource::CaptureCallback* capture_callback);
  AudioStream(const AudioStream&) = delete;
  AudioStream& operator=(const AudioStream&) = delete;
  ~AudioStream();

  const std::string& device_id() const;

  bool has_dead_stream_detection() const;

 private:
  void Start();

  void OnAudioSteamFactoryReady(
      mojo::PendingRemote<audio::mojom::StreamFactory> audio_stream_factory);

  void Stop();

  audio::DeadStreamDetection DeadStreamDetection() const;

  media::AudioParameters GetAudioParameters() const;

  // Device used for recording.
  std::string device_id_;
  bool detect_dead_stream_;
  assistant_client::BufferFormat buffer_format_;
  AudioStreamFactoryDelegate* const delegate_;
  media::AudioCapturerSource::CaptureCallback* const capture_callback_;
  scoped_refptr<media::AudioCapturerSource> source_;
  base::WeakPtrFactory<AudioStream> weak_ptr_factory_{this};
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PLATFORM_AUDIO_STREAM_H_
