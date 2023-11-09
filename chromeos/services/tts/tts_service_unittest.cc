// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/tts/tts_service.h"

#include "chromeos/services/tts/public/mojom/tts_service.mojom.h"
#include "chromeos/services/tts/tts_test_utils.h"

namespace chromeos {
namespace tts {
namespace {

// Tests the TtsService interface, the main interface that handles TTS
// requests from clients on ChromeOS. For more info, please see
// chromeos/services/tts/public/mojom/tts_service.mojom.
class TtsServiceTest : public TtsTestBase {
 public:
  TtsServiceTest() : service_(remote_service_.BindNewPipeAndPassReceiver()) {}
  TtsServiceTest(const TtsServiceTest&) = delete;
  TtsServiceTest& operator=(const TtsServiceTest&) = delete;
  ~TtsServiceTest() override = default;

 protected:
  void InitPlaybackTtsStream(mojo::Remote<mojom::PlaybackTtsStream>* stream) {
    // Audio stream factory is here to get a basic environment working only.
    // Unbind and rebind if needed.
    if (audio_stream_factory_.is_bound())
      audio_stream_factory_.reset();

    auto callback = base::BindOnce([](mojom::AudioParametersPtr) {
      // Do nothing.
    });

    mojom::AudioParametersPtr desired_audio_parameters =
        mojom::AudioParameters::New(20000 /* sample rate */,
                                    128 /* buffer size */);
    remote_service_->BindPlaybackTtsStream(
        stream->BindNewPipeAndPassReceiver(),
        audio_stream_factory_.BindNewPipeAndPassRemote(),
        std::move(desired_audio_parameters), std::move(callback));
    remote_service_.FlushForTesting();
  }

  // testing::Test:
  void SetUp() override { service_.set_keep_process_alive_for_testing(true); }

