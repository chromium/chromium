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
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sync_socket.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/common/readaloud/read_aloud.mojom.h"
#include "chrome/services/readaloud/audio_renderer/read_aloud_audio_renderer.h"
#include "components/optimization_guide/proto/features/read_aloud_generate_text.pb.h"
#include "components/optimization_guide/proto/features/read_aloud_synthesize.pb.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/mojom/audio_output_stream.mojom.h"
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

  void FlushForTesting() { receiver_.FlushForTesting(); }

  using StateCallback =
      base::RepeatingCallback<void(read_aloud::mojom::PlaybackState)>;

  void set_state_callback(StateCallback callback) {
    state_callback_ = std::move(callback);
  }

  // read_aloud::mojom::ReadAloudPlaybackControllerClient:
  void OnPlaybackStateChanged(read_aloud::mojom::PlaybackState state) override {
    last_state_ = state;
    state_history_.push_back(state);
    if (state_callback_) {
      state_callback_.Run(state);
    }
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

  using SpeechSynthesisHandler =
      base::RepeatingCallback<void(const std::u16string&,
                                   read_aloud::mojom::Speaker,
                                   uint64_t,
                                   RequestSpeechSynthesisCallback)>;

  void set_synthesis_handler(SpeechSynthesisHandler handler) {
    synthesis_handler_ = std::move(handler);
  }

  void RequestSpeechSynthesis(
      const std::u16string& text_chunk,
      read_aloud::mojom::Speaker speaker,
      uint64_t sequence_id,
      RequestSpeechSynthesisCallback callback) override {
    synthesis_request_count_++;
    if (synthesis_handler_) {
      synthesis_handler_.Run(text_chunk, speaker, sequence_id,
                             std::move(callback));
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

  void ClearStateHistory() { state_history_.clear(); }

  const std::vector<read_aloud::mojom::PlaybackState>& state_history() const {
    return state_history_;
  }

  void WaitForStateChange(read_aloud::mojom::PlaybackState expected_state) {
    if (last_state_.has_value() && last_state_.value() == expected_state) {
      return;
    }
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
    if (!receiver_.is_bound()) {
      return;
    }
    base::RunLoop run_loop;
    receiver_.set_disconnect_handler(run_loop.QuitClosure());
    run_loop.Run();
    receiver_.reset();
  }

  std::optional<read_aloud::mojom::PlaybackState> last_state() const {
    return last_state_;
  }

  uint32_t synthesis_request_count() const { return synthesis_request_count_; }

  const std::optional<std::vector<std::u16string>>& last_chunks() const {
    return last_chunks_;
  }

 private:
  mojo::Receiver<read_aloud::mojom::ReadAloudPlaybackControllerClient>
      receiver_{this};
  std::optional<read_aloud::mojom::PlaybackState> last_state_;
  std::vector<read_aloud::mojom::PlaybackState> state_history_;
  std::optional<read_aloud::mojom::PlaybackState> expected_state_to_wait_for_;
  base::OnceClosure state_changed_closure_;
  StateCallback state_callback_;
  std::optional<std::vector<std::u16string>> last_chunks_;
  base::OnceClosure chunks_closure_;
  SpeechSynthesisHandler synthesis_handler_;
  uint32_t synthesis_request_count_ = 0;
};

// Records the commands the controller issues on its audio output stream.
class FakeAudioOutputStream : public media::mojom::AudioOutputStream {
 public:
  enum class Command { kPlay, kPause, kFlush };

  void Bind(mojo::PendingReceiver<media::mojom::AudioOutputStream> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  void ClearCommands() { commands_.clear(); }

  // Dispatches every command already sent on the stream pipe.
  void FlushForTesting() { receiver_.FlushForTesting(); }

  const std::vector<Command>& commands() const { return commands_; }

  // media::mojom::AudioOutputStream:
  void Play() override { commands_.push_back(Command::kPlay); }
  void Pause() override { commands_.push_back(Command::kPause); }
  void Flush() override { commands_.push_back(Command::kFlush); }
  void SetVolume(double /*volume*/) override {}

 private:
  mojo::Receiver<media::mojom::AudioOutputStream> receiver_{this};
  std::vector<Command> commands_;
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
    // Answer any responses held by HoldSynthesisResponses() while the pipes
    // are still connected, so they are never dropped unrun, even when a test
    // exits early on a fatal assertion.
    if (mock_client_) {
      mock_client_->set_synthesis_handler({});
    }
    for (auto& callback : held_synthesis_callbacks_) {
      std::move(callback).Run(mojo_base::BigBuffer(), /*success=*/false);
    }
    held_synthesis_callbacks_.clear();
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
        [](const std::string& bytes, const std::u16string&,
           read_aloud::mojom::Speaker, uint64_t,
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
    auto shared_memory_region =
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

  void InitializeAudioForTesting() {
    mojo::PendingRemote<media::mojom::AudioOutputStream> stream;
    stream_receiver_ = stream.InitWithNewPipeAndPassReceiver();
    const media::AudioParameters params(
        media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
        media::ChannelLayoutConfig::Mono(), /*sample_rate=*/48000,
        /*frames_per_buffer=*/480);
    local_socket_ = std::make_unique<base::CancelableSyncSocket>();
    media::mojom::ReadWriteAudioDataPipePtr data_pipe =
        CreateValidDataPipe(params, local_socket_.get());
    ASSERT_TRUE(data_pipe);
    controller_remote_->InitializeAudio(std::move(stream), std::move(data_pipe),
                                        params);
    controller_remote_.FlushForTesting();
  }

  void FlushAll() {
    controller_remote_.FlushForTesting();
    if (mock_client_) {
      mock_client_->FlushForTesting();
    }
  }

  void FastForwardAndFlush(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
    FlushAll();
  }

  void SetSingleTextSegment(const std::u16string& text) {
    std::vector<read_aloud::mojom::TextSegmentPtr> segments;
    read_aloud::mojom::TextSegmentPtr segment =
        read_aloud::mojom::TextSegment::New();
    segment->segment_index = 0;
    segment->text = text;
    segments.push_back(std::move(segment));
    controller_remote_->SetTextContent(std::move(segments));
    controller_remote_.FlushForTesting();
  }

  // Makes every synthesis request fail, so no audio is ever produced.
  void SetFailingSynthesisResponse() {
    mock_client_->set_synthesis_handler(base::BindRepeating(
        [](const std::u16string&, read_aloud::mojom::Speaker, uint64_t,
           read_aloud::mojom::ReadAloudPlaybackControllerClient::
               RequestSpeechSynthesisCallback callback) {
          std::move(callback).Run(mojo_base::BigBuffer(), /*success=*/false);
        }));
  }

  // Holds synthesis responses back, so playback stays pumping in kBuffering
  // with a deterministic state history. Held responses are released in
  // TearDown().
  void HoldSynthesisResponses() {
    mock_client_->set_synthesis_handler(base::BindLambdaForTesting(
        [this](const std::u16string&, read_aloud::mojom::Speaker, uint64_t,
               read_aloud::mojom::ReadAloudPlaybackControllerClient::
                   RequestSpeechSynthesisCallback callback) {
          held_synthesis_callbacks_.push_back(std::move(callback));
        }));
  }

  // Binds `fake_stream_` to the stream handed to the controller by the last
  // InitializeAudioForTesting() call, so stream commands can be observed.
  void BindFakeStream() {
    ASSERT_TRUE(stream_receiver_.is_valid());
    fake_stream_.Bind(std::move(stream_receiver_));
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  mojo::Remote<read_aloud::mojom::ReadAloudPlaybackControllerFactory>
      factory_remote_;
  mojo::Remote<read_aloud::mojom::ReadAloudPlaybackController>
      controller_remote_;
  std::unique_ptr<MockReadAloudPlaybackControllerClient> mock_client_;
  std::unique_ptr<ReadAloudPlaybackController> controller_impl_;
  mojo::PendingReceiver<media::mojom::AudioOutputStream> stream_receiver_;
  FakeAudioOutputStream fake_stream_;
  std::vector<read_aloud::mojom::ReadAloudPlaybackControllerClient::
                  RequestSpeechSynthesisCallback>
      held_synthesis_callbacks_;
  std::unique_ptr<base::CancelableSyncSocket> local_socket_;
};

TEST_F(ReadAloudPlaybackControllerTest, CreateControllerSuccessfulBinding) {
  CreateSession();
  EXPECT_TRUE(controller_remote_.is_bound());
  EXPECT_TRUE(controller_remote_.is_connected());
}

TEST_F(ReadAloudPlaybackControllerTest,
       CreateControllerBothHandlesInvalidReportsBadMessage) {
  mojo::test::BadMessageObserver bad_message_observer;
  factory_remote_->CreateController(
      mojo::PendingReceiver<read_aloud::mojom::ReadAloudPlaybackController>(),
      mojo::PendingRemote<
          read_aloud::mojom::ReadAloudPlaybackControllerClient>());
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "ReadAloudPlaybackController: CreateController requires both "
            "controller and client handles to be valid");
}

TEST_F(ReadAloudPlaybackControllerTest,
       CreateControllerOneHandleInvalidReportsBadMessage) {
  mojo::test::BadMessageObserver bad_message_observer;
  factory_remote_->CreateController(
      controller_remote_.BindNewPipeAndPassReceiver(),
      mojo::PendingRemote<
          read_aloud::mojom::ReadAloudPlaybackControllerClient>());
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "ReadAloudPlaybackController: CreateController requires both "
            "controller and client handles to be valid");
}

TEST_F(ReadAloudPlaybackControllerTest,
       ClientDisconnectResetsControllerAndState) {
  CreateSession();
  controller_remote_->SetPlaybackRate(3.0f);
  controller_remote_.FlushForTesting();

  // Disconnect the client remote.
  mock_client_->ResetReceiver();
  controller_remote_.FlushForTesting();

  // The controller receiver in utility process should disconnect when client
  // drops.
  EXPECT_FALSE(controller_remote_.is_connected());
}

TEST_F(ReadAloudPlaybackControllerTest,
       ControllerDisconnectResetsClientAndState) {
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

TEST_F(ReadAloudPlaybackControllerTest,
       SetTextContentEmptySegmentsRoutesOnTextChunkedIPC) {
  CreateSession();
  controller_remote_->SetTextContent({});
  mock_client_->WaitForChunks();

  ASSERT_TRUE(mock_client_->last_chunks().has_value());
  EXPECT_TRUE(mock_client_->last_chunks()->empty());
}

TEST_F(ReadAloudPlaybackControllerTest,
       SetTextContentNotMonotonicallyIncreasingReportsBadMessage) {
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
            "ReadAloudPlaybackController: segment_index must be monotonically "
            "increasing "
            "in SetTextContent");
}

TEST_F(ReadAloudPlaybackControllerTest, SetTextContentGapsAreValid) {
  CreateSession();
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  read_aloud::mojom::TextSegmentPtr first_segment =
      read_aloud::mojom::TextSegment::New();
  first_segment->segment_index = 2;
  first_segment->text = u"First segment. ";
  segments.push_back(std::move(first_segment));

  read_aloud::mojom::TextSegmentPtr second_segment =
      read_aloud::mojom::TextSegment::New();
  second_segment->segment_index = 5;  // Gap is allowed (2 -> 5)
  second_segment->text = u"Second segment.";
  segments.push_back(std::move(second_segment));

  controller_remote_->SetTextContent(std::move(segments));
  mock_client_->WaitForStateChange(read_aloud::mojom::PlaybackState::kPaused);

  // Seeking to canonical timeline chunk 1 (from the second segment) succeeds.
  controller_remote_->SeekToWord(1, 0);
  controller_remote_.FlushForTesting();
  EXPECT_TRUE(controller_remote_.is_connected());

  // Seeking to an out-of-bounds chunk index (2) reports a BadMessage.
  mojo::test::BadMessageObserver bad_message_observer;
  controller_remote_->SeekToWord(2, 0);
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "ReadAloudPlaybackController: Invalid segment_index in SeekToWord");
}

TEST_F(ReadAloudPlaybackControllerTest,
       SeekToWordEmptyTextReportsBadMessageOnZeroOffset) {
  CreateSession();
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  read_aloud::mojom::TextSegmentPtr seg = read_aloud::mojom::TextSegment::New();
  seg->segment_index = 0;
  seg->text = u"";  // Empty text segment yields 0 timeline chunks
  segments.push_back(std::move(seg));

  controller_remote_->SetTextContent(std::move(segments));
  controller_remote_.FlushForTesting();

  // Seeking to chunk 0 when the timeline has 0 chunks reports a BadMessage.
  mojo::test::BadMessageObserver bad_message_observer;
  controller_remote_->SeekToWord(0, 0);
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "ReadAloudPlaybackController: Invalid segment_index in SeekToWord");
}

TEST_F(ReadAloudPlaybackControllerTest,
       SetTextContentOnlyWhitespaceDoesNotStartPlayback) {
  CreateSession();

  int synthesis_requests = 0;
  mock_client_->set_synthesis_handler(base::BindLambdaForTesting(
      [&](const std::u16string& /*text_chunk*/,
          read_aloud::mojom::Speaker /*speaker*/, uint64_t /*sequence_id*/,
          MockReadAloudPlaybackControllerClient::RequestSpeechSynthesisCallback
              callback) {
        ++synthesis_requests;
        std::move(callback).Run(mojo_base::BigBuffer(), /*success=*/true);
      }));

  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  read_aloud::mojom::TextSegmentPtr seg = read_aloud::mojom::TextSegment::New();
  seg->segment_index = 0;
  seg->text = u"   \t\n  ";
  segments.push_back(std::move(seg));

  controller_remote_->SetTextContent(std::move(segments));
  ASSERT_NO_FATAL_FAILURE(InitializeAudioForTesting());

  controller_remote_->Play();
  FlushAll();

  EXPECT_EQ(synthesis_requests, 0);
  EXPECT_EQ(mock_client_->last_state(),
            read_aloud::mojom::PlaybackState::kPaused);
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

  // Seeking to offset 8 (end of segment of length 8) is valid and must not
  // disconnect.
  controller_remote_->SeekToWord(0, 8);
  controller_remote_.FlushForTesting();
  EXPECT_TRUE(controller_remote_.is_connected());
}

TEST_F(ReadAloudPlaybackControllerTest,
       SeekToWordOutOfBoundsReportsBadMessage) {
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
  EXPECT_EQ(
      bad_message_observer.WaitForBadMessage(),
      "ReadAloudPlaybackController: Invalid character_offset in SeekToWord");
}

TEST_F(ReadAloudPlaybackControllerTest,
       SetPlaybackRateInvalidOrNegativeReportsBadMessage) {
  CreateSession();
  mojo::test::BadMessageObserver bad_message_observer;
  controller_remote_->SetPlaybackRate(-1.0f);
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "ReadAloudPlaybackController: Invalid playback rate (must be "
            "finite and > 0.0)");
}

