// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_READALOUD_READ_ALOUD_PLAYBACK_CONTROLLER_H_
#define CHROME_SERVICES_READALOUD_READ_ALOUD_PLAYBACK_CONTROLLER_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/common/readaloud/read_aloud.mojom.h"
#include "chrome/common/readaloud/read_aloud_constants.h"
#include "chrome/services/readaloud/decoded_audio_segment.h"
#include "chrome/services/readaloud/decoder/opus_decoder_helper.h"
#include "chrome/services/readaloud/decoder/read_aloud_decoder_sequencer.h"
#include "chrome/services/readaloud/prefetch/prefetch_manager.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "media/mojo/mojom/audio_output_stream.mojom.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {
class AudioDeviceThread;
class AudioOutputDeviceThreadCallback;
class AudioParameters;
}  // namespace media

namespace readaloud {

class AudioSegmentQueue;
class ReadAloudAudioRenderer;

// Implements both ReadAloudPlaybackControllerFactory (the service entry point)
// and ReadAloudPlaybackController (the control interface) for simplicity.
class ReadAloudPlaybackController
    : public read_aloud::mojom::ReadAloudPlaybackControllerFactory,
      public read_aloud::mojom::ReadAloudPlaybackController {
 public:
  explicit ReadAloudPlaybackController(
      mojo::PendingReceiver<
          read_aloud::mojom::ReadAloudPlaybackControllerFactory> receiver);

  ReadAloudPlaybackController(const ReadAloudPlaybackController&) = delete;
  ReadAloudPlaybackController& operator=(const ReadAloudPlaybackController&) =
      delete;

  ~ReadAloudPlaybackController() override;

  float playback_rate() const { return playback_rate_; }

 private:
  // read_aloud::mojom::ReadAloudPlaybackControllerFactory:
  void CreateController(
      mojo::PendingReceiver<read_aloud::mojom::ReadAloudPlaybackController>
          controller,
      mojo::PendingRemote<read_aloud::mojom::ReadAloudPlaybackControllerClient>
          client) override;

  // read_aloud::mojom::ReadAloudPlaybackController:
  void InitializeAudio(
      mojo::PendingRemote<media::mojom::AudioOutputStream> stream,
      media::mojom::ReadWriteAudioDataPipePtr data_pipe,
      const media::AudioParameters& params) override;
  void SetTextContent(
      std::vector<read_aloud::mojom::TextSegmentPtr> segments) override;
  void Play() override;
  void Pause() override;
  void SeekToWord(uint32_t segment_index, uint32_t character_offset) override;
  void SeekToTime(base::TimeDelta position) override;
  void SetVoice(const std::string& voice_id) override;
  void SetPlaybackRate(float rate) override;
  void FlushBuffers() override;

 private:
  // Mojo disconnect handlers:
  void OnReceiverDisconnected();
  void OnControllerDisconnected();
  void OnClientDisconnected();

  // Resets active session state, clears segments, and resets playback rate.
  void ResetSession();

  // Invoked by `prefetch_manager_` when an in-flight synthesis request is
  // dispatched.
  void OnPrefetchSynthesisRequest(uint32_t chunk_index,
                                  std::u16string_view text);

  // Invoked by `prefetch_manager_` when text content is chunked.
  void OnTextChunked(const std::vector<std::u16string>& chunks);

  // Callback for Mojo RequestSpeechSynthesis responses from the client.
  void OnSpeechSynthesisResponse(uint64_t sequence_id,
                                 uint32_t chunk_index,
                                 mojo_base::BigBuffer response_bytes,
                                 bool success);

  mojo::Receiver<read_aloud::mojom::ReadAloudPlaybackControllerFactory>
      receiver_;
  mojo::Receiver<read_aloud::mojom::ReadAloudPlaybackController>
      controller_receiver_{this};
  mojo::Remote<read_aloud::mojom::ReadAloudPlaybackControllerClient> client_;

  // Active text segments currently loaded for playback in this session.
  std::vector<read_aloud::mojom::TextSegmentPtr> segments_;
  // Current playback rate multiplier (clamped between kMinPlaybackRate and
  // kMaxPlaybackRate).
  float playback_rate_ = 1.0f;

  // Manages document-bound speech synthesis caching and sentence timeline.
  PrefetchManager prefetch_manager_;

  // Asynchronous Opus decoder helper for background demuxing and decoding.
  OpusDecoderHelper decoder_helper_;

  // Coordinates demand-driven in-order decoding from prefetch_manager_ to
  // audio_segment_queue.
  ReadAloudDecoderSequencer decoder_sequencer_{&prefetch_manager_,
                                               &decoder_helper_};

  struct AudioResources {
    AudioResources();
    AudioResources(AudioResources&&);
    AudioResources& operator=(AudioResources&&);
    ~AudioResources();

    // Members are declared in dependency order so that their reverse-order
    // destruction (bottom-to-top) is safe:
    mojo::Remote<media::mojom::AudioOutputStream> audio_output_stream;
    // 3. `audio_renderer` holds a raw pointer to `audio_segment_queue` to
    //    pull synthesis data, so it must be destroyed before the queue.
    std::unique_ptr<AudioSegmentQueue> audio_segment_queue;
    std::unique_ptr<ReadAloudAudioRenderer> audio_renderer;
    // 2. `audio_callback` holds a raw pointer to and calls into
    //    `audio_renderer`, so it must be destroyed next.
    std::unique_ptr<media::AudioOutputDeviceThreadCallback> audio_callback;
    // 1. `audio_thread` runs a real-time thread calling into
    //    `audio_callback`, so it must be stopped/destroyed first.
    std::unique_ptr<media::AudioDeviceThread> audio_thread;
  };
  std::optional<AudioResources> audio_resources_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<ReadAloudPlaybackController> session_weak_factory_{this};
  base::WeakPtrFactory<ReadAloudPlaybackController> factory_weak_factory_{this};
};

}  // namespace readaloud

#endif  // CHROME_SERVICES_READALOUD_READ_ALOUD_PLAYBACK_CONTROLLER_H_
