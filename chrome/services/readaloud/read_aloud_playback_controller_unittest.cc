// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/read_aloud_playback_controller.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/dcheck_is_on.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sync_socket.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/common/readaloud/read_aloud.mojom.h"
#include "components/optimization_guide/proto/features/read_aloud_synthesize.pb.h"
#include "media/base/audio_parameters.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_features.mojom-features.h"

namespace readaloud {

namespace {

class MockReadAloudPlaybackControllerClient
    : public read_aloud::mojom::ReadAloudPlaybackControllerClient {
 public:
  MockReadAloudPlaybackControllerClient() = default;
  ~MockReadAloudPlaybackControllerClient() override = default;

  mojo::PendingRemote<read_aloud::mojom::ReadAloudPlaybackControllerClient>
  BindAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void ResetReceiver() { receiver_.reset(); }

  // read_aloud::mojom::ReadAloudPlaybackControllerClient:
  void OnPlaybackStateChanged(read_aloud::mojom::PlaybackState state) override {
    last_state_ = state;
    if (state_changed_closure_ &&
        (!expected_state_to_wait_for_.has_value() ||
         state == expected_state_to_wait_for_.value())) {
      std::move(state_changed_closure_).Run();
    }
  }

  void OnPlaybackDurationChanged(base::TimeDelta /*duration*/) override {
    // No-op for testing.
  }

  void OnWordBoundaryReached(uint32_t /*segment_index*/,
                             uint32_t /*character_offset*/,
                             base::TimeDelta /*audio_timestamp*/) override {
    // No-op for testing.
  }

  using SpeechSynthesisHandler = base::RepeatingCallback<void(
      const std::u16string&, uint64_t, RequestSpeechSynthesisCallback)>;

  void set_synthesis_handler(SpeechSynthesisHandler handler) {
    synthesis_handler_ = std::move(handler);
  }

  void RequestSpeechSynthesis(
      const std::u16string& text_chunk,
      uint64_t sequence_id,
      RequestSpeechSynthesisCallback callback) override {
    if (synthesis_handler_) {
      synthesis_handler_.Run(text_chunk, sequence_id, std::move(callback));
      return;
    }
    std::move(callback).Run(mojo_base::BigBuffer(), true);
  }

  void OnTextChunked(const std::vector<std::u16string>& chunks) override {
    last_chunks_ = chunks;
    if (chunks_closure_) {
      std::move(chunks_closure_).Run();
    }
  }

  void ClearLastState() { last_state_.reset(); }

  void WaitForStateChange(read_aloud::mojom::PlaybackState expected_state) {
    if (last_state_.has_value() && last_state_.value() == expected_state)
      return;
    expected_state_to_wait_for_ = expected_state;
    while (!last_state_.has_value() || last_state_.value() != expected_state) {
      base::RunLoop run_loop;
      state_changed_closure_ = run_loop.QuitClosure();
      run_loop.Run();
    }
    expected_state_to_wait_for_.reset();
    EXPECT_EQ(last_state_.value(), expected_state);
  }

  void WaitForChunks() {
    if (last_chunks_.has_value()) {
      return;
    }
    base::RunLoop run_loop;
    chunks_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void WaitForDisconnect() {
    if (!receiver_.is_bound())
      return;
    base::RunLoop run_loop;
    receiver_.set_disconnect_handler(run_loop.QuitClosure());
    run_loop.Run();
    receiver_.reset();
  }

  std::optional<read_aloud::mojom::PlaybackState> last_state() const {
    return last_state_;
  }

  const std::optional<std::vector<std::u16string>>& last_chunks() const {
    return last_chunks_;
  }

 private:
  mojo::Receiver<read_aloud::mojom::ReadAloudPlaybackControllerClient>
      receiver_{this};
  std::optional<read_aloud::mojom::PlaybackState> last_state_;
  std::optional<read_aloud::mojom::PlaybackState> expected_state_to_wait_for_;
  base::OnceClosure state_changed_closure_;
  std::optional<std::vector<std::u16string>> last_chunks_;
  base::OnceClosure chunks_closure_;
  SpeechSynthesisHandler synthesis_handler_;
};

}  // namespace

class ReadAloudPlaybackControllerTest : public testing::Test {
 public:
  ReadAloudPlaybackControllerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitAndEnableFeature(
        ax::mojom::features::kReadAloudNative);
  }

