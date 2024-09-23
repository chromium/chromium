// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/proto_util.h"

#include <tuple>
#include <vector>

#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/wire/capability.pb.h"
#include "components/feed/core/proto/v2/wire/chrome_client_info.pb.h"
#include "components/feed/core/proto/v2/wire/feed_entry_point_data.pb.h"
#include "components/feed/core/proto/v2/wire/feed_entry_point_source.pb.h"
#include "components/feed/core/proto/v2/wire/feed_query.pb.h"
#include "components/feed/core/proto/v2/wire/feed_request.pb.h"
#include "components/feed/core/proto/v2/wire/request.pb.h"
#include "components/feed/core/proto/v2/wire/table.pb.h"
#include "components/feed/core/proto/v2/wire/web_feed_id.pb.h"
#include "components/feed/core/proto/v2/wire/web_feed_identifier_token.pb.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/public/feed_api.h"
#include "components/feed/feed_feature_list.h"
#include "components/reading_list/features/reading_list_switches.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace feed {
namespace {
using feedwire::Capability;

feedwire::Version::Architecture GetBuildArchitecture() {
#if defined(ARCH_CPU_X86_64)
  return feedwire::Version::X86_64;
#elif defined(ARCH_CPU_X86)
  return feedwire::Version::X86;
#elif defined(ARCH_CPU_MIPS64)
  return feedwire::Version::MIPS64;
#elif defined(ARCH_CPU_MIPS)
  return feedwire::Version::MIPS;
#elif defined(ARCH_CPU_ARM64)
  return feedwire::Version::ARM64;
#elif defined(ARCH_CPU_ARMEL)
  return feedwire::Version::ARM;
#else
  return feedwire::Version::UNKNOWN_ARCHITECTURE;
#endif
}

feedwire::Version::Architecture GetSystemArchitecture() {
  // By default, use |GetBuildArchitecture()|.
  // In the case of x86 and ARM, the OS might be x86_64 or ARM_64.
  feedwire::Version::Architecture build_arch = GetBuildArchitecture();
  if (build_arch == feedwire::Version::X86 &&
      base::SysInfo::OperatingSystemArchitecture() == "x86_64") {
    return feedwire::Version::X86_64;
  }
  if (feedwire::Version::ARM &&
      base::SysInfo::OperatingSystemArchitecture() == "arm64") {
    return feedwire::Version::ARM64;
  }
  return build_arch;
}

feedwire::Version::BuildType GetBuildType(version_info::Channel channel) {
  switch (channel) {
    case version_info::Channel::CANARY:
      return feedwire::Version::ALPHA;
    case version_info::Channel::DEV:
      return feedwire::Version::DEV;
    case version_info::Channel::BETA:
      return feedwire::Version::BETA;
    case version_info::Channel::STABLE:
      return feedwire::Version::RELEASE;
    default:
      return feedwire::Version::UNKNOWN_BUILD_TYPE;
  }
}

feedwire::Version GetPlatformVersionMessage() {
  feedwire::Version result;
  result.set_architecture(GetSystemArchitecture());
  result.set_build_type(feedwire::Version::RELEASE);

  int32_t major, minor, revision;
  base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &revision);
  result.set_major(major);
  result.set_minor(minor);
  result.set_revision(revision);
#if BUILDFLAG(IS_ANDROID)
  result.set_api_version(base::android::BuildInfo::GetInstance()->sdk_int());
#endif
  return result;
}

feedwire::Version GetAppVersionMessage(const ChromeInfo& chrome_info) {
  feedwire::Version result;
  result.set_architecture(GetBuildArchitecture());
  result.set_build_type(GetBuildType(chrome_info.channel));
  // Chrome's version is in the format: MAJOR,MINOR,BUILD,PATCH.
  const std::vector<uint32_t>& numbers = chrome_info.version.components();
  if (numbers.size() > 3) {
    result.set_major(static_cast<int32_t>(numbers[0]));
    result.set_minor(static_cast<int32_t>(numbers[1]));
    result.set_build(static_cast<int32_t>(numbers[2]));
    result.set_revision(static_cast<int32_t>(numbers[3]));
  }

#if BUILDFLAG(IS_ANDROID)
  result.set_api_version(base::android::BuildInfo::GetInstance()->sdk_int());
#endif
  return result;
}