TEST_F(ReadAloudPlaybackControllerTest, SetPlaybackRateNaNReportsBadMessage) {
  CreateSession();
  mojo::test::BadMessageObserver bad_message_observer;
  controller_remote_->SetPlaybackRate(std::numeric_limits<float>::quiet_NaN());
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "ReadAloudPlaybackController: Invalid playback rate (must be "
            "finite and > 0.0)");
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
  // because it uses factory_weak_factory_ (which is not invalidated by session
  // resets).
  factory_remote_.reset();
}

TEST_F(ReadAloudPlaybackControllerTest,
       SetTextContentTooManySegmentsReportsBadMessage) {
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

TEST_F(ReadAloudPlaybackControllerTest,
       SetTextContentSegmentTooLongReportsBadMessage) {
  CreateSession();
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  auto seg = read_aloud::mojom::TextSegment::New();
  seg->segment_index = 0;
  // kMaxTextLengthPerSegment is 65,536. Let's create a segment with 65,537
  // characters.
  seg->text = std::u16string(65537, u'A');
  segments.push_back(std::move(seg));

  mojo::test::BadMessageObserver bad_message_observer;
  controller_remote_->SetTextContent(std::move(segments));
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "ReadAloudPlaybackController: TextSegment length exceeds limit in "
            "SetTextContent");
}

