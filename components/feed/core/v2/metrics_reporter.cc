// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/feed/core/v2/metrics_reporter.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <ratio>
#include <string>
#include <string_view>

#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feedstore_util.h"
#include "components/feed/core/v2/prefs.h"
#include "components/feed/core/v2/public/common_enums.h"
#include "components/feed/core/v2/public/feed_api.h"
#include "components/feed/core/v2/public/stream_type.h"
#include "components/feed/core/v2/public/web_feed_subscriptions.h"
#include "components/feed/feed_feature_list.h"

// Define a VVLOG macro for verbose logging. We want logging on release builds
// so that instrumentation tests can enable logs here. For official builds, use
// DVLOG instead to avoid any logging overhead.
#ifndef OFFICIAL_BUILD
#define VVLOG VLOG(2)
#else
#define VVLOG DVLOG(2)
#endif

namespace feed {
namespace {
StreamKind kStreamKinds[] = {StreamKind::kForYou, StreamKind::kSupervisedUser,
                             StreamKind::kFollowing,
                             StreamKind::kSingleWebFeed};
// TODO(crbug.com/40869325) Add kSingleWebFeed streams to metrics reporting
// below
using feed::FeedEngagementType;
using feed::FeedUserActionType;
const int kMaxSuggestionsTotal = 50;
// Maximum time to wait before declaring a load operation failed.
// For both ContentSuggestions.Feed.UserJourney.OpenFeed
// and ContentSuggestions.Feed.UserJourney.GetMore.
constexpr base::TimeDelta kLoadTimeout = base::Seconds(15);
// Maximum time to wait before declaring opening a card a failure.
// For ContentSuggestions.Feed.UserJourney.OpenCard.
constexpr base::TimeDelta kOpenTimeout = base::Seconds(20);
// For ContentSuggestions.Feed.TimeSpentInFeed, we want to get a measure
// of how much time the user is spending with the Feed. If the user stops
// interacting with the Feed, we stop counting it as time spent after this
// timeout.
constexpr base::TimeDelta kTimeSpentInFeedInteractionTimeout =
    base::Seconds(30);
// The maximum time between sequential interactions with the feed that are
// considered as a single visit.
constexpr base::TimeDelta kVisitTimeout = base::Minutes(5);
// A feed visit is "good" if the user spends at least this much time in the feed
// and scrolls at least once.
constexpr base::TimeDelta kGoodTimeInFeed = base::Minutes(1);
// A feed visit is "good" if the user spends at least this much time in an
// article.
constexpr base::TimeDelta kLongOpenTime = base::Seconds(10);
// When calculating time spent in feed for good visits, drop periods of
// viewport-stable feed viewing shorter than this.
constexpr base::TimeDelta kMinStableContentSliceVisibilityTime =
    base::Milliseconds(500);
// When calculating time spent in feed for good visits, cap long periods of
// viewport-stable feed viewing to this time.
constexpr base::TimeDelta kMaxStableContentSliceVisibilityTime =
    base::Seconds(30);

std::string_view HistogramReplacement(const StreamType& stream_type) {
  switch (stream_type.GetKind()) {
    case StreamKind::kSupervisedUser:
      return "Feed.SupervisedFeed.";
    case StreamKind::kForYou:
      return "Feed.";
    case StreamKind::kFollowing:
      return "Feed.WebFeed.";
    case StreamKind::kSingleWebFeed:
      return "Feed.SingleWebFeed.";
    case StreamKind::kUnknown:
      DCHECK(false) << "unknown feed kind";
      return "Feed.";
  }
}

void ReportEngagementTypeHistogram(const StreamType& stream_type,
                                   FeedEngagementType engagement_type) {
  base::UmaHistogramEnumeration(
      base::StrCat({"ContentSuggestions.", HistogramReplacement(stream_type),
                    "EngagementType"}),
      engagement_type);
}

void ReportCombinedEngagementTypeHistogram(FeedEngagementType engagement_type) {
  base::UmaHistogramEnumeration(
      "ContentSuggestions.Feed.AllFeeds.EngagementType", engagement_type);
}

void ReportContentSuggestionsOpened(const StreamType& stream_type,
                                    int index_in_stream) {
  switch (stream_type.GetKind()) {
    case StreamKind::kForYou:
      base::UmaHistogramExactLinear("NewTabPage.ContentSuggestions.Opened",
                                    index_in_stream, kMaxSuggestionsTotal);
      break;
    case StreamKind::kFollowing:
      base::UmaHistogramExactLinear("ContentSuggestions.Feed.WebFeed.Opened",
                                    index_in_stream, kMaxSuggestionsTotal);
      break;
    case StreamKind::kSingleWebFeed:
      base::UmaHistogramExactLinear(
          "ContentSuggestions.Feed.SingleWebFeed.Opened", index_in_stream,
          kMaxSuggestionsTotal);
      break;
    case StreamKind::kSupervisedUser:
      base::UmaHistogramExactLinear(
          "ContentSuggestions.Feed.SupervisedFeed.Opened", index_in_stream,
          kMaxSuggestionsTotal);
      break;
    case StreamKind::kUnknown:
      DCHECK(false) << "unknown feed kind";
      break;
  }
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

std::string_view ContentOrderToString(ContentOrder content_order) {
  switch (content_order) {
    case ContentOrder::kUnspecified:
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];
    case ContentOrder::kGrouped:
      return "Grouped";
    case ContentOrder::kReverseChron:
      return "ReverseChron";
  }
}

FeedSortType GetSortTypeFromContentOrder(ContentOrder content_order) {
  switch (content_order) {
    case ContentOrder::kUnspecified:
      return FeedSortType::kUnspecifiedSortType;
    case ContentOrder::kGrouped:
      return FeedSortType::kGroupedByPublisher;
    case ContentOrder::kReverseChron:
      return FeedSortType::kSortedByLatest;
  }
}

void ReportLoadLatencies(std::unique_ptr<LoadLatencyTimes> latencies) {
  for (const LoadLatencyTimes::Step& step : latencies->steps()) {
    // TODO(crbug.com/40158714): Add a WebFeed-specific histogram for this.
    base::UmaHistogramCustomTimes("ContentSuggestions.Feed.LoadStepLatency." +
                                      LoadLatencyStepName(step.kind),
                                  step.latency, base::Milliseconds(50),
                                  kLoadTimeout, 50);
  }
}

void ReportContentLifetimeStaleAge(base::TimeDelta content_lifetime_stale_age) {
  if (content_lifetime_stale_age.is_zero()) {
    base::UmaHistogramBoolean(
        "ContentSuggestions.Feed.ContentLifetime.StaleAgeIsPresent", false);
    return;
  }
  base::UmaHistogramBoolean(
      "ContentSuggestions.Feed.ContentLifetime.StaleAgeIsPresent", true);
  base::UmaHistogramCustomTimes(
      "ContentSuggestions.Feed.ContentLifetime.StaleAge",
      content_lifetime_stale_age, base::Minutes(1), base::Days(7),
      /*buckets=*/50);
}

void ReportContentLifetimeInvalidAge(
    base::TimeDelta content_lifetime_invalid_age) {
  if (content_lifetime_invalid_age.is_zero()) {
    base::UmaHistogramBoolean(
        "ContentSuggestions.Feed.ContentLifetime.InvalidAgeIsPresent", false);
    return;
  }
  base::UmaHistogramBoolean(
      "ContentSuggestions.Feed.ContentLifetime.InvalidAgeIsPresent", true);
  base::UmaHistogramCustomTimes(
      "ContentSuggestions.Feed.ContentLifetime.InvalidAge",
      content_lifetime_invalid_age, base::Minutes(1), base::Days(7),
      /*buckets=*/50);
}

std::string_view NetworkRequestTypeUmaName(NetworkRequestType type) {
  switch (type) {
    case NetworkRequestType::kFeedQuery:
      return "FeedQuery";
    case NetworkRequestType::kUploadActions:
      return "UploadActions";
    case NetworkRequestType::kNextPage:
      return "NextPage";
    case NetworkRequestType::kListWebFeeds:
      return "ListFollowedWebFeeds";
    case NetworkRequestType::kUnfollowWebFeed:
      return "UnfollowWebFeed";
    case NetworkRequestType::kFollowWebFeed:
      return "FollowWebFeed";
    case NetworkRequestType::kListRecommendedWebFeeds:
      return "ListRecommendedWebFeeds";
    case NetworkRequestType::kWebFeedListContents:
      return "WebFeedListContents";
    case NetworkRequestType::kSingleWebFeedListContents:
      return "SingleWebFeedListContents";
    case NetworkRequestType::kQueryInteractiveFeed:
      return "QueryInteractiveFeed";
    case NetworkRequestType::kQueryBackgroundFeed:
      return "QueryBackgroundFeed";
    case NetworkRequestType::kQueryNextPage:
      return "QueryNextPage";
    case NetworkRequestType::kQueryWebFeed:
      return "QueryWebFeed";
    case NetworkRequestType::kSupervisedFeed:
      return "SupervisedFeed";
  }
}

std::string InfoCardActionUmaName(const StreamType& stream_type,
                                  std::string_view action_name) {
  return base::StrCat({"ContentSuggestions.", HistogramReplacement(stream_type),
                       "InfoCard.", action_name});
}

UserSettingsOnStart GetUserSettingsOnStart(
    bool isEnabledByEnterprisePolicy,
    bool isFeedVisible,
    bool isSignedIn,
    bool isEnabled,
    const feedstore::Metadata& metadata) {
  if (!isEnabledByEnterprisePolicy)
    return UserSettingsOnStart::kFeedNotEnabledByPolicy;
  if (!isEnabled)
    return UserSettingsOnStart::kFeedNotEnabled;
  if (!isFeedVisible) {
    if (isSignedIn)
      return UserSettingsOnStart::kFeedNotVisibleSignedIn;
    return UserSettingsOnStart::kFeedNotVisibleSignedOut;
  }
  if (!isSignedIn)
    return UserSettingsOnStart::kSignedOut;

  const base::Time now = base::Time::Now();
  bool has_recent_data = false;
  for (const feedstore::Metadata::StreamMetadata& stream_meta :
       metadata.stream_metadata()) {
    base::TimeDelta delta = now - feedstore::FromTimestampMillis(
                                      stream_meta.last_fetch_time_millis());
    if (delta >= base::TimeDelta() && delta <= kUserSettingsMaxAge)
      has_recent_data = true;
  }

  if (!has_recent_data)
    return UserSettingsOnStart::kSignedInNoRecentData;

  if (metadata.web_and_app_activity_enabled()) {
    if (metadata.discover_personalization_enabled())
      return UserSettingsOnStart::kSignedInWaaOnDpOn;
    return UserSettingsOnStart::kSignedInWaaOnDpOff;
  } else {
    if (metadata.discover_personalization_enabled())
      return UserSettingsOnStart::kSignedInWaaOffDpOn;
    return UserSettingsOnStart::kSignedInWaaOffDpOff;
  }
}

void ReportSubscriptionCountAtEngagementTime(const StreamType& stream_type,
                                             int subscription_count) {
  base::UmaHistogramSparse(
      base::StrCat({"ContentSuggestions.", HistogramReplacement(stream_type),
                    "FollowCount.Engaged2"}),
      subscription_count);
}

void ReportCombinedSubscriptionCountAtEngagementTime(int subscription_count) {
  base::UmaHistogramSparse(
      "ContentSuggestions.Feed.AllFeeds.FollowCount.Engaged2",
      subscription_count);
}

bool IsGoodExplicitInteraction(FeedUserActionType action) {
  switch (action) {
    case FeedUserActionType::kAddedToReadLater:
    case FeedUserActionType::kTappedFollowButton:
    case FeedUserActionType::kShare:
    case FeedUserActionType::kTappedAddToReadingList:
    case FeedUserActionType::kTappedDownload:
    case FeedUserActionType::kTappedOpenInNewIncognitoTab:
      return true;
    default:
      return false;
  }
}

}  // namespace
MetricsReporter::LoadStreamResultSummary::LoadStreamResultSummary() = default;
MetricsReporter::LoadStreamResultSummary::LoadStreamResultSummary(
    const LoadStreamResultSummary& src) = default;
MetricsReporter::LoadStreamResultSummary::~LoadStreamResultSummary() = default;

MetricsReporter::SurfaceWaiting::SurfaceWaiting() = default;
MetricsReporter::SurfaceWaiting::SurfaceWaiting(
    const feed::StreamType& stream_type,
    base::TimeTicks wait_start)
    : stream_type(stream_type), wait_start(wait_start) {}

MetricsReporter::SurfaceWaiting::~SurfaceWaiting() = default;
MetricsReporter::SurfaceWaiting::SurfaceWaiting(const SurfaceWaiting&) =
    default;
MetricsReporter::SurfaceWaiting::SurfaceWaiting(SurfaceWaiting&&) = default;
MetricsReporter::SurfaceWaiting& MetricsReporter::SurfaceWaiting::operator=(
    const SurfaceWaiting&) = default;
MetricsReporter::SurfaceWaiting& MetricsReporter::SurfaceWaiting::operator=(
    SurfaceWaiting&&) = default;

MetricsReporter::MetricsReporter(PrefService* profile_prefs)
    : profile_prefs_(profile_prefs), good_visit_state_(persistent_data_) {
  persistent_data_ = prefs::GetPersistentMetricsData(*profile_prefs_);
  ReportPersistentDataIfDayIsDone();
}

MetricsReporter::~MetricsReporter() {
  FinalizeMetrics();
}

void MetricsReporter::Initialize(Delegate* delegate) {
  delegate_ = delegate;
}

void MetricsReporter::OnMetadataInitialized(
    bool isEnabledByEnterprisePolicy,
    bool isFeedVisible,
    bool isSignedIn,
    bool isEnabled,
    const feedstore::Metadata& metadata) {
  UserSettingsOnStart settings =
      GetUserSettingsOnStart(isEnabledByEnterprisePolicy, isFeedVisible,
                             isSignedIn, isEnabled, metadata);
  delegate_->RegisterFeedUserSettingsFieldTrial(ToString(settings));
  base::UmaHistogramEnumeration("ContentSuggestions.Feed.UserSettingsOnStart",
                                settings);
}

void MetricsReporter::OnEnterBackground() {
  FinalizeMetrics();
}

// Engagement Tracking.

void MetricsReporter::RecordInteraction(const StreamType& stream_type) {
  RecordEngagement(stream_type, /*scroll_distance_dp=*/0, /*interacted=*/true);
  ReportEngagementTypeHistogram(stream_type,
                                FeedEngagementType::kFeedInteracted);
  ReportCombinedEngagementTypeHistogram(FeedEngagementType::kFeedInteracted);
}

void MetricsReporter::LogContentStats(const StreamType& stream_type,
                                      const ContentStats& content_stats) {
  // Don't report anything if there's no content.
  if (content_stats.card_count == 0)
    return;

  base::UmaHistogramCounts10000(
      base::StrCat({"ContentSuggestions.", HistogramReplacement(stream_type),
                    "StreamContentSizeKB"}),
      content_stats.total_content_frame_size_bytes / 1024);

  base::UmaHistogramCounts10000(
      base::StrCat({"ContentSuggestions.", HistogramReplacement(stream_type),
                    "SharedStateSizeKB"}),
      content_stats.shared_state_size / 1024);
}

void MetricsReporter::TrackTimeSpentInFeed(bool interacted_or_scrolled) {
  if (time_in_feed_start_) {
    ReportPersistentDataIfDayIsDone();
    persistent_data_.accumulated_time_spent_in_feed +=
        std::min(kTimeSpentInFeedInteractionTimeout,
                 base::TimeTicks::Now() - *time_in_feed_start_);
    time_in_feed_start_ = std::nullopt;
  }

  if (interacted_or_scrolled) {
    time_in_feed_start_ = base::TimeTicks::Now();
  }
}

void MetricsReporter::FinalizeVisit() {
  bool has_engagement = false;
  for (const StreamKind& stream_type : kStreamKinds) {
    StreamStats& data = ForStream(StreamType(stream_type));
    if (!data.engaged_simple_reported)
      continue;
    has_engagement = true;
    data.engaged_reported = false;
    data.engaged_simple_reported = false;
    data.scrolled_reported = false;
  }
  if (has_engagement)
    TrackTimeSpentInFeed(false);
  combined_stats_.engaged_reported = false;
  combined_stats_.engaged_simple_reported = false;
  combined_stats_.scrolled_reported = false;
}

void MetricsReporter::RecordEngagement(const StreamType& stream_type,
                                       int scroll_distance_dp,
                                       bool interacted) {
  scroll_distance_dp = std::abs(scroll_distance_dp);
  // Determine if this interaction is part of a new feed 'visit'.
  base::TimeTicks now = base::TimeTicks::Now();
  if (now - visit_start_time_ > kVisitTimeout) {
    FinalizeVisit();
  }
  // Reset the last active time for visit measurement.
  visit_start_time_ = now;

  TrackTimeSpentInFeed(true);

  StreamStats& data = ForStream(stream_type);
  // Report the user as engaged-simple if they have scrolled any amount or
  // interacted with the card, and we have not already reported it for this
  // chrome run.
  if (!data.engaged_simple_reported && (scroll_distance_dp > 0 || interacted)) {
    ReportEngagementTypeHistogram(stream_type,
                                  FeedEngagementType::kFeedEngagedSimple);
    data.engaged_simple_reported = true;
    if (!combined_stats_.engaged_simple_reported) {
      ReportCombinedEngagementTypeHistogram(
          FeedEngagementType::kFeedEngagedSimple);
      combined_stats_.engaged_simple_reported = true;
    }
  }

  // Report the user as engaged if they have scrolled more than the threshold or
  // interacted with the card, and we have not already reported it this chrome
  // run.
  const int kMinScrollThresholdDp = 160;  // 1 inch.
  if (!data.engaged_reported &&
      (scroll_distance_dp > kMinScrollThresholdDp || interacted)) {
    ReportEngagementTypeHistogram(stream_type,
                                  FeedEngagementType::kFeedEngaged);

    data.engaged_reported = true;
    if (!combined_stats_.engaged_reported) {
      ReportCombinedEngagementTypeHistogram(FeedEngagementType::kFeedEngaged);
      // Reports subscription count for the specific feed and for the combined
      // histogram.
      delegate_->SubscribedWebFeedCount(base::BindOnce(
          [](const StreamType& st, int sc) {
            ReportSubscriptionCountAtEngagementTime(st, sc);
            ReportCombinedSubscriptionCountAtEngagementTime(sc);
          },
          stream_type));

      combined_stats_.engaged_reported = true;
    } else {
      // Reports subscription count for the specific feed only.
      delegate_->SubscribedWebFeedCount(base::BindOnce(
          &ReportSubscriptionCountAtEngagementTime, stream_type));
    }

    // Record sorting order for web feed when engaged.
    if (stream_type.IsWebFeed()) {
      FeedSortType sort_type =
          GetSortTypeFromContentOrder(delegate_->GetContentOrder(stream_type));
      base::UmaHistogramEnumeration(
          "ContentSuggestions.Feed.WebFeed.SortTypeWhenEngaged", sort_type);
    }
  }
}

void MetricsReporter::StreamScrollStart() {
  // Note that |TrackTimeSpentInFeed()| is called as a result of
  // |StreamScrolled()| as well. Tracking the start of scroll events ensures we
  // don't miss out on long and slow scrolling.
  TrackTimeSpentInFeed(true);
}

void MetricsReporter::StreamScrolled(const StreamType& stream_type,
                                     int distance_dp) {
  RecordEngagement(stream_type, distance_dp, /*interacted=*/false);

  StreamStats& data = ForStream(stream_type);
  if (!data.scrolled_reported) {
    ReportEngagementTypeHistogram(stream_type,
                                  FeedEngagementType::kFeedScrolled);
    data.scrolled_reported = true;
    if (!combined_stats_.scrolled_reported) {
      ReportCombinedEngagementTypeHistogram(FeedEngagementType::kFeedScrolled);
      combined_stats_.scrolled_reported = true;
    }
  }

  good_visit_state_.OnScroll();
}

void MetricsReporter::ContentSliceViewed(const StreamType& stream_type,
                                         int index_in_stream,
                                         int stream_slice_count) {
  switch (stream_type.GetKind()) {
    case StreamKind::kForYou:
      base::UmaHistogramExactLinear("NewTabPage.ContentSuggestions.Shown",
                                    index_in_stream, kMaxSuggestionsTotal);
      break;
    case StreamKind::kFollowing:
      base::UmaHistogramExactLinear("ContentSuggestions.Feed.WebFeed.Shown",
                                    index_in_stream, kMaxSuggestionsTotal);
      break;
    case StreamKind::kSingleWebFeed:
      base::UmaHistogramExactLinear(
          "ContentSuggestions.Feed.SingleWebFeed.Shown", index_in_stream,
          kMaxSuggestionsTotal);
      break;
    case StreamKind::kSupervisedUser:
      base::UmaHistogramExactLinear(
          "ContentSuggestions.Feed.SupervisedFeed.Shown", index_in_stream,
          kMaxSuggestionsTotal);
      break;
    case StreamKind::kUnknown:
      DCHECK(false) << "unknown feed kind";
      break;
  }

  if (index_in_stream == stream_slice_count - 1) {
    base::UmaHistogramExactLinear(
        base::StrCat({"ContentSuggestions.", HistogramReplacement(stream_type),
                      "ReachedEndOfFeed"}),
        stream_slice_count, kMaxSuggestionsTotal);
  }
}

void MetricsReporter::FeedViewed(SurfaceId surface_id) {
  if (load_latencies_) {
    load_latencies_->StepComplete(LoadLatencyTimes::kStreamViewed);

    // Log latencies for debugging.
    if (VLOG_IS_ON(2)) {
      for (const LoadLatencyTimes::Step& step : load_latencies_->steps()) {
        VVLOG << "LoadStepLatency." << LoadLatencyStepName(step.kind) << " = "
              << step.latency;
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
  good_visit_state_.ExtendOrStartNewVisit();
}

void MetricsReporter::OpenAction(const StreamType& stream_type,
                                 int index_in_stream,
                                 OpenActionType action_type) {
  CardOpenBegin(stream_type);
  switch (action_type) {
    case OpenActionType::kDefault:
      ReportUserActionHistogram(FeedUserActionType::kTappedOnCard);
      base::RecordAction(
          base::UserMetricsAction("ContentSuggestions.Feed.CardAction.Open"));
      break;
    case OpenActionType::kNewTab:
      ReportUserActionHistogram(FeedUserActionType::kTappedOpenInNewTab);
      base::RecordAction(base::UserMetricsAction(
          "ContentSuggestions.Feed.CardAction.OpenInNewTab"));
      break;
    case OpenActionType::kNewTabInGroup:
      ReportUserActionHistogram(FeedUserActionType::kTappedOpenInNewTabInGroup);
      base::RecordAction(base::UserMetricsAction(
          "ContentSuggestions.Feed.CardAction.OpenInNewTabInGroup"));
      break;
  }
  ReportContentSuggestionsOpened(stream_type, index_in_stream);
  RecordInteraction(stream_type);
  good_visit_state_.ExtendOrStartNewVisit();
}

void MetricsReporter::OpenVisitComplete(base::TimeDelta visit_time) {
  base::UmaHistogramLongTimes("ContentSuggestions.Feed.VisitDuration",
                              visit_time);

  good_visit_state_.OnOpenComplete(visit_time);
}

void MetricsReporter::PageLoaded() {
  ReportCardOpenEndIfNeeded(true);
}

void MetricsReporter::OtherUserAction(const StreamType& stream_type,
                                      FeedUserActionType action_type) {
  VVLOG << "Feed OtherUserAction " << stream_type << " id=" << action_type;

  if (IsGoodExplicitInteraction(action_type)) {
    good_visit_state_.OnGoodExplicitInteraction();
  }

  ReportUserActionHistogram(action_type);
  switch (action_type) {
    case FeedUserActionType::kTappedOnCard:
    case FeedUserActionType::kTappedOpenInNewTab:
    case FeedUserActionType::kTappedOpenInNewTabInGroup:
      DCHECK(false) << "This should be reported with OpenAction() instead";
      break;
    case FeedUserActionType::kShownCard_DEPRECATED:
      DCHECK(false) << "deprecated";
      break;
    case FeedUserActionType::kOpenedFeedSurface:
      DCHECK(false) << "This should be reported with SurfaceOpened() instead";
      break;
    case FeedUserActionType::kTappedSendFeedback:
      base::RecordAction(base::UserMetricsAction(
          "ContentSuggestions.Feed.CardAction.SendFeedback"));
      RecordInteraction(stream_type);
      break;
    case FeedUserActionType::kTappedLearnMore:
      base::RecordAction(base::UserMetricsAction(
          "ContentSuggestions.Feed.CardAction.LearnMore"));
      RecordInteraction(stream_type);
      break;
    case FeedUserActionType::kTappedHideStory:
      // TODO(crbug.com/40708979): This action is not visible to client code, so
      // not yet used.
      base::RecordAction(base::UserMetricsAction(
          "ContentSuggestions.Feed.CardAction.HideStory"));
      RecordInteraction(stream_type);
      break;
    case FeedUserActionType::kTappedNotInterestedIn:
      // TODO(crbug.com/40708979): This action is not visible to client code, so
      // not yet used.
      base::RecordAction(base::UserMetricsAction(
          "ContentSuggestions.Feed.CardAction.NotInterestedIn"));
      RecordInteraction(stream_type);
      break;
    case FeedUserActionType::kTappedManageInterests:
      base::RecordAction(base::UserMetricsAction(
          "ContentSuggestions.Feed.CardAction.ManageInterests"));
      RecordInteraction(stream_type);
      break;
    case FeedUserActionType::kTappedDownload:
      base::RecordAction(base::UserMetricsAction(
          "ContentSuggestions.Feed.CardAction.Download"));
      RecordInteraction(stream_type);
      break;
    case FeedUserActionType::kTappedAddToReadingList:
      base::RecordAction(base::UserMetricsAction(
          "ContentSuggestions.Feed.CardAction.ReadLater"));
      RecordInteraction(stream_type);
      break;
    case FeedUserActionType::kOpenedContextMenu:
      base::RecordAction(base::UserMetricsAction(
          "ContentSuggestions.Feed.CardAction.ContextMenu"));
      break;
    case FeedUserActionType::kTappedOpenInNewIncognitoTab:
      base::RecordAction(base::UserMetricsAction(
          "ContentSuggestions.Feed.CardAction.OpenInNewIncognitoTab"));
      RecordInteraction(stream_type);
      break;
    case FeedUserActionType::kTappedManageActivity:
      base::RecordAction(base::UserMetricsAction(
          "ContentSuggestions.Feed.CardAction.ManageActivity"));
      RecordInteraction(stream_type);
      break;
    case FeedUserActionType::kTappedManageReactions:
      base::RecordAction(base::UserMetricsAction(
          "ContentSuggestions.Feed.CardAction.ManageReactions"));
      RecordInteraction(stream_type);
      break;
    case FeedUserActionType::kShare:
      base::RecordAction(
          base::UserMetricsAction("ContentSuggestions.Feed.CardAction.Share"));
      RecordInteraction(stream_type);
      break;
    case FeedUserActionType::kTappedManage:
      base::RecordAction(
          base::UserMetricsAction("ContentSuggestions.Feed.CardAction.Manage"));
      RecordInteraction(stream_type);
      break;
    case FeedUserActionType::kTappedManageHidden:
      base::RecordAction(base::UserMetricsAction(
          "ContentSuggestions.Feed.CardAction.ManageHidden"));
      RecordInteraction(stream_type);
      break;
    case FeedUserActionType::kAddedToReadLater:
    case FeedUserActionType::kTappedFollowButton:
    case FeedUserActionType::kEphemeralChange:
    case FeedUserActionType::kEphemeralChangeRejected:
    case FeedUserActionType::kTappedTurnOn:
    case FeedUserActionType::kTappedTurnOff:
    case FeedUserActionType::kClosedContextMenu:
    case FeedUserActionType::kEphemeralChangeCommited:
    case FeedUserActionType::kOpenedDialog:
    case FeedUserActionType::kClosedDialog:
    case FeedUserActionType::kShowSnackbar:
    case FeedUserActionType::kOpenedNativeActionSheet:
    case FeedUserActionType::kOpenedNativeContextMenu:
    case FeedUserActionType::kClosedNativeContextMenu:
    case FeedUserActionType::kOpenedNativePulldownMenu:
    case FeedUserActionType::kClosedNativePulldownMenu:
    case FeedUserActionType::kTappedManageFollowing:
    case FeedUserActionType::kTappedFollowOnManagementSurface:
    case FeedUserActionType::kTappedUnfollowOnManagementSurface:
    case FeedUserActionType::kTappedFollowOnFollowAccelerator:
    case FeedUserActionType::kTappedFollowTryAgainOnSnackbar:
    case FeedUserActionType::kTappedRefollowAfterUnfollowOnSnackbar:
    case FeedUserActionType::kTappedUnfollowTryAgainOnSnackbar:
    case FeedUserActionType::kTappedGoToFeedPostFollowActiveHelp:
    case FeedUserActionType::kTappedDismissPostFollowActiveHelp:
    case FeedUserActionType::kTappedDiscoverFeedPreview:
    case FeedUserActionType::kOpenedAutoplaySettings:
    case FeedUserActionType::kDiscoverFeedSelected:
    case FeedUserActionType::kFollowingFeedSelected:
    case FeedUserActionType::kTappedUnfollowButton:
    case FeedUserActionType::kShowFollowSucceedSnackbar:
    case FeedUserActionType::kShowFollowFailedSnackbar:
    case FeedUserActionType::kShowUnfollowSucceedSnackbar:
    case FeedUserActionType::kShowUnfollowFailedSnackbar:
    case FeedUserActionType::kTappedGoToFeedOnSnackbar:
    case FeedUserActionType::kFirstFollowSheetShown:
    case FeedUserActionType::kFirstFollowSheetTappedGoToFeed:
    case FeedUserActionType::kFirstFollowSheetTappedGotIt:
    case FeedUserActionType::kFollowRecommendationIPHShown:
    case FeedUserActionType::kFollowingFeedSelectedGroupByPublisher:
    case FeedUserActionType::kFollowingFeedSelectedSortByLatest:
    case FeedUserActionType::kTappedFollowOnRecommendationFollowAccelerator:
    case FeedUserActionType::kTappedGotItFeedPostFollowActiveHelp:
    case FeedUserActionType::kTappedRefreshFollowingFeedOnSnackbar:
    case FeedUserActionType::kNonSwipeManualRefresh:
      // Nothing additional for these actions. Note that some of these are iOS
      // only.

      break;
  }
}

void MetricsReporter::ReportStableContentSliceVisibilityTimeForGoodVisits(
    base::TimeDelta delta) {
  good_visit_state_.AddTimeInFeed(delta);
}

void MetricsReporter::SurfaceOpened(
    const StreamType& stream_type,
    SurfaceId surface_id,
    SingleWebFeedEntryPoint single_web_feed_entry_point) {
  VVLOG << "Feed SurfaceOpened " << stream_type << " id=" << surface_id;
  ReportPersistentDataIfDayIsDone();
  surfaces_waiting_for_content_.emplace(
      surface_id, SurfaceWaiting{stream_type, base::TimeTicks::Now()});
  ReportUserActionHistogram(FeedUserActionType::kOpenedFeedSurface);
  if (stream_type.IsSingleWebFeed()) {
    base::UmaHistogramEnumeration("ContentSuggestions.SingleWebFeed.EntryPoint",
                                  single_web_feed_entry_point);
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&MetricsReporter::ReportOpenFeedIfNeeded, GetWeakPtr(),
                     surface_id, false),
      kLoadTimeout);
}

void MetricsReporter::SurfaceClosed(SurfaceId surface_id) {
  VVLOG << "Feed SurfaceClosed " << surface_id;
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
  SurfaceWaiting surface_waiting = std::move(iter->second);
  surfaces_waiting_for_content_.erase(iter);

  base::UmaHistogramCustomTimes(
      base::StrCat({"ContentSuggestions.Feed.UserJourney.Open",
                    HistogramReplacement(surface_waiting.stream_type),
                    success ? "SuccessDuration" : "FailureDuration"}),
      base::TimeTicks::Now() - surface_waiting.wait_start,
      base::Milliseconds(50), kLoadTimeout, 50);
}

void MetricsReporter::ReportGetMoreIfNeeded(SurfaceId surface_id,
                                            bool success) {
  auto iter = surfaces_waiting_for_more_content_.find(surface_id);
  if (iter == surfaces_waiting_for_more_content_.end())
    return;
  SurfaceWaiting surface_waiting = std::move(iter->second);
  surfaces_waiting_for_more_content_.erase(iter);

  base::UmaHistogramCustomTimes(
      base::StrCat({"ContentSuggestions.Feed.UserJourney.GetMore.",
                    success ? "SuccessDuration" : "FailureDuration"}),
      base::TimeTicks::Now() - surface_waiting.wait_start,
      base::Milliseconds(50), kLoadTimeout, 50);
}

void MetricsReporter::CardOpenBegin(const StreamType& stream_type) {
  ReportCardOpenEndIfNeeded(false);
  pending_open_ = {stream_type, base::TimeTicks::Now()};
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&MetricsReporter::CardOpenTimeout, GetWeakPtr(),
                     pending_open_.wait_start),
      kOpenTimeout);
}

void MetricsReporter::CardOpenTimeout(base::TimeTicks start_ticks) {
  if (pending_open_ && start_ticks == pending_open_.wait_start)
    ReportCardOpenEndIfNeeded(false);
}

void MetricsReporter::ReportCardOpenEndIfNeeded(bool success) {
  if (!pending_open_)
    return;
  base::TimeDelta latency = base::TimeTicks::Now() - pending_open_.wait_start;

  std::string histogram_name =
      base::StrCat({"ContentSuggestions.Feed.UserJourney.OpenCard",
                    pending_open_.stream_type.IsWebFeed() ? ".WebFeed" : "",
                    success ? ".SuccessDuration" : ".Failure"});

  if (success) {
    base::UmaHistogramCustomTimes(histogram_name, latency,
                                  base::Milliseconds(100), kOpenTimeout, 50);
  } else {
    base::UmaHistogramBoolean(histogram_name, true);
  }

  pending_open_ = {};
}

void MetricsReporter::NetworkRefreshRequestStarted(
    const StreamType& stream_type,
    ContentOrder content_order) {
  if (stream_type.IsWebFeed()) {
    base::UmaHistogramEnumeration(
        "ContentSuggestions.Feed.WebFeed.RefreshContentOrder", content_order);
  }
}

void MetricsReporter::NetworkRequestComplete(
    NetworkRequestType type,
    const NetworkResponseInfo& response_info) {
  VVLOG << "Network Request Complete type=" << NetworkRequestTypeUmaName(type)
        << " status=" << response_info.status_code
        << " url=" << response_info.base_request_url
        << " account_info=" << response_info.account_info
        << " response_size=" << response_info.encoded_size_bytes
        << " duration=" << response_info.fetch_duration;

  std::string_view request_name = NetworkRequestTypeUmaName(type);
  base::UmaHistogramSparse(
      base::StrCat(
          {"ContentSuggestions.Feed.Network.ResponseStatus.", request_name}),
      response_info.status_code);
  base::UmaHistogramMediumTimes(
      base::StrCat({"ContentSuggestions.Feed.Network.Duration.", request_name}),
      response_info.fetch_duration);
  base::UmaHistogramCounts10000(
      base::StrCat({"ContentSuggestions.Feed.Network.CompressedResponseSizeKB.",
                    request_name}),
      response_info.encoded_size_bytes / 1024);
}

void MetricsReporter::OnLoadStream(
    const StreamType& stream_type,
    const LoadStreamResultSummary& result_summary,
    const ContentStats& content_stats,
    std::unique_ptr<LoadLatencyTimes> load_latencies) {
  LoadStreamStatus load_from_store_status =
      result_summary.load_from_store_status;
  LoadStreamStatus final_status = result_summary.final_status;
  bool is_initial_load = result_summary.is_initial_load;
  bool loaded_new_content_from_network =
      result_summary.loaded_new_content_from_network;
  base::TimeDelta stored_content_age = result_summary.stored_content_age;
  std::optional<feedstore::Metadata::StreamMetadata> stream_metadata =
      result_summary.stream_metadata;
  ContentOrder content_order = result_summary.content_order;
  VVLOG << "OnLoadStream load_from_store_status=" << load_from_store_status
        << " final_status=" << final_status;
  load_latencies_ = std::move(load_latencies);

  std::string load_type_name = is_initial_load ? "Initial" : "ManualRefresh";
  base::UmaHistogramEnumeration(
      base::StrCat({"ContentSuggestions.", HistogramReplacement(stream_type),
                    "LoadStreamStatus.", load_type_name}),
      final_status);

  if (stream_metadata.has_value()) {
    feedstore::Metadata::StreamMetadata::ContentLifetime content_lifetime =
        stream_metadata->content_lifetime();
    base::TimeDelta content_lifetime_stale_age_delta =
        base::Milliseconds(content_lifetime.stale_age_ms());
    ReportContentLifetimeStaleAge(content_lifetime_stale_age_delta);

    base::TimeDelta content_lifetime_invalid_age_delta =
        base::Milliseconds(content_lifetime.invalid_age_ms());
    ReportContentLifetimeInvalidAge(content_lifetime_invalid_age_delta);
  }

  if (!is_initial_load)
    return;

  if (load_from_store_status != LoadStreamStatus::kNoStatus) {
    base::UmaHistogramEnumeration(
        base::StrCat({"ContentSuggestions.", HistogramReplacement(stream_type),
                      "LoadStreamStatus.InitialFromStore"}),
        load_from_store_status);
  }

  // For stored_content_age, the zero-value means there was no content loaded
  // from the store. A negative value means there was content loaded, but it had
  // a timestamp from the future. In either case, we'll avoid recording the
  // content age.
  if (stored_content_age.is_positive()) {
    if (loaded_new_content_from_network) {
      base::UmaHistogramCustomTimes(
          "ContentSuggestions.Feed.ContentAgeOnLoad.BlockingRefresh",
          stored_content_age, base::Minutes(5), base::Days(7),
          /*buckets=*/50);
    } else {
      base::UmaHistogramCustomTimes(
          "ContentSuggestions.Feed.ContentAgeOnLoad.NotRefreshed",
          stored_content_age, base::Seconds(5), base::Days(7),
          /*buckets=*/50);
    }
  }

  if (IsLoadingSuccessfulAndFresh(final_status)) {
    base::UmaHistogramSparse(
        base::StrCat({"ContentSuggestions.", HistogramReplacement(stream_type),
                      "LoadedCardCount"}),
        content_stats.card_count);
    if (stream_type.IsWebFeed()) {
      base::UmaHistogramSparse(
          base::StrCat({"ContentSuggestions.Feed.WebFeed.LoadedCardCount.",
                        ContentOrderToString(content_order)}),
          content_stats.card_count);
    }
  }
  if (stream_type.IsWebFeed()) {
    delegate_->SubscribedWebFeedCount(base::BindOnce(
        &MetricsReporter::ReportFollowCountOnLoad, base::Unretained(this),
        /*content_shown=*/content_stats.card_count != 0));
  }
  LogContentStats(stream_type, content_stats);
}

void MetricsReporter::OnBackgroundRefresh(const StreamType& stream_type,
                                          LoadStreamStatus final_status) {
  base::UmaHistogramEnumeration(
      base::StrCat({"ContentSuggestions.", HistogramReplacement(stream_type),
                    "LoadStreamStatus.BackgroundRefresh"}),
      final_status);
}

void MetricsReporter::OnManualRefresh(const StreamType& stream_type,
                                      const feedstore::Metadata& metadata,
                                      const ContentHashSet& content_hashes) {
  if (!stream_type.IsForYou())
    return;
  const feedstore::Metadata::StreamMetadata* stream_metadata =
      FindMetadataForStream(metadata, stream_type);
  if (stream_metadata == nullptr)
    return;

  base::UmaHistogramCustomTimes(
      "ContentSuggestions.Feed.ManualRefreshInterval",
      base::Time::Now() - feedstore::FromTimestampMillis(
                              stream_metadata->last_fetch_time_millis()),
      base::Minutes(1), base::Days(1), /*buckets=*/50);

  int viewed_count = 0;
  int viewed_percentage = 0;
  if (content_hashes.original_hashes().size() > 0) {
    viewed_count = stream_metadata->viewed_content_hashes_size();
    viewed_percentage =
        100 * viewed_count / content_hashes.original_hashes().size();
  }

  base::UmaHistogramCounts100(
      "ContentSuggestions.Feed.ViewedCardCountAtManualRefresh", viewed_count);

  base::UmaHistogramCounts100(
      "ContentSuggestions.Feed.ViewedCardPercentageAtManualRefresh",
      viewed_percentage);
}

void MetricsReporter::OnLoadMoreBegin(const StreamType& stream_type,
                                      SurfaceId surface_id) {
  ReportGetMoreIfNeeded(surface_id, false);
  surfaces_waiting_for_more_content_.emplace(
      surface_id, SurfaceWaiting{stream_type, base::TimeTicks::Now()});

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&MetricsReporter::ReportGetMoreIfNeeded, GetWeakPtr(),
                     surface_id, false),
      kLoadTimeout);
}

