// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feed_internals/feedv2_internals_page_handler.h"

#include <utility>

#include "base/feature_list.h"
#include "base/metrics/statistics_recorder.h"
#include "base/time/time.h"
#include "chrome/browser/ui/webui/feed_internals/feed_internals.mojom.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/proto/v2/ui.pb.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#include "components/feed/core/v2/public/feed_api.h"
#include "components/feed/core/v2/public/feed_service.h"
#include "components/feed/core/v2/public/types.h"
#include "components/feed/feed_feature_list.h"
#include "components/offline_pages/core/prefetch/prefetch_prefs.h"
#include "components/offline_pages/core/prefetch/suggestions_provider.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "url/gurl.h"

namespace {

const char kFeedHistogramPrefix[] = "ContentSuggestions.Feed.";

// Converts |t| to a delta from the JS epoch, or 0 if |t| is null.
base::TimeDelta ToJsTimeDelta(base::Time t) {
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
  properties->is_prefetching_enabled =
      offline_pages::prefetch_prefs::IsEnabled(pref_service_);
  properties->is_web_feed_ui_enabled = IsWebFeedUIEnabled();
  if (debug_data.fetch_info)
    properties->feed_fetch_url = debug_data.fetch_info->base_request_url;
  if (debug_data.upload_info)
    properties->feed_actions_url = debug_data.upload_info->base_request_url;

  properties->load_stream_status = debug_data.load_stream_status;

  std::move(callback).Run(std::move(properties));
}

void FeedV2InternalsPageHandler::GetUserClassifierProperties(
    GetUserClassifierPropertiesCallback callback) {
  // TODO(crbug.com/1066230): Either implement this or remove it.

  std::move(callback).Run(feed_internals::mojom::UserClassifier::New());
}

void FeedV2InternalsPageHandler::GetLastFetchProperties(
    GetLastFetchPropertiesCallback callback) {
  auto properties = feed_internals::mojom::LastFetchProperties::New();
  feed::DebugStreamData debug_data = feed_stream_->GetDebugStreamData();

  if (debug_data.fetch_info) {
    const feed::NetworkResponseInfo& net_info = *debug_data.fetch_info;
    properties->last_fetch_status = net_info.status_code;
    properties->last_fetch_time = ToJsTimeDelta(net_info.fetch_time);
    properties->last_bless_nonce = net_info.bless_nonce;
  }
  if (debug_data.upload_info) {
    const feed::NetworkResponseInfo& net_info = *debug_data.upload_info;
    properties->last_action_upload_status = net_info.status_code;
    properties->last_action_upload_time = ToJsTimeDelta(net_info.fetch_time);
  }

  std::move(callback).Run(std::move(properties));
}

void FeedV2InternalsPageHandler::ClearUserClassifierProperties() {
  // TODO(crbug.com/1066230): Remove or implement this.
}

void FeedV2InternalsPageHandler::ClearCachedDataAndRefreshFeed() {
  // TODO(crbug.com/1066230): Not sure we need to clear cache since we don't
  // retain data on refresh.
  feed_stream_->ForceRefreshForDebugging();
}

void FeedV2InternalsPageHandler::RefreshFeed() {
  feed_stream_->ForceRefreshForDebugging();
}

void FeedV2InternalsPageHandler::GetCurrentContent(
    GetCurrentContentCallback callback) {
  if (!IsFeedAllowed()) {
    std::move(callback).Run({});
    return;
  }
  // TODO(crbug.com/1066230): Content metadata is (yet?) available. I wasn't
  // able to get this to work for v1 either, so maybe it's not that important
  // to implement. We should remove |GetCurrentContent| if it's not needed.
  std::move(callback).Run({});
}

void FeedV2InternalsPageHandler::GetFeedProcessScopeDump(
    GetFeedProcessScopeDumpCallback callback) {
  std::move(callback).Run(feed_stream_->DumpStateForDebugging());
}

bool FeedV2InternalsPageHandler::IsFeedAllowed() {
  return pref_service_->GetBoolean(feed::prefs::kEnableSnippets);
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

bool FeedV2InternalsPageHandler::IsWebFeedUIEnabled() {
  return pref_service_->GetBoolean(feed::prefs::kEnableWebFeedUI);
}

void FeedV2InternalsPageHandler::SetWebFeedUIEnabled(const bool enabled) {
  pref_service_->SetBoolean(feed::prefs::kEnableWebFeedUI, enabled);
}
