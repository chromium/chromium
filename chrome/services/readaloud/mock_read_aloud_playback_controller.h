// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_READALOUD_MOCK_READ_ALOUD_PLAYBACK_CONTROLLER_H_
#define CHROME_SERVICES_READALOUD_MOCK_READ_ALOUD_PLAYBACK_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/common/readaloud/read_aloud.mojom.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "media/mojo/mojom/audio_output_stream.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace readaloud {

// Mock implementation of `ReadAloudPlaybackController` used for unit testing
// speech playback state transitions and word boundary timers.
class MockReadAloudPlaybackController
    : public read_aloud::mojom::ReadAloudPlaybackController {
 public:
  static constexpr base::TimeDelta kDefaultWordDuration =
      base::Milliseconds(250);

  explicit MockReadAloudPlaybackController(
      mojo::PendingReceiver<read_aloud::mojom::ReadAloudPlaybackController>
          receiver);

  MockReadAloudPlaybackController(const MockReadAloudPlaybackController&) =
      delete;
  MockReadAloudPlaybackController& operator=(
      const MockReadAloudPlaybackController&) = delete;

  ~MockReadAloudPlaybackController() override;

  // Binds the client remote to receive playback notifications.
  void InitializeClient(
      mojo::PendingRemote<read_aloud::mojom::ReadAloudPlaybackControllerClient>
          client);

  // read_aloud::mojom::ReadAloudPlaybackController interface (MOCK_METHODs):
  MOCK_METHOD(void,
              InitializeAudio,
              (mojo::PendingRemote<media::mojom::AudioOutputStream> stream,
               media::mojom::ReadWriteAudioDataPipePtr data_pipe),
              (override));
  MOCK_METHOD(void,
              SetTextContent,
              (std::vector<read_aloud::mojom::TextSegmentPtr> segments),
              (override));
  MOCK_METHOD(void, Play, (), (override));
  MOCK_METHOD(void, Pause, (), (override));
  MOCK_METHOD(void,
              SeekToWord,
              (uint32_t segment_index, uint32_t character_offset),
              (override));
  MOCK_METHOD(void, SeekToTime, (base::TimeDelta position), (override));
  MOCK_METHOD(void, SetVoice, (const std::string& voice_id), (override));
  MOCK_METHOD(void, SetPlaybackRate, (float rate), (override));
  MOCK_METHOD(void, FlushBuffers, (), (override));

  // Default fake implementation methods invoked by ON_CALL:
  void DefaultSetTextContent(
      std::vector<read_aloud::mojom::TextSegmentPtr> segments);
  void DefaultPlay();
  void DefaultPause();
  void DefaultSeekToWord(uint32_t segment_index, uint32_t character_offset);
  void DefaultSeekToTime(base::TimeDelta position);
  void DefaultSetPlaybackRate(float rate);

  float playback_rate() const { return playback_rate_; }

 private:
  struct WordBoundary {
    uint32_t segment_index;
    uint32_t character_offset;
    base::TimeDelta audio_timestamp;
  };

  base::TimeDelta CalculateTotalDuration() const;
  void StartTimer();
  void OnTimerFired();
  void TriggerWordBoundary();
  void UpdatePlaybackState(read_aloud::mojom::PlaybackState state);

  mojo::Receiver<read_aloud::mojom::ReadAloudPlaybackController> receiver_;
  mojo::Remote<read_aloud::mojom::ReadAloudPlaybackControllerClient> client_;

  std::vector<read_aloud::mojom::TextSegmentPtr> segments_;
  std::vector<WordBoundary> word_boundaries_;
  size_t current_boundary_index_ = 0;
  float playback_rate_ = 1.0f;

  base::RepeatingTimer timer_;
  read_aloud::mojom::PlaybackState state_ =
      read_aloud::mojom::PlaybackState::kPaused;

  base::WeakPtrFactory<MockReadAloudPlaybackController> weak_factory_{this};
};

}  // namespace readaloud

#endif  // CHROME_SERVICES_READALOUD_MOCK_READ_ALOUD_PLAYBACK_CONTROLLER_H_
