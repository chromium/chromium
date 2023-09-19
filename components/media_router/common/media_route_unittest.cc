// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/media_route.h"

#include "components/media_router/common/media_sink.h"
#include "components/media_router/common/media_source.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {
constexpr char kRouteId1[] =
    "urn:x-org.chromium:media:route:1/cast-sink1/http://foo.com";
constexpr char kRouteId2[] =
    "urn:x-org.chromium:media:route:2/cast-sink2/http://foo.com";
constexpr char kPresentationUrl[] = "http://www.example.com/presentation.html";
constexpr char kDescription[] = "Description";
constexpr char kSource[] = "not-a-mirroring_source";
constexpr char kTabSource[] = "urn:x-org.chromium.media:source:tab:1";
constexpr char kSinkId[] = "sinkId";
}  // namespace

namespace media_router {

TEST(MediaRouteTest, TestEquals) {
  const MediaSource& media_source =
      MediaSource::ForPresentationUrl(GURL(kPresentationUrl));
  MediaRoute route1(kRouteId1, media_source, kSinkId, kDescription, false);

  MediaRoute route1_copy(route1);
  EXPECT_EQ(route1, route1_copy);

  // Same as route1 with different sink ID.
  MediaRoute route2(kRouteId1, media_source, "differentSinkId", kDescription,
                    false);
  EXPECT_FALSE(route1 == route2);

  // Same as route1 with different description.
  MediaRoute route3(kRouteId1, media_source, kSinkId, "differentDescription",
                    false);
  EXPECT_FALSE(route1 == route3);

  // Same as route1 with different is_local.
  MediaRoute route4(kRouteId1, media_source, kSinkId, kDescription, true);
  EXPECT_FALSE(route1 == route4);

  // The ID is different from route1's.
  MediaRoute route5(kRouteId2, media_source, kSinkId, kDescription, false);
  EXPECT_FALSE(route1 == route5);
}

TEST(MediaRouteTest, TestParsingMediaRouteId) {
  // Parse a valid RouteId.
  EXPECT_EQ(MediaRoute::GetPresentationIdFromMediaRouteId(kRouteId1), "1");
  EXPECT_EQ(MediaRoute::GetSinkIdFromMediaRouteId(kRouteId1), "cast-sink1");
  EXPECT_EQ(MediaRoute::GetMediaSourceIdFromMediaRouteId(kRouteId1),
            "http://foo.com");

  // Parse a RouteId without a proper RouteId prefix or at least two slashes.
  EXPECT_EQ(MediaRoute::GetPresentationIdFromMediaRouteId("InvalidRouteId"),
            "");
  EXPECT_EQ(MediaRoute::GetPresentationIdFromMediaRouteId(
                "urn:x-org.chromium:media:route:1/cast-sink1"),
            "");
  EXPECT_EQ(MediaRoute::GetSinkIdFromMediaRouteId("InvalidRouteId"), "");
  EXPECT_EQ(MediaRoute::GetSinkIdFromMediaRouteId(
                "urn:x-org.chromium:media:route:1/cast-sink1"),
            "");
  EXPECT_EQ(MediaRoute::GetMediaSourceIdFromMediaRouteId("InvalidRouteId"), "");
  EXPECT_EQ(MediaRoute::GetMediaSourceIdFromMediaRouteId(
                "urn:x-org.chromium:media:route:1/cast-sink1"),
            "");
}

TEST(MediaRouteTest, TestIsLocalMirroringRoute) {
  MediaRoute local_nonmirroring_route(kRouteId1, MediaSource(kSource), kSinkId,
                                      kDescription, /*is_local=*/true);
  EXPECT_FALSE(local_nonmirroring_route.IsLocalMirroringRoute());

  MediaRoute nonlocal_mirroring_route(kRouteId1, MediaSource(kTabSource),
                                      kSinkId, kDescription,
                                      /*is_local=*/false);
  EXPECT_FALSE(nonlocal_mirroring_route.IsLocalMirroringRoute());

  MediaRoute local_mirroring_route(kRouteId1, MediaSource(kTabSource), kSinkId,
                                   kDescription, /*is_local=*/true);
  EXPECT_TRUE(local_mirroring_route.IsLocalMirroringRoute());

  MediaRoute local_route_with_mirroring_controller(
      kRouteId1, MediaSource(kSource), kSinkId, kDescription,
      /*is_local=*/true);
  local_route_with_mirroring_controller.set_controller_type(
      RouteControllerType::kMirroring);
  EXPECT_TRUE(local_route_with_mirroring_controller.IsLocalMirroringRoute());
}

}  // namespace media_router
