// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/tts/tts_player.h"

#include <memory>

#include "base/containers/span.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/sync_socket.h"
#include "chromeos/services/tts/constants.h"
#include "chromeos/services/tts/tts_test_utils.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {
namespace tts {
namespace {

// Tests TtsPlayer, which is used to render TTS audio bytes for
// playback via a media::AudioBus.
class TtsPlayerTest : public TtsTestBase {
 public:
  TtsPlayerTest()
      : tts_player_(audio_stream_factory_.BindNewPipeAndPassRemote(),
                    std::move(media::AudioParameters(
                        media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                        media::ChannelLayoutConfig::Mono(),
                        kDefaultSampleRate,
                        kDefaultBufferSize))),
        observer_(&backing_observer_) {}

  TtsPlayerTest(const TtsPlayerTest&) = delete;
  TtsPlayerTest& operator=(const TtsPlayerTest&) = delete;
  ~TtsPlayerTest() override = default;

 protected:
  void AddAudioBuffer(const std::vector<float>& frames,
                      int32_t char_index,
                      int32_t status) {
    TtsPlayer::AudioBuffer buffer;
    buffer.frames = frames;
    buffer.char_index = char_index;
    buffer.status = status;
    tts_player_.AddAudioBuffer(std::move(buffer));
  }

  // testing::Test:
  void SetUp() override {
    tts_player_.Play(base::BindOnce(
        [](mojo::Receiver<mojom::TtsEventObserver>* receiver,
           mojo::PendingReceiver<mojom::TtsEventObserver> pending_receiver) {
          receiver->Bind(std::move(pending_receiver));
        },
        &observer_));
  }