  void SetUp() override {
    testing::FLAGS_gtest_death_test_style = "threadsafe";
    ResetRemotes();
    controller_impl_ = std::make_unique<ReadAloudPlaybackController>(
        factory_remote_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override {
    ResetRemotes();
  }

  void ResetRemotes() {
    if (controller_remote_.is_bound() && controller_remote_.is_connected()) {
      base::RunLoop run_loop;
      controller_remote_.set_disconnect_handler(run_loop.QuitClosure());
      controller_impl_.reset();
      run_loop.Run();
    }
    controller_impl_.reset();
    mock_client_.reset();
    controller_remote_.reset();
    factory_remote_.reset();
  }

  void CreateSession() {
    mock_client_ = std::make_unique<MockReadAloudPlaybackControllerClient>();
    factory_remote_->CreateController(
        controller_remote_.BindNewPipeAndPassReceiver(),
        mock_client_->BindAndGetRemote());
    factory_remote_.FlushForTesting();
  }

  void SetMockSynthesisResponse(const std::string& response_bytes) {
    mock_client_->set_synthesis_handler(base::BindRepeating(
        [](const std::string& bytes, const std::u16string&, uint64_t,
           read_aloud::mojom::ReadAloudPlaybackControllerClient::
               RequestSpeechSynthesisCallback callback) {
          std::move(callback).Run(
              mojo_base::BigBuffer(base::as_byte_span(bytes)), true);
        },
        response_bytes));
  }

  media::mojom::ReadWriteAudioDataPipePtr CreateValidDataPipe(
      const media::AudioParameters& params,
      base::CancelableSyncSocket* local_socket) {
    uint32_t buffer_size = media::ComputeAudioOutputBufferSize(params);
    base::UnsafeSharedMemoryRegion shared_memory_region =
        base::UnsafeSharedMemoryRegion::Create(buffer_size);
    if (!shared_memory_region.IsValid()) {
      return nullptr;
    }

    {
      base::WritableSharedMemoryMapping mapping = shared_memory_region.Map();
      if (!mapping.IsValid()) {
        return nullptr;
      }
      base::span<uint8_t> span = mapping.GetMemoryAsSpan<uint8_t>(buffer_size);
      std::fill(span.begin(), span.end(), 0);
    }

    base::CancelableSyncSocket foreign_socket;
    if (!base::CancelableSyncSocket::CreatePair(local_socket,
                                                &foreign_socket)) {
      return nullptr;
    }

    return media::mojom::ReadWriteAudioDataPipe::New(
        std::move(shared_memory_region),
        mojo::PlatformHandle(foreign_socket.Take()));
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  mojo::Remote<read_aloud::mojom::ReadAloudPlaybackControllerFactory>
      factory_remote_;
  mojo::Remote<read_aloud::mojom::ReadAloudPlaybackController>
      controller_remote_;
  std::unique_ptr<MockReadAloudPlaybackControllerClient> mock_client_;
  std::unique_ptr<ReadAloudPlaybackController> controller_impl_;
};

TEST_F(ReadAloudPlaybackControllerTest, CreateControllerSuccessfulBinding) {
  CreateSession();
  EXPECT_TRUE(controller_remote_.is_bound());
  EXPECT_TRUE(controller_remote_.is_connected());
}

TEST_F(ReadAloudPlaybackControllerTest, CreateControllerBothHandlesInvalidReportsBadMessage) {
  mojo::test::BadMessageObserver bad_message_observer;
  factory_remote_->CreateController(
      mojo::PendingReceiver<read_aloud::mojom::ReadAloudPlaybackController>(),
      mojo::PendingRemote<read_aloud::mojom::ReadAloudPlaybackControllerClient>());
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "ReadAloudPlaybackController: CreateController requires both "
            "controller and client handles to be valid");
}

TEST_F(ReadAloudPlaybackControllerTest, CreateControllerOneHandleInvalidReportsBadMessage) {
  mojo::test::BadMessageObserver bad_message_observer;
  factory_remote_->CreateController(
      controller_remote_.BindNewPipeAndPassReceiver(),
      mojo::PendingRemote<read_aloud::mojom::ReadAloudPlaybackControllerClient>());
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "ReadAloudPlaybackController: CreateController requires both "
            "controller and client handles to be valid");
}

TEST_F(ReadAloudPlaybackControllerTest, ClientDisconnectResetsControllerAndState) {
  CreateSession();
  controller_remote_->SetPlaybackRate(3.0f);
  controller_remote_.FlushForTesting();

  // Disconnect the client remote.
  mock_client_->ResetReceiver();
  controller_remote_.FlushForTesting();

  // The controller receiver in utility process should disconnect when client drops.
  EXPECT_FALSE(controller_remote_.is_connected());
}

TEST_F(ReadAloudPlaybackControllerTest, ControllerDisconnectResetsClientAndState) {
  CreateSession();
  controller_remote_.reset();
  mock_client_->WaitForDisconnect();
}

TEST_F(ReadAloudPlaybackControllerTest, SetTextContentNotifiesClientPaused) {
  CreateSession();
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  auto seg = read_aloud::mojom::TextSegment::New();
  seg->segment_index = 0;
  seg->text = u"Hello Chromium read aloud world.";
  segments.push_back(std::move(seg));

  controller_remote_->SetTextContent(std::move(segments));
  mock_client_->WaitForStateChange(read_aloud::mojom::PlaybackState::kPaused);
}

TEST_F(ReadAloudPlaybackControllerTest, SetTextContentRoutesOnTextChunkedIPC) {
  CreateSession();
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  auto seg = read_aloud::mojom::TextSegment::New();
  seg->segment_index = 0;
  seg->text = u"First sentence. Second sentence!";
  segments.push_back(std::move(seg));

  controller_remote_->SetTextContent(std::move(segments));
  mock_client_->WaitForChunks();

  ASSERT_TRUE(mock_client_->last_chunks().has_value());
  const std::vector<std::u16string>& chunks =
      mock_client_->last_chunks().value();
  EXPECT_THAT(chunks,
              testing::ElementsAre(u"First sentence.", u"Second sentence!"));
}

TEST_F(ReadAloudPlaybackControllerTest, SetTextContentNotMonotonicallyIncreasingReportsBadMessage) {
  CreateSession();
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  {
    auto seg = read_aloud::mojom::TextSegment::New();
    seg->segment_index = 5;
    seg->text = u"First segment";
    segments.push_back(std::move(seg));
  }
  {
    auto seg = read_aloud::mojom::TextSegment::New();
    seg->segment_index = 3;  // Decreasing index
    seg->text = u"Second segment (invalid)";
    segments.push_back(std::move(seg));
  }

  mojo::test::BadMessageObserver bad_message_observer;
  controller_remote_->SetTextContent(std::move(segments));
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "ReadAloudPlaybackController: segment_index must be monotonically increasing "
            "in SetTextContent");
}

TEST_F(ReadAloudPlaybackControllerTest, SetTextContentGapsAreValid) {
  CreateSession();
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  {
    auto seg = read_aloud::mojom::TextSegment::New();
    seg->segment_index = 2;
    seg->text = u"First segment";
    segments.push_back(std::move(seg));
  }
  {
    auto seg = read_aloud::mojom::TextSegment::New();
    seg->segment_index = 5;  // Gap is allowed (2 -> 5)
    seg->text = u"Second segment";
    segments.push_back(std::move(seg));
  }

  controller_remote_->SetTextContent(std::move(segments));
  mock_client_->WaitForStateChange(read_aloud::mojom::PlaybackState::kPaused);

  // Seeking to an existing segment with a gap index (5) should succeed.
  controller_remote_->SeekToWord(5, 0);
  controller_remote_.FlushForTesting();
  EXPECT_TRUE(controller_remote_.is_connected());

  // Seeking to an unsent index in the gap (3) should report a BadMessage.
  mojo::test::BadMessageObserver bad_message_observer;
  controller_remote_->SeekToWord(3, 0);
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "ReadAloudPlaybackController: Invalid segment_index in SeekToWord");
}

