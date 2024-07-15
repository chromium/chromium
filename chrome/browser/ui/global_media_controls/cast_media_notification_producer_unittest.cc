// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/cast_media_notification_producer.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/test/base/testing_profile.h"
#include "components/global_media_controls/public/test/mock_media_item_manager.h"
#include "components/media_message_center/mock_media_notification_view.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "components/media_router/common/media_route.h"
#include "components/media_router/common/pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "media/base/media_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/vector_icon_types.h"

using media_router::MediaRoute;
using media_router::RouteControllerType;
using testing::_;
using testing::NiceMock;

namespace {

constexpr char kCastSourceId[] = "cast:123456";

MediaRoute CreateRoute(const std::string& route_id,
                       const std::string& source_id = kCastSourceId) {
  media_router::MediaRoute route(route_id, media_router::MediaSource(source_id),
                                 "sink_id", "description", /* is_local */ true);
  route.set_controller_type(media_router::RouteControllerType::kGeneric);
  return route;
}

}  // namespace

class CastMediaNotificationProducerTest : public testing::Test {
 public:
  void SetUp() override {
#if !BUILDFLAG(IS_CHROMEOS)
    feature_list_.InitAndEnableFeature(media::kGlobalMediaControlsUpdatedUI);
#endif
    notification_producer_ = std::make_unique<CastMediaNotificationProducer>(
        &profile_, &router_, &item_manager_);
  }

  void TearDown() override { notification_producer_.reset(); }

  TestingProfile* profile() { return &profile_; }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<CastMediaNotificationProducer> notification_producer_;
  NiceMock<global_media_controls::test::MockMediaItemManager> item_manager_;
  NiceMock<media_router::MockMediaRouter> router_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(CastMediaNotificationProducerTest, AddAndRemoveRoute) {
  const std::string route_id_1 = "route-id-1";
  const std::string route_id_2 = "route-id-2";
  const std::string route_id_3 = "route-id-3";
  const std::string route_id_4 = "route-id-4";
  MediaRoute cast_route = CreateRoute(route_id_1);
  MediaRoute site_initiated_mirroring_route =
      CreateRoute(route_id_2, "cast:0F5096E8");
  MediaRoute dial_route = CreateRoute(route_id_3, "dial:123456");
  MediaRoute cast_off_screen_tab_route =
      CreateRoute(route_id_4, "https://example.com/");

  EXPECT_CALL(item_manager_, OnItemsChanged());
  notification_producer_->OnRoutesUpdated(
      {cast_route, site_initiated_mirroring_route, dial_route,
       cast_off_screen_tab_route});
  testing::Mock::VerifyAndClearExpectations(&item_manager_);
  EXPECT_EQ(4u, notification_producer_->GetActiveItemCount());
  EXPECT_NE(nullptr, notification_producer_->GetMediaItem(route_id_1));
  EXPECT_NE(nullptr, notification_producer_->GetMediaItem(route_id_2));
  EXPECT_NE(nullptr, notification_producer_->GetMediaItem(route_id_3));
  EXPECT_NE(nullptr, notification_producer_->GetMediaItem(route_id_4));

  EXPECT_CALL(item_manager_, OnItemsChanged());
  notification_producer_->OnRoutesUpdated({});
  testing::Mock::VerifyAndClearExpectations(&item_manager_);
  EXPECT_EQ(0u, notification_producer_->GetActiveItemCount());
}

TEST_F(CastMediaNotificationProducerTest, UpdateRoute) {
  const std::string route_id = "route-id-1";
  MediaRoute route = CreateRoute(route_id);

  notification_producer_->OnRoutesUpdated({route});
  auto* item = static_cast<CastMediaNotificationItem*>(
      notification_producer_->GetMediaItem(route_id).get());
  NiceMock<media_message_center::test::MockMediaNotificationView> view;
  item->SetView(&view);

  const std::string new_sink = "new sink";
  const std::string new_description = "new description";
  route.set_media_sink_name(new_sink);
  route.set_description(new_description);

  EXPECT_CALL(view, UpdateWithMediaMetadata(_))
      .WillOnce([&](const media_session::MediaMetadata& metadata) {
        const std::string separator = " \xC2\xB7 ";
#if BUILDFLAG(IS_CHROMEOS)
        EXPECT_EQ(base::UTF8ToUTF16(new_description + separator + new_sink),
                  metadata.source_title);
#else
        EXPECT_EQ(base::UTF8ToUTF16(new_description), metadata.source_title);
        EXPECT_EQ(new_sink, item->device_name());
#endif
      });
  notification_producer_->OnRoutesUpdated({route});
}

TEST_F(CastMediaNotificationProducerTest, DismissNotification) {
  const std::string route_id1 = "route-id-1";
  const std::string route_id2 = "route-id-2";
  MediaRoute route1 = CreateRoute(route_id1);
  MediaRoute route2 = CreateRoute(route_id2);
  notification_producer_->OnRoutesUpdated({route1});
  EXPECT_EQ(1u, notification_producer_->GetActiveItemCount());

  notification_producer_->OnMediaItemUIDismissed(route_id1);
  EXPECT_EQ(0u, notification_producer_->GetActiveItemCount());

  // Adding another route should not bring back the dismissed notification.
  notification_producer_->OnRoutesUpdated({route1, route2});
  EXPECT_EQ(1u, notification_producer_->GetActiveItemCount());
}

TEST_F(CastMediaNotificationProducerTest, RoutesWithoutNotifications) {
  // These routes should not have notification items created for them.
  MediaRoute mirroring_route =
      CreateRoute("route-1", "urn:x-org.chromium.media:source:tab:*");
  MediaRoute multizone_member_route = CreateRoute("route-2", "cast:705D30C6");
  MediaRoute connecting_route = CreateRoute("route-3");
  connecting_route.set_is_connecting(true);
  MediaRoute remote_streaming_route = CreateRoute("route-4", "cast:0F5096E8");
  remote_streaming_route.set_local(false);

  notification_producer_->OnRoutesUpdated(
      {mirroring_route, multizone_member_route, connecting_route,
       remote_streaming_route});
  EXPECT_EQ(0u, notification_producer_->GetActiveItemCount());
}

TEST_F(CastMediaNotificationProducerTest, NonLocalRoutesWithoutNotifications) {
  MediaRoute non_local_route = CreateRoute("non-local-route");
  non_local_route.set_local(false);
  sync_preferences::TestingPrefServiceSyncable* pref_service =
      profile()->GetTestingPrefService();

  EXPECT_CALL(item_manager_, ShowItem).Times(0);
  pref_service->SetBoolean(
      media_router::prefs::kMediaRouterShowCastSessionsStartedByOtherDevices,
      false);
  notification_producer_->OnRoutesUpdated({non_local_route});
  testing::Mock::VerifyAndClearExpectations(&item_manager_);

  // When the pref changes to show the non-local route, it is shown the next
  // time `OnRoutesUpdated()` is called.
  EXPECT_CALL(item_manager_, ShowItem).Times(1);
  pref_service->SetBoolean(
      media_router::prefs::kMediaRouterShowCastSessionsStartedByOtherDevices,
      true);
  notification_producer_->OnRoutesUpdated({non_local_route});
  testing::Mock::VerifyAndClearExpectations(&item_manager_);
  EXPECT_EQ(1u, notification_producer_->GetActiveItemCount());

  // There is no need to call `OnRouteUpdated()` here because this is a
  // client-side change.
  pref_service->SetBoolean(
      media_router::prefs::kMediaRouterShowCastSessionsStartedByOtherDevices,
      false);
  EXPECT_EQ(0u, notification_producer_->GetActiveItemCount());
}