  mojo::Remote<mojom::TtsService> remote_service_;
  TtsService service_;
};

TEST_F(TtsServiceTest, DisconnectPlaybackStream) {
  // Create the first tts stream factory and request a playback stream.
  mojo::Remote<mojom::PlaybackTtsStream> stream1;
  InitPlaybackTtsStream(&stream1);

  // There's an active playback stream, so the tts service receiver should still
  // be bound.
  EXPECT_TRUE(service_.receiver_for_testing()->is_bound());

  // Simulate disconnecting the remote here (e.g. extension closes).
  stream1.reset();
  service_.playback_tts_stream_for_testing()->FlushForTesting();

  // The tts service receiver should have been reset, indicating the
  // process would have been exited in production.
  EXPECT_FALSE(service_.receiver_for_testing()->is_bound());
}

TEST_F(TtsServiceTest, BasicAudioBuffering) {
  mojo::Remote<mojom::PlaybackTtsStream> playback_tts_stream;
  InitPlaybackTtsStream(&playback_tts_stream);

  MockTtsEventObserver backing_observer;
  mojo::Receiver<mojom::TtsEventObserver> observer(&backing_observer);
  playback_tts_stream->Play(base::BindOnce(
      [](mojo::Receiver<mojom::TtsEventObserver>* receiver,
         mojo::PendingReceiver<mojom::TtsEventObserver> pending_receiver) {
        receiver->Bind(std::move(pending_receiver));
      },
      &observer));
  playback_tts_stream.FlushForTesting();
  service_.playback_tts_stream_for_testing()->FlushForTesting();

  auto bus = media::AudioBus::Create(1 /* channels */, 512 /* frames */);
  service_.playback_tts_stream_for_testing()->tts_player_for_testing()->Render(
      base::Seconds(0), base::TimeTicks::Now(), {} /* glitch info */,
      bus.get());
  observer.FlushForTesting();

  // The playback stream pushes an empty buffer to trigger a start event.
  EXPECT_EQ(1, backing_observer.start_count);
  EXPECT_TRUE(backing_observer.char_indices.empty());
  EXPECT_EQ(0, backing_observer.end_count);

  playback_tts_stream->SendAudioBuffer(
      std::vector<float>(), 100 /* char_index */, false /* last buffer */);
  playback_tts_stream.FlushForTesting();
  service_.playback_tts_stream_for_testing()->tts_player_for_testing()->Render(
      base::Seconds(0), base::TimeTicks::Now(), {} /* glitch info */,
      bus.get());
  observer.FlushForTesting();
  EXPECT_EQ(1, backing_observer.start_count);
  EXPECT_EQ(1U, backing_observer.char_indices.size());
  EXPECT_EQ(100, backing_observer.char_indices[0]);
  EXPECT_EQ(0, backing_observer.end_count);

  // Note that the cahr index is ignored for the end of all audio as it's
  // assumed to be the length of the utterance.
  playback_tts_stream->SendAudioBuffer(
      std::vector<float>(), 9999 /* char_index */, true /* last buffer */);
  playback_tts_stream.FlushForTesting();
  service_.playback_tts_stream_for_testing()->tts_player_for_testing()->Render(
      base::Seconds(0), base::TimeTicks::Now(), {} /* glitch info */,
      bus.get());
  observer.FlushForTesting();
  EXPECT_EQ(1, backing_observer.start_count);
  EXPECT_EQ(1U, backing_observer.char_indices.size());
  EXPECT_EQ(1, backing_observer.end_count);
}

TEST_F(TtsServiceTest, ExplicitAudioTimepointing) {
  mojo::Remote<mojom::PlaybackTtsStream> playback_tts_stream;
  InitPlaybackTtsStream(&playback_tts_stream);

  MockTtsEventObserver backing_observer;
  mojo::Receiver<mojom::TtsEventObserver> observer(&backing_observer);
  playback_tts_stream->Play(base::BindOnce(
      [](mojo::Receiver<mojom::TtsEventObserver>* receiver,
         mojo::PendingReceiver<mojom::TtsEventObserver> pending_receiver) {
        receiver->Bind(std::move(pending_receiver));
      },
      &observer));
  playback_tts_stream.FlushForTesting();

  auto bus = media::AudioBus::Create(1 /* channels */, 512 /* frames */);
  service_.playback_tts_stream_for_testing()->tts_player_for_testing()->Render(
      base::Seconds(0), base::TimeTicks::Now(), {} /* glitch info */,
      bus.get());
  observer.FlushForTesting();

  // The playback stream pushes an empty buffer to trigger a start event.
  EXPECT_EQ(1, backing_observer.start_count);
  EXPECT_TRUE(backing_observer.char_indices.empty());
  EXPECT_EQ(0, backing_observer.end_count);

  playback_tts_stream->SendAudioBuffer(
      std::vector<float>(), -1 /* char_index */, false /* last buffer */);
  playback_tts_stream.FlushForTesting();
  service_.playback_tts_stream_for_testing()->tts_player_for_testing()->Render(
      base::Seconds(0), base::TimeTicks::Now(), {} /* glitch info */,
      bus.get());
  observer.FlushForTesting();
  EXPECT_EQ(1, backing_observer.start_count);
  EXPECT_TRUE(backing_observer.char_indices.empty());
  EXPECT_EQ(0, backing_observer.end_count);

  playback_tts_stream->SendAudioBuffer(
      std::vector<float>(), -1 /* char_index */, false /* last buffer */);
  service_.playback_tts_stream_for_testing()
      ->tts_player_for_testing()
      ->AddExplicitTimepoint(100, base::Seconds(0));
  service_.playback_tts_stream_for_testing()
      ->tts_player_for_testing()
      ->AddExplicitTimepoint(200, base::Seconds(0));
  service_.playback_tts_stream_for_testing()
      ->tts_player_for_testing()
      ->AddExplicitTimepoint(300, base::Seconds(0));
  playback_tts_stream.FlushForTesting();
  service_.playback_tts_stream_for_testing()->tts_player_for_testing()->Render(
      base::Seconds(0), base::TimeTicks::Now(), {} /* glitch info */,
      bus.get());
  observer.FlushForTesting();
  EXPECT_EQ(1, backing_observer.start_count);
  EXPECT_EQ(3U, backing_observer.char_indices.size());
  EXPECT_EQ(100, backing_observer.char_indices[0]);
  EXPECT_EQ(200, backing_observer.char_indices[1]);
  EXPECT_EQ(300, backing_observer.char_indices[2]);
  EXPECT_EQ(0, backing_observer.end_count);

  playback_tts_stream->SendAudioBuffer(
      std::vector<float>(), 9999 /* char_index */, true /* last buffer */);
  playback_tts_stream.FlushForTesting();
  service_.playback_tts_stream_for_testing()->tts_player_for_testing()->Render(
      base::Seconds(0), base::TimeTicks::Now(), {} /* glitch info */,
      bus.get());
  observer.FlushForTesting();
  EXPECT_EQ(1, backing_observer.start_count);
  EXPECT_EQ(3U, backing_observer.char_indices.size());
  EXPECT_EQ(1, backing_observer.end_count);
}

}  // namespace
}  // namespace tts
}  // namespace chromeos