TEST_F(ReadAloudPlaybackControllerTest, SeekToWordEmptyTextDoesNotCrashOnZeroOffset) {
  CreateSession();
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  auto seg = read_aloud::mojom::TextSegment::New();
  seg->segment_index = 0;
  seg->text = u"";  // Empty text segment
  segments.push_back(std::move(seg));

  controller_remote_->SetTextContent(std::move(segments));
  controller_remote_.FlushForTesting();

  // Seeking to 0 on an empty segment should NOT report bad message or crash.
  controller_remote_->SeekToWord(0, 0);
  controller_remote_.FlushForTesting();
  EXPECT_TRUE(controller_remote_.is_connected());
}

TEST_F(ReadAloudPlaybackControllerTest, SeekToWordEndOfStringIsValid) {
  CreateSession();
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  auto seg = read_aloud::mojom::TextSegment::New();
  seg->segment_index = 0;
  seg->text = u"Chromium";
  segments.push_back(std::move(seg));

  controller_remote_->SetTextContent(std::move(segments));
  controller_remote_.FlushForTesting();

  // Seeking to offset 8 (end of segment of length 8) is valid and must not disconnect.
  controller_remote_->SeekToWord(0, 8);
  controller_remote_.FlushForTesting();
  EXPECT_TRUE(controller_remote_.is_connected());
}

