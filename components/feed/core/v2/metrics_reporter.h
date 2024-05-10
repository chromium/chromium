// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_METRICS_REPORTER_H_
#define COMPONENTS_FEED_CORE_V2_METRICS_REPORTER_H_

#include <climits>
#include <map>
#include <optional>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/public/common_enums.h"
#include "components/feed/core/v2/public/stream_type.h"
#include "components/feed/core/v2/public/web_feed_subscriptions.h"
#include "components/feed/core/v2/types.h"

class PrefService;
namespace feedstore {
class Metadata;
}

namespace feed {
// If cached user setting info is older than this, it will not be reported.
constexpr base::TimeDelta kUserSettingsMaxAge = base::Days(14);

// Reports UMA metrics for feed.
// Note this is inherited only for testing.
class MetricsReporter {
 public:
  // For 'index_in_stream' parameters, when the card index is unknown.
  // This is most likely to happen when the action originates from the bottom
  // sheet.
  static const int kUnknownCardIndex = INT_MAX;

  class Delegate {
   public:
    // Calls `callback` with the number of Web Feeds for which the user is
    // subscribed.
    virtual void SubscribedWebFeedCount(
        base::OnceCallback<void(int)> callback) = 0;
    virtual void RegisterFeedUserSettingsFieldTrial(std::string_view group) = 0;
    virtual ContentOrder GetContentOrder(
        const StreamType& stream_type) const = 0;
  };

  explicit MetricsReporter(PrefService* profile_prefs);
  virtual ~MetricsReporter();
  MetricsReporter(const MetricsReporter&) = delete;
  MetricsReporter& operator=(const MetricsReporter&) = delete;

  // Two-step initialization, required for circular dependency.
  void Initialize(Delegate* delegate);

  void OnMetadataInitialized(bool isEnabledByEnterprisePolicy,
                             bool isFeedVisible,
                             bool isSignedIn,
                             bool isEnabled,
                             const feedstore::Metadata& metadata);

  // User interactions. See |FeedApi| for definitions.

  virtual void ContentSliceViewed(const StreamType& stream_type,
                                  int index_in_stream,
                                  int stream_slice_count);
  void FeedViewed(SurfaceId surface_id);
  void OpenAction(const StreamType& stream_type,
                  int index_in_stream,
                  OpenActionType action_type);
  void OpenVisitComplete(base::TimeDelta visit_time);
  void PageLoaded();
  void OtherUserAction(const StreamType& stream_type,
                       FeedUserActionType action_type);
  // Report a period of time during which at least one content slice was visible
  // enough or covering enough of the viewport.
  void ReportStableContentSliceVisibilityTimeForGoodVisits(
      base::TimeDelta delta);

  // Indicates the user scrolled the feed by |distance_dp| and then stopped
  // scrolling.
  void StreamScrolled(const StreamType& stream_type, int distance_dp);
  void StreamScrollStart();

  // Called when the Feed surface is opened and closed.
  void SurfaceOpened(const StreamType& stream_type,
                     SurfaceId surface_id,
                     SingleWebFeedEntryPoint single_web_feed_entry_point =
                         SingleWebFeedEntryPoint::kOther);
  void SurfaceClosed(SurfaceId surface_id);

  // Network metrics.

  void NetworkRefreshRequestStarted(const StreamType& stream_type,
                                    ContentOrder content_order);
  static void NetworkRequestComplete(NetworkRequestType type,
                                     const NetworkResponseInfo& response_info);

  // Stream events.

  struct LoadStreamResultSummary {
    LoadStreamResultSummary();
    LoadStreamResultSummary(const LoadStreamResultSummary& src);
    ~LoadStreamResultSummary();
    LoadStreamStatus load_from_store_status = LoadStreamStatus::kNoStatus;
    LoadStreamStatus final_status = LoadStreamStatus::kNoStatus;
    bool is_initial_load = false;
    bool loaded_new_content_from_network = false;
    base::TimeDelta stored_content_age;
    ContentOrder content_order = ContentOrder::kUnspecified;
    std::optional<feedstore::Metadata::StreamMetadata> stream_metadata;
  };
  virtual void OnLoadStream(const StreamType& stream_type,
                            const LoadStreamResultSummary& result_summary,
                            const ContentStats& content_stats,
                            std::unique_ptr<LoadLatencyTimes> load_latencies);
  virtual void OnBackgroundRefresh(const StreamType& stream_type,
                                   LoadStreamStatus final_status);
  void OnManualRefresh(const StreamType& stream_type,
                       const feedstore::Metadata& metadata,
                       const ContentHashSet& content_hashes);
  virtual void OnLoadMoreBegin(const StreamType& stream_type,
                               SurfaceId surface_id);
  virtual void OnLoadMore(const StreamType& stream_type,
                          LoadStreamStatus final_status,
                          const ContentStats& content_stats);
  // Called each time the surface receives new content.
  void SurfaceReceivedContent(SurfaceId surface_id);
  // Called when Chrome is entering the background.
  void OnEnterBackground();

  static void OnImageFetched(const GURL& url, int net_error_or_http_status);
  static void OnResourceFetched(int net_error_or_http_status);

  // Actions upload.
  static void OnUploadActionsBatch(UploadActionsBatchStatus status);
  virtual void OnUploadActions(UploadActionsStatus status);

  static void ActivityLoggingEnabled(bool response_has_logging_enabled);
  static void NoticeCardFulfilled(bool response_has_notice_card);
  static void NoticeCardFulfilledObsolete(bool response_has_notice_card);

