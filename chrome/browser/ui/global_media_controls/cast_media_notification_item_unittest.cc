// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/cast_media_notification_item.h"

#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "chrome/test/base/testing_profile.h"
#include "components/media_message_center/media_notification_controller.h"
#include "components/media_message_center/media_notification_view.h"
#include "components/media_router/common/media_route.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/test/browser_task_environment.h"
#include "net/url_request/referrer_policy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/vector_icon_types.h"

using media_router::mojom::MediaStatus;
using media_session::mojom::MediaPlaybackState;
using media_session::mojom::MediaSessionAction;
using media_session::mojom::MediaSessionInfo;
using media_session::mojom::MediaSessionInfoPtr;
using testing::_;
using testing::AtLeast;

namespace {

constexpr char kRouteDesc[] = "My App";
constexpr char kRouteId[] = "route_id";
constexpr char kSinkName[] = "My Sink";

media_router::MediaRoute CreateMediaRoute() {
  media_router::MediaRoute route(
      kRouteId, media_router::MediaSource("source_id"), "sink_id", kRouteDesc,
      /* is_local */ true, /* for_display */ true);
  route.set_media_sink_name(kSinkName);
  return route;
}

class MockBitmapFetcher : public BitmapFetcher {
 public:
  MockBitmapFetcher(const GURL& url,
                    BitmapFetcherDelegate* delegate,
                    const net::NetworkTrafficAnnotationTag& traffic_annotation)
      : BitmapFetcher(url, delegate, traffic_annotation) {}
  ~MockBitmapFetcher() override = default;

  MOCK_METHOD3(Init,
               void(const std::string& referrer,
                    net::ReferrerPolicy referrer_policy,
                    network::mojom::CredentialsMode credentials_mode));
  MOCK_METHOD1(Start, void(network::mojom::URLLoaderFactory* loader_factory));
};

class MockMediaNotificationController
    : public media_message_center::MediaNotificationController {
 public:
  MOCK_METHOD(void, ShowNotification, (const std::string&));
  MOCK_METHOD(void, HideNotification, (const std::string&));
  MOCK_METHOD(void, RemoveItem, (const std::string&));
  MOCK_METHOD(scoped_refptr<base::SequencedTaskRunner>,
              GetTaskRunner,
              (),
              (const));
  MOCK_METHOD(void,
              LogMediaSessionActionButtonPressed,
              (const std::string&, MediaSessionAction));
};

class MockMediaNotificationView
    : public media_message_center::MediaNotificationView {
 public:
  MOCK_METHOD1(SetExpanded, void(bool));
  MOCK_METHOD2(UpdateCornerRadius, void(int, int));
  MOCK_METHOD1(SetForcedExpandedState, void(bool*));
  MOCK_METHOD1(UpdateWithMediaSessionInfo, void(const MediaSessionInfoPtr&));
  MOCK_METHOD1(UpdateWithMediaMetadata,
               void(const media_session::MediaMetadata&));
  MOCK_METHOD1(UpdateWithMediaActions,
               void(const base::flat_set<MediaSessionAction>&));
  MOCK_METHOD1(UpdateWithMediaArtwork, void(const gfx::ImageSkia&));
  MOCK_METHOD1(UpdateWithFavicon, void(const gfx::ImageSkia&));
  MOCK_METHOD1(UpdateWithVectorIcon, void(const gfx::VectorIcon& vector_icon));
  MOCK_METHOD1(UpdateDeviceSelectorAvailability, void(bool availability));
};

class MockSessionController : public CastMediaSessionController {
 public:
  MockSessionController(
      mojo::Remote<media_router::mojom::MediaController> remote)
      : CastMediaSessionController(std::move(remote)) {}

  MOCK_METHOD1(Send, void(media_session::mojom::MediaSessionAction));
  MOCK_METHOD1(OnMediaStatusUpdated, void(media_router::mojom::MediaStatusPtr));
};

}  // namespace

class CastMediaNotificationItemTest : public testing::Test {
 public:
  void SetUp() override {
    auto session_controller = std::make_unique<MockSessionController>(
        mojo::Remote<media_router::mojom::MediaController>());
    session_controller_ = session_controller.get();
    item_ = std::make_unique<CastMediaNotificationItem>(
        CreateMediaRoute(), &notification_controller_,
        std::move(session_controller), &profile_);
    item_->set_bitmap_fetcher_factory_for_testing_(
        base::BindRepeating(&CastMediaNotificationItemTest::CreateBitmapFetcher,
                            base::Unretained(this)));
  }

