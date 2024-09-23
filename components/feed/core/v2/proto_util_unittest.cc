// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/proto_util.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/feed/core/proto/v2/wire/capability.pb.h"
#include "components/feed/core/proto/v2/wire/client_info.pb.h"
#include "components/feed/core/proto/v2/wire/feed_entry_point_source.pb.h"
#include "components/feed/core/proto/v2/wire/feed_request.pb.h"
#include "components/feed/core/proto/v2/wire/info_card.pb.h"
#include "components/feed/core/proto/v2/wire/request.pb.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/public/feed_api.h"
#include "components/feed/core/v2/test/proto_printer.h"
#include "components/feed/core/v2/test/test_util.h"
#include "components/feed/core/v2/types.h"
#include "components/feed/feed_feature_list.h"
#include "components/reading_list/features/reading_list_switches.h"
#include "components/version_info/channel.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace {

using feedwire::InfoCardTrackingState;
using ::testing::Contains;
using ::testing::IsSupersetOf;
using ::testing::Not;

TEST(ProtoUtilTest, CreateClientInfo) {
  RequestMetadata request_metadata;
  request_metadata.chrome_info.version = base::Version({1, 2, 3, 4});
  request_metadata.chrome_info.channel = version_info::Channel::STABLE;
  request_metadata.display_metrics.density = 1;
  request_metadata.display_metrics.width_pixels = 2;
  request_metadata.display_metrics.height_pixels = 3;
  request_metadata.language_tag = "en-US";

  feedwire::ClientInfo result = CreateClientInfo(request_metadata);
  EXPECT_EQ(feedwire::ClientInfo::CHROME_ANDROID, result.app_type());
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
      CreateFeedQueryRefreshRequest(
          StreamType(StreamKind::kForYou), feedwire::FeedQuery::MANUAL_REFRESH,
          /*request_metadata=*/{},
          /*consistency_token=*/std::string(), SingleWebFeedEntryPoint::kOther,
          /*doc_view_counts=*/{})
          .feed_request();

  // Additional features may be present based on the current testing config.
  ASSERT_THAT(
      request.client_capability(),
      testing::IsSupersetOf(
          {feedwire::Capability::REQUEST_SCHEDULE,
           feedwire::Capability::LOTTIE_ANIMATIONS,
           feedwire::Capability::LONG_PRESS_CARD_MENU,
           feedwire::Capability::OPEN_IN_TAB,
           feedwire::Capability::OPEN_IN_INCOGNITO,
           feedwire::Capability::CARD_MENU, feedwire::Capability::INFINITE_FEED,
           feedwire::Capability::DISMISS_COMMAND,
           feedwire::Capability::MATERIAL_NEXT_BASELINE,
           feedwire::Capability::UI_THEME_V2,
           feedwire::Capability::UNDO_FOR_DISMISS_COMMAND,
           feedwire::Capability::PREFETCH_METADATA, feedwire::Capability::SHARE,
           feedwire::Capability::CONTENT_LIFETIME,
           feedwire::Capability::INFO_CARD_ACKNOWLEDGEMENT_TRACKING,
           feedwire::Capability::SPORTS_IN_GAME_UPDATE}));
}

TEST(ProtoUtilTest, HeartsEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kInterestFeedV2Hearts}, {});
  feedwire::FeedRequest request =
      CreateFeedQueryRefreshRequest(
          StreamType(StreamKind::kForYou), feedwire::FeedQuery::MANUAL_REFRESH,
          /*request_metadata=*/{},
          /*consistency_token=*/std::string(), SingleWebFeedEntryPoint::kOther,
          /*doc_view_counts=*/{})
          .feed_request();

  ASSERT_THAT(request.client_capability(),
              Contains(feedwire::Capability::HEART));
}

// kFeedBottomSyncStringRemoval is mobile-only.
#if BUILDFLAG(IS_ANDROID)
TEST(ProtoUtilTest, SyncRestringEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeedBottomSyncStringRemoval}, {});
  feedwire::FeedRequest request =
      CreateFeedQueryRefreshRequest(
          StreamType(StreamKind::kForYou), feedwire::FeedQuery::MANUAL_REFRESH,
          /*request_metadata=*/{},
          /*consistency_token=*/std::string(), SingleWebFeedEntryPoint::kOther,
          /*doc_view_counts=*/{})
          .feed_request();

  ASSERT_THAT(request.client_capability(),
              Contains(feedwire::Capability::SYNC_STRING_REMOVAL));
}
#endif

