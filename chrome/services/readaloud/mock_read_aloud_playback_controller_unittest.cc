// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/mock_read_aloud_playback_controller.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/common/readaloud/read_aloud.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/mojom/base/work_in_progress.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace readaloud {

class MockClient : public read_aloud::mojom::ReadAloudPlaybackControllerClient {
 public:
  explicit MockClient(
      mojo::PendingReceiver<
          read_aloud::mojom::ReadAloudPlaybackControllerClient> receiver)
      : receiver_(this, std::move(receiver)) {}

  MOCK_METHOD(void,
              OnPlaybackStateChanged,
              (read_aloud::mojom::PlaybackState state),
              (override));
  MOCK_METHOD(void,
              OnPlaybackDurationChanged,
              (base::TimeDelta duration),
              (override));
  MOCK_METHOD(void,
              OnWordBoundaryReached,
              (uint32_t segment_index,
               uint32_t character_offset,
               base::TimeDelta audio_timestamp),
              (override));

  void RequestSpeechSynthesis(
      const std::u16string& text_chunk,
      uint64_t sequence_id,
      RequestSpeechSynthesisCallback callback) override {
    std::move(callback).Run(mojo_base::BigBuffer(), false);
  }

 private:
  mojo::Receiver<read_aloud::mojom::ReadAloudPlaybackControllerClient>
      receiver_;
};

class MockPlaybackControllerTest : public ::testing::Test {
 public:
  MockPlaybackControllerTest() {
    scoped_feature_list_.InitAndEnableFeature(
        mojo_base::mojom::kMojomWorkInProgress);
  }

