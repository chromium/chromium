// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/global_media_controls/public/media_session_notification_item.h"

#include <memory>
#include <utility>

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "components/global_media_controls/public/test/mock_media_session_notification_item_delegate.h"
#include "components/media_message_center/mock_media_notification_view.h"
#include "media/base/media_switches.h"
#include "services/media_session/public/cpp/test/test_media_controller.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using media_session::mojom::MediaSessionAction;
using testing::_;
using testing::NiceMock;

namespace global_media_controls {

namespace {

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
        &delegate_, kRequestId, std::string(), source_id_,
        controller_.CreateMediaControllerRemote(), std::move(session_info));
    item_->SetView(&view_);
  }

  media_message_center::test::MockMediaNotificationView& view() {
    return view_;
  }

  test::MockMediaSessionNotificationItemDelegate& delegate() {
    return delegate_;
  }

  media_session::test::TestMediaController& controller() { return controller_; }

  MediaSessionNotificationItem& item() { return *item_; }

  base::UnguessableToken source_id() { return source_id_; }

  void AdvanceClockMilliseconds(int milliseconds) {
    task_environment_.FastForwardBy(base::Milliseconds(milliseconds));
  }

 private:
  NiceMock<media_message_center::test::MockMediaNotificationView> view_;
  NiceMock<test::MockMediaSessionNotificationItemDelegate> delegate_;
  media_session::test::TestMediaController controller_;
  std::unique_ptr<MediaSessionNotificationItem> item_;
  base::UnguessableToken source_id_{base::UnguessableToken::Create()};

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(MediaSessionNotificationItemTest, Freezing_DoNotUpdateMetadata) {
  media_session::MediaMetadata metadata;
  metadata.title = u"title2";
  metadata.artist = u"artist2";
  metadata.album = u"album";

  std::vector<media_session::ChapterInformation> expected_chapters;
  media_session::MediaImage test_image_1;
  test_image_1.src = GURL("https://www.google.com");
  media_session::ChapterInformation test_chapter_1(
      /*title=*/u"chapter1", /*start_time=*/base::Seconds(10),
      /*artwork=*/{test_image_1});
  expected_chapters.push_back(test_chapter_1);
  metadata.chapters = expected_chapters;

  EXPECT_CALL(view(), UpdateWithMediaMetadata(_)).Times(0);
  item().Freeze(base::DoNothing());
  item().MediaSessionMetadataChanged(metadata);
}