  void SetView() {
    EXPECT_CALL(view_, UpdateWithVectorIcon(_))
        .WillOnce([](const gfx::VectorIcon& vector_icon) {
          EXPECT_EQ(vector_icons::kMediaRouterIdleIcon.reps, vector_icon.reps);
        });
    EXPECT_CALL(view_, UpdateWithMediaSessionInfo(_))
        .WillOnce([&](const MediaSessionInfoPtr& session_info) {
          EXPECT_EQ(MediaSessionInfo::SessionState::kSuspended,
                    session_info->state);
          EXPECT_FALSE(session_info->force_duck);
          EXPECT_EQ(MediaPlaybackState::kPaused, session_info->playback_state);
          EXPECT_TRUE(session_info->is_controllable);
          EXPECT_FALSE(session_info->prefer_stop_for_gain_focus_loss);
        });
    EXPECT_CALL(view_, UpdateWithMediaActions(_))
        .WillOnce([&](const base::flat_set<MediaSessionAction>& actions) {
          EXPECT_EQ(0u, actions.size());
        });
    EXPECT_CALL(view_, UpdateWithMediaMetadata(_))
        .WillOnce([&](const media_session::MediaMetadata& metadata) {
          const std::string separator = " \xC2\xB7 ";
          EXPECT_EQ(base::UTF8ToUTF16(kRouteDesc + separator + kSinkName),
                    metadata.source_title);
        });
    item_->SetView(&view_);
    testing::Mock::VerifyAndClearExpectations(&view_);
  }

 protected:
  MOCK_METHOD3(CreateBitmapFetcher,
               std::unique_ptr<BitmapFetcher>(
                   const GURL& url,
                   BitmapFetcherDelegate* delegate,
                   const net::NetworkTrafficAnnotationTag& traffic_annotation));

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  MockMediaNotificationController notification_controller_;
  MockSessionController* session_controller_ = nullptr;
  MockMediaNotificationView view_;
  std::unique_ptr<CastMediaNotificationItem> item_;
};

TEST_F(CastMediaNotificationItemTest, UpdateSessionInfo) {
  SetView();
  auto status = MediaStatus::New();
  status->play_state = MediaStatus::PlayState::PLAYING;
  EXPECT_CALL(view_, UpdateWithMediaSessionInfo(_))
      .WillOnce([&](const MediaSessionInfoPtr& session_info) {
        EXPECT_EQ(MediaSessionInfo::SessionState::kActive, session_info->state);
        EXPECT_EQ(MediaPlaybackState::kPlaying, session_info->playback_state);
      });
  item_->OnMediaStatusUpdated(std::move(status));
  testing::Mock::VerifyAndClearExpectations(&view_);

  status = MediaStatus::New();
  status->play_state = MediaStatus::PlayState::PAUSED;
  EXPECT_CALL(view_, UpdateWithMediaSessionInfo(_))
      .WillOnce([&](const MediaSessionInfoPtr& session_info) {
        EXPECT_EQ(MediaSessionInfo::SessionState::kSuspended,
                  session_info->state);
        EXPECT_EQ(MediaPlaybackState::kPaused, session_info->playback_state);
      });
  item_->OnMediaStatusUpdated(std::move(status));
}

TEST_F(CastMediaNotificationItemTest, UpdateMetadata) {
  SetView();
  auto status = MediaStatus::New();
  const std::string title = "my title";
  const std::string secondary_title = "my artist";
  status->title = title;
  status->secondary_title = secondary_title;
  EXPECT_CALL(view_, UpdateWithMediaMetadata(_))
      .WillOnce([&](const media_session::MediaMetadata& metadata) {
        EXPECT_EQ(base::UTF8ToUTF16(title), metadata.title);
        EXPECT_EQ(base::UTF8ToUTF16(secondary_title), metadata.artist);
      });
  item_->OnMediaStatusUpdated(std::move(status));
}

