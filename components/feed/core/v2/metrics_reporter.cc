// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/feed/core/v2/metrics_reporter.h"

#include <algorithm>
#include <cmath>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "components/feed/core/v2/prefs.h"

namespace feed {
namespace {
using feed::internal::FeedEngagementType;
using feed::internal::FeedUserActionType;
const int kMaxSuggestionsTotal = 50;
// Maximum time to wait before declaring a load operation failed.
// For both ContentSuggestions.Feed.UserJourney.OpenFeed
// and ContentSuggestions.Feed.UserJourney.GetMore.
constexpr base::TimeDelta kLoadTimeout = base::TimeDelta::FromSeconds(15);
// Maximum time to wait before declaring opening a card a failure.
// For ContentSuggestions.Feed.UserJourney.OpenCard.
constexpr base::TimeDelta kOpenTimeout = base::TimeDelta::FromSeconds(20);
// For ContentSuggestions.Feed.TimeSpentInFeed, we want to get a measure
// of how much time the user is spending with the Feed. If the user stops
// interacting with the Feed, we stop counting it as time spent after this
// timeout.
constexpr base::TimeDelta kTimeSpentInFeedInteractionTimeout =
    base::TimeDelta::FromSeconds(30);

void ReportEngagementTypeHistogram(FeedEngagementType engagement_type) {
  base::UmaHistogramEnumeration("ContentSuggestions.Feed.EngagementType",
                                engagement_type);
}

void ReportContentSuggestionsOpened(int index_in_stream) {
  base::UmaHistogramExactLinear("NewTabPage.ContentSuggestions.Opened",
                                index_in_stream, kMaxSuggestionsTotal);
}

void ReportUserActionHistogram(FeedUserActionType action_type) {
  base::UmaHistogramEnumeration("ContentSuggestions.Feed.UserActions",
                                action_type);
}

std::string LoadLatencyStepName(LoadLatencyTimes::StepKind kind) {
  switch (kind) {
    case LoadLatencyTimes::kTaskExecution:
      return "TaskStart";
    case LoadLatencyTimes::kLoadFromStore:
      return "LoadFromStore";
    case LoadLatencyTimes::kUploadActions:
      return "ActionUpload";
    case LoadLatencyTimes::kQueryRequest:
      return "QueryRequest";
    case LoadLatencyTimes::kStreamViewed:
      return "StreamView";
  }
}

void ReportLoadLatencies(std::unique_ptr<LoadLatencyTimes> latencies) {
  for (const LoadLatencyTimes::Step& step : latencies->steps()) {
    base::UmaHistogramCustomTimes(
        "ContentSuggestions.Feed.LoadStepLatency." +
            LoadLatencyStepName(step.kind),
        step.latency, base::TimeDelta::FromMilliseconds(50), kLoadTimeout, 50);
  }
}

}  // namespace

MetricsReporter::MetricsReporter(const base::TickClock* clock,
                                 PrefService* profile_prefs)
    : clock_(clock), profile_prefs_(profile_prefs) {
  persistent_data_ = prefs::GetPersistentMetricsData(*profile_prefs_);
  ReportPersistentDataIfDayIsDone();
}

MetricsReporter::~MetricsReporter() {
  FinalizeMetrics();
}

void MetricsReporter::OnEnterBackground() {
  FinalizeMetrics();
}

// Engagement Tracking.

void MetricsReporter::RecordInteraction() {
  RecordEngagement(/*scroll_distance_dp=*/0, /*interacted=*/true);
  ReportEngagementTypeHistogram(FeedEngagementType::kFeedInteracted);
}

void MetricsReporter::TrackTimeSpentInFeed(bool interacted_or_scrolled) {
  if (time_in_feed_start_) {
    ReportPersistentDataIfDayIsDone();
    persistent_data_.accumulated_time_spent_in_feed +=
        std::min(kTimeSpentInFeedInteractionTimeout,
                 clock_->NowTicks() - *time_in_feed_start_);
    time_in_feed_start_ = base::nullopt;
  }

  if (interacted_or_scrolled) {
    time_in_feed_start_ = clock_->NowTicks();
  }
}

void MetricsReporter::FinalizeVisit() {
  if (!engaged_simple_reported_)
    return;
  engaged_reported_ = false;
  engaged_simple_reported_ = false;
  scrolled_reported_ = false;
  TrackTimeSpentInFeed(false);
}

void MetricsReporter::RecordEngagement(int scroll_distance_dp,
                                       bool interacted) {
  scroll_distance_dp = std::abs(scroll_distance_dp);
  // Determine if this interaction is part of a new 'session'.
  auto now = clock_->NowTicks();
  const base::TimeDelta kVisitTimeout = base::TimeDelta::FromMinutes(5);
  if (now - visit_start_time_ > kVisitTimeout) {
    FinalizeVisit();
  }
  // Reset the last active time for session measurement.
  visit_start_time_ = now;

  TrackTimeSpentInFeed(true);

  // Report the user as engaged-simple if they have scrolled any amount or
  // interacted with the card, and we have not already reported it for this
  // chrome run.
  if (!engaged_simple_reported_ && (scroll_distance_dp > 0 || interacted)) {
    ReportEngagementTypeHistogram(FeedEngagementType::kFeedEngagedSimple);
    engaged_simple_reported_ = true;
  }

  // Report the user as engaged if they have scrolled more than the threshold or
  // interacted with the card, and we have not already reported it this chrome
  // run.
  const int kMinScrollThresholdDp = 160;  // 1 inch.
  if (!engaged_reported_ &&
      (scroll_distance_dp > kMinScrollThresholdDp || interacted)) {
    ReportEngagementTypeHistogram(FeedEngagementType::kFeedEngaged);
    engaged_reported_ = true;
  }
}

void MetricsReporter::StreamScrollStart() {
  // Note that |TrackTimeSpentInFeed()| is called as a result of
  // |StreamScrolled()| as well. Tracking the start of scroll events ensures we
  // don't miss out on long and slow scrolling.
  TrackTimeSpentInFeed(true);
}

void MetricsReporter::StreamScrolled(int distance_dp) {
  RecordEngagement(distance_dp, /*interacted=*/false);

  if (!scrolled_reported_) {
    ReportEngagementTypeHistogram(FeedEngagementType::kFeedScrolled);
    scrolled_reported_ = true;
  }
}

void MetricsReporter::ContentSliceViewed(SurfaceId surface_id,
                                         int index_in_stream) {
  base::UmaHistogramExactLinear("NewTabPage.ContentSuggestions.Shown",
                                index_in_stream, kMaxSuggestionsTotal);
}

void MetricsReporter::FeedViewed(SurfaceId surface_id) {
  if (load_latencies_) {
    load_latencies_->StepComplete(LoadLatencyTimes::kStreamViewed);

    // Log latencies for debugging.
    if (VLOG_IS_ON(2)) {
      for (const LoadLatencyTimes::Step& step : load_latencies_->steps()) {
        DVLOG(2) << "LoadStepLatency." << LoadLatencyStepName(step.kind)
                 << " = " << step.latency;
      }
    }

    if (!load_latencies_recorded_) {
      // Use |load_latencies_recorded_| to only report load latencies once.
      // This generally will be the worst-case, since caches are likely to be
      // cold, a network request is more likely to be required, and Chrome
      // may be working on initializing other systems.
      load_latencies_recorded_ = true;
      ReportLoadLatencies(std::move(load_latencies_));
    }
    load_latencies_ = nullptr;
  }
  ReportOpenFeedIfNeeded(surface_id, true);
}

void MetricsReporter::OpenAction(int index_in_stream) {
  CardOpenBegin();
  ReportUserActionHistogram(FeedUserActionType::kTappedOnCard);
  base::RecordAction(
      base::UserMetricsAction("ContentSuggestions.Feed.CardAction.Open"));
  ReportContentSuggestionsOpened(index_in_stream);
  RecordInteraction();
}

void MetricsReporter::OpenVisitComplete(base::TimeDelta visit_time) {
  base::UmaHistogramLongTimes("ContentSuggestions.Feed.VisitDuration",
                              visit_time);
}

void MetricsReporter::OpenInNewTabAction(int index_in_stream) {
  CardOpenBegin();
  ReportUserActionHistogram(FeedUserActionType::kTappedOpenInNewTab);
  base::RecordAction(base::UserMetricsAction(
      "ContentSuggestions.Feed.CardAction.OpenInNewTab"));
  ReportContentSuggestionsOpened(index_in_stream);
  RecordInteraction();
}

void MetricsReporter::OpenInNewIncognitoTabAction() {
  ReportUserActionHistogram(FeedUserActionType::kTappedOpenInNewIncognitoTab);
  base::RecordAction(base::UserMetricsAction(
      "ContentSuggestions.Feed.CardAction.OpenInNewIncognitoTab"));
  RecordInteraction();
}

void MetricsReporter::SendFeedbackAction() {
  ReportUserActionHistogram(FeedUserActionType::kTappedSendFeedback);
  base::RecordAction(base::UserMetricsAction(
      "ContentSuggestions.Feed.CardAction.SendFeedback"));
  RecordInteraction();
}

void MetricsReporter::DownloadAction() {
  ReportUserActionHistogram(FeedUserActionType::kTappedDownload);
  base::RecordAction(
      base::UserMetricsAction("ContentSuggestions.Feed.CardAction.Download"));
  RecordInteraction();
}

void MetricsReporter::LearnMoreAction() {
  ReportUserActionHistogram(FeedUserActionType::kTappedLearnMore);
  base::RecordAction(
      base::UserMetricsAction("ContentSuggestions.Feed.CardAction.LearnMore"));
  RecordInteraction();
}

void MetricsReporter::TurnOnAction() {
  ReportUserActionHistogram(FeedUserActionType::kTappedTurnOn);
}

void MetricsReporter::TurnOffAction() {
  ReportUserActionHistogram(FeedUserActionType::kTappedTurnOff);
}

void MetricsReporter::NavigationStarted() {
  // TODO(harringtond): Use this or remove it.
}

void MetricsReporter::PageLoaded() {
  ReportCardOpenEndIfNeeded(true);
}

void MetricsReporter::RemoveAction() {
  ReportUserActionHistogram(FeedUserActionType::kTappedHideStory);
  base::RecordAction(
      base::UserMetricsAction("ContentSuggestions.Feed.CardAction.HideStory"));
  RecordInteraction();
}

void MetricsReporter::NotInterestedInAction() {
  ReportUserActionHistogram(FeedUserActionType::kTappedNotInterestedIn);
  base::RecordAction(base::UserMetricsAction(
      "ContentSuggestions.Feed.CardAction.NotInterestedIn"));
  RecordInteraction();
}

void MetricsReporter::ManageInterestsAction() {
  ReportUserActionHistogram(FeedUserActionType::kTappedManageInterests);
  base::RecordAction(base::UserMetricsAction(
      "ContentSuggestions.Feed.CardAction.ManageInterests"));
  RecordInteraction();
}

void MetricsReporter::ContextMenuOpened() {
  ReportUserActionHistogram(FeedUserActionType::kOpenedContextMenu);
  base::RecordAction(base::UserMetricsAction(
      "ContentSuggestions.Feed.CardAction.ContextMenu"));
}

void MetricsReporter::EphemeralStreamChange() {
  ReportUserActionHistogram(FeedUserActionType::kEphemeralChange);
}

void MetricsReporter::EphemeralStreamChangeRejected() {
  ReportUserActionHistogram(FeedUserActionType::kEphemeralChangeRejected);
}

void MetricsReporter::SurfaceOpened(SurfaceId surface_id) {
  ReportPersistentDataIfDayIsDone();
  surfaces_waiting_for_content_.emplace(surface_id, clock_->NowTicks());
  ReportUserActionHistogram(FeedUserActionType::kOpenedFeedSurface);
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&MetricsReporter::ReportOpenFeedIfNeeded, GetWeakPtr(),
                     surface_id, false),
      kLoadTimeout);
}