TEST_F(ReadAloudPlaybackControllerTest, SeekToWordOutOfBoundsReportsBadMessage) {
  CreateSession();
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  auto seg = read_aloud::mojom::TextSegment::New();
  seg->segment_index = 0;
  seg->text = u"Chromium";
  segments.push_back(std::move(seg));

  controller_remote_->SetTextContent(std::move(segments));
  controller_remote_.FlushForTesting();

  mojo::test::BadMessageObserver bad_message_observer;
  controller_remote_->SeekToWord(0, 100);  // strictly > text size (8)
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "ReadAloudPlaybackController: Invalid character_offset in SeekToWord");
}

TEST_F(ReadAloudPlaybackControllerTest, SetPlaybackRateInvalidOrNegativeReportsBadMessage) {
  CreateSession();
  mojo::test::BadMessageObserver bad_message_observer;
  controller_remote_->SetPlaybackRate(-1.0f);
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "ReadAloudPlaybackController: Invalid playback rate (must be finite and > 0.0)");
}

TEST_F(ReadAloudPlaybackControllerTest, SetPlaybackRateNaNReportsBadMessage) {
  CreateSession();
  mojo::test::BadMessageObserver bad_message_observer;
  controller_remote_->SetPlaybackRate(std::numeric_limits<float>::quiet_NaN());
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "ReadAloudPlaybackController: Invalid playback rate (must be finite and > 0.0)");
}

TEST_F(ReadAloudPlaybackControllerTest, SetPlaybackRateClampsBelowMinimum) {
  CreateSession();
  controller_remote_->SetPlaybackRate(0.1f);
  controller_remote_.FlushForTesting();

  EXPECT_FLOAT_EQ(controller_impl_->playback_rate(), kMinPlaybackRate);
  EXPECT_TRUE(controller_remote_.is_connected());
}

TEST_F(ReadAloudPlaybackControllerTest, SetPlaybackRateClampsAboveMaximum) {
  CreateSession();
  controller_remote_->SetPlaybackRate(10.0f);
  controller_remote_.FlushForTesting();

  EXPECT_FLOAT_EQ(controller_impl_->playback_rate(), kMaxPlaybackRate);
  EXPECT_TRUE(controller_remote_.is_connected());
}

TEST_F(ReadAloudPlaybackControllerTest, SetPlaybackRateValidWithinRange) {
  CreateSession();
  controller_remote_->SetPlaybackRate(1.5f);
  controller_remote_.FlushForTesting();

  EXPECT_FLOAT_EQ(controller_impl_->playback_rate(), 1.5f);
  EXPECT_TRUE(controller_remote_.is_connected());
}

TEST_F(ReadAloudPlaybackControllerTest, SeekToTimeNegativeReportsBadMessage) {
  CreateSession();
  mojo::test::BadMessageObserver bad_message_observer;
  controller_remote_->SeekToTime(base::Seconds(-5));
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "ReadAloudPlaybackController: Invalid position in SeekToTime");
}

TEST_F(ReadAloudPlaybackControllerTest, SeekToTimeMaxReportsBadMessage) {
  CreateSession();
  mojo::test::BadMessageObserver bad_message_observer;
  controller_remote_->SeekToTime(base::TimeDelta::Max());
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "ReadAloudPlaybackController: Invalid position in SeekToTime");
}

TEST_F(ReadAloudPlaybackControllerTest,
       FactoryDisconnectTriggersReceiverTeardownAfterSessionReset) {
  CreateSession();
  EXPECT_TRUE(factory_remote_.is_connected());
  EXPECT_TRUE(controller_remote_.is_connected());

  // Disconnect session remote, triggering ResetSession() on controller.
  controller_remote_.reset();
  mock_client_->WaitForDisconnect();

  // Disconnect factory remote; OnReceiverDisconnected should STILL fire cleanly
  // because it uses factory_weak_factory_ (which is not invalidated by session resets).
  factory_remote_.reset();
}

