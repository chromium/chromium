// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/services/tts/tts_player.h"

#include "chromeos/services/tts/constants.h"
#include "chromeos/services/tts/tts_test_utils.h"
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
  std::vector<float> actual(bus->channel(0), bus->channel(0) + 1);
  std::vector<float> expected = {0.7};
  EXPECT_THAT(actual, testing::ElementsAreArray(expected));
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
  std::vector<float> actual(bus->channel(0), bus->channel(0) + 5);
  std::vector<float> expected = {0.1, 0.2, 0.3, 0.4, 0.5};
  EXPECT_THAT(actual, testing::ElementsAreArray(expected));
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
  std::vector<float> actual(bus->channel(0), bus->channel(0) + 5);
  std::vector<float> expected = {0.1, 0.2, 0.3, 0.4, 0.5};
  EXPECT_THAT(actual, testing::ElementsAreArray(expected));
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
  std::vector<float> actual(first_bus->channel(0), first_bus->channel(0) + 5);
  std::vector<float> expected = {0.1, 0.2, 0.3, 0.4, 0.5};
  EXPECT_THAT(actual, testing::ElementsAreArray(expected));

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
  actual =
      std::vector<float>(second_bus->channel(0), second_bus->channel(0) + 2);
  expected = {0.6, 0.7};
  EXPECT_THAT(actual, testing::ElementsAreArray(expected));
}

}  // namespace
}  // namespace tts
}  // namespace chromeos
