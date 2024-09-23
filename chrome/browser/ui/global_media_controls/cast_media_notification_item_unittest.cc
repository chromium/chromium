// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/cast_media_notification_item.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "chrome/browser/media/router/chrome_media_router_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/global_media_controls/public/test/mock_media_item_manager.h"
#include "components/media_message_center/mock_media_notification_view.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "components/media_router/common/media_route.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/test/browser_task_environment.h"
#include "media/base/media_switches.h"
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
      /* is_local */ true);
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

  MOCK_METHOD(void,
              Init,
              (net::ReferrerPolicy referrer_policy,
               network::mojom::CredentialsMode credentials_mode,
               const net::HttpRequestHeaders& additional_headers,
               const url::Origin& initiator),
              (override));
  MOCK_METHOD(void,
              Start,
              (network::mojom::URLLoaderFactory * loader_factory),
              (override));
};

class MockSessionController : public CastMediaSessionController {
 public:
  MockSessionController(
      mojo::Remote<media_router::mojom::MediaController> remote)
      : CastMediaSessionController(std::move(remote)) {}

  MOCK_METHOD(void, Send, (media_session::mojom::MediaSessionAction));
  MOCK_METHOD(void,
              OnMediaStatusUpdated,
              (media_router::mojom::MediaStatusPtr));
  MOCK_METHOD(void, SeekTo, (base::TimeDelta));
  MOCK_METHOD(void, SetVolume, (float));
  MOCK_METHOD(void, SetMute, (bool));
};

}  // namespace

class CastMediaNotificationItemTest : public testing::Test {
 public:
  void SetUp() override {
#if !BUILDFLAG(IS_CHROMEOS)
    feature_list_.InitAndEnableFeature(media::kGlobalMediaControlsUpdatedUI);
#endif
    auto session_controller =
        std::make_unique<testing::NiceMock<MockSessionController>>(
            mojo::Remote<media_router::mojom::MediaController>());
    session_controller_ = session_controller.get();
    item_ = std::make_unique<CastMediaNotificationItem>(
        CreateMediaRoute(), &item_manager_, std::move(session_controller),
        &profile_);
    item_->set_bitmap_fetcher_factory_for_testing_(
        base::BindRepeating(&CastMediaNotificationItemTest::CreateBitmapFetcher,
                            base::Unretained(this)));
  }

