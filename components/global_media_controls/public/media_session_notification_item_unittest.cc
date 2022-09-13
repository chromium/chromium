// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/global_media_controls/public/media_session_notification_item.h"

#include <memory>
#include <utility>

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/media_message_center/mock_media_notification_view.h"
#include "services/media_session/public/cpp/test/test_media_controller.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using media_session::mojom::MediaSessionAction;
using testing::_;
using testing::NiceMock;

namespace global_media_controls {

namespace {

class MockMediaSessionNotificationItemDelegate
    : public MediaSessionNotificationItem::Delegate {
 public:
  MockMediaSessionNotificationItemDelegate() = default;
  MockMediaSessionNotificationItemDelegate(
      const MockMediaSessionNotificationItemDelegate&) = delete;
  MockMediaSessionNotificationItemDelegate& operator=(
      const MockMediaSessionNotificationItemDelegate&) = delete;
  ~MockMediaSessionNotificationItemDelegate() override = default;

  MOCK_METHOD(void, ActivateItem, (const std::string&));
  MOCK_METHOD(void, HideItem, (const std::string&));
  MOCK_METHOD(void, RemoveItem, (const std::string&));
  MOCK_METHOD(void,
              LogMediaSessionActionButtonPressed,
              (const std::string&, MediaSessionAction));
};

const char kRequestId[] = "requestid";

}  // namespace

class MediaSessionNotificationItemTest : public testing::Test {
 public:
  MediaSessionNotificationItemTest() = default;
  MediaSessionNotificationItemTest(const MediaSessionNotificationItemTest&) =
      delete;
  MediaSessionNotificationItemTest& operator=(
      const MediaSessionNotificationItemTest&) = delete;
  ~MediaSessionNotificationItemTest() override = default;

  void SetUp() override {
    auto session_info = media_session::mojom::MediaSessionInfo::New();
    session_info->is_controllable = true;
    item_ = std::make_unique<MediaSessionNotificationItem>(
        &delegate_, kRequestId, std::string(),
        controller_.CreateMediaControllerRemote(), std::move(session_info));
    item_->SetView(&view_);
  }

  media_message_center::test::MockMediaNotificationView& view() {
    return view_;
  }

  MockMediaSessionNotificationItemDelegate& delegate() { return delegate_; }

  media_session::test::TestMediaController& controller() { return controller_; }

  MediaSessionNotificationItem& item() { return *item_; }

  void AdvanceClockMilliseconds(int milliseconds) {
    task_environment_.FastForwardBy(base::Milliseconds(milliseconds));
  }

