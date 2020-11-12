// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/proto_util.h"

#include "base/test/scoped_feature_list.h"
#include "components/feed/core/proto/v2/wire/capability.pb.h"
#include "components/feed/core/proto/v2/wire/client_info.pb.h"
#include "components/feed/core/proto/v2/wire/feed_request.pb.h"
#include "components/feed/core/proto/v2/wire/request.pb.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/test/proto_printer.h"
#include "components/feed/core/v2/types.h"
#include "components/feed/feed_feature_list.h"
#include "components/version_info/channel.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace {

bool HasCapability(const feedwire::FeedRequest& request,
                   feedwire::Capability wanted_capability) {
  for (auto capability : request.client_capability()) {
    if (wanted_capability == capability)
      return true;
  }
  return false;
}

TEST(ProtoUtilTest, CreateClientInfo) {
  RequestMetadata request_metadata;
  request_metadata.chrome_info.version = base::Version({1, 2, 3, 4});
  request_metadata.chrome_info.channel = version_info::Channel::STABLE;
  request_metadata.display_metrics.density = 1;
  request_metadata.display_metrics.width_pixels = 2;
  request_metadata.display_metrics.height_pixels = 3;
  request_metadata.language_tag = "en-US";

  feedwire::ClientInfo result = CreateClientInfo(request_metadata);
  EXPECT_EQ(feedwire::ClientInfo::CLANK, result.app_type());
  EXPECT_EQ(feedwire::Version::RELEASE, result.app_version().build_type());
  EXPECT_EQ(1, result.app_version().major());
  EXPECT_EQ(2, result.app_version().minor());
  EXPECT_EQ(3, result.app_version().build());
  EXPECT_EQ(4, result.app_version().revision());

  EXPECT_EQ(R"({
  screen_density: 1
  screen_width_in_pixels: 2
  screen_height_in_pixels: 3
}
)",
            ToTextProto(result.display_info(0)));
  EXPECT_EQ("en-US", result.locale());
}

TEST(ProtoUtilTest, DefaultCapabilities) {
  feedwire::FeedRequest request =
      CreateFeedQueryRefreshRequest(feedwire::FeedQuery::MANUAL_REFRESH,
                                    /*request_metadata=*/{},
                                    /*consistency_token=*/std::string())
          .feed_request();

  ASSERT_EQ(10, request.client_capability_size());
  EXPECT_TRUE(HasCapability(request, feedwire::Capability::BASE_UI));
  EXPECT_TRUE(HasCapability(request, feedwire::Capability::REQUEST_SCHEDULE));
  EXPECT_TRUE(HasCapability(request, feedwire::Capability::OPEN_IN_TAB));
  EXPECT_TRUE(HasCapability(request, feedwire::Capability::CARD_MENU));
  EXPECT_TRUE(HasCapability(request, feedwire::Capability::DOWNLOAD_LINK));
  EXPECT_TRUE(HasCapability(request, feedwire::Capability::INFINITE_FEED));
  EXPECT_TRUE(HasCapability(request, feedwire::Capability::DISMISS_COMMAND));
  EXPECT_TRUE(HasCapability(request, feedwire::Capability::UI_THEME_V2));
  EXPECT_TRUE(
      HasCapability(request, feedwire::Capability::UNDO_FOR_DISMISS_COMMAND));
  EXPECT_TRUE(HasCapability(request, feedwire::Capability::PREFETCH_METADATA));
}

TEST(ProtoUtilTest, DisableCapabilitiesWithFinch) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kInterestFeedV2,
      {{"enable_BASE_UI", "false"}, {"enable_INFINITE_FEED", "false"}});
  OverrideConfigWithFinchForTesting();

  feedwire::FeedRequest request =
      CreateFeedQueryRefreshRequest(feedwire::FeedQuery::MANUAL_REFRESH,
                                    /*request_metadata=*/{},
                                    /*consistency_token=*/std::string())
          .feed_request();

  ASSERT_EQ(9, request.client_capability_size());

  // Optional capabilities can be disabled.
  EXPECT_FALSE(HasCapability(request, feedwire::Capability::INFINITE_FEED));

  // Required capabilities can't be disabled.
  EXPECT_TRUE(HasCapability(request, feedwire::Capability::BASE_UI));

  EXPECT_TRUE(HasCapability(request, feedwire::Capability::REQUEST_SCHEDULE));
  EXPECT_TRUE(HasCapability(request, feedwire::Capability::OPEN_IN_TAB));
  EXPECT_TRUE(HasCapability(request, feedwire::Capability::CARD_MENU));
  EXPECT_TRUE(HasCapability(request, feedwire::Capability::DOWNLOAD_LINK));
  EXPECT_TRUE(HasCapability(request, feedwire::Capability::DISMISS_COMMAND));
  EXPECT_TRUE(HasCapability(request, feedwire::Capability::UI_THEME_V2));
  EXPECT_TRUE(
      HasCapability(request, feedwire::Capability::UNDO_FOR_DISMISS_COMMAND));
  EXPECT_TRUE(HasCapability(request, feedwire::Capability::PREFETCH_METADATA));
}

}  // namespace
}  // namespace feed