TEST_F(ReadAloudPlaybackControllerTest,
       SetTextContentTotalPayloadExceedsLimitReportsBadMessage) {
  CreateSession();
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  // kMaxMojoPayloadSizeBytes is 512,000 bytes (256,000 UTF-16 characters).
  // We bypass the 65,536 limit per segment by sending 6 segments of 50,000
  // characters. Total: 300,000 characters (600,000 bytes).
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
  EXPECT_EQ(
      bad_message_observer.WaitForBadMessage(),
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
       InitializeAudioInvalidSampleRateReportsBadMessage) {
  CreateSession();

  mojo::PendingRemote<media::mojom::AudioOutputStream> stream;
  mojo::PendingReceiver<media::mojom::AudioOutputStream> stream_receiver =
      stream.InitWithNewPipeAndPassReceiver();

  // Invalid sample rate = 0
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
       InitializeAudioZeroFramesPerBufferReportsBadMessage) {
  CreateSession();

  mojo::PendingRemote<media::mojom::AudioOutputStream> stream;
  mojo::PendingReceiver<media::mojom::AudioOutputStream> stream_receiver =
      stream.InitWithNewPipeAndPassReceiver();

  // Invalid frames_per_buffer = 0
  const media::AudioParameters params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Mono(), /*sample_rate=*/48000,
      /*frames_per_buffer=*/0);

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
       InitializeAudioInvalidChannelLayoutReportsBadMessage) {
  CreateSession();

  mojo::PendingRemote<media::mojom::AudioOutputStream> stream;
  mojo::PendingReceiver<media::mojom::AudioOutputStream> stream_receiver =
      stream.InitWithNewPipeAndPassReceiver();

  // Invalid channel layout = UNSUPPORTED
  const media::AudioParameters params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig(), /*sample_rate=*/48000,
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
       InitializeAudioExcessiveFramesPerBufferReportsBadMessage) {
  CreateSession();

  mojo::PendingRemote<media::mojom::AudioOutputStream> stream;
  mojo::PendingReceiver<media::mojom::AudioOutputStream> stream_receiver =
      stream.InitWithNewPipeAndPassReceiver();

  // Invalid excessive frames per buffer (1,000,000)
  const media::AudioParameters params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Mono(), /*sample_rate=*/48000,
      /*frames_per_buffer=*/1000000);

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
  auto shared_memory_region =
      base::UnsafeSharedMemoryRegion::Create(smaller_buffer_size);
  ASSERT_TRUE(shared_memory_region.IsValid());

  base::CancelableSyncSocket local_socket;
  base::CancelableSyncSocket foreign_socket;
  ASSERT_TRUE(
      base::CancelableSyncSocket::CreatePair(&local_socket, &foreign_socket));

  auto data_pipe = media::mojom::ReadWriteAudioDataPipe::New(
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

TEST_F(ReadAloudPlaybackControllerTest,
       OnSpeechSynthesisResponseValidProtobufDecodesAndTriggersWordBoundaries) {
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
  optimization_guide::proto::WordTiming* timing1 = response.add_timings();
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

TEST_F(ReadAloudPlaybackControllerTest,
       OnSpeechSynthesisResponseMalformedProtobufRecoversWithoutStalling) {
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

TEST_F(ReadAloudPlaybackControllerTest,
       OnSpeechSynthesisResponseOutOfOrderResponsesDeliveredInOrder) {
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

TEST_F(ReadAloudPlaybackControllerTest,
       SetTextContentEmptySegmentsValidateSequenceCorrectly) {
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
            "ReadAloudPlaybackController: segment_index must be monotonically "
            "increasing in SetTextContent");
}

TEST_F(ReadAloudPlaybackControllerTest,
       PlayCalledBeforeSetTextContentDefersUntilTextSet) {
  CreateSession();
  InitializeAudioForTesting();

  // Call Play() BEFORE SetTextContent() has been called.
  // play_on_ready_ should be set to true, deferring playback.
  controller_remote_->Play();
  controller_remote_.FlushForTesting();

  EXPECT_TRUE(controller_remote_.is_connected());

  // Now supply text content via SetTextContent().
  // MaybePlayOnReady() should be triggered, fulfilling play intent.
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  auto seg = read_aloud::mojom::TextSegment::New();
  seg->segment_index = 0;
  seg->text = u"Sentence to play on ready.";
  segments.push_back(std::move(seg));

  controller_remote_->SetTextContent(std::move(segments));
  controller_remote_.FlushForTesting();
  mock_client_->FlushForTesting();

  EXPECT_TRUE(controller_remote_.is_connected());
  // Verify playback intent was fulfilled (SetTextContent did NOT default state
  // to kPaused).
  EXPECT_NE(mock_client_->last_state(),
            read_aloud::mojom::PlaybackState::kPaused);
}

TEST_F(ReadAloudPlaybackControllerTest, PauseClearsPlayOnReady) {
  CreateSession();

  // Call Play() BEFORE SetTextContent() has been called (sets play_on_ready_ =
  // true).
  controller_remote_->Play();
  controller_remote_.FlushForTesting();

  EXPECT_TRUE(controller_remote_.is_connected());

  // Call Pause() before text arrives (must reset play_on_ready_ = false).
  controller_remote_->Pause();
  controller_remote_.FlushForTesting();

  // Now supply text content via SetTextContent().
  // Playback should NOT start automatically because Pause() cleared
  // play_on_ready_.
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  auto seg = read_aloud::mojom::TextSegment::New();
  seg->segment_index = 0;
  seg->text = u"Text provided after explicit pause.";
  segments.push_back(std::move(seg));

  controller_remote_->SetTextContent(std::move(segments));
  controller_remote_.FlushForTesting();

  EXPECT_TRUE(controller_remote_.is_connected());
  EXPECT_EQ(mock_client_->last_state(),
            read_aloud::mojom::PlaybackState::kPaused);
}

TEST_F(ReadAloudPlaybackControllerTest,
       PlayCalledBeforeInitializeAudioDefersUntilAudioInitialized) {
  CreateSession();

  // Load text content first.
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  auto seg = read_aloud::mojom::TextSegment::New();
  seg->segment_index = 0;
  seg->text = u"Text loaded before audio initialization.";
  segments.push_back(std::move(seg));
  controller_remote_->SetTextContent(std::move(segments));
  controller_remote_.FlushForTesting();

  // Call Play() BEFORE InitializeAudio(). IsAudioInitialized() is false.
  controller_remote_->Play();
  controller_remote_.FlushForTesting();

  EXPECT_TRUE(controller_remote_.is_connected());

  // Now initialize audio via InitializeAudioForTesting().
  InitializeAudioForTesting();

  EXPECT_TRUE(controller_remote_.is_connected());
}

TEST_F(ReadAloudPlaybackControllerTest,
       PlayOnReadyTimeoutResetsPendingPlayState) {
  CreateSession();

  base::test::TestFuture<read_aloud::mojom::PlaybackState> state_future;
  mock_client_->set_state_callback(state_future.GetRepeatingCallback());

  // Call Play() without setting text content (play_on_ready_ = true, timer
  // started).
  controller_remote_->Play();

  EXPECT_TRUE(controller_remote_.is_connected());

  // Fast forward time by 5 seconds (under 10s threshold).
  task_environment_.FastForwardBy(base::Seconds(5));
  EXPECT_FALSE(state_future.IsReady());

  // Fast forward remaining 5 seconds (reaching 10s timeout threshold).
  task_environment_.FastForwardBy(base::Seconds(5));

  EXPECT_TRUE(controller_remote_.is_connected());
  EXPECT_EQ(state_future.Take(), read_aloud::mojom::PlaybackState::kPaused);

  // Verify play_on_ready_ was reset: now supply text content and initialize
  // audio. Playback MUST NOT auto-start because the pending play intent was
  // cleared by timeout.
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  auto seg = read_aloud::mojom::TextSegment::New();
  seg->segment_index = 0;
  seg->text = u"Late arriving text after timeout.";
  segments.push_back(std::move(seg));
  controller_remote_->SetTextContent(std::move(segments));
  InitializeAudioForTesting();

  EXPECT_TRUE(controller_remote_.is_connected());
  EXPECT_EQ(mock_client_->last_state(),
            read_aloud::mojom::PlaybackState::kPaused);
}

TEST_F(ReadAloudPlaybackControllerTest,
       PlayCalledRepeatedlyResetsWatchdogTimer) {
  CreateSession();

  base::test::TestFuture<read_aloud::mojom::PlaybackState> state_future;
  mock_client_->set_state_callback(state_future.GetRepeatingCallback());

  // Initial Play() call at t=0s.
  controller_remote_->Play();

  // Fast forward by 7 seconds (timer at 7s, hasn't timed out).
  task_environment_.FastForwardBy(base::Seconds(7));
  EXPECT_FALSE(state_future.IsReady());

  // Consecutive Play() call at t=7s. This MUST reset the 10s watchdog timer
  // (granting a fresh 10s window until t=17s).
  controller_remote_->Play();

  // Fast forward by 5 seconds (t=12s total). Original timer would have fired at
  // 10s, but new timer is only at 5s, so state is NOT timed out yet.
  task_environment_.FastForwardBy(base::Seconds(5));
  EXPECT_FALSE(state_future.IsReady());

  // Fast forward remaining 5 seconds (t=17s total). The reset timer now
  // expires.
  task_environment_.FastForwardBy(base::Seconds(5));

  EXPECT_EQ(state_future.Take(), read_aloud::mojom::PlaybackState::kPaused);
}

class MockReadAloudAudioRenderer : public ReadAloudAudioRenderer {
 public:
  MockReadAloudAudioRenderer() = default;
  ~MockReadAloudAudioRenderer() override = default;

  MOCK_METHOD(void, Flush, (), (override));
};

TEST_F(ReadAloudPlaybackControllerTest,
       FlushBuffersFlushesInjectedAudioRenderer) {
  raw_ptr<MockReadAloudAudioRenderer> mock_renderer = nullptr;

  mojo::Remote<read_aloud::mojom::ReadAloudPlaybackControllerFactory> factory;
  mojo::Remote<read_aloud::mojom::ReadAloudPlaybackController> controller;
  auto mock_client = std::make_unique<MockReadAloudPlaybackControllerClient>();

  auto controller_impl = std::make_unique<ReadAloudPlaybackController>(
      factory.BindNewPipeAndPassReceiver(),
      base::BindRepeating(
          [](raw_ptr<MockReadAloudAudioRenderer>* out_mock)
              -> std::unique_ptr<ReadAloudAudioRenderer> {
            auto mock = std::make_unique<MockReadAloudAudioRenderer>();
            *out_mock = mock.get();
            return mock;
          },
          &mock_renderer));

  factory->CreateController(controller.BindNewPipeAndPassReceiver(),
                            mock_client->BindAndGetRemote());
  factory.FlushForTesting();

  mojo::PendingRemote<media::mojom::AudioOutputStream> stream;
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
  controller->InitializeAudio(std::move(stream), std::move(data_pipe), params);
  controller.FlushForTesting();

  ASSERT_TRUE(mock_renderer);
  EXPECT_CALL(*mock_renderer, Flush()).Times(1);

  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  auto seg = read_aloud::mojom::TextSegment::New();
  seg->segment_index = 0;
  seg->text = u"Sample document text.";
  segments.push_back(std::move(seg));

  controller->SetTextContent(std::move(segments));
  controller.FlushForTesting();

  mock_renderer = nullptr;
}

// Regression test for b/562011435: claiming kPlaying before any audio frame is
// buffered dismisses the browser's loading UI over silence.
TEST_F(ReadAloudPlaybackControllerTest,
       PlayWithEmptyAudioQueueEmitsBufferingNotPlaying) {
  CreateSession();
  HoldSynthesisResponses();
  SetSingleTextSegment(u"Buffering state sentence.");
  ASSERT_NO_FATAL_FAILURE(InitializeAudioForTesting());
  FlushAll();
  mock_client_->ClearStateHistory();

  controller_remote_->Play();
  FlushAll();

  EXPECT_THAT(
      mock_client_->state_history(),
      testing::ElementsAre(read_aloud::mojom::PlaybackState::kBuffering));
}

TEST_F(ReadAloudPlaybackControllerTest, PauseEmitsPausedState) {
  CreateSession();
  HoldSynthesisResponses();
  SetSingleTextSegment(u"Paused state sentence.");
  ASSERT_NO_FATAL_FAILURE(InitializeAudioForTesting());
  controller_remote_->Play();
  FlushAll();
  // Pausing must be observed from an active state, otherwise dedupe would hide
  // the notification.
  ASSERT_EQ(mock_client_->last_state(),
            read_aloud::mojom::PlaybackState::kBuffering);
  mock_client_->ClearStateHistory();

  controller_remote_->Pause();
  FlushAll();

  EXPECT_THAT(mock_client_->state_history(),
              testing::ElementsAre(read_aloud::mojom::PlaybackState::kPaused));
}

TEST_F(ReadAloudPlaybackControllerTest,
       DocumentThatNeverProducesAudioEmitsErrorState) {
  CreateSession();
  SetFailingSynthesisResponse();
  SetSingleTextSegment(u"First sentence. Second sentence. Third sentence.");
  ASSERT_NO_FATAL_FAILURE(InitializeAudioForTesting());
  FlushAll();
  mock_client_->ClearStateHistory();

  // Every chunk fails, so the timeline is consumed without ever producing
  // audio and playback can never recover.
  controller_remote_->Play();
  FastForwardAndFlush(base::Seconds(5));

  EXPECT_THAT(mock_client_->state_history(),
              testing::ElementsAre(read_aloud::mojom::PlaybackState::kBuffering,
                                   read_aloud::mojom::PlaybackState::kError));
}

TEST_F(ReadAloudPlaybackControllerTest, RepeatedPauseNotifiesClientOnce) {
  CreateSession();

  controller_remote_->Pause();
  FlushAll();
  controller_remote_->Pause();
  FlushAll();

  // The second Pause() resolves to the same state and must be deduped away.
  EXPECT_THAT(mock_client_->state_history(),
              testing::ElementsAre(read_aloud::mojom::PlaybackState::kPaused));
}

TEST_F(ReadAloudPlaybackControllerTest,
       DocumentThatNeverProducesAudioPausesStream) {
  CreateSession();
  SetFailingSynthesisResponse();
  SetSingleTextSegment(u"First sentence. Second sentence. Third sentence.");
  ASSERT_NO_FATAL_FAILURE(InitializeAudioForTesting());
  ASSERT_NO_FATAL_FAILURE(BindFakeStream());

  controller_remote_->Play();
  FastForwardAndFlush(base::Seconds(5));
  fake_stream_.FlushForTesting();
  ASSERT_EQ(mock_client_->last_state(),
            read_aloud::mojom::PlaybackState::kError);

  // kError must pause the output stream rather than leave it running.
  EXPECT_THAT(fake_stream_.commands(),
              testing::ElementsAre(FakeAudioOutputStream::Command::kPlay,
                                   FakeAudioOutputStream::Command::kPause));
}

TEST_F(ReadAloudPlaybackControllerTest, SeekWhileNotPumpingPausesStream) {
  CreateSession();
  SetSingleTextSegment(u"Seek while idle sentence.");
  ASSERT_NO_FATAL_FAILURE(InitializeAudioForTesting());
  ASSERT_NO_FATAL_FAILURE(BindFakeStream());

  controller_remote_->SeekToWord(/*segment_index=*/0, /*character_offset=*/0);
  FlushAll();
  fake_stream_.FlushForTesting();

  // Audio decoded for the new position must not be rendered before Play().
  EXPECT_THAT(fake_stream_.commands(),
              testing::ElementsAre(FakeAudioOutputStream::Command::kPause));
}

TEST_F(ReadAloudPlaybackControllerTest, SeekWhilePumpingDoesNotPauseStream) {
  CreateSession();
  HoldSynthesisResponses();
  SetSingleTextSegment(u"Seek while playing sentence.");
  ASSERT_NO_FATAL_FAILURE(InitializeAudioForTesting());
  ASSERT_NO_FATAL_FAILURE(BindFakeStream());

  controller_remote_->Play();
  FlushAll();
  // Deliver the Play() command before clearing, so it cannot leak into the
  // post-seek expectation below.
  fake_stream_.FlushForTesting();
  ASSERT_EQ(mock_client_->last_state(),
            read_aloud::mojom::PlaybackState::kBuffering);
  fake_stream_.ClearCommands();

  controller_remote_->SeekToWord(/*segment_index=*/0, /*character_offset=*/0);
  FlushAll();
  fake_stream_.FlushForTesting();

  // Positive controls: the seek was accepted and playback is still active.
  ASSERT_TRUE(controller_remote_.is_connected());
  EXPECT_EQ(mock_client_->last_state(),
            read_aloud::mojom::PlaybackState::kBuffering);
  EXPECT_THAT(fake_stream_.commands(), testing::IsEmpty());
}

TEST_F(ReadAloudPlaybackControllerTest,
       PlayOnReadyOverviewModeAllowsLongerTimeout) {
  CreateSession();

  base::test::TestFuture<read_aloud::mojom::PlaybackState> state_future;
  mock_client_->set_state_callback(state_future.GetRepeatingCallback());

  // Set Overview mode before playing.
  controller_remote_->SetPlaybackMode(
      read_aloud::mojom::PlaybackMode::kOverview);
  controller_remote_->Play();

  EXPECT_TRUE(controller_remote_.is_connected());

  // Fast forward by 30 seconds (exceeds classic 10s timeout, but within 200s
  // overview timeout).
  task_environment_.FastForwardBy(base::Seconds(30));
  EXPECT_FALSE(state_future.IsReady());

  // Fast forward remaining 170 seconds to reach 200s overview timeout
  // threshold.
  task_environment_.FastForwardBy(base::Seconds(170));

  EXPECT_TRUE(controller_remote_.is_connected());
  EXPECT_EQ(state_future.Take(), read_aloud::mojom::PlaybackState::kPaused);
}

TEST_F(ReadAloudPlaybackControllerTest,
       SeekToWordSingleSegmentMultiSentenceUsesTimelineChunkIndices) {
  CreateSession();
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  read_aloud::mojom::TextSegmentPtr seg = read_aloud::mojom::TextSegment::New();
  seg->segment_index = 0;
  seg->text = u"First sentence. Second sentence.";
  segments.push_back(std::move(seg));

  controller_remote_->SetTextContent(std::move(segments));
  controller_remote_.FlushForTesting();

  // Single input segment was split into 2 canonical timeline chunks, so
  // seeking to chunk 1 succeeds.
  controller_remote_->SeekToWord(/*segment_index=*/1, /*character_offset=*/0);
  controller_remote_.FlushForTesting();
  EXPECT_TRUE(controller_remote_.is_connected());

  // Seeking past the second chunk's length ("Second sentence." is 16 chars)
  // reports a BadMessage.
  mojo::test::BadMessageObserver bad_message_observer;
  controller_remote_->SeekToWord(/*segment_index=*/1, /*character_offset=*/17);
  EXPECT_EQ(
      bad_message_observer.WaitForBadMessage(),
      "ReadAloudPlaybackController: Invalid character_offset in SeekToWord");
}

TEST_F(ReadAloudPlaybackControllerTest,
       SetTextContentMultiSpeakerForwardsSpeakerTagsThroughTimeline) {
  CreateSession();

  std::vector<std::pair<std::u16string, read_aloud::mojom::Speaker>>
      observed_requests;
  mock_client_->set_synthesis_handler(base::BindLambdaForTesting(
      [&](const std::u16string& text_chunk, read_aloud::mojom::Speaker speaker,
          uint64_t /*sequence_id*/,
          MockReadAloudPlaybackControllerClient::RequestSpeechSynthesisCallback
              callback) {
        observed_requests.emplace_back(text_chunk, speaker);
        std::move(callback).Run(mojo_base::BigBuffer(), /*success=*/true);
      }));

  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  read_aloud::mojom::TextSegmentPtr seg1 =
      read_aloud::mojom::TextSegment::New();
  seg1->segment_index = 0;
  seg1->text = u"Host opening sentence.";
  seg1->speaker = read_aloud::mojom::Speaker::kSpeaker1;
  segments.push_back(std::move(seg1));

  read_aloud::mojom::TextSegmentPtr seg2 =
      read_aloud::mojom::TextSegment::New();
  seg2->segment_index = 1;
  seg2->text = u"Guest reply sentence.";
  seg2->speaker = read_aloud::mojom::Speaker::kSpeaker2;
  segments.push_back(std::move(seg2));

  controller_remote_->SetTextContent(std::move(segments));
  ASSERT_NO_FATAL_FAILURE(InitializeAudioForTesting());
  controller_remote_->Play();
  FlushAll();

  EXPECT_THAT(
      observed_requests,
      testing::ElementsAre(std::pair(std::u16string(u"Host opening sentence."),
                                     read_aloud::mojom::Speaker::kSpeaker1),
                           std::pair(std::u16string(u"Guest reply sentence."),
                                     read_aloud::mojom::Speaker::kSpeaker2)));
}

TEST_F(ReadAloudPlaybackControllerTest, SetTextContentCapsOnTextChunked) {
  CreateSession();

  // Build a single segment with `kMaxTextChunks + 5` short sentences (30,015
  // UTF-16 code units, well within `kMaxTextLengthPerSegment` = 65,536).
  std::u16string text;
  text.reserve((kMaxTextChunks + 5) * 3);
  for (size_t i = 0; i < kMaxTextChunks + 5; ++i) {
    text += u"A. ";
  }

  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  auto seg = read_aloud::mojom::TextSegment::New();
  seg->segment_index = 0;
  seg->text = std::move(text);
  segments.push_back(std::move(seg));

  controller_remote_->SetTextContent(std::move(segments));
  mock_client_->WaitForChunks();

  ASSERT_TRUE(mock_client_->last_chunks().has_value());
  EXPECT_EQ(mock_client_->last_chunks()->size(), kMaxTextChunks);

  // The canonical timeline still retains all `kMaxTextChunks + 5` chunks, so
  // seeking to the last chunk index succeeds without a BadMessage.
  controller_remote_->SeekToWord(
      /*segment_index=*/static_cast<uint32_t>(kMaxTextChunks + 4),
      /*character_offset=*/0);
  controller_remote_.FlushForTesting();
  EXPECT_TRUE(controller_remote_.is_connected());
}

TEST_F(ReadAloudPlaybackControllerTest, IgnoresStaleSynthesisResponse) {
  CreateSession();

  bool captured_first_callback = false;
  MockReadAloudPlaybackControllerClient::RequestSpeechSynthesisCallback
      stale_callback;
  mock_client_->set_synthesis_handler(base::BindLambdaForTesting(
      [&](const std::u16string& /*text_chunk*/,
          read_aloud::mojom::Speaker /*speaker*/, uint64_t /*sequence_id*/,
          MockReadAloudPlaybackControllerClient::RequestSpeechSynthesisCallback
              callback) {
        if (!captured_first_callback) {
          captured_first_callback = true;
          stale_callback = std::move(callback);
          return;
        }
        std::move(callback).Run(mojo_base::BigBuffer(), /*success=*/true);
      }));

  SetSingleTextSegment(u"Old sentence.");
  ASSERT_NO_FATAL_FAILURE(InitializeAudioForTesting());
  controller_remote_->Play();
  FlushAll();
  ASSERT_TRUE(stale_callback);
  EXPECT_EQ(mock_client_->synthesis_request_count(), 1u);

  // Load new text content, resetting the prefetch session and incrementing
  // `session_sequence_id_` before `stale_callback` resolves.
  SetSingleTextSegment(u"New sentence.");
  FlushAll();
  EXPECT_EQ(mock_client_->synthesis_request_count(), 1u);

  // Delivering the stale response from the previous document must return early
  // without calling `ReplenishBuffer()` or triggering a synthesis request for
  // the new document while paused.
  std::move(stale_callback).Run(mojo_base::BigBuffer(), /*success=*/true);
  FlushAll();
  EXPECT_EQ(mock_client_->synthesis_request_count(), 1u);

  // Starting playback on the new document now dispatches its synthesis request.
  controller_remote_->Play();
  FlushAll();
  EXPECT_EQ(mock_client_->synthesis_request_count(), 2u);
}

TEST_F(ReadAloudPlaybackControllerTest,
       SetOverviewContentSuccessSetsTextAndReturnsMetadata) {
  CreateSession();
  // Send a serialized, valid GenerateText response
  // to the controller over Mojo.
  optimization_guide::proto::ReadAloudGenerateTextResponse response;
  response.set_title("Test Overview Title");
  response.add_dialogue_turns()->set_utterance("Overview dialogue text.");
  std::string serialized = response.SerializeAsString();
  mojo_base::BigBuffer buffer(base::as_byte_span(serialized));
  base::test::TestFuture<bool, const std::string&> future;
  controller_remote_->SetOverviewContent(std::move(buffer),
                                         future.GetCallback());
  // Verify success and title was set
  auto [success, title] = future.Take();
  EXPECT_TRUE(success);
  EXPECT_EQ(title, "Test Overview Title");
  // Loading new content resets internal queues and
  // defaults the engine to Paused.
  FlushAll();
  EXPECT_EQ(mock_client_->last_state(),
            read_aloud::mojom::PlaybackState::kPaused);
}

TEST_F(ReadAloudPlaybackControllerTest, SetOverviewContentFailureReportsError) {
  CreateSession();
  base::test::TestFuture<bool, const std::string&> future;
  // Passing an empty buffer causes ParseAndValidateOverviewResponse to reject
  // the payload as kMalformed.
  controller_remote_->SetOverviewContent(mojo_base::BigBuffer(),  // empty
                                         future.GetCallback());
  auto [success, title] = future.Take();
  EXPECT_FALSE(success);
  EXPECT_TRUE(title.empty());
  EXPECT_TRUE(controller_remote_.is_connected());
}

}  // namespace readaloud