TEST_F(MediaSessionNotificationItemTest,
       UpdateMetadataOriginWithPresentationRequestOrigin) {
  media_session::MediaMetadata metadata;
  metadata.source_title = u"source_title_test";

  EXPECT_CALL(view(), UpdateWithMediaMetadata(metadata)).Times(1);
  item().MediaSessionMetadataChanged(metadata);

  media_session::MediaMetadata updated_metadata;
  updated_metadata.source_title = u"example.com";

  EXPECT_CALL(view(), UpdateWithMediaMetadata(updated_metadata)).Times(2);
  item().UpdatePresentationRequestOrigin(
      url::Origin::Create(GURL("https://example.com")));
  // Make sure presentation request origin persists for the duration of the view
  // despite the update of metadata.
  item().MediaSessionMetadataChanged(metadata);
  item().SetView(nullptr);

  // Make sure that presentation request origin was reset after the view is set
  // to null in SetView().
  EXPECT_CALL(view(), UpdateWithMediaMetadata(metadata)).Times(1);
  item().SetView(&view());
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

TEST_F(MediaSessionNotificationItemTest, Freezing_DoNotUpdateChapterImage) {
  SkBitmap image;
  image.allocN32Pixels(10, 10);
  image.eraseColor(SK_ColorMAGENTA);

  EXPECT_CALL(view(), UpdateWithChapterArtwork(_, _)).Times(0);
  item().Freeze(base::DoNothing());
  item().MediaControllerChapterImageChanged(0, image);
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
  item().MediaSessionMetadataChanged(std::nullopt);

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

TEST_F(MediaSessionNotificationItemTest, SemiUnfreezesWithoutArtwork_Timeout) {
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

  // Set an chapter image before freezing.
  EXPECT_CALL(view(), UpdateWithChapterArtwork(_, _));
  SkBitmap initial_chapter_image;
  initial_chapter_image.allocN32Pixels(10, 10);
  initial_chapter_image.eraseColor(SK_ColorMAGENTA);
  item().MediaControllerChapterImageChanged(0, initial_chapter_image);
  testing::Mock::VerifyAndClearExpectations(&view());

  // Freeze the item and clear the metadata.
  base::MockOnceClosure unfrozen_callback;
  EXPECT_CALL(unfrozen_callback, Run).Times(0);
  EXPECT_CALL(view(), UpdateWithMediaSessionInfo(_)).Times(0);
  EXPECT_CALL(view(), UpdateWithMediaMetadata(_)).Times(0);
  EXPECT_CALL(view(), UpdateWithMediaArtwork(_)).Times(0);
  EXPECT_CALL(view(), UpdateWithChapterArtwork(_, _)).Times(0);
  item().Freeze(unfrozen_callback.Get());
  item().MediaSessionInfoChanged(nullptr);
  item().MediaSessionMetadataChanged(std::nullopt);
  item().MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, SkBitmap());
  item().MediaControllerChapterImageChanged(0, SkBitmap());

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

  // Update the metadata. This should unfreeze everything except the artwork.
  EXPECT_CALL(unfrozen_callback, Run);
  EXPECT_CALL(view(), UpdateWithMediaSessionInfo(_));
  EXPECT_CALL(view(), UpdateWithMediaMetadata(_));
  EXPECT_CALL(view(), UpdateWithMediaArtwork(_)).Times(0);
  EXPECT_CALL(view(), UpdateWithChapterArtwork(_, _)).Times(0);
  media_session::MediaMetadata metadata;
  metadata.title = u"title2";
  metadata.artist = u"artist2";
  item().MediaSessionMetadataChanged(metadata);

  // The item should no longer be frozen, but will be waiting for a new image.
  EXPECT_FALSE(item().frozen());
  testing::Mock::VerifyAndClearExpectations(&unfrozen_callback);
  testing::Mock::VerifyAndClearExpectations(&view());

  // Once the freeze timer fires, the artwork should unfreeze even if there's no
  // artwork. Since we've received no artwork, the artwork should be null.
  EXPECT_CALL(view(), UpdateWithMediaArtwork(_))
      .WillOnce(testing::Invoke(
          [](const gfx::ImageSkia& image) { EXPECT_TRUE(image.isNull()); }));
  AdvanceClockMilliseconds(2600);
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
  item().MediaSessionMetadataChanged(std::nullopt);
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
       SemiUnfreezesWithoutArtwork_ReceiveArtwork) {
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
  item().MediaSessionMetadataChanged(std::nullopt);
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

  // Update the metadata. This should unfreeze everything except the artwork.
  EXPECT_CALL(unfrozen_callback, Run);
  EXPECT_CALL(view(), UpdateWithMediaSessionInfo(_));
  EXPECT_CALL(view(), UpdateWithMediaMetadata(_));
  EXPECT_CALL(view(), UpdateWithMediaArtwork(_)).Times(0);
  media_session::MediaMetadata metadata;
  metadata.title = u"title2";
  metadata.artist = u"artist2";
  item().MediaSessionMetadataChanged(metadata);

  // The item should no longer be frozen, but will be waiting for a new image.
  EXPECT_FALSE(item().frozen());
  testing::Mock::VerifyAndClearExpectations(&unfrozen_callback);
  testing::Mock::VerifyAndClearExpectations(&view());

  // Once we receive artwork, the artwork should unfreeze.
  EXPECT_CALL(view(), UpdateWithMediaArtwork(_));
  SkBitmap new_image;
  new_image.allocN32Pixels(10, 10);
  new_image.eraseColor(SK_ColorYELLOW);
  item().MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, new_image);
  testing::Mock::VerifyAndClearExpectations(&view());
}

TEST_F(MediaSessionNotificationItemTest, RequestMediaRemoting) {
  EXPECT_EQ(0, controller().request_media_remoting_count());

  item().RequestMediaRemoting();
  item().FlushForTesting();

  EXPECT_EQ(1, controller().request_media_remoting_count());
}

TEST_F(MediaSessionNotificationItemTest, GetMediaSessionActions) {
  item().MediaSessionActionsChanged(
      {MediaSessionAction::kPlay, MediaSessionAction::kPause,
       MediaSessionAction::kEnterPictureInPicture});
  EXPECT_TRUE(item().GetMediaSessionActions().contains(
      MediaSessionAction::kEnterPictureInPicture));

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kMediaRemotingWithoutFullscreen);

  auto session_info = media_session::mojom::MediaSessionInfo::New();
  auto remote_playback_metadata =
      media_session::mojom::RemotePlaybackMetadata::New(
          "video_codec", "audio_codec", false, true, "device_friendly_name",
          false);
  session_info->remote_playback_metadata = std::move(remote_playback_metadata);
  item().MediaSessionInfoChanged(std::move(session_info));
  EXPECT_FALSE(item().GetMediaSessionActions().contains(
      MediaSessionAction::kEnterPictureInPicture));
}