TEST(ProtoUtilTest, DisableCapabilitiesWithFinch) {
  // Try to disable _INFINITE_FEED.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kInterestFeedV2, {{"enable_MATERIAL_NEXT_BASELINE", "false"}});
  OverrideConfigWithFinchForTesting();

  feedwire::FeedRequest request =
      CreateFeedQueryRefreshRequest(
          StreamType(StreamKind::kForYou), feedwire::FeedQuery::MANUAL_REFRESH,
          /*request_metadata=*/{},
          /*consistency_token=*/std::string(), SingleWebFeedEntryPoint::kOther,
          /*doc_view_counts=*/{})
          .feed_request();

  // Additional features may be present based on the current testing config.
  ASSERT_THAT(request.client_capability(),
              Not(Contains(feedwire::Capability::MATERIAL_NEXT_BASELINE)));

  ASSERT_THAT(request.client_capability(),
              Contains(feedwire::Capability::CONTENT_LIFETIME));
}

TEST(ProtoUtilTest, PrivacyNoticeCardAcknowledged) {
  RequestMetadata request_metadata;
  request_metadata.notice_card_acknowledged = true;
  feedwire::Request request = CreateFeedQueryRefreshRequest(
      StreamType(StreamKind::kForYou), feedwire::FeedQuery::MANUAL_REFRESH,
      request_metadata,
      /*consistency_token=*/std::string(), SingleWebFeedEntryPoint::kOther,
      /*doc_view_counts=*/{});

  EXPECT_TRUE(request.feed_request()
                  .feed_query()
                  .chrome_fulfillment_info()
                  .notice_card_acknowledged());
}

TEST(ProtoUtilTest, PrivacyNoticeCardNotAcknowledged) {
  RequestMetadata request_metadata;
  request_metadata.notice_card_acknowledged = false;
  feedwire::Request request = CreateFeedQueryRefreshRequest(
      StreamType(StreamKind::kForYou), feedwire::FeedQuery::MANUAL_REFRESH,
      request_metadata,
      /*consistency_token=*/std::string(), SingleWebFeedEntryPoint::kOther,
      /*doc_view_counts=*/{});

  EXPECT_FALSE(request.feed_request()
                   .feed_query()
                   .chrome_fulfillment_info()
                   .notice_card_acknowledged());
}

TEST(ProtoUtilTest, InfoCardTrackingStates) {
  RequestMetadata request_metadata;
  InfoCardTrackingState state1;
  state1.set_type(101);
  state1.set_view_count(2);
  InfoCardTrackingState state2;
  state1.set_type(2000);
  state1.set_view_count(5);
  state1.set_click_count(2);
  state1.set_explicitly_dismissed_count(1);
  request_metadata.info_card_tracking_states = {state1, state2};
  feedwire::Request request = CreateFeedQueryRefreshRequest(
      StreamType(StreamKind::kForYou), feedwire::FeedQuery::MANUAL_REFRESH,
      request_metadata,
      /*consistency_token=*/std::string(), SingleWebFeedEntryPoint::kOther,
      /*doc_view_counts=*/{});

  ASSERT_EQ(2, request.feed_request()
                   .feed_query()
                   .chrome_fulfillment_info()
                   .info_card_tracking_state_size());
  EXPECT_THAT(state1, EqualsProto(request.feed_request()
                                      .feed_query()
                                      .chrome_fulfillment_info()
                                      .info_card_tracking_state(0)));
  EXPECT_THAT(state2, EqualsProto(request.feed_request()
                                      .feed_query()
                                      .chrome_fulfillment_info()
                                      .info_card_tracking_state(1)));
}

TEST(ProtoUtilTest, StampEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeedStamp}, {});
  feedwire::FeedRequest request =
      CreateFeedQueryRefreshRequest(
          StreamType(StreamKind::kForYou), feedwire::FeedQuery::MANUAL_REFRESH,
          /*request_metadata=*/{},
          /*consistency_token=*/std::string(), SingleWebFeedEntryPoint::kOther,
          /*doc_view_counts=*/{})
          .feed_request();

  ASSERT_THAT(request.client_capability(),
              Contains(feedwire::Capability::SILK_AMP_OPEN_COMMAND));
  ASSERT_THAT(request.client_capability(),
              Contains(feedwire::Capability::AMP_STORY_PLAYER));
  ASSERT_THAT(request.client_capability(),
              Contains(feedwire::Capability::AMP_GROUP_DATASTORE));
}