TEST_F(ReadAloudPlaybackControllerTest, SetTextContentTooManySegmentsReportsBadMessage) {
  CreateSession();
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  // kMaxTextSegments is 1,000. Let's create 1,001 segments.
  for (size_t i = 0; i <= 1000; ++i) {
    auto seg = read_aloud::mojom::TextSegment::New();
    seg->segment_index = i;
    seg->text = u"A";
    segments.push_back(std::move(seg));
  }

  mojo::test::BadMessageObserver bad_message_observer;
  controller_remote_->SetTextContent(std::move(segments));
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "ReadAloudPlaybackController: Too many segments in SetTextContent");
}

TEST_F(ReadAloudPlaybackControllerTest, SetTextContentSegmentTooLongReportsBadMessage) {
  CreateSession();
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  auto seg = read_aloud::mojom::TextSegment::New();
  seg->segment_index = 0;
  // kMaxTextLengthPerSegment is 65,536. Let's create a segment with 65,537 characters.
  seg->text = std::u16string(65537, u'A');
  segments.push_back(std::move(seg));

  mojo::test::BadMessageObserver bad_message_observer;
  controller_remote_->SetTextContent(std::move(segments));
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "ReadAloudPlaybackController: TextSegment length exceeds limit in "
            "SetTextContent");
}

TEST_F(ReadAloudPlaybackControllerTest, SetTextContentTotalPayloadExceedsLimitReportsBadMessage) {
  CreateSession();
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  // kMaxMojoPayloadSizeBytes is 512,000 bytes (256,000 UTF-16 characters).
  // We bypass the 65,536 limit per segment by sending 6 segments of 50,000 characters.
  // Total: 300,000 characters (600,000 bytes).
  for (size_t i = 0; i < 5; ++i) {
    auto seg = read_aloud::mojom::TextSegment::New();
    seg->segment_index = i;
    seg->text = std::u16string(50000, u'A');
    segments.push_back(std::move(seg));
  }
  {
    auto seg = read_aloud::mojom::TextSegment::New();
    seg->segment_index = 5;
    seg->text = std::u16string(10000, u'A');
    segments.push_back(std::move(seg));
  }

  mojo::test::BadMessageObserver bad_message_observer;
  controller_remote_->SetTextContent(std::move(segments));
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "ReadAloudPlaybackController: Total text payload exceeds safety limit "
            "in SetTextContent");
}

TEST_F(ReadAloudPlaybackControllerTest, InitializeAudioSuccess) {
  CreateSession();

  mojo::PendingRemote<media::mojom::AudioOutputStream> stream;
  // Keep the receiver alive on the stack to prevent the Mojo pipe from
  // immediately disconnecting during the test.
  mojo::PendingReceiver<media::mojom::AudioOutputStream> stream_receiver =
      stream.InitWithNewPipeAndPassReceiver();

  const media::AudioParameters params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Mono(), /*sample_rate=*/48000,
      /*frames_per_buffer=*/480);

  base::CancelableSyncSocket local_socket;
  media::mojom::ReadWriteAudioDataPipePtr data_pipe =
      CreateValidDataPipe(params, &local_socket);
  ASSERT_TRUE(data_pipe);

  controller_remote_->InitializeAudio(std::move(stream), std::move(data_pipe),
                                      params);
  controller_remote_.FlushForTesting();

  EXPECT_TRUE(controller_remote_.is_connected());
}

TEST_F(ReadAloudPlaybackControllerTest,
       InitializeAudioInvalidAudioParamsReportsBadMessage) {
  CreateSession();

  mojo::PendingRemote<media::mojom::AudioOutputStream> stream;
  mojo::PendingReceiver<media::mojom::AudioOutputStream> stream_receiver =
      stream.InitWithNewPipeAndPassReceiver();

  // Invalid sample rate (0)
  const media::AudioParameters params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Mono(), /*sample_rate=*/0,
      /*frames_per_buffer=*/480);

  base::CancelableSyncSocket local_socket;
  const media::AudioParameters valid_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Mono(), /*sample_rate=*/48000,
      /*frames_per_buffer=*/480);
  media::mojom::ReadWriteAudioDataPipePtr data_pipe =
      CreateValidDataPipe(valid_params, &local_socket);
  ASSERT_TRUE(data_pipe);

  mojo::test::BadMessageObserver bad_message_observer;
  controller_remote_->InitializeAudio(std::move(stream), std::move(data_pipe),
                                      params);
  std::string bad_message = bad_message_observer.WaitForBadMessage();
  EXPECT_TRUE(bad_message.find("VALIDATION_ERROR_DESERIALIZATION_FAILED") !=
                  std::string::npos ||
              bad_message.find(
                  "ReadAloudPlaybackController: Invalid audio parameters") !=
                  std::string::npos);
}

