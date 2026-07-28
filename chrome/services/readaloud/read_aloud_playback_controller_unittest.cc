// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/read_aloud_playback_controller.h"

#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/common/readaloud/read_aloud.mojom.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/mojom/base/work_in_progress.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

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

  void RequestSpeechSynthesis(
      const std::u16string& /*text_chunk*/,
      uint64_t /*sequence_id*/,
      RequestSpeechSynthesisCallback callback) override {
    std::move(callback).Run(mojo_base::BigBuffer(), true);
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

 private:
  mojo::Receiver<read_aloud::mojom::ReadAloudPlaybackControllerClient>
      receiver_{this};
  std::optional<read_aloud::mojom::PlaybackState> last_state_;
  std::optional<read_aloud::mojom::PlaybackState> expected_state_to_wait_for_;
  base::OnceClosure state_changed_closure_;
};

}  // namespace

class ReadAloudPlaybackControllerTest : public testing::Test {
 public:
  ReadAloudPlaybackControllerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitAndEnableFeature(
        mojo_base::mojom::kMojomWorkInProgress);
  }

  void SetUp() override {
    ResetRemotes();
    controller_impl_ = std::make_unique<ReadAloudPlaybackController>(
        factory_remote_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override {
    ResetRemotes();
  }

  void ResetRemotes() {
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

}  // namespace readaloud