 private:
  NiceMock<media_message_center::test::MockMediaNotificationView> view_;
  NiceMock<MockMediaSessionNotificationItemDelegate> delegate_;
  media_session::test::TestMediaController controller_;
  std::unique_ptr<MediaSessionNotificationItem> item_;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(MediaSessionNotificationItemTest, Freezing_DoNotUpdateMetadata) {
  media_session::MediaMetadata metadata;
  metadata.title = u"title2";
  metadata.artist = u"artist2";
  metadata.album = u"album";

  EXPECT_CALL(view(), UpdateWithMediaMetadata(_)).Times(0);
  item().Freeze(base::DoNothing());
  item().MediaSessionMetadataChanged(metadata);
}

TEST_F(MediaSessionNotificationItemTest, Freezing_DoNotUpdateImage) {
  SkBitmap image;
  image.allocN32Pixels(10, 10);
  image.eraseColor(SK_ColorMAGENTA);

  EXPECT_CALL(view(), UpdateWithMediaArtwork(_)).Times(0);
  item().Freeze(base::DoNothing());
  item().MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, image);
}

TEST_F(MediaSessionNotificationItemTest, Freezing_DoNotUpdatePlaybackState) {
  EXPECT_CALL(view(), UpdateWithMediaSessionInfo(_)).Times(0);

  item().Freeze(base::DoNothing());

  media_session::mojom::MediaSessionInfoPtr session_info(
      media_session::mojom::MediaSessionInfo::New());
  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPlaying;
  item().MediaSessionInfoChanged(session_info.Clone());
}

TEST_F(MediaSessionNotificationItemTest, Freezing_DoNotUpdateActions) {
  EXPECT_CALL(view(), UpdateWithMediaActions(_)).Times(0);

  item().Freeze(base::DoNothing());
  item().MediaSessionActionsChanged({MediaSessionAction::kSeekForward});
}

TEST_F(MediaSessionNotificationItemTest, Freezing_DisableInteraction) {
  EXPECT_CALL(delegate(), LogMediaSessionActionButtonPressed(_, _)).Times(0);
  EXPECT_EQ(0, controller().next_track_count());

  item().Freeze(base::DoNothing());
  item().OnMediaSessionActionButtonPressed(MediaSessionAction::kNextTrack);
  item().FlushForTesting();

  EXPECT_EQ(0, controller().next_track_count());
}

TEST_F(MediaSessionNotificationItemTest, UpdatesViewWithActions) {
  EXPECT_CALL(view(), UpdateWithMediaActions(_))
      .WillOnce(testing::Invoke(
          [](const base::flat_set<MediaSessionAction>& actions) {
            EXPECT_EQ(2u, actions.size());
            EXPECT_TRUE(actions.contains(MediaSessionAction::kPlay));
            EXPECT_TRUE(actions.contains(MediaSessionAction::kPause));
          }));
  item().MediaSessionActionsChanged(
      {MediaSessionAction::kPlay, MediaSessionAction::kPause});
}

TEST_F(MediaSessionNotificationItemTest, UnfreezingDoesntMissUpdates) {
  item().MediaSessionActionsChanged(
      {MediaSessionAction::kPlay, MediaSessionAction::kPause});

  // Freeze the item and clear the metadata.
  base::MockOnceClosure unfrozen_callback;
  EXPECT_CALL(unfrozen_callback, Run).Times(0);
  EXPECT_CALL(view(), UpdateWithMediaSessionInfo(_)).Times(0);
  EXPECT_CALL(view(), UpdateWithMediaMetadata(_)).Times(0);
  item().Freeze(unfrozen_callback.Get());
  item().MediaSessionInfoChanged(nullptr);
  item().MediaSessionMetadataChanged(absl::nullopt);

  // The item should be frozen.
  EXPECT_TRUE(item().frozen());

  // Bind the item to a new controller that's playing instead of paused.
  auto new_media_controller =
      std::make_unique<media_session::test::TestMediaController>();
  auto session_info = media_session::mojom::MediaSessionInfo::New();
  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPlaying;
  session_info->is_controllable = true;
  item().SetController(new_media_controller->CreateMediaControllerRemote(),
                       session_info.Clone());

  // The item will receive a MediaSessionInfoChanged.
  item().MediaSessionInfoChanged(session_info.Clone());

  // The item should still be frozen, and the view should contain the old data.
  EXPECT_TRUE(item().frozen());
  testing::Mock::VerifyAndClearExpectations(&unfrozen_callback);
  testing::Mock::VerifyAndClearExpectations(&view());

  // Update the metadata.
  EXPECT_CALL(unfrozen_callback, Run);
  EXPECT_CALL(view(), UpdateWithMediaMetadata(_));
  media_session::MediaMetadata metadata;
  metadata.title = u"title2";
  metadata.artist = u"artist2";
  item().MediaSessionMetadataChanged(metadata);

  // The item should no longer be frozen, and we should see the updated data.
  EXPECT_FALSE(item().frozen());
  testing::Mock::VerifyAndClearExpectations(&unfrozen_callback);
  testing::Mock::VerifyAndClearExpectations(&view());
}

TEST_F(MediaSessionNotificationItemTest, UnfreezingWaitsForArtwork_Timeout) {
  item().MediaSessionActionsChanged(
      {MediaSessionAction::kPlay, MediaSessionAction::kPause});

  // Set an image before freezing.
  EXPECT_CALL(view(), UpdateWithMediaArtwork(_));
  SkBitmap initial_image;
  initial_image.allocN32Pixels(10, 10);
  initial_image.eraseColor(SK_ColorMAGENTA);
  item().MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, initial_image);
  testing::Mock::VerifyAndClearExpectations(&view());

