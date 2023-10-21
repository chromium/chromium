// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feed_internals/feedv2_internals_page_handler.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/statistics_recorder.h"
#include "base/time/time.h"
#include "chrome/browser/ui/webui/feed_internals/feed_internals.mojom.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/proto/v2/ui.pb.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/public/feed_api.h"
#include "components/feed/core/v2/public/feed_service.h"
#include "components/feed/core/v2/public/stream_type.h"
#include "components/feed/core/v2/public/types.h"
#include "components/feed/core/v2/public/web_feed_subscriptions.h"
#include "components/feed/feed_feature_list.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "url/gurl.h"

namespace {

const char kFeedHistogramPrefix[] = "ContentSuggestions.Feed.";

// Converts |t| to a delta from the JS epoch, or 0 if |t| is null.
base::TimeDelta InMillisecondsFSinceUnixEpochDelta(base::Time t) {
  return t.is_null() ? base::TimeDelta() : t - base::Time::UnixEpoch();
}

}  // namespace

FeedV2InternalsPageHandler::FeedV2InternalsPageHandler(
    mojo::PendingReceiver<feed_internals::mojom::PageHandler> receiver,
    feed::FeedService* feed_service,
    PrefService* pref_service)
    : receiver_(this, std::move(receiver)), pref_service_(pref_service) {
  feed_stream_ = feed_service->GetStream();
}

FeedV2InternalsPageHandler::~FeedV2InternalsPageHandler() = default;

void FeedV2InternalsPageHandler::GetGeneralProperties(
    GetGeneralPropertiesCallback callback) {
  const feed::DebugStreamData debug_data = feed_stream_->GetDebugStreamData();

  auto properties = feed_internals::mojom::Properties::New();

  properties->is_feed_enabled =
      base::FeatureList::IsEnabled(feed::kInterestFeedV2);

  properties->is_feed_visible = feed_stream_->IsArticlesListVisible();
  properties->is_feed_allowed = IsFeedAllowed();
  properties->is_prefetching_enabled = false;
  properties->is_web_feed_follow_intro_debug_enabled =
      IsWebFeedFollowIntroDebugEnabled();
  properties->use_feed_query_requests = ShouldUseFeedQueryRequests();
  if (debug_data.fetch_info)
    properties->feed_fetch_url = debug_data.fetch_info->base_request_url;
  if (debug_data.upload_info)
    properties->feed_actions_url = debug_data.upload_info->base_request_url;

  properties->load_stream_status = debug_data.load_stream_status;

  properties->following_feed_order = GetFollowingFeedOrder();

  std::move(callback).Run(std::move(properties));
}

void FeedV2InternalsPageHandler::GetLastFetchProperties(
    GetLastFetchPropertiesCallback callback) {
  auto properties = feed_internals::mojom::LastFetchProperties::New();
  feed::DebugStreamData debug_data = feed_stream_->GetDebugStreamData();

  if (debug_data.fetch_info) {
    const feed::NetworkResponseInfo& net_info = *debug_data.fetch_info;
    properties->last_fetch_status = net_info.status_code;
    properties->last_fetch_time =
        InMillisecondsFSinceUnixEpochDelta(net_info.fetch_time);
    properties->last_bless_nonce = net_info.bless_nonce;
  }
  if (debug_data.upload_info) {
    const feed::NetworkResponseInfo& net_info = *debug_data.upload_info;
    properties->last_action_upload_status = net_info.status_code;
    properties->last_action_upload_time =
        InMillisecondsFSinceUnixEpochDelta(net_info.fetch_time);
  }

  std::move(callback).Run(std::move(properties));
}

void FeedV2InternalsPageHandler::RefreshForYouFeed() {
  feed_stream_->ForceRefreshForDebugging(
      feed::StreamType(feed::StreamKind::kForYou));
}

void FeedV2InternalsPageHandler::RefreshFollowingFeed() {
  feed_stream_->ForceRefreshForDebugging(
      feed::StreamType(feed::StreamKind::kFollowing));
}