TEST_F(MediaSessionNotificationItemTest, GetSessionMetadata) {
  media_session::MediaMetadata metadata;
  metadata.source_title = u"source_title";
  item().MediaSessionMetadataChanged(metadata);
  EXPECT_EQ(u"source_title", item().GetSessionMetadata().source_title);

  base::test::ScopedFeatureList feature_list;
#if BUILDFLAG(IS_CHROMEOS)
  feature_list.InitAndEnableFeature(media::kMediaRemotingWithoutFullscreen);
#else
  feature_list.InitWithFeatures({media::kMediaRemotingWithoutFullscreen},
                                {media::kGlobalMediaControlsUpdatedUI});
#endif

  auto session_info = media_session::mojom::MediaSessionInfo::New();
  auto remote_playback_metadata =
      media_session::mojom::RemotePlaybackMetadata::New(
          "video_codec", "audio_codec", false, true, "device_friendly_name",
          false);
  session_info->remote_playback_metadata = std::move(remote_playback_metadata);
  item().MediaSessionInfoChanged(std::move(session_info));
  item().UpdateDeviceName("device_friendly_name");

  EXPECT_EQ(u"source_title \xB7 device_friendly_name",
            item().GetSessionMetadata().source_title);
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(MediaSessionNotificationItemTest, GetSessionMetadataForUpdatedUI) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kGlobalMediaControlsUpdatedUI);

  media_session::MediaMetadata metadata;
  metadata.source_title = u"source_title";
  item().MediaSessionMetadataChanged(metadata);
  item().UpdateDeviceName("device_friendly_name");
  EXPECT_EQ(u"source_title", item().GetSessionMetadata().source_title);
}
#endif

TEST_F(MediaSessionNotificationItemTest, GetRemotePlaybackMetadata) {
  auto session_info = media_session::mojom::MediaSessionInfo::New();
  item().MediaSessionInfoChanged(session_info.Clone());
  EXPECT_TRUE(item().GetRemotePlaybackMetadata().is_null());

  // Remote Playback disabled.
  session_info->remote_playback_metadata =
      media_session::mojom::RemotePlaybackMetadata::New(
          "video_codec", "audio_codec", /* remote_playback_disabled */ true,
          /* remote_playback_started */ true, "device_friendly_name",
          /* is_encrypted_media */ false);
  item().MediaSessionInfoChanged(session_info.Clone());
  EXPECT_TRUE(item().GetRemotePlaybackMetadata().is_null());

  // Encrypted media.
  session_info->remote_playback_metadata =
      media_session::mojom::RemotePlaybackMetadata::New(
          "video_codec", "audio_codec", /* remote_playback_disabled */ false,
          /* remote_playback_started */ true, "device_friendly_name",
          /* is_encrypted_media */ true);
  item().MediaSessionInfoChanged(session_info.Clone());
  EXPECT_TRUE(item().GetRemotePlaybackMetadata().is_null());

  // All criteria are met.
  session_info->remote_playback_metadata =
      media_session::mojom::RemotePlaybackMetadata::New(
          "video_codec", "audio_codec", /* remote_playback_disabled */ false,
          /* remote_playback_started */ true, "device_friendly_name",
          /* is_encrypted_media */ false);
  item().MediaSessionInfoChanged(session_info.Clone());
  EXPECT_FALSE(item().GetRemotePlaybackMetadata().is_null());

  // Content's duration is too short.
  item().MediaSessionPositionChanged(media_session::MediaPosition(
      /*playback_rate=*/1, /*duration=*/base::Seconds(10),
      /*position=*/base::Seconds(1), /*end_of_media=*/false));
  EXPECT_TRUE(item().GetRemotePlaybackMetadata().is_null());
}

TEST_F(MediaSessionNotificationItemTest, GetSourceId) {
  EXPECT_EQ(source_id(), *item().GetSourceId());
}

TEST_F(MediaSessionNotificationItemTest, ShouldShowNotification) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kMediaRemotingWithoutFullscreen);

  media_session::MediaMetadata metadata;
  metadata.title = u"title";
  item().MediaSessionMetadataChanged(metadata);

  // Hide uncontrollable sessions.
  auto session_info = media_session::mojom::MediaSessionInfo::New();
  item().MediaSessionInfoChanged(mojo::Clone(session_info));
  EXPECT_FALSE(item().ShouldShowNotification());
  session_info->is_controllable = true;
  item().MediaSessionInfoChanged(mojo::Clone(session_info));
  EXPECT_TRUE(item().ShouldShowNotification());

  // Hide sessions with Cast presentation.
  session_info->has_presentation = true;
  item().MediaSessionInfoChanged(mojo::Clone(session_info));
  EXPECT_FALSE(item().ShouldShowNotification());

  // Show sessions with Remote Playback presentation.
  session_info->remote_playback_metadata =
      media_session::mojom::RemotePlaybackMetadata::New(
          "video_codec", "audio_codec", /* remote_playback_disabled */ false,
          /* remote_playback_started */ true, "device_friendly_name",
          /* is_encrypted_media */ false);
  item().MediaSessionInfoChanged(mojo::Clone(session_info));
  EXPECT_TRUE(item().ShouldShowNotification());
}

}  // namespace global_media_controls