  TtsPlayer tts_player_;
  MockTtsEventObserver backing_observer_;
  mojo::Receiver<mojom::TtsEventObserver> observer_;
};

TEST_F(TtsPlayerTest, RenderNoFramesFromEmptyBuffer) {
  auto bus = media::AudioBus::Create(/*channels=*/1, /*frames=*/512);
  tts_player_.Render(base::Seconds(0), base::TimeTicks::Now(),
                     {} /* glitch info */, bus.get());

  // The playback stream pushes an empty buffer to trigger a start event.
  observer_.FlushForTesting();
  EXPECT_EQ(0, backing_observer_.start_count);
  EXPECT_TRUE(backing_observer_.char_indices.empty());
  EXPECT_EQ(0, backing_observer_.end_count);

  // Push an empty buffer.
  AddAudioBuffer(std::vector<float>(), 0 /* char_index */, 1 /* status */);
  int frames_rendered =
      tts_player_.Render(base::Seconds(0), base::TimeTicks::Now(),
                         {} /* glitch info */, bus.get());
  observer_.FlushForTesting();
  EXPECT_EQ(0, backing_observer_.start_count);
  ASSERT_EQ(1U, backing_observer_.char_indices.size());
  EXPECT_EQ(0, backing_observer_.char_indices[0]);
  EXPECT_EQ(0, backing_observer_.end_count);

  EXPECT_EQ(0, frames_rendered);
}

TEST_F(TtsPlayerTest, RenderSingleFrame) {
  auto bus = media::AudioBus::Create(/*channels=*/1, /*frames=*/512);
  tts_player_.Render(base::Seconds(0), base::TimeTicks::Now(),
                     {} /* glitch info */, bus.get());

  // The playback stream pushes an empty buffer to trigger a start event.
  observer_.FlushForTesting();
  EXPECT_EQ(0, backing_observer_.start_count);
  EXPECT_TRUE(backing_observer_.char_indices.empty());
  EXPECT_EQ(0, backing_observer_.end_count);

  // Renders a single frame.
  AddAudioBuffer(std::vector<float>({0.7}), 1 /* char_index */,
                 0 /* last buffer */);
  int frames_rendered =
      tts_player_.Render(base::Seconds(0), base::TimeTicks::Now(),
                         {} /* glitch info */, bus.get());
  observer_.FlushForTesting();
  EXPECT_EQ(0, backing_observer_.start_count);
  EXPECT_TRUE(backing_observer_.char_indices.empty());
  EXPECT_EQ(1, backing_observer_.end_count);

  EXPECT_EQ(1, frames_rendered);
  auto actual = bus->channel(0).first<1u>();
  constexpr std::array<float, 1> kExpected = {0.7};
  EXPECT_EQ(actual, base::span(kExpected));
}

TEST_F(TtsPlayerTest, RenderFramesFromPartialBuffers) {
  auto bus = media::AudioBus::Create(/*channels=*/1, /*frames=*/5);
  tts_player_.Render(base::Seconds(0), base::TimeTicks::Now(),
                     {} /* glitch info */, bus.get());

  // The playback stream pushes an empty buffer to trigger a start event.
  observer_.FlushForTesting();
  EXPECT_EQ(0, backing_observer_.start_count);
  EXPECT_TRUE(backing_observer_.char_indices.empty());
  EXPECT_EQ(0, backing_observer_.end_count);

  // Renders a full frame from two partial buffers.
  AddAudioBuffer(std::vector<float>({0.1, 0.2, 0.3}), 1 /* char_index */,
                 1 /* status */);
  AddAudioBuffer(std::vector<float>({0.4, 0.5}), 2 /* char_index */,
                 0 /* last buffer */);
  int frames_rendered =
      tts_player_.Render(base::Seconds(0), base::TimeTicks::Now(),
                         {} /* glitch info */, bus.get());
  observer_.FlushForTesting();
  EXPECT_EQ(0, backing_observer_.start_count);
  ASSERT_EQ(1U, backing_observer_.char_indices.size());
  EXPECT_EQ(1, backing_observer_.char_indices[0]);
  EXPECT_EQ(1, backing_observer_.end_count);

  EXPECT_EQ(5, frames_rendered);
  auto actual = bus->channel(0).first<5u>();
  constexpr std::array<float, 5> kExpected = {0.1, 0.2, 0.3, 0.4, 0.5};
  EXPECT_EQ(actual, base::span(kExpected));
}

TEST_F(TtsPlayerTest, RenderBusWithFramesFromEmptyAndPartialBuffers) {
  auto bus = media::AudioBus::Create(/*channels=*/1, /*frames=*/5);
  tts_player_.Render(base::Seconds(0), base::TimeTicks::Now(),
                     {} /* glitch info */, bus.get());

  // The playback stream pushes an empty buffer to trigger a start event.
  observer_.FlushForTesting();
  EXPECT_EQ(0, backing_observer_.start_count);
  EXPECT_TRUE(backing_observer_.char_indices.empty());
  EXPECT_EQ(0, backing_observer_.end_count);

  // Renders a full frame from four empty and three partial buffers.
  AddAudioBuffer(std::vector<float>({}), 0 /* char_index */, 1 /* status */);
  AddAudioBuffer(std::vector<float>({0.1, 0.2, 0.3}), 1 /* char_index */,
                 1 /* status */);
  AddAudioBuffer(std::vector<float>({0.4}), 2 /* char_index */, 1 /* status */);
  AddAudioBuffer(std::vector<float>({}), 3 /* char_index */, 1 /* status */);
  AddAudioBuffer(std::vector<float>({}), 4 /* char_index */, 1 /* status */);
  AddAudioBuffer(std::vector<float>({0.5}), 5 /* char_index */, 1 /* status */);
  AddAudioBuffer(std::vector<float>({}), 6 /* char_index */,
                 0 /* last buffer */);
  int frames_rendered =
      tts_player_.Render(base::Seconds(0), base::TimeTicks::Now(),
                         {} /* glitch info */, bus.get());
  observer_.FlushForTesting();
  EXPECT_EQ(0, backing_observer_.start_count);
  ASSERT_EQ(6U, backing_observer_.char_indices.size());
  EXPECT_EQ(0, backing_observer_.char_indices[0]);
  EXPECT_EQ(1, backing_observer_.char_indices[1]);
  EXPECT_EQ(2, backing_observer_.char_indices[2]);
  EXPECT_EQ(3, backing_observer_.char_indices[3]);
  EXPECT_EQ(4, backing_observer_.char_indices[4]);
  EXPECT_EQ(5, backing_observer_.char_indices[5]);
  EXPECT_EQ(0, backing_observer_.end_count);

  EXPECT_EQ(5, frames_rendered);
  auto actual = bus->channel(0).first<5u>();
  constexpr std::array<float, 5> kExpected = {0.1, 0.2, 0.3, 0.4, 0.5};
  EXPECT_EQ(actual, base::span(kExpected));
}

TEST_F(TtsPlayerTest, RenderMultiBusFromMultiBuffers) {
  auto first_bus = media::AudioBus::Create(/*channels=*/1, /*frames=*/5);
  tts_player_.Render(base::Seconds(0), base::TimeTicks::Now(),
                     {} /* glitch info */, first_bus.get());

  // The playback stream pushes an empty buffer to trigger a start event.
  observer_.FlushForTesting();
  EXPECT_EQ(0, backing_observer_.start_count);
  EXPECT_TRUE(backing_observer_.char_indices.empty());
  EXPECT_EQ(0, backing_observer_.end_count);

  // Renders two busses of frames from two empty and two partial buffers. Wrap
  // the buffers around the busses, so each bus holds as many frames as it can.
  AddAudioBuffer(std::vector<float>({}), 0 /* char_index */, 1 /* status */);
  AddAudioBuffer(std::vector<float>({0.1, 0.2, 0.3}), 1 /* char_index */,
                 1 /* status */);
  AddAudioBuffer(std::vector<float>({0.4, 0.5, 0.6, 0.7}), 2 /* char_index */,
                 1 /* status */);
  AddAudioBuffer(std::vector<float>({}), 3 /* char_index */,
                 0 /* last buffer */);

  // Render five frames to first bus.
  int frames_rendered =
      tts_player_.Render(base::Seconds(0), base::TimeTicks::Now(),
                         {} /* glitch info */, first_bus.get());
  observer_.FlushForTesting();
  EXPECT_EQ(0, backing_observer_.start_count);
  ASSERT_EQ(2U, backing_observer_.char_indices.size());
  EXPECT_EQ(0, backing_observer_.char_indices[0]);
  EXPECT_EQ(1, backing_observer_.char_indices[1]);
  EXPECT_EQ(0, backing_observer_.end_count);

  EXPECT_EQ(5, frames_rendered);
  auto first_actual = first_bus->channel(0).first<5u>();
  constexpr std::array<float, 5> kFirstExpected = {0.1, 0.2, 0.3, 0.4, 0.5};
  EXPECT_EQ(first_actual, base::span(kFirstExpected));

  // Render two frames to second bus.
  auto second_bus = media::AudioBus::Create(/*channels=*/1, /*frames=*/5);
  frames_rendered = tts_player_.Render(base::Seconds(0), base::TimeTicks::Now(),
                                       {} /* glitch info */, second_bus.get());
  observer_.FlushForTesting();
  EXPECT_EQ(0, backing_observer_.start_count);
  ASSERT_EQ(3U, backing_observer_.char_indices.size());
  EXPECT_EQ(0, backing_observer_.char_indices[0]);
  EXPECT_EQ(1, backing_observer_.char_indices[1]);
  EXPECT_EQ(2, backing_observer_.char_indices[2]);
  EXPECT_EQ(1, backing_observer_.end_count);

  EXPECT_EQ(2, frames_rendered);
  auto second_actual = second_bus->channel(0).first<2u>();
  constexpr std::array<float, 2> kSecondExpected = {0.6, 0.7};
  EXPECT_THAT(second_actual, base::span(kSecondExpected));
}

// Unlike MockAudioStreamFactory, provides a real data pipe so OutputDevice
// creates a real audio render thread.
class RealPipeAudioStreamFactory : public MockAudioStreamFactory {
 public:
  void CreateOutputStream(
      mojo::PendingReceiver<media::mojom::AudioOutputStream> stream,
      mojo::PendingAssociatedRemote<media::mojom::AudioOutputStreamObserver>
          observer,
      mojo::PendingRemote<media::mojom::AudioLog> log,
      const std::string& device_id,
      const media::AudioParameters& params,
      const base::UnguessableToken& group_id,
      base::OnceCallback<void(media::mojom::ReadWriteAudioDataPipePtr)>
          callback) override {
    audio_output_stream_ = std::move(stream);

    uint32_t buffer_size = media::ComputeAudioOutputBufferSize(params);
    auto shared_memory_region =
        base::UnsafeSharedMemoryRegion::Create(buffer_size);
    CHECK(shared_memory_region.IsValid());

    {
      auto mapping = shared_memory_region.Map();
      CHECK(mapping.IsValid());
      auto span = mapping.GetMemoryAsSpan<uint8_t>(buffer_size);
      std::ranges::fill(span, 0);
    }

    base::CancelableSyncSocket foreign_socket;
    CHECK(base::CancelableSyncSocket::CreatePair(&local_socket_,
                                                  &foreign_socket));

    std::move(callback).Run(media::mojom::ReadWriteAudioDataPipe::New(
        std::move(shared_memory_region),
        mojo::PlatformHandle(foreign_socket.Take())));
  }

