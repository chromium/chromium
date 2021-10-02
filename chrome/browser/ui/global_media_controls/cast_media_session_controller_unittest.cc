// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/cast_media_session_controller.h"

#include <memory>
#include <utility>

#include "base/time/time.h"
#include "components/media_router/common/mojom/media_status.mojom.h"
#include "content/public/test/browser_task_environment.h"
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
  MOCK_METHOD0(Play, void());
  MOCK_METHOD0(Pause, void());
  MOCK_METHOD1(SetMute, void(bool));
  MOCK_METHOD1(SetVolume, void(float));
  MOCK_METHOD1(Seek, void(base::TimeDelta));
  MOCK_METHOD0(NextTrack, void());
  MOCK_METHOD0(PreviousTrack, void());
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