TEST(ProtoUtilTest, DynamicColorEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeedDynamicColors}, {});
  feedwire::FeedRequest request =
      CreateFeedQueryRefreshRequest(
          StreamType(StreamKind::kForYou), feedwire::FeedQuery::MANUAL_REFRESH,
          /*request_metadata=*/{},
          /*consistency_token=*/std::string(), SingleWebFeedEntryPoint::kOther,
          /*doc_view_counts=*/{})
          .feed_request();

  ASSERT_THAT(request.client_capability(),
              Contains(feedwire::Capability::DYNAMIC_COLORS));
}

// ReadLater is enabled by default everywhere with the exception of iOS which
// has a build-flag to enable it.
#if !BUILDFLAG(IS_IOS)

TEST(ProtoUtilTest, ReadLaterEnabled) {
  feedwire::FeedRequest request =
      CreateFeedQueryRefreshRequest(
          StreamType(StreamKind::kForYou), feedwire::FeedQuery::MANUAL_REFRESH,
          /*request_metadata=*/{},
          /*consistency_token=*/std::string(), SingleWebFeedEntryPoint::kOther,
          /*doc_view_counts=*/{})
          .feed_request();

  ASSERT_THAT(request.client_capability(),
              Contains(feedwire::Capability::READ_LATER));
  ASSERT_THAT(request.client_capability(),
              Not(Contains((feedwire::Capability::DOWNLOAD_LINK))));
}

#endif

TEST(ProtoUtilTest, CormorantEnabled) {
  RequestMetadata request_metadata;
  request_metadata.country = "US";

  feedwire::FeedRequest request =
      CreateFeedQueryRefreshRequest(
          StreamType(StreamKind::kSingleWebFeed, "test_web_id"),
          feedwire::FeedQuery::MANUAL_REFRESH, request_metadata,
          /*consistency_token=*/std::string(), SingleWebFeedEntryPoint::kMenu,
          /*doc_view_counts=*/{})
          .feed_request();

  ASSERT_THAT(request.client_capability(),
              Contains(feedwire::Capability::OPEN_WEB_FEED_COMMAND));
  ASSERT_EQ(request.feed_query()
                .feed_entry_point_data()
                .feed_entry_point_source_value(),
            feedwire::FeedEntryPointSource::CHROME_SINGLE_WEB_FEED_MENU);
}

TEST(ProtoUtilTest, InfoCardAcknowledgementTrackingDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({}, {kInfoCardAcknowledgementTracking});
  feedwire::FeedRequest request =
      CreateFeedQueryRefreshRequest(
          StreamType(StreamKind::kForYou), feedwire::FeedQuery::MANUAL_REFRESH,
          /*request_metadata=*/{},
          /*consistency_token=*/std::string(), SingleWebFeedEntryPoint::kOther,
          /*doc_view_counts=*/{})
          .feed_request();

  ASSERT_THAT(
      request.client_capability(),
      Not(Contains(feedwire::Capability::INFO_CARD_ACKNOWLEDGEMENT_TRACKING)));
}

TEST(ProtoUtilTest, FeedSignedOutViewDemotionEnablesCapability) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeedSignedOutViewDemotion}, {});
  feedwire::FeedRequest request =
      CreateFeedQueryRefreshRequest(
          StreamType(StreamKind::kForYou), feedwire::FeedQuery::MANUAL_REFRESH,
          /*request_metadata=*/{},
          /*consistency_token=*/std::string(), SingleWebFeedEntryPoint::kOther,
          /*doc_view_counts=*/{})
          .feed_request();

  ASSERT_THAT(request.client_capability(),
              Contains(feedwire::Capability::ON_DEVICE_VIEW_HISTORY));
}

TEST(ProtoUtilTest, TabGroupsEnabledForReplaced) {
  RequestMetadata request_metadata;
  request_metadata.tab_group_enabled_state = TabGroupEnabledState::kReplaced;

  feedwire::FeedRequest request =
      CreateFeedQueryRefreshRequest(
          StreamType(StreamKind::kForYou), feedwire::FeedQuery::MANUAL_REFRESH,
          request_metadata,
          /*consistency_token=*/std::string(), SingleWebFeedEntryPoint::kOther,
          /*doc_view_counts=*/{})
          .feed_request();

  ASSERT_THAT(request.client_capability(),
              Contains(feedwire::Capability::OPEN_IN_NEW_TAB_IN_GROUP));
  ASSERT_THAT(request.client_capability(),
              Not(Contains(feedwire::Capability::OPEN_IN_TAB)));
}