  void SignalRender() {
    uint32_t pending_data = 0;
    local_socket_.Send(base::byte_span_from_ref(pending_data));
  }

  base::CancelableSyncSocket local_socket_;
};

// Regression test for crbug.com/502514784. Destroying a TtsPlayer while the
// audio thread is in Render() would UAF on buffers_ if output_device_ were
// declared before them (wrong C++ destruction order). TSAN/ASAN catch this.
TEST(TtsPlayerDestructionTest, DestroyDuringActiveRenderIsNotUAF) {
  base::test::TaskEnvironment task_environment;
  const media::AudioParameters params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Mono(), kDefaultSampleRate,
      kDefaultBufferSize);

  RealPipeAudioStreamFactory real_pipe_factory;
  mojo::Receiver<media::mojom::AudioStreamFactory> factory_receiver(
      &real_pipe_factory);

  auto player = std::make_unique<TtsPlayer>(
      factory_receiver.BindNewPipeAndPassRemote(), params);

  // Let mojo deliver the data pipe so the real audio thread starts.
  task_environment.RunUntilIdle();

  // Many small buffers keep Render() busy long enough to race with destruction.
  for (int i = 0; i < 10000; i++) {
    TtsPlayer::AudioBuffer buffer;
    buffer.frames = {0.5f};
    buffer.status = 1;
    buffer.char_index = -1;
    player->AddAudioBuffer(std::move(buffer));
  }

  real_pipe_factory.SignalRender();
  player.reset();
}

}  // namespace
}  // namespace tts
}  // namespace chromeos