  // Freeze the item and clear the metadata.
  base::MockOnceClosure unfrozen_callback;
  EXPECT_CALL(unfrozen_callback, Run).Times(0);
  EXPECT_CALL(view(), UpdateWithMediaSessionInfo(_)).Times(0);
  EXPECT_CALL(view(), UpdateWithMediaMetadata(_)).Times(0);
  EXPECT_CALL(view(), UpdateWithMediaArtwork(_)).Times(0);
  item().Freeze(unfrozen_callback.Get());
  item().MediaSessionInfoChanged(nullptr);
  item().MediaSessionMetadataChanged(absl::nullopt);
  item().MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, SkBitmap());

  // The item should be frozen and the view should contain the old data.
  EXPECT_TRUE(item().frozen());

  // Bind the item to a new controller that's playing instead of paused.
  auto new_media_controller =
      std::make_unique<media_session::test::TestMediaController>();
  auto session_info = media_session::mojom::MediaSessionInfo::New();
  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPlaying;
  session_info->is_controllable = true;
  item().SetController(new_media_controller->CreateMediaControllerRemote(),
                       session_info.Clone());

  // The item will receive a MediaSessionInfoChanged.
  item().MediaSessionInfoChanged(session_info.Clone());

  // The item should still be frozen, and the view should contain the old data.
  EXPECT_TRUE(item().frozen());

  // Update the metadata.
  media_session::MediaMetadata metadata;
  metadata.title = u"title2";
  metadata.artist = u"artist2";
  item().MediaSessionMetadataChanged(metadata);

  // The item should still be frozen, and waiting for a new image.
  EXPECT_TRUE(item().frozen());
  testing::Mock::VerifyAndClearExpectations(&unfrozen_callback);
  testing::Mock::VerifyAndClearExpectations(&view());

  // Once the freeze timer fires, the item should unfreeze even if there's no
  // artwork.
  EXPECT_CALL(unfrozen_callback, Run);
  EXPECT_CALL(view(), UpdateWithMediaSessionInfo(_));
  EXPECT_CALL(view(), UpdateWithMediaMetadata(_));
  EXPECT_CALL(view(), UpdateWithMediaArtwork(_));
  AdvanceClockMilliseconds(2600);

  EXPECT_FALSE(item().frozen());
  testing::Mock::VerifyAndClearExpectations(&unfrozen_callback);
  testing::Mock::VerifyAndClearExpectations(&view());
}

TEST_F(MediaSessionNotificationItemTest, UnfreezingWaitsForActions) {
  item().MediaSessionActionsChanged(
      {MediaSessionAction::kPlay, MediaSessionAction::kPause,
       MediaSessionAction::kNextTrack, MediaSessionAction::kPreviousTrack});

  // Freeze the item and clear the metadata and actions.
  base::MockOnceClosure unfrozen_callback;
  EXPECT_CALL(unfrozen_callback, Run).Times(0);
  EXPECT_CALL(view(), UpdateWithMediaSessionInfo(_)).Times(0);
  EXPECT_CALL(view(), UpdateWithMediaMetadata(_)).Times(0);
  EXPECT_CALL(view(), UpdateWithMediaActions(_)).Times(0);

  item().Freeze(unfrozen_callback.Get());
  item().MediaSessionInfoChanged(nullptr);
  item().MediaSessionMetadataChanged(absl::nullopt);
  item().MediaSessionActionsChanged({});

  // The item should be frozen and the view should contain the old data.
  EXPECT_TRUE(item().frozen());

  // Bind the item to a new controller that's playing instead of paused.
  auto new_media_controller =
      std::make_unique<media_session::test::TestMediaController>();
  auto session_info = media_session::mojom::MediaSessionInfo::New();
  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPlaying;
  session_info->is_controllable = true;
  item().SetController(new_media_controller->CreateMediaControllerRemote(),
                       session_info.Clone());

  // The item will receive a MediaSessionInfoChanged.
  item().MediaSessionInfoChanged(session_info.Clone());

  // The item should still be frozen, and the view should contain the old data.
  EXPECT_TRUE(item().frozen());

  // Update the metadata.
  media_session::MediaMetadata metadata;
  metadata.title = u"title2";
  metadata.artist = u"artist2";
  item().MediaSessionMetadataChanged(metadata);

  // The item should still be frozen, and waiting for new actions.
  EXPECT_TRUE(item().frozen());
  testing::Mock::VerifyAndClearExpectations(&unfrozen_callback);
  testing::Mock::VerifyAndClearExpectations(&view());

  // Once we receive actions, the item should unfreeze.
  EXPECT_CALL(unfrozen_callback, Run);
  EXPECT_CALL(view(), UpdateWithMediaSessionInfo(_));
  EXPECT_CALL(view(), UpdateWithMediaMetadata(_));
  EXPECT_CALL(view(), UpdateWithMediaActions(_));

  item().MediaSessionActionsChanged(
      {MediaSessionAction::kPlay, MediaSessionAction::kPause,
       MediaSessionAction::kSeekForward, MediaSessionAction::kSeekBackward});

  EXPECT_FALSE(item().frozen());
  testing::Mock::VerifyAndClearExpectations(&unfrozen_callback);
  testing::Mock::VerifyAndClearExpectations(&view());
}