feedwire::Request CreateFeedQueryRequest(
    const StreamType& stream_type,
    feedwire::FeedQuery::RequestReason request_reason,
    const RequestMetadata& request_metadata,
    const std::string& consistency_token,
    const std::string& next_page_token,
    const SingleWebFeedEntryPoint single_feed_entry_point) {
  feedwire::Request request;
  request.set_request_version(feedwire::Request::FEED_QUERY);

  feedwire::FeedRequest& feed_request = *request.mutable_feed_request();

  for (Capability capability :
       {Capability::CARD_MENU, Capability::LOTTIE_ANIMATIONS,
        Capability::LONG_PRESS_CARD_MENU, Capability::SHARE,
        Capability::OPEN_IN_INCOGNITO, Capability::DISMISS_COMMAND,
        Capability::INFINITE_FEED, Capability::PREFETCH_METADATA,
        Capability::REQUEST_SCHEDULE, Capability::UI_THEME_V2,
        Capability::UNDO_FOR_DISMISS_COMMAND,
        Capability::SPORTS_IN_GAME_UPDATE}) {
    feed_request.add_client_capability(capability);
  }

  for (auto capability : GetFeedConfig().experimental_capabilities)
    feed_request.add_client_capability(capability);

#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(kFeedBottomSyncStringRemoval)) {
    feed_request.add_client_capability(Capability::SYNC_STRING_REMOVAL);
  }