TEST(ProtoUtilTest, TabGroupsEnabledForBoth) {
  RequestMetadata request_metadata;
  request_metadata.tab_group_enabled_state = TabGroupEnabledState::kBoth;

  feedwire::FeedRequest request =
      CreateFeedQueryRefreshRequest(
          StreamType(StreamKind::kForYou), feedwire::FeedQuery::MANUAL_REFRESH,
          request_metadata,
          /*consistency_token=*/std::string(), SingleWebFeedEntryPoint::kOther,
          /*doc_view_counts=*/{})
          .feed_request();

  ASSERT_THAT(request.client_capability(),
              Contains(feedwire::Capability::OPEN_IN_NEW_TAB_IN_GROUP));
  ASSERT_THAT(request.client_capability(),
              Contains(feedwire::Capability::OPEN_IN_TAB));
}

TEST(ProtoUtilTest, SignInStatusSetOnRequest) {
  RequestMetadata request_metadata;
  request_metadata.sign_in_status = feedwire::ChromeSignInStatus::NOT_SIGNED_IN;

  feedwire::Request request = CreateFeedQueryRefreshRequest(
      StreamType(StreamKind::kForYou), feedwire::FeedQuery::MANUAL_REFRESH,
      request_metadata,
      /*consistency_token=*/std::string(), SingleWebFeedEntryPoint::kOther,
      /*doc_view_counts=*/{});

  feedwire::ChromeSignInStatus::SignInStatus status =
      request.feed_request()
          .feed_query()
          .chrome_fulfillment_info()
          .sign_in_status()
          .sign_in_status();
  ASSERT_EQ(status, request_metadata.sign_in_status);
}

TEST(ProtoUtilTest, WithoutDocIds) {
  feedwire::FeedRequest request =
      CreateFeedQueryRefreshRequest(
          StreamType(StreamKind::kForYou), feedwire::FeedQuery::MANUAL_REFRESH,
          /*request_metadata=*/{},
          /*consistency_token=*/std::string(), SingleWebFeedEntryPoint::kOther,
          /*doc_view_counts=*/{})
          .feed_request();

  ASSERT_THAT(
      request.client_user_profiles().has_discover_user_actions_profile(),
      testing::IsFalse());
}

TEST(ProtoUtilTest, WithDocIds) {
  feedwire::FeedRequest request =
      CreateFeedQueryRefreshRequest(
          StreamType(StreamKind::kForYou), feedwire::FeedQuery::MANUAL_REFRESH,
          /*request_metadata=*/{},
          /*consistency_token=*/std::string(), SingleWebFeedEntryPoint::kOther,
          /*doc_view_counts=*/{DocViewCount{123, 1}, DocViewCount{456, 2}})
          .feed_request();

  EXPECT_STRINGS_EQUAL(
      R"({
  view_demotion_profile {
    tables {
      name: "url_all_ondevice"
      num_rows: 2
      columns {
        type: 4
        name: "dimension_key"
        uint64_values: 123
        uint64_values: 456
      }
      columns {
        type: 2
        name: "FEED_CARD_VIEW"
        int64_values: 1
        int64_values: 2
      }
    }
  }
}
)",
      ToTextProto(request.client_user_profiles().view_demotion_profile()));
}

TEST(ProtoUtilTest, DefaultSearchEngineSetOnRequest) {
  RequestMetadata request_metadata;
  request_metadata.default_search_engine =
      feedwire::DefaultSearchEngine::ENGINE_GOOGLE;

  feedwire::Request request = CreateFeedQueryRefreshRequest(
      StreamType(StreamKind::kForYou), feedwire::FeedQuery::MANUAL_REFRESH,
      request_metadata,
      /*consistency_token=*/std::string(), SingleWebFeedEntryPoint::kOther,
      /*doc_view_counts=*/{});

  feedwire::DefaultSearchEngine::SearchEngine search_engine =
      request.feed_request()
          .feed_query()
          .chrome_fulfillment_info()
          .default_search_engine()
          .search_engine();
  ASSERT_EQ(search_engine, request_metadata.default_search_engine);
}

}  // namespace
}  // namespace feed