void FeedV2InternalsPageHandler::RefreshWebFeedSuggestions() {
  feed_stream_->subscriptions().RefreshRecommendedFeeds(base::DoNothing());
}

void FeedV2InternalsPageHandler::GetFeedProcessScopeDump(
    GetFeedProcessScopeDumpCallback callback) {
  std::move(callback).Run(feed_stream_->DumpStateForDebugging());
}

bool FeedV2InternalsPageHandler::IsFeedAllowed() {
  return feed::FeedService::IsEnabled(*pref_service_);
}

void FeedV2InternalsPageHandler::GetFeedHistograms(
    GetFeedHistogramsCallback callback) {
  std::string log;
  base::StatisticsRecorder::WriteGraph(kFeedHistogramPrefix, &log);
  std::move(callback).Run(std::move(log));
}

void FeedV2InternalsPageHandler::OverrideFeedHost(const GURL& host) {
  return pref_service_->SetString(
      feed::prefs::kHostOverrideHost,
      host.is_valid() ? host.spec() : std::string());
}
void FeedV2InternalsPageHandler::OverrideDiscoverApiEndpoint(
    const GURL& endpoint_url) {
  return pref_service_->SetString(
      feed::prefs::kDiscoverAPIEndpointOverride,
      endpoint_url.is_valid() ? endpoint_url.spec() : std::string());
}

void FeedV2InternalsPageHandler::OverrideFeedStreamData(
    const std::vector<uint8_t>& data) {
  feedui::StreamUpdate stream_update;
  feedui::Slice* slice = stream_update.add_updated_slices()->mutable_slice();
  slice->set_slice_id("SetByInternalsPage");
  slice->mutable_xsurface_slice()->set_xsurface_frame(data.data(), data.size());
  feed_stream_->SetForcedStreamUpdateForDebugging(stream_update);
}

bool FeedV2InternalsPageHandler::IsWebFeedFollowIntroDebugEnabled() {
  return pref_service_->GetBoolean(feed::prefs::kEnableWebFeedFollowIntroDebug);
}

void FeedV2InternalsPageHandler::SetWebFeedFollowIntroDebugEnabled(
    const bool enabled) {
  pref_service_->SetBoolean(feed::prefs::kEnableWebFeedFollowIntroDebug,
                            enabled);
}

bool FeedV2InternalsPageHandler::ShouldUseFeedQueryRequests() {
  return feed::GetFeedConfig().use_feed_query_requests;
}

void FeedV2InternalsPageHandler::SetUseFeedQueryRequests(
    const bool use_legacy) {
  feed::SetUseFeedQueryRequests(use_legacy);
}

feed_internals::mojom::FeedOrder
FeedV2InternalsPageHandler::GetFollowingFeedOrder() {
  feed::ContentOrder order = feed_stream_->GetContentOrderFromPrefs(
      feed::StreamType(feed::StreamKind::kFollowing));
  switch (order) {
    case feed::ContentOrder::kUnspecified:
      return feed_internals::mojom::FeedOrder::kUnspecified;
    case feed::ContentOrder::kGrouped:
      return feed_internals::mojom::FeedOrder::kGrouped;
    case feed::ContentOrder::kReverseChron:
      return feed_internals::mojom::FeedOrder::kReverseChron;
  }
}

void FeedV2InternalsPageHandler::SetFollowingFeedOrder(
    const feed_internals::mojom::FeedOrder new_order) {
  feed::ContentOrder order_to_set;
  switch (new_order) {
    case feed_internals::mojom::FeedOrder::kUnspecified:
      order_to_set = feed::ContentOrder::kUnspecified;
      break;
    case feed_internals::mojom::FeedOrder::kGrouped:
      order_to_set = feed::ContentOrder::kGrouped;
      break;
    case feed_internals::mojom::FeedOrder::kReverseChron:
      order_to_set = feed::ContentOrder::kReverseChron;
      break;
  }
  feed_stream_->SetContentOrder(feed::StreamType(feed::StreamKind::kFollowing),
                                order_to_set);
}