 protected:
  // Helper to create a single-segment text payload for test brevity.
  std::vector<read_aloud::mojom::TextSegmentPtr> CreateSingleSegment(
      uint32_t segment_index,
      const std::u16string& text) {
    std::vector<read_aloud::mojom::TextSegmentPtr> segments;
    read_aloud::mojom::TextSegmentPtr segment =
        read_aloud::mojom::TextSegment::New();
    segment->segment_index = segment_index;
    segment->text = text;
    segments.push_back(std::move(segment));
    return segments;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  mojo::Remote<read_aloud::mojom::ReadAloudPlaybackController> controller_;
  std::unique_ptr<::testing::NiceMock<MockReadAloudPlaybackController>>
      controller_impl_;
  std::unique_ptr<::testing::NiceMock<MockClient>> client_;
};

// Verifies a standard playback sequence: initialization -> duration update ->
// word boundary timer firing -> completion pause.
TEST_F(MockPlaybackControllerTest, PlaybackSequence) {
  mojo::PendingRemote<read_aloud::mojom::ReadAloudPlaybackControllerClient>
      client_remote;
  client_ = std::make_unique<::testing::NiceMock<MockClient>>(
      client_remote.InitWithNewPipeAndPassReceiver());

  controller_impl_ =
      std::make_unique<::testing::NiceMock<MockReadAloudPlaybackController>>(
          controller_.BindNewPipeAndPassReceiver());
  controller_impl_->InitializeClient(std::move(client_remote));

  // Step 1: Verify initial duration callback upon setting text content.
  EXPECT_CALL(*client_,
              OnPlaybackStateChanged(read_aloud::mojom::PlaybackState::kPaused))
      .Times(1);
  EXPECT_CALL(*client_, OnPlaybackDurationChanged(base::TimeDelta())).Times(1);
  EXPECT_CALL(*client_, OnPlaybackDurationChanged(base::Milliseconds(500)))
      .Times(1);

  controller_->SetTextContent(CreateSingleSegment(0, u"Hello World"));
  controller_.FlushForTesting();
  ::testing::Mock::VerifyAndClearExpectations(client_.get());

  // Step 2: Playback starts, fires boundaries at 0ms and 250ms, then pauses at end.
  {
    ::testing::InSequence seq;
    EXPECT_CALL(*client_, OnPlaybackStateChanged(
                              read_aloud::mojom::PlaybackState::kPlaying));
    EXPECT_CALL(*client_, OnWordBoundaryReached(0, 0, base::TimeDelta()));
    EXPECT_CALL(*client_, OnWordBoundaryReached(0, 6, base::Milliseconds(250)));
    EXPECT_CALL(*client_, OnPlaybackStateChanged(
                              read_aloud::mojom::PlaybackState::kPaused));
  }

  controller_->Play();
  controller_.FlushForTesting();
  task_environment_.FastForwardBy(base::Milliseconds(250));
  task_environment_.FastForwardBy(base::Milliseconds(250));
}

// Verifies seeking to a word boundary, pausing mid-speech, and resuming.
TEST_F(MockPlaybackControllerTest, SeekAndPause) {
  mojo::PendingRemote<read_aloud::mojom::ReadAloudPlaybackControllerClient>
      client_remote;
  client_ = std::make_unique<::testing::NiceMock<MockClient>>(
      client_remote.InitWithNewPipeAndPassReceiver());

  controller_impl_ =
      std::make_unique<::testing::NiceMock<MockReadAloudPlaybackController>>(
          controller_.BindNewPipeAndPassReceiver());
  controller_impl_->InitializeClient(std::move(client_remote));

  controller_->SetTextContent(CreateSingleSegment(0, u"One Two Three Four"));
  controller_.FlushForTesting();

  // Step 1: Start playback.
  EXPECT_CALL(*client_, OnPlaybackStateChanged(
                            read_aloud::mojom::PlaybackState::kPlaying))
      .Times(1);
  EXPECT_CALL(*client_, OnWordBoundaryReached(0, 0, base::TimeDelta()))
      .Times(1);

  controller_->Play();
  controller_.FlushForTesting();
  ::testing::Mock::VerifyAndClearExpectations(client_.get());

  // Step 2: Advance to first word boundary ("Two").
  EXPECT_CALL(*client_, OnWordBoundaryReached(0, 4, base::Milliseconds(250)))
      .Times(1);
  task_environment_.FastForwardBy(base::Milliseconds(250));
  ::testing::Mock::VerifyAndClearExpectations(client_.get());

  // Step 3: Pause playback.
  EXPECT_CALL(*client_,
              OnPlaybackStateChanged(read_aloud::mojom::PlaybackState::kPaused))
      .Times(1);
  controller_->Pause();
  controller_.FlushForTesting();
  ::testing::Mock::VerifyAndClearExpectations(client_.get());

  // Step 4: Seek to word "Three" at character offset 8.
  EXPECT_CALL(*client_, OnWordBoundaryReached(0, 8, base::Milliseconds(500)))
      .Times(1);
  controller_->SeekToWord(0, 8);
  controller_.FlushForTesting();
  ::testing::Mock::VerifyAndClearExpectations(client_.get());

  // Step 5: Resume playback from seek position.
  EXPECT_CALL(*client_, OnPlaybackStateChanged(
                            read_aloud::mojom::PlaybackState::kPlaying))
      .Times(1);
  EXPECT_CALL(*client_, OnWordBoundaryReached(0, 14, base::Milliseconds(750)))
      .Times(1);
  controller_->Play();
  controller_.FlushForTesting();
  ::testing::Mock::VerifyAndClearExpectations(client_.get());

  EXPECT_CALL(*client_,
              OnPlaybackStateChanged(read_aloud::mojom::PlaybackState::kPaused))
      .Times(1);
  task_environment_.FastForwardBy(base::Milliseconds(250));
  ::testing::Mock::VerifyAndClearExpectations(client_.get());
}

// Verifies seeking to a target time position in audio.
TEST_F(MockPlaybackControllerTest, SeekToTime) {
  mojo::PendingRemote<read_aloud::mojom::ReadAloudPlaybackControllerClient>
      client_remote;
  client_ = std::make_unique<::testing::NiceMock<MockClient>>(
      client_remote.InitWithNewPipeAndPassReceiver());

  controller_impl_ =
      std::make_unique<::testing::NiceMock<MockReadAloudPlaybackController>>(
          controller_.BindNewPipeAndPassReceiver());
  controller_impl_->InitializeClient(std::move(client_remote));

  controller_->SetTextContent(CreateSingleSegment(0, u"One Two Three Four"));
  controller_.FlushForTesting();

  // Seek to 500ms should locate nearest word boundary ("Three").
  EXPECT_CALL(*client_, OnWordBoundaryReached(0, 8, base::Milliseconds(500)))
      .Times(1);
  controller_->SeekToTime(base::Milliseconds(500));
  controller_.FlushForTesting();
  ::testing::Mock::VerifyAndClearExpectations(client_.get());
}

// Verifies playback rate clamping and invalid input handling.
TEST_F(MockPlaybackControllerTest, PlaybackRateEdgeCases) {
  mojo::PendingRemote<read_aloud::mojom::ReadAloudPlaybackControllerClient>
      client_remote;
  client_ = std::make_unique<::testing::NiceMock<MockClient>>(
      client_remote.InitWithNewPipeAndPassReceiver());

  controller_impl_ =
      std::make_unique<::testing::NiceMock<MockReadAloudPlaybackController>>(
          controller_.BindNewPipeAndPassReceiver());
  controller_impl_->InitializeClient(std::move(client_remote));

  EXPECT_EQ(controller_impl_->playback_rate(), 1.0f);

  // Invalid or NaN rates should be ignored.
  controller_->SetPlaybackRate(std::numeric_limits<float>::quiet_NaN());
  controller_.FlushForTesting();
  EXPECT_EQ(controller_impl_->playback_rate(), 1.0f);

  controller_->SetPlaybackRate(-1.0f);
  controller_.FlushForTesting();
  EXPECT_EQ(controller_impl_->playback_rate(), 1.0f);

  controller_->SetPlaybackRate(0.0f);
  controller_.FlushForTesting();
  EXPECT_EQ(controller_impl_->playback_rate(), 1.0f);

  // Out-of-bounds rates should be clamped to range [0.25, 4.0].
  controller_->SetPlaybackRate(0.1f);
  controller_.FlushForTesting();
  EXPECT_EQ(controller_impl_->playback_rate(), 0.25f);

  controller_->SetPlaybackRate(10.0f);
  controller_.FlushForTesting();
  EXPECT_EQ(controller_impl_->playback_rate(), 4.0f);
}

// Verifies setting new text content resets playback state to Paused.
TEST_F(MockPlaybackControllerTest, SetTextContentResetsState) {
  mojo::PendingRemote<read_aloud::mojom::ReadAloudPlaybackControllerClient>
      client_remote;
  client_ = std::make_unique<::testing::NiceMock<MockClient>>(
      client_remote.InitWithNewPipeAndPassReceiver());

  controller_impl_ =
      std::make_unique<::testing::NiceMock<MockReadAloudPlaybackController>>(
          controller_.BindNewPipeAndPassReceiver());
  controller_impl_->InitializeClient(std::move(client_remote));

  controller_->SetTextContent(CreateSingleSegment(0, u"Hello World"));
  controller_.FlushForTesting();

  // Start playback
  controller_->Play();
  controller_.FlushForTesting();

  // Setting new text content must pause playback
  EXPECT_CALL(*client_,
              OnPlaybackStateChanged(read_aloud::mojom::PlaybackState::kPaused))
      .Times(1);

  controller_->SetTextContent(CreateSingleSegment(0, u"New Text"));
  controller_.FlushForTesting();
}

// Verifies client tests can override default ON_CALL behavior with EXPECT_CALL.
TEST_F(MockPlaybackControllerTest, CanOverrideWithExpectCall) {
  mojo::PendingRemote<read_aloud::mojom::ReadAloudPlaybackControllerClient>
      client_remote;
  client_ = std::make_unique<::testing::NiceMock<MockClient>>(
      client_remote.InitWithNewPipeAndPassReceiver());

  controller_impl_ =
      std::make_unique<::testing::NiceMock<MockReadAloudPlaybackController>>(
          controller_.BindNewPipeAndPassReceiver());
  controller_impl_->InitializeClient(std::move(client_remote));

  EXPECT_CALL(*controller_impl_, SetPlaybackRate(2.0f)).Times(1);

  controller_->SetPlaybackRate(2.0f);
  controller_.FlushForTesting();
}

}  // namespace readaloud