void MetricsReporter::OnLoadMore(const StreamType& stream_type,
                                 LoadStreamStatus status,
                                 const ContentStats& content_stats) {
  VVLOG << "OnLoadMore status=" << status;
  base::UmaHistogramEnumeration(
      "ContentSuggestions.Feed.LoadStreamStatus.LoadMore", status);
  LogContentStats(stream_type, content_stats);
}

void MetricsReporter::OnImageFetched(const GURL& url,
                                     int net_error_or_http_status) {
  VVLOG << "OnImageFetched status=" << net_error_or_http_status << " " << url;
  base::UmaHistogramSparse("ContentSuggestions.Feed.ImageFetchStatus",
                           net_error_or_http_status);
}

void MetricsReporter::OnResourceFetched(int net_error_or_http_status) {
  base::UmaHistogramSparse("ContentSuggestions.Feed.ResourceFetchStatus",
                           net_error_or_http_status);
}

void MetricsReporter::OnUploadActionsBatch(UploadActionsBatchStatus status) {
  VVLOG << "UploadActionsBatchStatus: " << status;
  base::UmaHistogramEnumeration(
      "ContentSuggestions.Feed.UploadActionsBatchStatus", status);
}

void MetricsReporter::OnUploadActions(UploadActionsStatus status) {
  VVLOG << "UploadActionsTask finished with status " << status;
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
    if (since_day_start > base::Days(1)
        // Allow up to 1 hour of negative delta, for expected clock changes.
        || since_day_start < -base::Hours(1)) {
      if (persistent_data_.accumulated_time_spent_in_feed.is_positive()) {
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

MetricsReporter::StreamStats& MetricsReporter::ForStream(
    const StreamType& stream_type) {
  switch (stream_type.GetKind()) {
    case StreamKind::kForYou:
      return for_you_stats_;
    case StreamKind::kSupervisedUser:
      return supervised_feed_stats_;
    case StreamKind::kFollowing:
    case StreamKind::kSingleWebFeed:
      return web_feed_stats_;
    case StreamKind::kUnknown:
      DCHECK(false) << "unknown feed kind";
      return web_feed_stats_;
  }
}

void MetricsReporter::OnFollowAttempt(
    bool followed_with_id,
    const WebFeedSubscriptions::FollowWebFeedResult& result) {
  VVLOG << "OnFollowAttempt web_feed_id="
        << result.web_feed_metadata.web_feed_id
        << " status=" << result.request_status;

  if (followed_with_id) {
    base::UmaHistogramEnumeration(
        "ContentSuggestions.Feed.WebFeed.FollowByIdResult",
        result.request_status);
  } else {
    base::UmaHistogramEnumeration(
        "ContentSuggestions.Feed.WebFeed.FollowUriResult",
        result.request_status);
  }
  if (result.request_status == WebFeedSubscriptionRequestStatus::kSuccess) {
    base::UmaHistogramSparse(
        "ContentSuggestions.Feed.WebFeed.FollowCount.AfterFollow",
        result.subscription_count);
    base::UmaHistogramBoolean(
        "ContentSuggestions.Feed.WebFeed.NewFollow.IsRecommended",
        result.web_feed_metadata.is_recommended);
    if (result.change_reason) {
      // Because WebFeedChangeReason_MAX is not an enum value, we can't use
      // UmaHistogramEnumeration, but UmaHistogramExactLinear is equivalent.
      base::UmaHistogramExactLinear(
          "ContentSuggestions.Feed.WebFeed.NewFollow.ChangeReason",
          static_cast<int>(result.change_reason),
          feedwire::webfeed::WebFeedChangeReason_MAX + 1);
    }
  }
}

void MetricsReporter::OnUnfollowAttempt(
    const WebFeedSubscriptions::UnfollowWebFeedResult& result) {
  VVLOG << "OnUnfollowAttempt status=" << result.request_status;
  base::UmaHistogramEnumeration(
      "ContentSuggestions.Feed.WebFeed.UnfollowResult", result.request_status);

  if (result.request_status == WebFeedSubscriptionRequestStatus::kSuccess) {
    base::UmaHistogramSparse(
        "ContentSuggestions.Feed.WebFeed.FollowCount.AfterUnfollow",
        result.subscription_count);
  }
}

void MetricsReporter::OnQueryAttempt(
    const WebFeedSubscriptions::QueryWebFeedResult& result) {
  VVLOG << "OnQueryAttempt status=" << result.request_status;
  base::UmaHistogramEnumeration("ContentSuggestions.Feed.WebFeed.QueryResult",
                                result.request_status);
}

void MetricsReporter::RefreshRecommendedWebFeedsAttempted(
    WebFeedRefreshStatus status,
    int recommended_web_feed_count) {
  VVLOG << "RefreshRecommendedWebFeedsAttempted status=" << status
        << " count=" << recommended_web_feed_count;
  base::UmaHistogramEnumeration(
      "ContentSuggestions.Feed.WebFeed.RefreshRecommendedFeeds", status);
}

void MetricsReporter::RefreshSubscribedWebFeedsAttempted(
    bool subscriptions_were_stale,
    WebFeedRefreshStatus status,
    int subscribed_web_feed_count) {
  VVLOG << "RefreshSubscribedWebFeedsAttempted status=" << status
        << " count=" << subscribed_web_feed_count;
  if (subscriptions_were_stale) {
    base::UmaHistogramEnumeration(
        "ContentSuggestions.Feed.WebFeed.RefreshSubscribedFeeds.Stale", status);
  } else {
    base::UmaHistogramEnumeration(
        "ContentSuggestions.Feed.WebFeed.RefreshSubscribedFeeds.Force", status);
  }
}

void MetricsReporter::ReportFollowCountOnLoad(bool content_shown,
                                              int subscription_count) {
  base::UmaHistogramSparse(
      base::StrCat({"ContentSuggestions.Feed.WebFeed.FollowCount.",
                    content_shown ? "ContentShown" : "NoContentShown"}),
      subscription_count);
}

void MetricsReporter::OnInfoCardTrackViewStarted(const StreamType& stream_type,
                                                 int info_card_type) {
  base::UmaHistogramSparse(InfoCardActionUmaName(stream_type, "Started"),
                           info_card_type);
}

void MetricsReporter::OnInfoCardViewed(const StreamType& stream_type,
                                       int info_card_type) {
  base::UmaHistogramSparse(InfoCardActionUmaName(stream_type, "Viewed"),
                           info_card_type);
}

void MetricsReporter::OnInfoCardClicked(const StreamType& stream_type,
                                        int info_card_type) {
  base::UmaHistogramSparse(InfoCardActionUmaName(stream_type, "Clicked"),
                           info_card_type);
}

void MetricsReporter::OnInfoCardDismissedExplicitly(
    const StreamType& stream_type,
    int info_card_type) {
  base::UmaHistogramSparse(InfoCardActionUmaName(stream_type, "Dismissed"),
                           info_card_type);
}

void MetricsReporter::OnInfoCardStateReset(const StreamType& stream_type,
                                           int info_card_type) {
  base::UmaHistogramSparse(InfoCardActionUmaName(stream_type, "Reset"),
                           info_card_type);
}

MetricsReporter::GoodVisitState::GoodVisitState(PersistentMetricsData& data)
    : data_(data) {}

void MetricsReporter::GoodVisitState::OnScroll() {
  ExtendOrStartNewVisit();
  data_->did_scroll_in_visit = true;
  if (data_->time_in_feed_for_good_visit >= kGoodTimeInFeed) {
    MaybeReportGoodVisit();
  }
}

void MetricsReporter::GoodVisitState::OnGoodExplicitInteraction() {
  ExtendOrStartNewVisit();
  MaybeReportGoodVisit();
}

void MetricsReporter::GoodVisitState::OnOpenComplete(
    base::TimeDelta open_duration) {
  if (open_duration >= kLongOpenTime) {
    MaybeReportGoodVisit();
  }
}

void MetricsReporter::GoodVisitState::ExtendOrStartNewVisit() {
  const base::Time now = base::Time::Now();

  // Reset visit state if enough time has passed since visit_end_.
  if (now - data_->visit_end >= kVisitTimeout) {
    Reset();
  }

  if (data_->visit_start == base::Time()) {
    data_->visit_start = now;
  }
  data_->visit_end = now;
}

void MetricsReporter::GoodVisitState::AddTimeInFeed(base::TimeDelta time) {
  if (time < kMinStableContentSliceVisibilityTime) {
    return;
  }

  if (time > kMaxStableContentSliceVisibilityTime) {
    time = kMaxStableContentSliceVisibilityTime;
  }

  ExtendOrStartNewVisit();

  data_->time_in_feed_for_good_visit += time;
  if (data_->did_scroll_in_visit &&
      data_->time_in_feed_for_good_visit >= kGoodTimeInFeed) {
    MaybeReportGoodVisit();
  }
}

void MetricsReporter::GoodVisitState::MaybeReportGoodVisit() {
  if (data_->did_report_good_visit) {
    return;
  }
  ReportCombinedEngagementTypeHistogram(FeedEngagementType::kGoodVisit);
  data_->did_report_good_visit = true;
}

void MetricsReporter::GoodVisitState::Reset() {
  data_->visit_start = base::Time();
  data_->visit_end = base::Time();
  data_->did_report_good_visit = false;
  data_->time_in_feed_for_good_visit = base::Seconds(0);
  data_->did_scroll_in_visit = false;
}

void MetricsReporter::ReportContentDuplication(
    bool is_duplicated_at_pos_1,
    bool is_duplicated_at_pos_2,
    bool is_duplicated_at_pos_3,
    int duplicate_percentage_for_first_10,
    int duplicate_percentage_for_all) {
  base::UmaHistogramBoolean(
      "ContentSuggestions.Feed.ContentDuplication2.Position1",
      is_duplicated_at_pos_1);
  base::UmaHistogramBoolean(
      "ContentSuggestions.Feed.ContentDuplication2.Position2",
      is_duplicated_at_pos_2);
  base::UmaHistogramBoolean(
      "ContentSuggestions.Feed.ContentDuplication2.Position3",
      is_duplicated_at_pos_3);
  base::UmaHistogramPercentage(
      "ContentSuggestions.Feed.ContentDuplication2.First10",
      duplicate_percentage_for_first_10);
  base::UmaHistogramPercentage(
      "ContentSuggestions.Feed.ContentDuplication2.All",
      duplicate_percentage_for_all);
}

}  // namespace feed
