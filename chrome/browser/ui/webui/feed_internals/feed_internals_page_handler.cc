// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feed_internals/feed_internals_page_handler.h"

#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/statistics_recorder.h"
#include "base/time/time.h"
#include "chrome/browser/android/feed/feed_debugging_bridge.h"
#include "chrome/browser/android/feed/feed_lifecycle_bridge.h"
#include "chrome/browser/ui/webui/feed_internals/feed_internals.mojom.h"
#include "components/feed/content/feed_host_service.h"
#include "components/feed/content/feed_offline_host.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/common/user_classifier.h"
#include "components/feed/core/feed_scheduler_host.h"
#include "components/feed/core/shared_prefs/pref_names.h"
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

std::string TriggerTypeToString(feed::TriggerType* trigger) {
  if (trigger == nullptr)
    return "Not set";
  switch (*trigger) {
    case feed::TriggerType::kNtpShown:
      return "NTP Shown";
    case feed::TriggerType::kForegrounded:
      return "Foregrounded";
    case feed::TriggerType::kFixedTimer:
      return "Fixed Timer";
  }
}

}  // namespace

FeedInternalsPageHandler::FeedInternalsPageHandler(
    mojo::PendingReceiver<feed_internals::mojom::PageHandler> receiver,
    feed::FeedHostService* feed_host_service,
    PrefService* pref_service)
    : receiver_(this, std::move(receiver)),
      feed_scheduler_host_(feed_host_service->GetSchedulerHost()),
      feed_offline_host_(feed_host_service->GetOfflineHost()),
      pref_service_(pref_service) {}

FeedInternalsPageHandler::~FeedInternalsPageHandler() = default;

void FeedInternalsPageHandler::GetGeneralProperties(
    GetGeneralPropertiesCallback callback) {
  auto properties = feed_internals::mojom::Properties::New();

  properties->is_feed_enabled = true;
  properties->is_feed_visible =
      pref_service_->GetBoolean(feed::prefs::kArticlesListVisible);
  properties->is_feed_allowed = IsFeedAllowed();
  properties->is_prefetching_enabled =
      offline_pages::prefetch_prefs::IsEnabled(pref_service_);
  properties->feed_fetch_url = feed::GetFeedFetchUrlForDebugging();

  std::move(callback).Run(std::move(properties));
}

void FeedInternalsPageHandler::GetUserClassifierProperties(
    GetUserClassifierPropertiesCallback callback) {
  auto properties = feed_internals::mojom::UserClassifier::New();

  feed::UserClassifier* user_classifier =
      feed_scheduler_host_->GetUserClassifierForDebugging();

  properties->user_class_description =
      user_classifier->GetUserClassDescriptionForDebugging();
  properties->avg_hours_between_views = user_classifier->GetEstimatedAvgTime(
      feed::UserClassifier::Event::kSuggestionsViewed);
  properties->avg_hours_between_uses = user_classifier->GetEstimatedAvgTime(
      feed::UserClassifier::Event::kSuggestionsUsed);

  std::move(callback).Run(std::move(properties));
}

void FeedInternalsPageHandler::GetLastFetchProperties(
    GetLastFetchPropertiesCallback callback) {
  auto properties = feed_internals::mojom::LastFetchProperties::New();

  properties->last_fetch_status =
      feed_scheduler_host_->GetLastFetchStatusForDebugging();
  properties->last_fetch_trigger = TriggerTypeToString(
      feed_scheduler_host_->GetLastFetchTriggerTypeForDebugging());
  properties->last_fetch_time =
      ToJsTimeDelta(pref_service_->GetTime(feed::prefs::kLastFetchAttemptTime));
  properties->refresh_suppress_time = ToJsTimeDelta(
      feed_scheduler_host_->GetSuppressRefreshesUntilForDebugging());
  properties->last_bless_nonce =
      pref_service_->GetString(feed::prefs::kHostOverrideBlessNonce);

  std::move(callback).Run(std::move(properties));
}

void FeedInternalsPageHandler::ClearUserClassifierProperties() {
  feed_scheduler_host_->GetUserClassifierForDebugging()
      ->ClearClassificationForDebugging();
}

void FeedInternalsPageHandler::ClearCachedDataAndRefreshFeed() {
  feed::FeedLifecycleBridge::ClearCachedData();
}

void FeedInternalsPageHandler::RefreshFeed() {
  feed::TriggerRefreshForDebugging();
}

void FeedInternalsPageHandler::GetCurrentContent(
    GetCurrentContentCallback callback) {
  if (!IsFeedAllowed()) {
    std::move(callback).Run(
        std::vector<feed_internals::mojom::SuggestionPtr>());
    return;
  }

  feed_offline_host_->GetCurrentArticleSuggestions(base::BindOnce(
      &FeedInternalsPageHandler::OnGetCurrentArticleSuggestionsDone,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FeedInternalsPageHandler::OnGetCurrentArticleSuggestionsDone(
    GetCurrentContentCallback callback,
    std::vector<offline_pages::PrefetchSuggestion> results) {
  std::vector<feed_internals::mojom::SuggestionPtr> suggestions;

  for (offline_pages::PrefetchSuggestion result : results) {
    auto suggestion = feed_internals::mojom::Suggestion::New();
    suggestion->title = std::move(result.article_title);
    suggestion->url = std::move(result.article_url);
    suggestion->publisher_name = std::move(result.article_attribution);
    suggestion->image_url = std::move(result.thumbnail_url);
    suggestion->favicon_url = std::move(result.favicon_url);

    suggestions.push_back(std::move(suggestion));
  }

  std::move(callback).Run(std::move(suggestions));
}

void FeedInternalsPageHandler::GetFeedProcessScopeDump(
    GetFeedProcessScopeDumpCallback callback) {
  std::move(callback).Run(feed::GetFeedProcessScopeDumpForDebugging());
}

bool FeedInternalsPageHandler::IsFeedAllowed() {
  return pref_service_->GetBoolean(feed::prefs::kEnableSnippets);
}

void FeedInternalsPageHandler::GetFeedHistograms(
    GetFeedHistogramsCallback callback) {
  std::string log;
  base::StatisticsRecorder::WriteGraph(kFeedHistogramPrefix, &log);
  std::move(callback).Run(log);
}

void FeedInternalsPageHandler::OverrideFeedHost(const GURL& host) {
  pref_service_->SetString(feed::prefs::kHostOverrideHost,
                           host.is_valid() ? host.spec() : std::string());
}

void FeedInternalsPageHandler::OverrideActionUploadEndpoint(
    const GURL& endpoint_url) {
  // Not implemented for Feed v1.
}
