// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/tts/tts_service.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chromeos/services/tts/public/mojom/tts_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

using mojo::PendingReceiver;
using mojo::PendingRemote;

namespace chromeos {
namespace tts {

using CreateOutputStreamCallback =
    base::OnceCallback<void(media::mojom::ReadWriteAudioDataPipePtr)>;
using CreateLoopbackStreamCallback =
    base::OnceCallback<void(media::mojom::ReadOnlyAudioDataPipePtr)>;

class MockAudioStreamFactory : public audio::mojom::StreamFactory {
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
      mojo::PendingAssociatedReceiver<audio::mojom::LocalMuter> receiver,
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
  void InitTtsStreamFactory(
      mojo::Remote<mojom::TtsStreamFactory>* tts_stream_factory) {
    // Audio stream factory is here to get a basic environment working only.
    // Unbind and rebind if needed.
    if (audio_stream_factory_.is_bound())
      audio_stream_factory_.reset();

    remote_service_->BindTtsStreamFactory(
        tts_stream_factory->BindNewPipeAndPassReceiver(),
        audio_stream_factory_.BindNewPipeAndPassRemote());
    remote_service_.FlushForTesting();
    EXPECT_TRUE(service_.tts_stream_factory_for_testing()->is_bound());
    EXPECT_TRUE(tts_stream_factory->is_connected());
  }

  void InitPlaybackTtsStream(
      mojo::Remote<mojom::PlaybackTtsStream>* playback_tts_stream) {
    mojo::Remote<mojom::TtsStreamFactory> tts_stream_factory;
    InitTtsStreamFactory(&tts_stream_factory);
    tts_stream_factory->CreatePlaybackTtsStream(base::BindOnce(
        [](mojo::Remote<mojom::PlaybackTtsStream>* playback_tts_stream,
           PendingRemote<mojom::PlaybackTtsStream> stream, int32_t sample_rate,
           int32_t buffer_size) {
          playback_tts_stream->Bind(std::move(stream));
        },
        playback_tts_stream));
    tts_stream_factory.FlushForTesting();
  }