void MetricsReporter::SurfaceClosed(SurfaceId surface_id) {
  ReportOpenFeedIfNeeded(surface_id, false);
  ReportGetMoreIfNeeded(surface_id, false);
}

void MetricsReporter::FinalizeMetrics() {
  FinalizeVisit();
  ReportCardOpenEndIfNeeded(false);
  for (auto iter = surfaces_waiting_for_content_.begin();
       iter != surfaces_waiting_for_content_.end();) {
    ReportOpenFeedIfNeeded((iter++)->first, false);
  }
  for (auto iter = surfaces_waiting_for_more_content_.begin();
       iter != surfaces_waiting_for_more_content_.end();) {
    ReportGetMoreIfNeeded((iter++)->first, false);
  }
  prefs::SetPersistentMetricsData(persistent_data_, *profile_prefs_);
}

void MetricsReporter::ReportOpenFeedIfNeeded(SurfaceId surface_id,
                                             bool success) {
  auto iter = surfaces_waiting_for_content_.find(surface_id);
  if (iter == surfaces_waiting_for_content_.end())
    return;
  base::TimeDelta latency = clock_->NowTicks() - iter->second;
  surfaces_waiting_for_content_.erase(iter);

  if (success) {
    base::UmaHistogramCustomTimes(
        "ContentSuggestions.Feed.UserJourney.OpenFeed.SuccessDuration", latency,
        base::TimeDelta::FromMilliseconds(50), kLoadTimeout, 50);
  } else {
    base::UmaHistogramCustomTimes(
        "ContentSuggestions.Feed.UserJourney.OpenFeed.FailureDuration", latency,
        base::TimeDelta::FromMilliseconds(50), kLoadTimeout, 50);
  }
}