TEST_F(ReadAloudPlaybackControllerTest,
       InitializeAudioBitstreamFormatReportsBadMessage) {
  CreateSession();

  mojo::PendingRemote<media::mojom::AudioOutputStream> stream;
  mojo::PendingReceiver<media::mojom::AudioOutputStream> stream_receiver =
      stream.InitWithNewPipeAndPassReceiver();

  // Bitstream format
  const media::AudioParameters params(
      media::AudioParameters::AUDIO_BITSTREAM_AC3,
      media::ChannelLayoutConfig::Mono(), /*sample_rate=*/48000,
      /*frames_per_buffer=*/480);

  base::CancelableSyncSocket local_socket;
  const media::AudioParameters valid_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Mono(), /*sample_rate=*/48000,
      /*frames_per_buffer=*/480);
  media::mojom::ReadWriteAudioDataPipePtr data_pipe =
      CreateValidDataPipe(valid_params, &local_socket);
  ASSERT_TRUE(data_pipe);

  mojo::test::BadMessageObserver bad_message_observer;
  controller_remote_->InitializeAudio(std::move(stream), std::move(data_pipe),
                                      params);
  std::string bad_message = bad_message_observer.WaitForBadMessage();
  EXPECT_TRUE(bad_message.find("VALIDATION_ERROR_DESERIALIZATION_FAILED") !=
                  std::string::npos ||
              bad_message.find(
                  "ReadAloudPlaybackController: Invalid audio parameters") !=
                  std::string::npos);
}

TEST_F(ReadAloudPlaybackControllerTest,
       InitializeAudioInvalidStreamRemoteReportsBadMessage) {
  CreateSession();

  // Invalid stream remote (null / default constructed)
  mojo::PendingRemote<media::mojom::AudioOutputStream> stream;

  const media::AudioParameters params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Mono(), /*sample_rate=*/48000,
      /*frames_per_buffer=*/480);

  base::CancelableSyncSocket local_socket;
  media::mojom::ReadWriteAudioDataPipePtr data_pipe =
      CreateValidDataPipe(params, &local_socket);
  ASSERT_TRUE(data_pipe);

#if DCHECK_IS_ON()
  // In debug builds, Mojo client-side validation will crash the process before
  // the message is sent because `stream` is non-nullable.
  EXPECT_DEATH(controller_remote_->InitializeAudio(
                   std::move(stream), std::move(data_pipe), params),
               "");
#else
  mojo::test::BadMessageObserver bad_message_observer;
  controller_remote_->InitializeAudio(std::move(stream), std::move(data_pipe),
                                      params);
  std::string bad_message = bad_message_observer.WaitForBadMessage();
  EXPECT_TRUE(
      bad_message.find("VALIDATION_ERROR_UNEXPECTED_INVALID_HANDLE") !=
          std::string::npos ||
      bad_message.find(
          "ReadAloudPlaybackController: Invalid audio output stream remote") !=
          std::string::npos);
#endif
}

TEST_F(ReadAloudPlaybackControllerTest,
       InitializeAudioNullDataPipeReportsBadMessage) {
  CreateSession();

  mojo::PendingRemote<media::mojom::AudioOutputStream> stream;
  mojo::PendingReceiver<media::mojom::AudioOutputStream> stream_receiver =
      stream.InitWithNewPipeAndPassReceiver();

  const media::AudioParameters params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Mono(), /*sample_rate=*/48000,
      /*frames_per_buffer=*/480);

#if DCHECK_IS_ON()
  EXPECT_DEATH(
      controller_remote_->InitializeAudio(std::move(stream), nullptr, params),
      "");
#else
  mojo::test::BadMessageObserver bad_message_observer;
  controller_remote_->InitializeAudio(std::move(stream), nullptr, params);
  std::string bad_message = bad_message_observer.WaitForBadMessage();
  EXPECT_TRUE(
      bad_message.find("VALIDATION_ERROR_UNEXPECTED_NULL_POINTER") !=
          std::string::npos ||
      bad_message.find(
          "ReadAloudPlaybackController: Invalid data pipe or handles") !=
          std::string::npos);
#endif
}