#endif

  if (base::FeatureList::IsEnabled(kInterestFeedV2Hearts)) {
    feed_request.add_client_capability(Capability::HEART);
  }

  if (base::FeatureList::IsEnabled(kFeedStamp)) {
    feed_request.add_client_capability(Capability::SILK_AMP_OPEN_COMMAND);
    feed_request.add_client_capability(Capability::AMP_STORY_PLAYER);
    feed_request.add_client_capability(Capability::AMP_GROUP_DATASTORE);
  }

  feed_request.add_client_capability(Capability::READ_LATER);
  // Cormorant is only enabled for en.* locales
  if (feed::IsCormorantEnabledForLocale(request_metadata.country)) {
    feed_request.add_client_capability(Capability::OPEN_WEB_FEED_COMMAND);
  }

  if (base::FeatureList::IsEnabled(kPersonalizeFeedUnsignedUsers)) {
    feed_request.add_client_capability(Capability::ON_DEVICE_USER_PROFILE);
  }

  if (base::FeatureList::IsEnabled(kFeedSignedOutViewDemotion)) {
    feed_request.add_client_capability(Capability::ON_DEVICE_VIEW_HISTORY);
  }

  if (base::FeatureList::IsEnabled(kInfoCardAcknowledgementTracking)) {
    feed_request.add_client_capability(
        Capability::INFO_CARD_ACKNOWLEDGEMENT_TRACKING);
  }

  if (base::FeatureList::IsEnabled(kSyntheticCapabilities)) {
    feed_request.add_client_capability(Capability::SYNTHETIC_CAPABILITIES);
  }

  if (base::FeatureList::IsEnabled(kFeedDynamicColors)) {
    feed_request.add_client_capability(Capability::DYNAMIC_COLORS);
  }

  switch (request_metadata.tab_group_enabled_state) {
    case TabGroupEnabledState::kNone:
      feed_request.add_client_capability(Capability::OPEN_IN_TAB);
      break;
    case TabGroupEnabledState::kReplaced:
      feed_request.add_client_capability(Capability::OPEN_IN_NEW_TAB_IN_GROUP);
      break;
    case TabGroupEnabledState::kBoth:
      feed_request.add_client_capability(Capability::OPEN_IN_TAB);
      feed_request.add_client_capability(Capability::OPEN_IN_NEW_TAB_IN_GROUP);
      break;
  }

  *feed_request.mutable_client_info() = CreateClientInfo(request_metadata);
  feedwire::FeedQuery& query = *feed_request.mutable_feed_query();
  query.set_reason(request_reason);
  switch (request_metadata.content_order) {
    case ContentOrder::kReverseChron:
      query.set_order_by(
          feedwire::FeedQuery::ContentOrder::FeedQuery_ContentOrder_RECENT);
      break;
    case ContentOrder::kGrouped:
      query.set_order_by(
          feedwire::FeedQuery::ContentOrder::FeedQuery_ContentOrder_GROUPED);
      break;
    case ContentOrder::kUnspecified:
      break;
  }

  // Set the feed entry point based on the stream type.
  feedwire::FeedEntryPointData& entry_point =
      *query.mutable_feed_entry_point_data();
  if (stream_type.IsForYou()) {
    entry_point.set_feed_entry_point_source_value(
        feedwire::FeedEntryPointSource::CHROME_DISCOVER_FEED);
  } else if (stream_type.IsWebFeed()) {
    entry_point.set_feed_entry_point_source_value(
        feedwire::FeedEntryPointSource::CHROME_FOLLOWING_FEED);
  } else if (stream_type.IsSingleWebFeed()) {
    switch (single_feed_entry_point) {
      case SingleWebFeedEntryPoint::kMenu:
        entry_point.set_feed_entry_point_source_value(
            feedwire::FeedEntryPointSource::CHROME_SINGLE_WEB_FEED_MENU);
        break;
      case SingleWebFeedEntryPoint::kAttribution:
        entry_point.set_feed_entry_point_source_value(
            feedwire::FeedEntryPointSource::CHROME_SINGLE_WEB_FEED_ATTRIBUTION);
        break;
      case SingleWebFeedEntryPoint::kRecommendation:
        entry_point.set_feed_entry_point_source_value(
            feedwire::FeedEntryPointSource::
                CHROME_SINGLE_WEB_FEED_RECOMMENDATION);
        break;
      case SingleWebFeedEntryPoint::kGroupHeader:
        entry_point.set_feed_entry_point_source_value(
            feedwire::FeedEntryPointSource::
                CHROME_SINGLE_WEB_FEED_GROUP_HEADER);
        break;
      case SingleWebFeedEntryPoint::kOther:
        entry_point.set_feed_entry_point_source_value(
            feedwire::FeedEntryPointSource::CHROME_SINGLE_WEB_FEED_OTHER);

        break;
    }
  }

  // |consistency_token|, for action reporting, is only applicable to signed-in
  // requests. The presence of |client_instance_id|, also signed-in only, can be
  // used a proxy for checking if we're creating a signed-in request.
  if (!consistency_token.empty() &&
      !request_metadata.client_instance_id.empty()) {
    feed_request.mutable_consistency_token()->set_token(consistency_token);
  }

  if (!next_page_token.empty()) {
    DCHECK_EQ(request_reason, feedwire::FeedQuery::NEXT_PAGE_SCROLL);
    query.mutable_next_page_token()
        ->mutable_next_page_token()
        ->set_next_page_token(next_page_token);
  }
  return request;
}

void SetNoticeCardAcknowledged(feedwire::Request* request,
                               const RequestMetadata& request_metadata) {
  if (request_metadata.notice_card_acknowledged) {
    request->mutable_feed_request()
        ->mutable_feed_query()
        ->mutable_chrome_fulfillment_info()
        ->set_notice_card_acknowledged(true);
  }
}

