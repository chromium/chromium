// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_READALOUD_AUDIO_RENDERER_READ_ALOUD_AUDIO_RENDERER_H_
#define CHROME_SERVICES_READALOUD_AUDIO_RENDERER_READ_ALOUD_AUDIO_RENDERER_H_

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_renderer_sink.h"

namespace media {
class AudioBus;
struct AudioGlitchInfo;
}  // namespace media

namespace readaloud {

class AudioSegmentQueue;

// Handles rendering of decoded audio segments for ReadAloud playback.
// Implements the RenderCallback interface, which is driven by the real-time
// audio thread.
class ReadAloudAudioRenderer final
    : public media::AudioRendererSink::RenderCallback {
 public:
  ReadAloudAudioRenderer();

  ReadAloudAudioRenderer(const ReadAloudAudioRenderer&) = delete;
  ReadAloudAudioRenderer& operator=(const ReadAloudAudioRenderer&) = delete;

  ~ReadAloudAudioRenderer() override;

  // Initializes the renderer with the target output parameters and the segment
  // queue. Must be called on the owning sequence before Render() is invoked.
  // Returns true if initialization succeeded, or false if parameters are
  // invalid.
  bool Initialize(const media::AudioParameters& params,
                  AudioSegmentQueue* queue);

  // media::AudioRendererSink::RenderCallback implementation:
  // Runs on the real-time audio thread.
  int Render(base::TimeDelta delay,
             base::TimeTicks delay_timestamp,
             const media::AudioGlitchInfo& glitch_info,
             media::AudioBus* dest) override;

  void OnRenderError() override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Written on the owning sequence during Initialize(), read on the real-time
  // audio thread during Render(). No synchronization is needed as long as
  // Initialize() completes before Render() begins.
  media::AudioParameters params_;
  raw_ptr<AudioSegmentQueue> queue_ = nullptr;
  bool initialized_ = false;
};

}  // namespace readaloud

#endif  // CHROME_SERVICES_READALOUD_AUDIO_RENDERER_READ_ALOUD_AUDIO_RENDERER_H_
