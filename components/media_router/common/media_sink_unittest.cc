// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/media_sink.h"

#include "components/media_router/common/mojom/media_route_provider_id.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media_router {

TEST(MediaSinkTest, TestEquals) {
  MediaSink sink1("sinkId", "Sink", SinkIconType::CAST,
                  mojom::MediaRouteProviderId::CAST);

  MediaSink sink1_copy(sink1);
  EXPECT_EQ(sink1, sink1_copy);

  // No name.
  MediaSink sink2("sinkId", "", SinkIconType::CAST,
                  mojom::MediaRouteProviderId::CAST);
  EXPECT_FALSE(sink1 == sink2);

  // Sink name is different from sink1's.
  MediaSink sink3("sinkId", "Other Sink", SinkIconType::CAST,
                  mojom::MediaRouteProviderId::CAST);
  EXPECT_FALSE(sink1 == sink3);

  // Sink ID is diffrent from sink1's.
  MediaSink sink4("otherSinkId", "Sink", SinkIconType::CAST,
                  mojom::MediaRouteProviderId::CAST);
  EXPECT_FALSE(sink1 == sink4);

  // Sink icon type is diffrent from sink1's.
  MediaSink sink5("otherSinkId", "Sink", SinkIconType::GENERIC,
                  mojom::MediaRouteProviderId::CAST);
  EXPECT_FALSE(sink1 == sink5);
}

}  // namespace media_router
