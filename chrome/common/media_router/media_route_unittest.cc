// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/media_router/media_route.h"

#include "chrome/common/media_router/media_sink.h"
#include "chrome/common/media_router/media_source.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {
constexpr char kRouteId1[] =
    "urn:x-org.chromium:media:route:1/cast-sink1/http://foo.com";
constexpr char kRouteId2[] =
    "urn:x-org.chromium:media:route:2/cast-sink2/http://foo.com";
constexpr char kPresentationUrl[] = "http://www.example.com/presentation.html";
}  // namespace

namespace media_router {

TEST(MediaRouteTest, TestEquals) {
  const MediaSource& media_source =
      MediaSource::ForPresentationUrl(GURL(kPresentationUrl));
  MediaRoute route1(kRouteId1, media_source, "sinkId", "Description", false,
                    false);

  MediaRoute route1_copy(route1);
  EXPECT_EQ(route1, route1_copy);

  // Same as route1 with different sink ID.
  MediaRoute route2(kRouteId1, media_source, "differentSinkId", "Description",
                    false, false);
  EXPECT_FALSE(route1 == route2);

  // Same as route1 with different description.
  MediaRoute route3(kRouteId1, media_source, "sinkId", "differentDescription",
                    false, false);
  EXPECT_FALSE(route1 == route3);

  // Same as route1 with different is_local.
  MediaRoute route4(kRouteId1, media_source, "sinkId", "Description", true,
                    false);
  EXPECT_FALSE(route1 == route4);

  // The ID is different from route1's.
  MediaRoute route5(kRouteId2, media_source, "sinkId", "Description", false,
                    false);
  EXPECT_FALSE(route1 == route5);

  // Same as route1 with different incognito.
  MediaRoute route6(kRouteId1, media_source, "sinkId", "Description", true,
                    false);
  route6.set_incognito(true);
  EXPECT_FALSE(route1 == route6);
}

}  // namespace media_router
