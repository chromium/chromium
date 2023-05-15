// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/cast_media_session_controller.h"

#include <memory>
#include <utility>

#include "base/time/time.h"
#include "components/media_router/common/mojom/media_status.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/media_session/public/mojom/constants.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using media_session::mojom::MediaSessionAction;

namespace {

constexpr base::TimeDelta kDefaultSeekSeconds =
    base::Seconds(media_session::mojom::kDefaultSeekTimeSeconds);
}

class MockMediaController : public media_router::mojom::MediaController {
 public:
  MOCK_METHOD(void, Play, ());
  MOCK_METHOD(void, Pause, ());
  MOCK_METHOD(void, SetMute, (bool));
  MOCK_METHOD(void, SetVolume, (float));
  MOCK_METHOD(void, Seek, (base::TimeDelta));
  MOCK_METHOD(void, NextTrack, ());
  MOCK_METHOD(void, PreviousTrack, ());
};

class CastMediaSessionControllerTest : public testing::Test {
 public:
  void SetUp() override {
    mojo::Remote<media_router::mojom::MediaController> mock_controller_remote;
    mock_controller_receiver_.Bind(
        mock_controller_remote.BindNewPipeAndPassReceiver());
    controller_ = std::make_unique<CastMediaSessionController>(
        std::move(mock_controller_remote));

    media_status_ = media_router::mojom::MediaStatus::New();
    media_status_->duration = base::Seconds(100);
    media_status_->current_time = base::Seconds(20);
    controller_->OnMediaStatusUpdated(media_status_.Clone());
  }

  void SendToController(MediaSessionAction command) {
    controller_->Send(command);
    controller_->FlushForTesting();
  }

  void WaitForOneSecond() {
    base::RunLoop run_loop;
    content::GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Seconds(1));
    run_loop.Run();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  MockMediaController mock_controller_;
  media_router::mojom::MediaStatusPtr media_status_;
  mojo::Receiver<media_router::mojom::MediaController>
      mock_controller_receiver_{&mock_controller_};
  std::unique_ptr<CastMediaSessionController> controller_;
};

TEST_F(CastMediaSessionControllerTest, SendPlayCommand) {
  EXPECT_CALL(mock_controller_, Play());
  SendToController(MediaSessionAction::kPlay);
}

TEST_F(CastMediaSessionControllerTest, SendPauseCommand) {
  EXPECT_CALL(mock_controller_, Pause());
  SendToController(MediaSessionAction::kPause);
}

TEST_F(CastMediaSessionControllerTest, SendPreviousTrackCommand) {
  EXPECT_CALL(mock_controller_, PreviousTrack());
  SendToController(MediaSessionAction::kPreviousTrack);
}

TEST_F(CastMediaSessionControllerTest, SendNextTrackCommand) {
  EXPECT_CALL(mock_controller_, NextTrack());
  SendToController(MediaSessionAction::kNextTrack);
}

TEST_F(CastMediaSessionControllerTest, SendSeekBackwardCommand) {
  EXPECT_CALL(mock_controller_,
              Seek(media_status_->current_time - kDefaultSeekSeconds));
  SendToController(MediaSessionAction::kSeekBackward);
}

TEST_F(CastMediaSessionControllerTest, SeekBackwardOutOfRange) {
  media_status_->current_time = base::Seconds(2);
  controller_->OnMediaStatusUpdated(media_status_.Clone());

  EXPECT_CALL(mock_controller_, Seek(base::TimeDelta()));
  SendToController(MediaSessionAction::kSeekBackward);
}

TEST_F(CastMediaSessionControllerTest, SeekBackwardAfterWaiting) {
  const base::TimeDelta wait = base::Seconds(3);
  task_environment_.FastForwardBy(wait);

  EXPECT_CALL(mock_controller_,
              Seek(media_status_->current_time + wait - kDefaultSeekSeconds));
  SendToController(MediaSessionAction::kSeekBackward);
}

TEST_F(CastMediaSessionControllerTest, SendSeekForwardCommand) {
  EXPECT_CALL(mock_controller_,
              Seek(media_status_->current_time + kDefaultSeekSeconds));
  SendToController(MediaSessionAction::kSeekForward);
}

TEST_F(CastMediaSessionControllerTest, SeekForwardOutOfRange) {
  media_status_->current_time = media_status_->duration - base::Seconds(2);
  controller_->OnMediaStatusUpdated(media_status_.Clone());

  EXPECT_CALL(mock_controller_, Seek(media_status_->duration));
  SendToController(MediaSessionAction::kSeekForward);
}

TEST_F(CastMediaSessionControllerTest, SeekForwardAfterWaiting) {
  const base::TimeDelta wait = base::Seconds(3);
  task_environment_.FastForwardBy(wait);

  EXPECT_CALL(mock_controller_,
              Seek(media_status_->current_time + wait + kDefaultSeekSeconds));
  SendToController(MediaSessionAction::kSeekForward);
}

TEST_F(CastMediaSessionControllerTest, SendStopCommand) {
  EXPECT_CALL(mock_controller_, Pause());
  SendToController(MediaSessionAction::kStop);
}

TEST_F(CastMediaSessionControllerTest, SeekTo) {
  auto seek_time = base::Seconds(2);
  EXPECT_CALL(mock_controller_, Seek(seek_time));
  controller_->SeekTo(seek_time);
  controller_->FlushForTesting();
}

TEST_F(CastMediaSessionControllerTest, SetMute) {
  bool mute = false;
  EXPECT_CALL(mock_controller_, SetMute(mute));
  controller_->SetMute(mute);
  controller_->FlushForTesting();
}

TEST_F(CastMediaSessionControllerTest, SetVolume) {
  float volume = 1.1f;
  EXPECT_CALL(mock_controller_, SetVolume(testing::FloatEq(volume)));
  controller_->SetVolume(volume);
  controller_->FlushForTesting();
}

TEST_F(CastMediaSessionControllerTest, IncrementCurrentTime) {
  //  Update controller with paused media status should not increment current
  //  time.
  media_status_->play_state =
      media_router::mojom::MediaStatus::PlayState::PAUSED;
  controller_->OnMediaStatusUpdated(media_status_.Clone());
  WaitForOneSecond();
  EXPECT_EQ(media_status_->current_time,
            controller_->GetMediaStatusForTesting()->current_time);

  // Update controller with playing media status should increment current time.
  media_status_->play_state =
      media_router::mojom::MediaStatus::PlayState::PLAYING;
  controller_->OnMediaStatusUpdated(media_status_.Clone());

  WaitForOneSecond();
  EXPECT_NE(media_status_->current_time,
            controller_->GetMediaStatusForTesting()->current_time);
}