  base::test::TaskEnvironment task_environment_;
  mojo::Remote<mojom::TtsService> remote_service_;
  TtsService service_;
  MockAudioStreamFactory mock_audio_stream_factory_;
  mojo::Receiver<audio::mojom::StreamFactory> audio_stream_factory_;
};

TEST_F(TtsServiceTest, BindMultipleStreamFactories) {
  EXPECT_FALSE(service_.tts_stream_factory_for_testing()->is_bound());

  // Create the first tts stream factory and request a playback stream.
  mojo::Remote<mojom::TtsStreamFactory> tts_stream_factory1;
  InitTtsStreamFactory(&tts_stream_factory1);
  EXPECT_TRUE(
      service_.pending_tts_stream_factory_receivers_for_testing().empty());
  tts_stream_factory1->CreatePlaybackTtsStream(
      base::BindOnce([](PendingRemote<mojom::PlaybackTtsStream> stream,
                        int32_t sample_rate, int32_t buffer_size) {}));
  tts_stream_factory1.FlushForTesting();

  // The receiver resets the connection once the playback stream is created.
  EXPECT_FALSE(tts_stream_factory1.is_connected());
  EXPECT_FALSE(service_.tts_stream_factory_for_testing()->is_bound());
  EXPECT_TRUE(
      service_.pending_tts_stream_factory_receivers_for_testing().empty());

  // Create the second tts stream factory and request a playback stream.
  mojo::Remote<mojom::TtsStreamFactory> tts_stream_factory2;
  InitTtsStreamFactory(&tts_stream_factory2);
  EXPECT_TRUE(
      service_.pending_tts_stream_factory_receivers_for_testing().empty());
  tts_stream_factory2->CreatePlaybackTtsStream(
      base::BindOnce([](PendingRemote<mojom::PlaybackTtsStream> stream,
                        int32_t sample_rate, int32_t buffer_size) {}));
  tts_stream_factory2.FlushForTesting();

  // Neither remote is connected.
  EXPECT_FALSE(tts_stream_factory1.is_connected());
  EXPECT_FALSE(tts_stream_factory2.is_connected());

  // And, the receiver again resets.
  EXPECT_FALSE(service_.tts_stream_factory_for_testing()->is_bound());
  EXPECT_TRUE(
      service_.pending_tts_stream_factory_receivers_for_testing().empty());
}

TEST_F(TtsServiceTest, BindMultipleStreamFactoriesCreateInterleaved) {
  EXPECT_FALSE(service_.tts_stream_factory_for_testing()->is_bound());

  // Create two tts stream factories; then interleave their requests to create
  // playback streams.
  mojo::Remote<mojom::TtsStreamFactory> tts_stream_factory1;
  InitTtsStreamFactory(&tts_stream_factory1);
  EXPECT_TRUE(
      service_.pending_tts_stream_factory_receivers_for_testing().empty());
  EXPECT_TRUE(tts_stream_factory1.is_connected());
  mojo::Remote<mojom::TtsStreamFactory> tts_stream_factory2;
  InitTtsStreamFactory(&tts_stream_factory2);
  EXPECT_EQ(1U,
            service_.pending_tts_stream_factory_receivers_for_testing().size());

  // Note that "connectedness" simply means the remote has not been reset by the
  // receiver and is bound to a PendingReceiver or Receiver. So, the second
  // factory is "connected" even though it is only bound to a PendingReceiver
  // (and not the concrete Receiver).
  EXPECT_TRUE(tts_stream_factory1.is_connected());
  EXPECT_TRUE(tts_stream_factory2.is_connected());

  // Simulate the first tts stream factory creating a playback stream.
  tts_stream_factory1->CreatePlaybackTtsStream(
      base::BindOnce([](PendingRemote<mojom::PlaybackTtsStream> stream,
                        int32_t sample_rate, int32_t buffer_size) {}));
  tts_stream_factory1.FlushForTesting();

  // The second tts stream factory is now bound. There's no easy way to check
  // this explicitly for the receiver.
  EXPECT_TRUE(service_.tts_stream_factory_for_testing()->is_bound());
  EXPECT_TRUE(
      service_.pending_tts_stream_factory_receivers_for_testing().empty());

  // The first tts stream factory gets reset on the receiver end.
  EXPECT_FALSE(tts_stream_factory1.is_connected());
  EXPECT_TRUE(tts_stream_factory2.is_connected());

  // Simulate the second tts stream factory creating a playback stream.
  tts_stream_factory2->CreatePlaybackTtsStream(
      base::BindOnce([](PendingRemote<mojom::PlaybackTtsStream> stream,
                        int32_t sample_rate, int32_t buffer_size) {}));
  tts_stream_factory2.FlushForTesting();

  // No other tts stream factory requests pending.
  EXPECT_FALSE(service_.tts_stream_factory_for_testing()->is_bound());
  EXPECT_TRUE(
      service_.pending_tts_stream_factory_receivers_for_testing().empty());

  // Both tts stream factories are done  with the second tts stream factory
  // reset by the receiver.
  EXPECT_FALSE(tts_stream_factory1.is_connected());
  EXPECT_FALSE(tts_stream_factory2.is_connected());
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

  auto bus = media::AudioBus::Create(1 /* channels */, 512 /* frames */);
  service_.Render(base::TimeDelta::FromSeconds(0), base::TimeTicks::Now(),
                  0 /* prior frames skipped */, bus.get());
  observer.FlushForTesting();

  // The playback stream pushes an empty buffer to trigger a start event.
  EXPECT_EQ(1, backing_observer.start_count);
  EXPECT_TRUE(backing_observer.char_indices.empty());
  EXPECT_EQ(0, backing_observer.end_count);

  playback_tts_stream->SendAudioBuffer(
      std::vector<float>(), 100 /* char_index */, false /* last buffer */);
  playback_tts_stream.FlushForTesting();
  service_.Render(base::TimeDelta::FromSeconds(0), base::TimeTicks::Now(),
                  0 /* prior frames skipped */, bus.get());
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
  service_.Render(base::TimeDelta::FromSeconds(0), base::TimeTicks::Now(),
                  0 /* prior frames skipped */, bus.get());
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
  service_.Render(base::TimeDelta::FromSeconds(0), base::TimeTicks::Now(),
                  0 /* prior frames skipped */, bus.get());
  observer.FlushForTesting();

  // The playback stream pushes an empty buffer to trigger a start event.
  EXPECT_EQ(1, backing_observer.start_count);
  EXPECT_TRUE(backing_observer.char_indices.empty());
  EXPECT_EQ(0, backing_observer.end_count);

  playback_tts_stream->SendAudioBuffer(
      std::vector<float>(), -1 /* char_index */, false /* last buffer */);
  playback_tts_stream.FlushForTesting();
  service_.Render(base::TimeDelta::FromSeconds(0), base::TimeTicks::Now(),
                  0 /* prior frames skipped */, bus.get());
  observer.FlushForTesting();
  EXPECT_EQ(1, backing_observer.start_count);
  EXPECT_TRUE(backing_observer.char_indices.empty());
  EXPECT_EQ(0, backing_observer.end_count);

  playback_tts_stream->SendAudioBuffer(
      std::vector<float>(), -1 /* char_index */, false /* last buffer */);
  service_.AddExplicitTimepoint(100, base::TimeDelta::FromSeconds(0));
  service_.AddExplicitTimepoint(200, base::TimeDelta::FromSeconds(0));
  service_.AddExplicitTimepoint(300, base::TimeDelta::FromSeconds(0));
  playback_tts_stream.FlushForTesting();
  service_.Render(base::TimeDelta::FromSeconds(0), base::TimeTicks::Now(),
                  0 /* prior frames skipped */, bus.get());
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
  service_.Render(base::TimeDelta::FromSeconds(0), base::TimeTicks::Now(),
                  0 /* prior frames skipped */, bus.get());
  observer.FlushForTesting();
  EXPECT_EQ(1, backing_observer.start_count);
  EXPECT_EQ(3U, backing_observer.char_indices.size());
  EXPECT_EQ(1, backing_observer.end_count);
}

}  // namespace tts
}  // namespace chromeos