void MetricsReporter::ReportGetMoreIfNeeded(SurfaceId surface_id,
                                            bool success) {
  auto iter = surfaces_waiting_for_more_content_.find(surface_id);
  if (iter == surfaces_waiting_for_more_content_.end())
    return;
  base::TimeDelta latency = clock_->NowTicks() - iter->second;
  surfaces_waiting_for_more_content_.erase(iter);
  if (success) {
    base::UmaHistogramCustomTimes(
        "ContentSuggestions.Feed.UserJourney.GetMore.SuccessDuration", latency,
        base::TimeDelta::FromMilliseconds(50), kLoadTimeout, 50);
  } else {
    base::UmaHistogramCustomTimes(
        "ContentSuggestions.Feed.UserJourney.GetMore.FailureDuration", latency,
        base::TimeDelta::FromMilliseconds(50), kLoadTimeout, 50);
  }
}

void MetricsReporter::CardOpenBegin() {
  ReportCardOpenEndIfNeeded(false);
  pending_open_ = clock_->NowTicks();
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&MetricsReporter::CardOpenTimeout, GetWeakPtr(),
                     *pending_open_),
      kOpenTimeout);
}

void MetricsReporter::CardOpenTimeout(base::TimeTicks start_ticks) {
  if (pending_open_ && start_ticks == *pending_open_)
    ReportCardOpenEndIfNeeded(false);
}