TEST_F(ReadAloudPlaybackControllerTest,
       InitializeAudioInvalidSocketReportsBadMessage) {
  CreateSession();

  mojo::PendingRemote<media::mojom::AudioOutputStream> stream;
  mojo::PendingReceiver<media::mojom::AudioOutputStream> stream_receiver =
      stream.InitWithNewPipeAndPassReceiver();

  const media::AudioParameters params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Mono(), /*sample_rate=*/48000,
      /*frames_per_buffer=*/480);

  // Create a data pipe with invalid socket.
  uint32_t buffer_size = media::ComputeAudioOutputBufferSize(params);
  base::UnsafeSharedMemoryRegion shared_memory_region =
      base::UnsafeSharedMemoryRegion::Create(buffer_size);
  ASSERT_TRUE(shared_memory_region.IsValid());

  media::mojom::ReadWriteAudioDataPipePtr data_pipe =
      media::mojom::ReadWriteAudioDataPipe::New(
          std::move(shared_memory_region),
          mojo::PlatformHandle());  // Invalid handle

#if DCHECK_IS_ON()
  EXPECT_DEATH(controller_remote_->InitializeAudio(
                   std::move(stream), std::move(data_pipe), params),
               "");
#else
  mojo::test::BadMessageObserver bad_message_observer;
  controller_remote_->InitializeAudio(std::move(stream), std::move(data_pipe),
                                      params);
  std::string bad_message = bad_message_observer.WaitForBadMessage();
  EXPECT_TRUE(
      bad_message.find("VALIDATION_ERROR_UNEXPECTED_INVALID_HANDLE") !=
          std::string::npos ||
      bad_message.find(
          "ReadAloudPlaybackController: Invalid data pipe or handles") !=
          std::string::npos);
#endif
}

TEST_F(ReadAloudPlaybackControllerTest,
       InitializeAudioInvalidSharedMemoryReportsBadMessage) {
  CreateSession();

  mojo::PendingRemote<media::mojom::AudioOutputStream> stream;
  mojo::PendingReceiver<media::mojom::AudioOutputStream> stream_receiver =
      stream.InitWithNewPipeAndPassReceiver();

  const media::AudioParameters params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Mono(), /*sample_rate=*/48000,
      /*frames_per_buffer=*/480);

  base::CancelableSyncSocket local_socket;
  base::CancelableSyncSocket foreign_socket;
  ASSERT_TRUE(
      base::CancelableSyncSocket::CreatePair(&local_socket, &foreign_socket));

  // Create a data pipe with invalid shared memory.
  media::mojom::ReadWriteAudioDataPipePtr data_pipe =
      media::mojom::ReadWriteAudioDataPipe::New(
          base::UnsafeSharedMemoryRegion(),  // Invalid shmem
          mojo::PlatformHandle(foreign_socket.Take()));

#if DCHECK_IS_ON()
  EXPECT_DEATH(controller_remote_->InitializeAudio(
                   std::move(stream), std::move(data_pipe), params),
               "");
#else
  mojo::test::BadMessageObserver bad_message_observer;
  controller_remote_->InitializeAudio(std::move(stream), std::move(data_pipe),
                                      params);
  std::string bad_message = bad_message_observer.WaitForBadMessage();
  EXPECT_TRUE(
      bad_message.find("VALIDATION_ERROR_UNEXPECTED_NULL_POINTER") !=
          std::string::npos ||
      bad_message.find("VALIDATION_ERROR_") != std::string::npos ||
      bad_message.find(
          "ReadAloudPlaybackController: Invalid data pipe or handles") !=
          std::string::npos);
#endif
}

TEST_F(ReadAloudPlaybackControllerTest,
       InitializeAudioSharedMemoryTooSmallReportsBadMessage) {
  CreateSession();

  mojo::PendingRemote<media::mojom::AudioOutputStream> stream;
  mojo::PendingReceiver<media::mojom::AudioOutputStream> stream_receiver =
      stream.InitWithNewPipeAndPassReceiver();

  const media::AudioParameters params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Mono(), /*sample_rate=*/48000,
      /*frames_per_buffer=*/480);

  // Required size: media::ComputeAudioOutputBufferSize(params)
  // Let's make it smaller.
  uint32_t required_buffer_size = media::ComputeAudioOutputBufferSize(params);
  ASSERT_GT(required_buffer_size, 0u);

  uint32_t smaller_buffer_size = required_buffer_size - 1;
  base::UnsafeSharedMemoryRegion shared_memory_region =
      base::UnsafeSharedMemoryRegion::Create(smaller_buffer_size);
  ASSERT_TRUE(shared_memory_region.IsValid());

  base::CancelableSyncSocket local_socket;
  base::CancelableSyncSocket foreign_socket;
  ASSERT_TRUE(
      base::CancelableSyncSocket::CreatePair(&local_socket, &foreign_socket));

  media::mojom::ReadWriteAudioDataPipePtr data_pipe =
      media::mojom::ReadWriteAudioDataPipe::New(
          std::move(shared_memory_region),
          mojo::PlatformHandle(foreign_socket.Take()));

  mojo::test::BadMessageObserver bad_message_observer;
  controller_remote_->InitializeAudio(std::move(stream), std::move(data_pipe),
                                      params);
  std::string bad_message = bad_message_observer.WaitForBadMessage();
  EXPECT_TRUE(
      bad_message.find("VALIDATION_ERROR_") != std::string::npos ||
      bad_message.find(
          "ReadAloudPlaybackController: Shared memory size is too small") !=
          std::string::npos);
}