TEST_F(CastMediaNotificationItemTest, UpdateActions) {
  SetView();
  auto status = MediaStatus::New();
  status->can_play_pause = true;
  status->can_skip_to_next_track = true;
  status->can_skip_to_previous_track = true;
  EXPECT_CALL(view_, UpdateWithMediaActions(_))
      .WillOnce([&](const base::flat_set<MediaSessionAction>& actions) {
        EXPECT_TRUE(actions.contains(MediaSessionAction::kPlay));
        EXPECT_TRUE(actions.contains(MediaSessionAction::kPause));
        EXPECT_TRUE(actions.contains(MediaSessionAction::kNextTrack));
        EXPECT_TRUE(actions.contains(MediaSessionAction::kPreviousTrack));

        EXPECT_FALSE(actions.contains(MediaSessionAction::kSeekBackward));
        EXPECT_FALSE(actions.contains(MediaSessionAction::kSeekForward));
        EXPECT_FALSE(actions.contains(MediaSessionAction::kSeekTo));
      });
  item_->OnMediaStatusUpdated(std::move(status));
  testing::Mock::VerifyAndClearExpectations(&view_);

  status = MediaStatus::New();
  status->can_seek = true;
  EXPECT_CALL(view_, UpdateWithMediaActions(_))
      .WillOnce([&](const base::flat_set<MediaSessionAction>& actions) {
        EXPECT_TRUE(actions.contains(MediaSessionAction::kSeekBackward));
        EXPECT_TRUE(actions.contains(MediaSessionAction::kSeekForward));
        EXPECT_TRUE(actions.contains(MediaSessionAction::kSeekTo));

        EXPECT_FALSE(actions.contains(MediaSessionAction::kPlay));
        EXPECT_FALSE(actions.contains(MediaSessionAction::kPause));
        EXPECT_FALSE(actions.contains(MediaSessionAction::kNextTrack));
        EXPECT_FALSE(actions.contains(MediaSessionAction::kPreviousTrack));
      });
  item_->OnMediaStatusUpdated(std::move(status));
}

TEST_F(CastMediaNotificationItemTest, SetViewToNull) {
  SetView();
  item_->SetView(nullptr);
}

TEST_F(CastMediaNotificationItemTest, HideNotificationOnDismiss) {
  EXPECT_CALL(notification_controller_, HideNotification(kRouteId))
      .Times(AtLeast(1));
  item_->Dismiss();
}

TEST_F(CastMediaNotificationItemTest, HideNotificationOnDelete) {
  EXPECT_CALL(notification_controller_, HideNotification(kRouteId));
  item_.reset();
}

TEST_F(CastMediaNotificationItemTest, SendMediaStatusToController) {
  auto status = MediaStatus::New();
  status->can_play_pause = true;
  EXPECT_CALL(*session_controller_, OnMediaStatusUpdated(_))
      .WillOnce([](const media_router::mojom::MediaStatusPtr& media_status) {
        EXPECT_TRUE(media_status->can_play_pause);
      });
  item_->OnMediaStatusUpdated(std::move(status));
}

TEST_F(CastMediaNotificationItemTest, SendActionToController) {
  auto status = MediaStatus::New();
  item_->OnMediaStatusUpdated(std::move(status));

  EXPECT_CALL(*session_controller_, Send(MediaSessionAction::kPlay));
  item_->OnMediaSessionActionButtonPressed(MediaSessionAction::kPlay);
}

TEST_F(CastMediaNotificationItemTest, DownloadImage) {
  SetView();
  GURL image_url("https://example.com/image.png");
  gfx::Size image_size(123, 456);
  auto image = media_router::mojom::MediaImage::New();
  image->url = image_url;
  image->size = image_size;
  auto status = MediaStatus::New();
  status->images.push_back(std::move(image));

  BitmapFetcherDelegate* bitmap_fetcher_delegate = nullptr;
  EXPECT_CALL(*this, CreateBitmapFetcher(_, _, _))
      .WillOnce(
          [&](const GURL& url, BitmapFetcherDelegate* delegate,
              const net::NetworkTrafficAnnotationTag& traffic_annotation) {
            auto bitmap_fetcher = std::make_unique<MockBitmapFetcher>(
                url, delegate, traffic_annotation);
            bitmap_fetcher_delegate = delegate;

            EXPECT_EQ(url, image_url);
            EXPECT_CALL(*bitmap_fetcher, Init(_, _, _));
            EXPECT_CALL(*bitmap_fetcher, Start(_));
            return bitmap_fetcher;
          });
  item_->OnMediaStatusUpdated(std::move(status));

  SkBitmap bitmap;
  EXPECT_CALL(view_, UpdateWithMediaArtwork(_));
  bitmap_fetcher_delegate->OnFetchComplete(image_url, &bitmap);
}