void MetricsReporter::ReportCardOpenEndIfNeeded(bool success) {
  if (!pending_open_)
    return;
  base::TimeDelta latency = clock_->NowTicks() - *pending_open_;
  pending_open_.reset();
  if (success) {
    base::UmaHistogramCustomTimes(
        "ContentSuggestions.Feed.UserJourney.OpenCard.SuccessDuration", latency,
        base::TimeDelta::FromMilliseconds(100), kOpenTimeout, 50);
  } else {
    base::UmaHistogramBoolean(
        "ContentSuggestions.Feed.UserJourney.OpenCard.Failure", true);
  }
}

void MetricsReporter::NetworkRequestComplete(NetworkRequestType type,
                                             int http_status_code) {
  switch (type) {
    case NetworkRequestType::kFeedQuery:
      base::UmaHistogramSparse(
          "ContentSuggestions.Feed.Network.ResponseStatus.FeedQuery",
          http_status_code);
      return;
    case NetworkRequestType::kUploadActions:
      base::UmaHistogramSparse(
          "ContentSuggestions.Feed.Network.ResponseStatus.UploadActions",
          http_status_code);
      return;
  }
}

void MetricsReporter::OnLoadStream(
    LoadStreamStatus load_from_store_status,
    LoadStreamStatus final_status,
    std::unique_ptr<LoadLatencyTimes> load_latencies) {
  DVLOG(1) << "OnLoadStream load_from_store_status=" << load_from_store_status
           << " final_status=" << final_status;
  load_latencies_ = std::move(load_latencies);
  base::UmaHistogramEnumeration(
      "ContentSuggestions.Feed.LoadStreamStatus.Initial", final_status);
  if (load_from_store_status != LoadStreamStatus::kNoStatus) {
    base::UmaHistogramEnumeration(
        "ContentSuggestions.Feed.LoadStreamStatus.InitialFromStore",
        load_from_store_status);
  }
}