TEST_F(MediaSessionNotificationItemTest,
       UnfreezingWaitsForArtwork_ReceiveArtwork) {
  item().MediaSessionActionsChanged(
      {MediaSessionAction::kPlay, MediaSessionAction::kPause});

  // Set an image before freezing.
  EXPECT_CALL(view(), UpdateWithMediaArtwork(_));
  SkBitmap initial_image;
  initial_image.allocN32Pixels(10, 10);
  initial_image.eraseColor(SK_ColorMAGENTA);
  item().MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, initial_image);
  testing::Mock::VerifyAndClearExpectations(&view());

  // Freeze the item and clear the metadata.
  base::MockOnceClosure unfrozen_callback;
  EXPECT_CALL(unfrozen_callback, Run).Times(0);
  EXPECT_CALL(view(), UpdateWithMediaSessionInfo(_)).Times(0);
  EXPECT_CALL(view(), UpdateWithMediaMetadata(_)).Times(0);
  EXPECT_CALL(view(), UpdateWithMediaArtwork(_)).Times(0);
  item().Freeze(unfrozen_callback.Get());
  item().MediaSessionInfoChanged(nullptr);
  item().MediaSessionMetadataChanged(absl::nullopt);
  item().MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, SkBitmap());

  // The item should be frozen and the view should contain the old data.
  EXPECT_TRUE(item().frozen());

  // Bind the item to a new controller that's playing instead of paused.
  auto new_media_controller =
      std::make_unique<media_session::test::TestMediaController>();
  auto session_info = media_session::mojom::MediaSessionInfo::New();
  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPlaying;
  session_info->is_controllable = true;
  item().SetController(new_media_controller->CreateMediaControllerRemote(),
                       session_info.Clone());

  // The item will receive a MediaSessionInfoChanged.
  item().MediaSessionInfoChanged(session_info.Clone());

  // The item should still be frozen, and the view should contain the old data.
  EXPECT_TRUE(item().frozen());

  // Update the metadata.
  media_session::MediaMetadata metadata;
  metadata.title = u"title2";
  metadata.artist = u"artist2";
  item().MediaSessionMetadataChanged(metadata);

  // The item should still be frozen, and waiting for a new image.
  EXPECT_TRUE(item().frozen());
  testing::Mock::VerifyAndClearExpectations(&unfrozen_callback);
  testing::Mock::VerifyAndClearExpectations(&view());

  // Once we receive artwork, the item should unfreeze.
  EXPECT_CALL(unfrozen_callback, Run);
  EXPECT_CALL(view(), UpdateWithMediaSessionInfo(_));
  EXPECT_CALL(view(), UpdateWithMediaMetadata(_));
  EXPECT_CALL(view(), UpdateWithMediaArtwork(_));
  SkBitmap new_image;
  new_image.allocN32Pixels(10, 10);
  new_image.eraseColor(SK_ColorYELLOW);
  item().MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, new_image);

  EXPECT_FALSE(item().frozen());
  testing::Mock::VerifyAndClearExpectations(&unfrozen_callback);
  testing::Mock::VerifyAndClearExpectations(&view());
}

}  // namespace global_media_controls
