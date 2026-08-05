// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_READALOUD_READ_ALOUD_PLAYBACK_CONTROLLER_H_
#define CHROME_SERVICES_READALOUD_READ_ALOUD_PLAYBACK_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/common/readaloud/read_aloud.mojom.h"
#include "chrome/common/readaloud/read_aloud_constants.h"
#include "chrome/services/readaloud/prefetch/prefetch_manager.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "media/mojo/mojom/audio_output_stream.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace readaloud {

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
      media::mojom::ReadWriteAudioDataPipePtr data_pipe) override;
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

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<ReadAloudPlaybackController> session_weak_factory_{this};
  base::WeakPtrFactory<ReadAloudPlaybackController> factory_weak_factory_{this};
};

}  // namespace readaloud

#endif  // CHROME_SERVICES_READALOUD_READ_ALOUD_PLAYBACK_CONTROLLER_H_