void MetricsReporter::OnBackgroundRefresh(LoadStreamStatus final_status) {
  base::UmaHistogramEnumeration(
      "ContentSuggestions.Feed.LoadStreamStatus.BackgroundRefresh",
      final_status);
}

void MetricsReporter::OnLoadMoreBegin(SurfaceId surface_id) {
  ReportGetMoreIfNeeded(surface_id, false);
  surfaces_waiting_for_more_content_.emplace(surface_id, clock_->NowTicks());

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&MetricsReporter::ReportGetMoreIfNeeded, GetWeakPtr(),
                     surface_id, false),
      kLoadTimeout);
}

void MetricsReporter::OnLoadMore(LoadStreamStatus status) {
  DVLOG(1) << "OnLoadMore status=" << status;
  base::UmaHistogramEnumeration(
      "ContentSuggestions.Feed.LoadStreamStatus.LoadMore", status);
}

void MetricsReporter::OnImageFetched(int net_error_or_http_status) {
  base::UmaHistogramSparse("ContentSuggestions.Feed.ImageFetchStatus",
                           net_error_or_http_status);
}

void MetricsReporter::OnUploadActionsBatch(UploadActionsBatchStatus status) {
  DVLOG(1) << "UploadActionsBatchStatus: " << status;
  base::UmaHistogramEnumeration(
      "ContentSuggestions.Feed.UploadActionsBatchStatus", status);
}

void MetricsReporter::OnUploadActions(UploadActionsStatus status) {
  DVLOG(1) << "UploadActionsTask finished with status " << status;
  base::UmaHistogramEnumeration("ContentSuggestions.Feed.UploadActionsStatus",
                                status);
}

void MetricsReporter::ActivityLoggingEnabled(
    bool response_has_logging_enabled) {
  base::UmaHistogramBoolean("ContentSuggestions.Feed.ActivityLoggingEnabled",
                            response_has_logging_enabled);
}

void MetricsReporter::NoticeCardFulfilled(bool response_has_notice_card) {
  base::UmaHistogramBoolean("ContentSuggestions.Feed.NoticeCardFulfilled2",
                            response_has_notice_card);
}

void MetricsReporter::NoticeCardFulfilledObsolete(
    bool response_has_notice_card) {
  base::UmaHistogramBoolean("ContentSuggestions.Feed.NoticeCardFulfilled",
                            response_has_notice_card);
}

void MetricsReporter::SurfaceReceivedContent(SurfaceId surface_id) {
  ReportGetMoreIfNeeded(surface_id, true);
}

void MetricsReporter::OnClearAll(base::TimeDelta time_since_last_clear) {
  base::UmaHistogramCustomTimes(
      "ContentSuggestions.Feed.Scheduler.TimeSinceLastFetchOnClear",
      time_since_last_clear, base::TimeDelta::FromSeconds(1),
      base::TimeDelta::FromDays(7),
      /*bucket_count=*/50);
}

void MetricsReporter::ReportPersistentDataIfDayIsDone() {
  // Reset the persistent data if 24 hours have elapsed, or if it has never
  // been initialized.
  bool reset_data = false;
  if (persistent_data_.current_day_start.is_null()) {
    reset_data = true;
  } else {
    // Report metrics if 24 hours have passed since the day started.
    const base::TimeDelta since_day_start =
        (base::Time::Now() - persistent_data_.current_day_start);
    if (since_day_start > base::TimeDelta::FromDays(1)
        // Allow up to 1 hour of negative delta, for expected clock changes.
        || since_day_start < -base::TimeDelta::FromHours(1)) {
      if (persistent_data_.accumulated_time_spent_in_feed > base::TimeDelta()) {
        base::UmaHistogramLongTimes(
            "ContentSuggestions.Feed.TimeSpentInFeed",
            persistent_data_.accumulated_time_spent_in_feed);
      }

      reset_data = true;
    }
  }

  if (reset_data) {
    persistent_data_ = PersistentMetricsData();
    persistent_data_.current_day_start = base::Time::Now().LocalMidnight();
    prefs::SetPersistentMetricsData(persistent_data_, *profile_prefs_);
  }
}

}  // namespace feed