void SetInfoCardTrackingStates(feedwire::Request* request,
                               const RequestMetadata& request_metadata) {
  for (const auto& state : request_metadata.info_card_tracking_states) {
    request->mutable_feed_request()
        ->mutable_feed_query()
        ->mutable_chrome_fulfillment_info()
        ->add_info_card_tracking_state()
        ->CopyFrom(state);
  }
}

// Set the chrome_feature_usage.times_followed_from_web_page_menu
// from the request_metadata.followed_from_web_page_menu_count.
void SetTimesFollowedFromWebPageMenu(feedwire::Request* request,
                                     const RequestMetadata& request_metadata) {
  request->mutable_feed_request()
      ->mutable_feed_query()
      ->mutable_chrome_fulfillment_info()
      ->mutable_chrome_feature_usage()
      ->set_times_followed_from_web_page_menu(
          request_metadata.followed_from_web_page_menu_count);
}

// Set the sign in status for the feed query to Discover from the request
// metadata.sign_in_status
void SetChromeSignInStatus(feedwire::Request* request,
                           const RequestMetadata& request_metadata) {
  request->mutable_feed_request()
      ->mutable_feed_query()
      ->mutable_chrome_fulfillment_info()
      ->mutable_sign_in_status()
      ->set_sign_in_status(request_metadata.sign_in_status);
}

// Set the default search engine currently set in Chrome.
void SetDefaultSearchEngine(feedwire::Request* request,
                            const RequestMetadata& request_metadata) {
  request->mutable_feed_request()
      ->mutable_feed_query()
      ->mutable_chrome_fulfillment_info()
      ->mutable_default_search_engine()
      ->set_search_engine(request_metadata.default_search_engine);
}

void WriteDocIdsTable(const std::vector<DocViewCount> doc_view_counts,
                      feedwire::Table& table) {
  table.set_name("url_all_ondevice");
  table.set_num_rows(doc_view_counts.size());

  feedwire::Table::Column* ids = table.add_columns();
  ids->set_name("dimension_key");
  ids->set_type(feedwire::TypeKind::TYPE_UINT64);
  feedwire::Table::Column* counts = table.add_columns();
  counts->set_name("FEED_CARD_VIEW");
  counts->set_type(feedwire::TypeKind::TYPE_INT64);
  for (const auto& doc_view_count : doc_view_counts) {
    ids->add_uint64_values(doc_view_count.docid);
    counts->add_int64_values(doc_view_count.view_count);
  }
}

}  // namespace

std::string ContentIdString(const feedwire::ContentId& content_id) {
  return base::StrCat({content_id.content_domain(), ",",
                       base::NumberToString(content_id.type()), ",",
                       base::NumberToString(content_id.id())});
}

bool Equal(const feedwire::ContentId& a, const feedwire::ContentId& b) {
  return a.content_domain() == b.content_domain() && a.id() == b.id() &&
         a.type() == b.type();
}

bool CompareContentId(const feedwire::ContentId& a,
                      const feedwire::ContentId& b) {
  // Local variables because tie() needs l-values.
  const int a_id = a.id();
  const int b_id = b.id();
  const int a_type = a.type();
  const int b_type = b.type();
  return std::tie(a.content_domain(), a_id, a_type) <
         std::tie(b.content_domain(), b_id, b_type);
}

bool CompareContent(const feedstore::Content& a, const feedstore::Content& b) {
  const ContentId& a_id = a.content_id();
  const ContentId& b_id = b.content_id();
  if (a_id.id() < b_id.id())
    return true;
  if (a_id.id() > b_id.id())
    return false;
  if (a_id.type() < b_id.type())
    return true;
  if (a_id.type() > b_id.type())
    return false;
  return a.frame() < b.frame();
}