TEST_F(ReadAloudPlaybackControllerTest, OnSpeechSynthesisResponseValidProtobufDecodesAndTriggersWordBoundaries) {
  CreateSession();

  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  auto seg = read_aloud::mojom::TextSegment::New();
  seg->segment_index = 0;
  seg->text = u"First sentence. Second sentence.";
  segments.push_back(std::move(seg));

  controller_remote_->SetTextContent(std::move(segments));
  controller_remote_.FlushForTesting();

  optimization_guide::proto::ReadAloudSynthesizeResponse response;
  response.set_audio_bytes("fake_opus_audio_data");
  auto* timing1 = response.add_timings();
  timing1->set_start_offset(0);
  timing1->set_end_offset(14);
  timing1->set_time_offset_ms(0);

  std::string serialized;
  ASSERT_TRUE(response.SerializeToString(&serialized));

  SetMockSynthesisResponse(serialized);

  controller_remote_->Play();
  controller_remote_.FlushForTesting();

  EXPECT_TRUE(controller_remote_.is_connected());
}

TEST_F(ReadAloudPlaybackControllerTest, OnSpeechSynthesisResponseMalformedProtobufRecoversWithoutStalling) {
  CreateSession();

  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  auto seg = read_aloud::mojom::TextSegment::New();
  seg->segment_index = 0;
  seg->text = u"Test sentence.";
  segments.push_back(std::move(seg));

  controller_remote_->SetTextContent(std::move(segments));
  controller_remote_.FlushForTesting();

  SetMockSynthesisResponse("not_a_valid_protobuf_payload");

  controller_remote_->Play();
  controller_remote_.FlushForTesting();

  // Engine should recover smoothly without disconnecting channel or stalling.
  EXPECT_TRUE(controller_remote_.is_connected());
}

TEST_F(ReadAloudPlaybackControllerTest, OnSpeechSynthesisResponseOutOfOrderResponsesDeliveredInOrder) {
  CreateSession();

  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  auto seg1 = read_aloud::mojom::TextSegment::New();
  seg1->segment_index = 0;
  seg1->text = u"First sentence.";
  segments.push_back(std::move(seg1));

  auto seg2 = read_aloud::mojom::TextSegment::New();
  seg2->segment_index = 1;
  seg2->text = u"Second sentence.";
  segments.push_back(std::move(seg2));

  controller_remote_->SetTextContent(std::move(segments));
  controller_remote_.FlushForTesting();

  optimization_guide::proto::ReadAloudSynthesizeResponse response;
  response.set_audio_bytes("audio_data");
  std::string serialized;
  response.SerializeToString(&serialized);

  SetMockSynthesisResponse(serialized);

  controller_remote_->Play();
  controller_remote_.FlushForTesting();

  EXPECT_TRUE(controller_remote_.is_connected());
}

TEST_F(ReadAloudPlaybackControllerTest, SetTextContentEmptySegmentsValidateSequenceCorrectly) {
  CreateSession();

  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  auto seg1 = read_aloud::mojom::TextSegment::New();
  seg1->segment_index = 5;
  seg1->text = u"Valid segment.";
  segments.push_back(std::move(seg1));

  // Empty segment with out-of-order index 2 (must trigger BadMessage)
  auto seg2 = read_aloud::mojom::TextSegment::New();
  seg2->segment_index = 2;
  seg2->text = u"";
  segments.push_back(std::move(seg2));

  mojo::test::BadMessageObserver bad_message_observer;
  controller_remote_->SetTextContent(std::move(segments));
  controller_remote_.FlushForTesting();

  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "ReadAloudPlaybackController: segment_index must be monotonically increasing in SetTextContent");
}

}  // namespace readaloud
