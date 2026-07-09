// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_READALOUD_READ_ALOUD_PLAYBACK_CONTROLLER_H_
#define CHROME_SERVICES_READALOUD_READ_ALOUD_PLAYBACK_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "chrome/common/readaloud/read_aloud.mojom.h"
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

  mojo::Receiver<read_aloud::mojom::ReadAloudPlaybackControllerFactory>
      receiver_;
  mojo::Receiver<read_aloud::mojom::ReadAloudPlaybackController>
      controller_receiver_{this};
  mojo::Remote<read_aloud::mojom::ReadAloudPlaybackControllerClient> client_;
};

}  // namespace readaloud

#endif  // CHROME_SERVICES_READALOUD_READ_ALOUD_PLAYBACK_CONTROLLER_H_
