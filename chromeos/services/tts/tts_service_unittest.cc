// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/tts/tts_service.h"

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chromeos/services/tts/public/mojom/tts_service.mojom.h"
#include "media/base/audio_glitch_info.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "media/mojo/mojom/audio_stream_factory.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

using mojo::PendingReceiver;
using mojo::PendingRemote;

namespace chromeos {
namespace tts {
namespace {

using CreateOutputStreamCallback =
    base::OnceCallback<void(media::mojom::ReadWriteAudioDataPipePtr)>;
using CreateLoopbackStreamCallback =
    base::OnceCallback<void(media::mojom::ReadOnlyAudioDataPipePtr)>;

class MockAudioStreamFactory : public media::mojom::AudioStreamFactory {
 public:
  void CreateInputStream(
      PendingReceiver<media::mojom::AudioInputStream> stream,
      PendingRemote<media::mojom::AudioInputStreamClient> client,
      PendingRemote<media::mojom::AudioInputStreamObserver> observer,
      PendingRemote<media::mojom::AudioLog> log,
      const std::string& device_id,
      const media::AudioParameters& params,
      uint32_t shared_memory_count,
      bool enable_agc,
      base::ReadOnlySharedMemoryRegion key_press_count_buffer,
      media::mojom::AudioProcessingConfigPtr processing_config,
      CreateInputStreamCallback callback) override {}
  void AssociateInputAndOutputForAec(
      const base::UnguessableToken& input_stream_id,
      const std::string& output_device_id) override {}

  void CreateOutputStream(
      PendingReceiver<media::mojom::AudioOutputStream> stream,
      mojo::PendingAssociatedRemote<media::mojom::AudioOutputStreamObserver>
          observer,
      PendingRemote<media::mojom::AudioLog> log,
      const std::string& device_id,
      const media::AudioParameters& params,
      const base::UnguessableToken& group_id,
      CreateOutputStreamCallback callback) override {
    audio_output_stream_ = std::move(stream);
    std::move(callback).Run(nullptr);
  }
  void BindMuter(
      mojo::PendingAssociatedReceiver<media::mojom::LocalMuter> receiver,
      const base::UnguessableToken& group_id) override {}

  void CreateLoopbackStream(
      PendingReceiver<media::mojom::AudioInputStream> receiver,
      PendingRemote<media::mojom::AudioInputStreamClient> client,
      PendingRemote<media::mojom::AudioInputStreamObserver> observer,
      const media::AudioParameters& params,
      uint32_t shared_memory_count,
      const base::UnguessableToken& group_id,
      CreateLoopbackStreamCallback callback) override {}

  PendingReceiver<media::mojom::AudioOutputStream> audio_output_stream_;
};

class MockTtsEventObserver : public mojom::TtsEventObserver {
 public:
  // mojom::TtsEventObserver:
  void OnStart() override { start_count++; }

  void OnTimepoint(int32_t char_index) override {
    char_indices.push_back(char_index);
  }

  void OnEnd() override { end_count++; }

  void OnError() override {}

  int start_count = 0;
  std::vector<int> char_indices;
  int end_count = 0;
};

class TtsServiceTest : public testing::Test {
 public:
  TtsServiceTest()
      : service_(remote_service_.BindNewPipeAndPassReceiver()),
        audio_stream_factory_(&mock_audio_stream_factory_) {}
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

  base::test::TaskEnvironment task_environment_;
  mojo::Remote<mojom::TtsService> remote_service_;
  TtsService service_;
  MockAudioStreamFactory mock_audio_stream_factory_;
  mojo::Receiver<media::mojom::AudioStreamFactory> audio_stream_factory_;
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