  // Web Feed events.
  void OnFollowAttempt(bool followed_with_id,
                       const WebFeedSubscriptions::FollowWebFeedResult& result);
  void OnUnfollowAttempt(
      const WebFeedSubscriptions::UnfollowWebFeedResult& status);
  void RefreshRecommendedWebFeedsAttempted(WebFeedRefreshStatus status,
                                           int recommended_web_feed_count);
  void RefreshSubscribedWebFeedsAttempted(bool subscriptions_were_stale,
                                          WebFeedRefreshStatus status,
                                          int subscribed_web_feed_count);
  void OnQueryAttempt(const WebFeedSubscriptions::QueryWebFeedResult& result);

  // Info card events.
  void OnInfoCardTrackViewStarted(const StreamType& stream_type,
                                  int info_card_type);
  void OnInfoCardViewed(const StreamType& stream_type, int info_card_type);
  void OnInfoCardClicked(const StreamType& stream_type, int info_card_type);
  void OnInfoCardDismissedExplicitly(const StreamType& stream_type,
                                     int info_card_type);
  void OnInfoCardStateReset(const StreamType& stream_type, int info_card_type);

  void ReportContentDuplication(bool is_duplicated_at_pos_1,
                                bool is_duplicated_at_pos_2,
                                bool is_duplicated_at_pos_3,
                                int duplicate_percentage_for_first_10,
                                int duplicate_percentaget_for_all);

 private:
  // State replicated for reporting per-stream-type metrics.
  struct StreamStats {
    bool engaged_simple_reported = false;
    bool engaged_reported = false;
    bool scrolled_reported = false;
  };
  struct SurfaceWaiting {
    explicit operator bool() const { return !wait_start.is_null(); }
    SurfaceWaiting();
    SurfaceWaiting(const feed::StreamType& stream_type,
                   base::TimeTicks wait_start);
    ~SurfaceWaiting();
    SurfaceWaiting(const SurfaceWaiting&);
    SurfaceWaiting(SurfaceWaiting&&);
    SurfaceWaiting& operator=(const SurfaceWaiting&);
    SurfaceWaiting& operator=(SurfaceWaiting&&);

    feed::StreamType stream_type;
    base::TimeTicks wait_start;
  };

  base::WeakPtr<MetricsReporter> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void ReportPersistentDataIfDayIsDone();
  void CardOpenBegin(const StreamType& stream_type);
  void CardOpenTimeout(base::TimeTicks start_ticks);
  void ReportCardOpenEndIfNeeded(bool success);
  void RecordEngagement(const StreamType& stream_type,
                        int scroll_distance_dp,
                        bool interacted);
  void LogContentStats(const StreamType& stream_type,
                       const ContentStats& content_stats);
  void TrackTimeSpentInFeed(bool interacted_or_scrolled);
  void RecordInteraction(const StreamType& stream_type);
  void ReportOpenFeedIfNeeded(SurfaceId surface_id, bool success);
  void ReportGetMoreIfNeeded(SurfaceId surface_id, bool success);
  void FinalizeMetrics();
  void FinalizeVisit();
  void ReportFollowCountOnLoad(bool content_shown, int subscription_count);

  StreamStats& ForStream(const StreamType& stream_type);

  raw_ptr<PrefService> profile_prefs_;
  raw_ptr<Delegate, DanglingUntriaged> delegate_ = nullptr;

  StreamStats for_you_stats_;
  StreamStats supervised_feed_stats_;
  StreamStats web_feed_stats_;
  StreamStats combined_stats_;

  // State below here is shared between all stream types.

  // Persistent data stored in prefs. Data is read in the constructor, and then
  // written back to prefs on backgrounding.
  PersistentMetricsData persistent_data_;

  base::TimeTicks visit_start_time_;

  // The time a surface was opened, for surfaces still waiting for content.
  std::map<SurfaceId, SurfaceWaiting> surfaces_waiting_for_content_;
  // The time a surface requested more content, for surfaces still waiting for
  // more content.
  std::map<SurfaceId, SurfaceWaiting> surfaces_waiting_for_more_content_;

  // Tracking ContentSuggestions.Feed.UserJourney.OpenCard.*:
  // We assume at most one card is opened at a time. The time the card was
  // tapped is stored here. Upon timeout, another open attempt, or
  // |ChromeStopping()|, the open is considered failed. Otherwise, if the
  // loading the page succeeds, the open is considered successful.
  SurfaceWaiting pending_open_;

  // For tracking time spent in the Feed.
  std::optional<base::TimeTicks> time_in_feed_start_;
  // For TimeSpentOnFeed.
  base::TimeDelta tracked_visit_time_in_feed_;
  // Non-null only directly after a stream load.
  std::unique_ptr<LoadLatencyTimes> load_latencies_;
  bool load_latencies_recorded_ = false;

  class GoodVisitState {
   public:
    explicit GoodVisitState(PersistentMetricsData& data);
    void OnScroll();
    void OnGoodExplicitInteraction();
    void OnOpenComplete(base::TimeDelta open_duration);
    void AddTimeInFeed(base::TimeDelta time);
    void ExtendOrStartNewVisit();

   private:
    void MaybeReportGoodVisit();
    void Reset();

    // Owned by MetricsReporter. Will live through the lifetime of
    // GoodVisitState.
    const raw_ref<PersistentMetricsData> data_;
  };
  GoodVisitState good_visit_state_;

  base::WeakPtrFactory<MetricsReporter> weak_ptr_factory_{this};
};
}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_METRICS_REPORTER_H_