feedwire::ClientInfo CreateClientInfo(const RequestMetadata& request_metadata) {
  feedwire::ClientInfo client_info;

  feedwire::DisplayInfo& display_info = *client_info.add_display_info();
  display_info.set_screen_density(request_metadata.display_metrics.density);
  display_info.set_screen_width_in_pixels(
      request_metadata.display_metrics.width_pixels);
  display_info.set_screen_height_in_pixels(
      request_metadata.display_metrics.height_pixels);

  client_info.set_locale(request_metadata.language_tag);

#if BUILDFLAG(IS_ANDROID)
  client_info.set_platform_type(feedwire::ClientInfo::ANDROID_ID);
#elif BUILDFLAG(IS_IOS)
  client_info.set_platform_type(feedwire::ClientInfo::IOS);
#endif
  client_info.set_app_type(feedwire::ClientInfo::CHROME_ANDROID);
  *client_info.mutable_platform_version() = GetPlatformVersionMessage();
  *client_info.mutable_app_version() =
      GetAppVersionMessage(request_metadata.chrome_info);

  // client_instance_id and session_id should not both be set at the same time.
  DCHECK(request_metadata.client_instance_id.empty() ||
         request_metadata.session_id.empty());

  // Populate client_instance_id, session_id, or neither.
  if (!request_metadata.client_instance_id.empty()) {
    client_info.set_client_instance_id(request_metadata.client_instance_id);
  } else if (!request_metadata.session_id.empty()) {
    client_info.mutable_chrome_client_info()->set_session_id(
        request_metadata.session_id);
  }
  return client_info;
}

feedwire::Request CreateFeedQueryRefreshRequest(
    const StreamType& stream_type,
    feedwire::FeedQuery::RequestReason request_reason,
    const RequestMetadata& request_metadata,
    const std::string& consistency_token,
    const SingleWebFeedEntryPoint single_feed_entry_point,
    const std::vector<DocViewCount> doc_view_counts) {
  feedwire::Request request = CreateFeedQueryRequest(
      stream_type, request_reason, request_metadata, consistency_token,
      std::string(), single_feed_entry_point);
  if (stream_type.IsWebFeed()) {
    // A special token that requests content for followed Web Feeds.
    constexpr char kChromeFollowToken[] = "\"\004\022\002\b5*\tFollowing";
    request.mutable_feed_request()
        ->mutable_feed_query()
        ->mutable_web_feed_token()
        ->mutable_web_feed_token()
        ->set_web_feed_token(kChromeFollowToken);
  } else if (stream_type.IsSingleWebFeed()) {
    // A special token that requests content for the Single Web Feed.
    feedwire::WebFeedIdentifierToken web_feed_id;
    web_feed_id.mutable_web_feed_id()
        ->mutable_domain_web_feed_id()
        ->set_web_feed_name(stream_type.GetWebFeedId().c_str());

    request.mutable_feed_request()
        ->mutable_feed_query()
        ->mutable_web_feed_token()
        ->mutable_web_feed_token()
        ->set_web_feed_token(web_feed_id.SerializeAsString());
  }
  SetNoticeCardAcknowledged(&request, request_metadata);
  SetInfoCardTrackingStates(&request, request_metadata);
  SetTimesFollowedFromWebPageMenu(&request, request_metadata);
  SetChromeSignInStatus(&request, request_metadata);
  SetDefaultSearchEngine(&request, request_metadata);

  if (!doc_view_counts.empty()) {
    WriteDocIdsTable(doc_view_counts, *request.mutable_feed_request()
                                           ->mutable_client_user_profiles()
                                           ->mutable_view_demotion_profile()
                                           ->mutable_view_demotion_profile()
                                           ->add_tables());
  }

  return request;
}

feedwire::Request CreateFeedQueryLoadMoreRequest(
    const RequestMetadata& request_metadata,
    const std::string& consistency_token,
    const std::string& next_page_token) {
  return CreateFeedQueryRequest(
      StreamType(StreamKind::kForYou), feedwire::FeedQuery::NEXT_PAGE_SCROLL,
      request_metadata, consistency_token, next_page_token,
      SingleWebFeedEntryPoint::kOther);
}

}  // namespace feed