  void SetView() {
    EXPECT_CALL(view_, UpdateWithVectorIcon(_))
        .WillOnce([](const gfx::VectorIcon* vector_icon) {
          EXPECT_EQ(vector_icons::kMediaRouterIdleIcon.reps.data(),
                    vector_icon->reps.data());
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

#if BUILDFLAG(IS_CHROMEOS)
    EXPECT_CALL(view_, UpdateWithMediaMetadata(_))
        .WillOnce([&](const media_session::MediaMetadata& metadata) {
          const std::string separator = " \xC2\xB7 ";
          EXPECT_EQ(base::UTF8ToUTF16(kRouteDesc + separator + kSinkName),
                    metadata.source_title);
        });
#else
    EXPECT_CALL(view_, UpdateWithMediaMetadata(_))
        .WillOnce([&](const media_session::MediaMetadata& metadata) {
          EXPECT_EQ(kRouteDesc, base::UTF16ToUTF8(metadata.source_title));
        });
#endif

    item_->SetView(&view_);
    testing::Mock::VerifyAndClearExpectations(&view_);
  }

 protected:
  MOCK_METHOD(std::unique_ptr<BitmapFetcher>,
              CreateBitmapFetcher,
              (const GURL& url,
               BitmapFetcherDelegate* delegate,
               const net::NetworkTrafficAnnotationTag& traffic_annotation));

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  testing::NiceMock<global_media_controls::test::MockMediaItemManager>
      item_manager_;
  raw_ptr<MockSessionController, DanglingUntriaged> session_controller_ =
      nullptr;
  // This needs to be a NiceMock, because the uninteresting mock function calls
  // slow down the tests enough to make
  // CastMediaNotificationItemTest.MediaPositionUpdate flaky.
  testing::NiceMock<media_message_center::test::MockMediaNotificationView>
      view_;
  std::unique_ptr<CastMediaNotificationItem> item_;
  base::test::ScopedFeatureList feature_list_;
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
  EXPECT_CALL(item_manager_, HideItem(kRouteId)).Times(AtLeast(1));
  item_->Dismiss();
}

TEST_F(CastMediaNotificationItemTest, HideNotificationOnDelete) {
  EXPECT_CALL(item_manager_, HideItem(kRouteId));
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

TEST_F(CastMediaNotificationItemTest, SendVolumeStatusToController) {
  auto status = MediaStatus::New();
  item_->OnMediaStatusUpdated(std::move(status));

  float volume = 0.5;
  EXPECT_CALL(*session_controller_, SetVolume(volume));
  item_->SetVolume(volume);

  bool muted = false;
  EXPECT_CALL(*session_controller_, SetMute(muted));
  item_->SetMute(muted);
}

TEST_F(CastMediaNotificationItemTest, SendSeekCommandToController) {
  auto seek_time = base::Seconds(2);
  EXPECT_CALL(*session_controller_, SeekTo(seek_time));
  item_->SeekTo(seek_time);
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
            EXPECT_CALL(*bitmap_fetcher, Init);
            EXPECT_CALL(*bitmap_fetcher, Start(_));
            return bitmap_fetcher;
          });
  item_->OnMediaStatusUpdated(std::move(status));

  SkBitmap bitmap;
  EXPECT_CALL(view_, UpdateWithMediaArtwork(_));
  bitmap_fetcher_delegate->OnFetchComplete(image_url, &bitmap);
}

// TODO(crbug.com/327498504): Fix the test flakiness on Win Arm64.
#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM64)
#define MAYBE_MediaPositionUpdate DISABLED_MediaPositionUpdate
#else
#define MAYBE_MediaPositionUpdate MediaPositionUpdate
#endif
TEST_F(CastMediaNotificationItemTest, MAYBE_MediaPositionUpdate) {
  SetView();
  const base::TimeDelta duration = base::Seconds(100);
  const base::TimeDelta current_time = base::Seconds(70);

  {
    // Test that media position updated correctly with playing video.
    auto status = MediaStatus::New();
    status->play_state = MediaStatus::PlayState::PLAYING;
    status->duration = duration;
    status->current_time = current_time;
    EXPECT_CALL(view_, UpdateWithMediaPosition(_))
        .WillOnce([&](const media_session::MediaPosition& position) {
          EXPECT_EQ(1.0, position.playback_rate());
          EXPECT_EQ(duration, position.duration());
          EXPECT_NEAR(current_time.InSecondsF(),
                      position.GetPosition().InSecondsF(), 1e-3);
        });
    item_->OnMediaStatusUpdated(std::move(status));
  }

  {
    // Test that media position updated correctly with paused video.
    auto status = MediaStatus::New();
    status->play_state = MediaStatus::PlayState::PAUSED;
    status->duration = duration;
    status->current_time = current_time;
    EXPECT_CALL(view_, UpdateWithMediaPosition(_))
        .WillOnce([&](const media_session::MediaPosition& position) {
          EXPECT_EQ(0.0, position.playback_rate());
          EXPECT_EQ(duration, position.duration());
          EXPECT_NEAR(current_time.InSecondsF(),
                      position.GetPosition().InSecondsF(), 1e-3);
        });
    item_->OnMediaStatusUpdated(std::move(status));
  }

  {
    // Test that media position should not be updated with 0 duration.
    auto status = MediaStatus::New();
    status->play_state = MediaStatus::PlayState::PLAYING;
    status->duration = base::TimeDelta();
    status->current_time = current_time;
    EXPECT_CALL(view_, UpdateWithMediaPosition(_)).Times(0);
    item_->OnMediaStatusUpdated(std::move(status));
  }

  {
    // Test that current time should not exceed duration.
    auto status = MediaStatus::New();
    status->play_state = MediaStatus::PlayState::PLAYING;
    status->duration = duration;
    status->current_time = duration + current_time;
    EXPECT_CALL(view_, UpdateWithMediaPosition(_))
        .WillOnce([&](const media_session::MediaPosition& position) {
          EXPECT_EQ(1.0, position.playback_rate());
          EXPECT_EQ(duration, position.duration());
          EXPECT_NEAR(duration.InSecondsF(),
                      position.GetPosition().InSecondsF(), 1e-3);
        });
    item_->OnMediaStatusUpdated(std::move(status));
  }
}

TEST_F(CastMediaNotificationItemTest, StopCasting) {
  media_router::ChromeMediaRouterFactory::GetInstance()->SetTestingFactory(
      &profile_, base::BindRepeating(&media_router::MockMediaRouter::Create));
  auto* mock_router = static_cast<media_router::MockMediaRouter*>(
      media_router::MediaRouterFactory::GetApiForBrowserContext(&profile_));

  EXPECT_CALL(*mock_router, TerminateRoute(item_->route_id()));
  EXPECT_CALL(item_manager_, FocusDialog());
  item_->StopCasting();
}

TEST_F(CastMediaNotificationItemTest, UpdateMediaSinkName) {
  EXPECT_EQ(kSinkName, item_->device_name());
  media_router::MediaRoute route(
      kRouteId, media_router::MediaSource("source_id"), "sink_id", kRouteDesc,
      /*is_local=*/true);
  route.set_media_sink_name("New Sink");
  item_->OnRouteUpdated(route);
  EXPECT_EQ(route.media_sink_name(), item_->device_name());
}
