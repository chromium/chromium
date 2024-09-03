// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_FEED_STREAM_H_
#define COMPONENTS_FEED_CORE_V2_FEED_STREAM_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/version.h"
#include "components/feed/core/proto/v2/ui.pb.h"
#include "components/feed/core/proto/v2/wire/reliability_logging_enums.pb.h"
#include "components/feed/core/proto/v2/wire/response.pb.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feed_stream_surface.h"
#include "components/feed/core/v2/launch_reliability_logger.h"
#include "components/feed/core/v2/metrics_reporter.h"
#include "components/feed/core/v2/persistent_key_value_store_impl.h"
#include "components/feed/core/v2/protocol_translator.h"
#include "components/feed/core/v2/public/feed_api.h"
#include "components/feed/core/v2/public/logging_parameters.h"
#include "components/feed/core/v2/public/stream_type.h"
#include "components/feed/core/v2/public/surface_renderer.h"
#include "components/feed/core/v2/public/types.h"
#include "components/feed/core/v2/request_throttler.h"
#include "components/feed/core/v2/scheduling.h"
#include "components/feed/core/v2/stream/info_card_tracker.h"
#include "components/feed/core/v2/stream/privacy_notice_card_tracker.h"
#include "components/feed/core/v2/stream_model.h"
#include "components/feed/core/v2/stream_surface_set.h"
#include "components/feed/core/v2/tasks/load_more_task.h"
#include "components/feed/core/v2/tasks/load_stream_task.h"
#include "components/feed/core/v2/tasks/wait_for_store_initialize_task.h"
#include "components/feed/core/v2/user_actions_collector.h"
#include "components/feed/core/v2/web_feed_subscription_coordinator.h"
#include "components/feed/core/v2/wire_response_translator.h"
#include "components/feed/core/v2/xsurface_datastore.h"
#include "components/offline_pages/task/task_queue.h"
#include "components/prefs/pref_member.h"
#include "components/search_engines/template_url_service.h"

class PrefService;

namespace feed {
namespace feed_stream {
class UnreadContentNotifier;
}
class FeedNetwork;
class FeedStore;
class WebFeedSubscriptionCoordinator;
class ImageFetcher;
class MetricsReporter;
class RefreshTaskScheduler;
class PersistentKeyValueStoreImpl;
class StreamModel;
class SurfaceUpdater;

// Implements FeedApi. |FeedStream| additionally exposes functionality
// needed by other classes within the Feed component.
class FeedStream : public FeedApi,
                   public offline_pages::TaskQueue::Delegate,
                   public MetricsReporter::Delegate,
                   public StreamModel::StoreObserver {
 public:
  class Delegate : public WebFeedSubscriptionCoordinator::Delegate {
   public:
    virtual ~Delegate() = default;
    // Returns true if Chrome's EULA has been accepted.
    virtual bool IsEulaAccepted() = 0;
    // Returns true if the device is offline.
    virtual bool IsOffline() = 0;
    virtual DisplayMetrics GetDisplayMetrics() = 0;
    virtual std::string GetLanguageTag() = 0;
    virtual TabGroupEnabledState GetTabGroupEnabledState() = 0;
    virtual void ClearAll() = 0;
    virtual AccountInfo GetAccountInfo() = 0;
    virtual bool IsSigninAllowed() = 0;
    virtual bool IsSupervisedAccount() = 0;
    virtual void PrefetchImage(const GURL& url) = 0;
    virtual void RegisterExperiments(const Experiments& experiments) = 0;
    virtual void RegisterFeedUserSettingsFieldTrial(std::string_view group) = 0;
    virtual std::string GetCountry() = 0;
  };

  FeedStream(RefreshTaskScheduler* refresh_task_scheduler,
             MetricsReporter* metrics_reporter,
             Delegate* delegate,
             PrefService* profile_prefs,
             FeedNetwork* feed_network,
             ImageFetcher* image_fetcher,
             FeedStore* feed_store,
             PersistentKeyValueStoreImpl* persistent_key_value_store,
             TemplateURLService* template_url_service,
             const ChromeInfo& chrome_info);
  ~FeedStream() override;

  FeedStream(const FeedStream&) = delete;
  FeedStream& operator=(const FeedStream&) = delete;

  // FeedApi.

  WebFeedSubscriptionCoordinator& subscriptions() override;
  std::string GetSessionId() const override;

  SurfaceId CreateSurface(const StreamType& type,
                          SingleWebFeedEntryPoint entry_point) override;
  void DestroySurface(SurfaceId surface) override;
  void AttachSurface(SurfaceId surface_id, SurfaceRenderer* renderer) override;
  void DetachSurface(SurfaceId surface_id) override;
  void UpdateUserProfileOnLinkClick(
      const GURL& url,
      const std::vector<int64_t>& entity_mids) override;
  void AddUnreadContentObserver(const StreamType& stream_type,
                                UnreadContentObserver* observer) override;
  void RemoveUnreadContentObserver(const StreamType& stream_type,
                                   UnreadContentObserver* observer) override;
  bool IsArticlesListVisible() override;
  void ExecuteRefreshTask(RefreshTaskId task_id) override;
  ImageFetchId FetchImage(
      const GURL& url,
      base::OnceCallback<void(NetworkResponse)> callback) override;
  void CancelImageFetch(ImageFetchId id) override;
  PersistentKeyValueStoreImpl& GetPersistentKeyValueStore() override;
  void LoadMore(SurfaceId surface_id,
                base::OnceCallback<void(bool)> callback) override;
  void ManualRefresh(SurfaceId surface_id,
                     base::OnceCallback<void(bool)> callback) override;
  void FetchResource(
      const GURL& url,
      const std::string& method,
      const std::vector<std::string>& header_names_and_values,
      const std::string& post_data,
      base::OnceCallback<void(NetworkResponse)> callback) override;
  void ExecuteOperations(
      SurfaceId surface_id,
      std::vector<feedstore::DataOperation> operations) override;
  EphemeralChangeId CreateEphemeralChange(
      SurfaceId surface_id,
      std::vector<feedstore::DataOperation> operations) override;
  EphemeralChangeId CreateEphemeralChangeFromPackedData(
      SurfaceId surface_id,
      std::string_view data) override;
  bool CommitEphemeralChange(SurfaceId surface_id,
                             EphemeralChangeId id) override;
  bool RejectEphemeralChange(SurfaceId surface_id,
                             EphemeralChangeId id) override;
  void ProcessThereAndBackAgain(
      std::string_view data,
      const LoggingParameters& logging_parameters) override;
  void ProcessViewAction(std::string_view data,
                         const LoggingParameters& logging_parameters) override;
  bool WasUrlRecentlyNavigatedFromFeed(const GURL& url) override;
  void InvalidateContentCacheFor(StreamKind stream_kind) override;
  void RecordContentViewed(SurfaceId surface_id, uint64_t docid) override;
  DebugStreamData GetDebugStreamData() override;
  void ForceRefreshForDebugging(const StreamType& stream_type) override;
  std::string DumpStateForDebugging() override;
  void SetForcedStreamUpdateForDebugging(
      const feedui::StreamUpdate& stream_update) override;

  void ReportSliceViewed(SurfaceId surface_id,
                         const std::string& slice_id) override;
  void ReportFeedViewed(SurfaceId surface_id) override;
  void ReportPageLoaded(SurfaceId surface_id) override;
  void ReportOpenAction(const GURL& url,
                        SurfaceId surface_id,
                        const std::string& slice_id,
                        OpenActionType action_type) override;
  void ReportOpenVisitComplete(SurfaceId surface_id,
                               base::TimeDelta visit_time) override;
  void ReportStreamScrolled(SurfaceId surface_id, int distance_dp) override;
  void ReportStreamScrollStart(SurfaceId surface_id) override;
  void ReportOtherUserAction(SurfaceId surface_id,
                             FeedUserActionType action_type) override;
  void ReportOtherUserAction(const StreamType& stream_type,
                             FeedUserActionType action_type) override;
  void ReportInfoCardTrackViewStarted(SurfaceId surface_id,
                                      int info_card_type) override;
  void ReportInfoCardViewed(SurfaceId surface_id,
                            int info_card_type,
                            int minimum_view_interval_seconds) override;
  void ReportInfoCardClicked(SurfaceId surface_id, int info_card_type) override;
  void ReportInfoCardDismissedExplicitly(SurfaceId surface_id,
                                         int info_card_type) override;
  void ResetInfoCardStates(SurfaceId surface_id, int info_card_type) override;
  void ReportContentSliceVisibleTimeForGoodVisits(
      SurfaceId surface_id,
      base::TimeDelta elapsed) override;
  base::Time GetLastFetchTime(SurfaceId surface_id) override;
  void SetContentOrder(const StreamType& stream_type,
                       ContentOrder content_order) override;
  ContentOrder GetContentOrder(const StreamType& stream_type) const override;
  ContentOrder GetContentOrderFromPrefs(const StreamType& stream_type) override;
  void IncrementFollowedFromWebPageMenuCount() override;

  // offline_pages::TaskQueue::Delegate.
  void OnTaskQueueIsIdle() override;

  // MetricsReporter::Delegate.
  void SubscribedWebFeedCount(base::OnceCallback<void(int)> callback) override;
  void RegisterFeedUserSettingsFieldTrial(std::string_view group) override;

  // StreamModel::StoreObserver.
  void OnStoreChange(StreamModel::StoreUpdate update) override;

  // Event indicators. These functions are called from an external source
  // to indicate an event.

  // Called when Chrome's EULA has been accepted. This should happen when
  // Delegate::IsEulaAccepted() changes from false to true.
  void OnEulaAccepted();
  // Invoked when Chrome is backgrounded.
  void OnEnterBackground();
  // The user signed in to Chrome.
  void OnSignedIn();
  // The user signed out of Chrome.
  void OnSignedOut();
  // The user has deleted all browsing history.
  void OnAllHistoryDeleted();
  // Chrome's cached data was cleared.
  void OnCacheDataCleared();

  // State shared for the sake of implementing FeedStream. Typically these
  // functions are used by tasks.

  void LoadModel(const StreamType& stream_type,
                 std::unique_ptr<StreamModel> model);

  // Store/upload an action and update the consistency token. |callback| is
  // called with |true| if the consistency token was written to the store.
  void UploadAction(
      feedwire::FeedAction action,
      const LoggingParameters& logging_parameters,
      bool upload_now,
      base::OnceCallback<void(UploadActionsTask::Result)> callback);

  FeedNetwork& GetNetwork() { return *feed_network_; }
  FeedStore& GetStore() { return *store_; }
  RequestThrottler& GetRequestThrottler() { return request_throttler_; }
  const feedstore::Metadata& GetMetadata() const;
  void SetMetadata(feedstore::Metadata metadata);
  bool SetMetadata(std::optional<feedstore::Metadata> metadata);
  void SetStreamStale(const StreamType& stream_type, bool is_stale);

  MetricsReporter& GetMetricsReporter() const { return *metrics_reporter_; }

  void PrefetchImage(const GURL& url);

  bool IsSigninAllowed() const { return delegate_->IsSigninAllowed(); }
  bool IsSignedIn() const { return !delegate_->GetAccountInfo().IsEmpty(); }
  AccountInfo GetAccountInfo() const { return delegate_->GetAccountInfo(); }

  // Determines if we should attempt loading the stream or refreshing at all.
  // Returns |LoadStreamStatus::kNoStatus| if loading may be attempted.
  LaunchResult ShouldAttemptLoad(const StreamType& stream_type,
                                 LoadType load_type,
                                 bool model_loading = false);

  // Whether the last scheduled refresh was missed.
  bool MissedLastRefresh(const StreamType& stream_type);

  // Determines if a FeedQuery request can be made. If successful,
  // returns |LoadStreamStatus::kNoStatus| and acquires throttler quota.
  // Otherwise returns the reason. If |consume_quota| is false, no quota is
  // consumed. This can be used to predict the likely result on a subsequent
  // call.
  LaunchResult ShouldMakeFeedQueryRequest(const StreamType& stream_type,
                                          LoadType load_type,
                                          bool consume_quota = true);

  // Returns the Chrome sign in status
  feedwire::ChromeSignInStatus::SignInStatus GetSignInStatus() const;

  // Unloads one stream model. Surfaces are not updated, and will remain frozen
  // until a model load is requested.
  void UnloadModel(const StreamType& stream_type);
  // Unloads all stream models.
  void UnloadModels();

  // Triggers a stream load. The load will be aborted if |ShouldAttemptLoad()|
  // is not true. Returns CARDS_UNSPECIFIED if loading is to proceed, or another
  // DiscoverLaunchResult if loading will not be attempted.
  feedwire::DiscoverLaunchResult TriggerStreamLoad(
      const StreamType& stream_type,
      SingleWebFeedEntryPoint entry_point = SingleWebFeedEntryPoint::kOther);

  // Only to be called by ClearAllTask. This clears other stream data stored in
  // memory.
  void FinishClearAll();

  // Only to be called by ClearStreamTask. This clears other stream data stored
  // in memory.
  void FinishClearStream(const StreamType& stream_type);

  // Returns the model associated with the stream type or surface if it is
  // loaded, or null otherwise.
  StreamModel* GetModel(const StreamType& stream_type);
  StreamModel* GetModel(SurfaceId surface_id);

  // Gets request metadata assuming the account is signed-in. This is useful for
  // uploading actions where stream type is not known, but sign-in status is
  // required.
  RequestMetadata GetSignedInRequestMetadata() const;

  // Gets request metadata, looking up if session ID or client instance ID
  // should be used based on the login state of Chrome and the model for the
  // appropriate Stream.
  RequestMetadata GetRequestMetadata(const StreamType& stream_type,
                                     bool is_for_next_page) const;

  bool HasUnreadContent(const StreamType& stream_type);

  bool IsOffline() const { return delegate_->IsOffline(); }

  offline_pages::TaskQueue& GetTaskQueue() { return task_queue_; }

  const WireResponseTranslator& GetWireResponseTranslator() const {
    return *wire_response_translator_;
  }

  LaunchReliabilityLogger& GetLaunchReliabilityLogger(
      const StreamType& stream_type);

  XsurfaceDatastoreSlice& GetGlobalXsurfaceDatastore() {
    return global_datastore_slice_;
  }

  // Testing functionality.
  offline_pages::TaskQueue& GetTaskQueueForTesting();
  // Loads |model|. Should be used for testing in place of typical model
  // loading from network or storage.
  void LoadModelForTesting(const StreamType& stream_type,
                           std::unique_ptr<StreamModel> model);
  void SetWireResponseTranslatorForTesting(
      const WireResponseTranslator* wire_response_translator) {
    wire_response_translator_ = wire_response_translator;
  }
  void SetIdleCallbackForTesting(base::RepeatingClosure idle_callback);

  bool ClearAllInProgress() const { return clear_all_in_progress_; }

  bool IsEnabledAndVisible();

  bool IsWebFeedEnabled();

  PrefService* profile_prefs() const { return profile_prefs_; }

  base::WeakPtr<FeedStream> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  bool GetStreamPresentForTest(StreamType stream_type) {
    return FindStream(stream_type) != nullptr;
  }

  // Used by tests to control the chained refresh of the web-feed.
  void SetChainedWebFeedRefreshEnabledForTesting(bool enabled) {
    chained_web_feed_refresh_enabled_ = enabled;
  }

 private:
  using UnreadContentNotifier = feed_stream::UnreadContentNotifier;

  struct Stream {
    explicit Stream(const StreamType& stream_type);
    ~Stream();
    Stream(const Stream&) = delete;
    Stream& operator=(const Stream&) = delete;
    StreamType type;
    // Whether the model is being loaded. Used to prevent multiple simultaneous
    // attempts to load the model.
    bool model_loading_in_progress = false;
    StreamSurfaceSet surfaces;
    std::unique_ptr<SurfaceUpdater> surface_updater;
    // The stream model. Null if not yet loaded.
    // Internally, this should only be changed by |LoadModel()| and
    // |UnloadModel()|.
    std::unique_ptr<StreamModel> model;
    int unload_on_detach_sequence_number = 0;
    ContentHashSet content_ids;
    std::vector<UnreadContentNotifier> unread_content_notifiers;
    std::vector<base::OnceCallback<void(bool)>> load_more_complete_callbacks;
    std::vector<base::OnceCallback<void(bool)>> refresh_complete_callbacks;
    bool is_activity_logging_enabled = false;
    // Cache the list of IDs of contents that have been viewed by the user.
    // This allows fast lookup. It is only used in for-you feed.
    base::flat_set<uint32_t> viewed_content_hashes;
  };

  void InitializeComplete(WaitForStoreInitializeTask::Result result);
  void CleanupDestroyedSurfaces();
  void SetRequestSchedule(const StreamType& stream_type,
                          RequestSchedule schedule);

  void SetRequestSchedule(RefreshTaskId task_id, RequestSchedule schedule);

  // A single function task to delete stored feed data and force a refresh.
  // To only be called from within a |Task|.
  void ForceRefreshForDebuggingTask(const StreamType& stream_type);
  void ForceRefreshTask(const StreamType& stream_type);

  void ScheduleModelUnloadIfNoSurfacesAttached(const StreamType& stream_type);
  void AddUnloadModelIfNoSurfacesAttachedTask(const StreamType& stream_type,
                                              int sequence_number);
  void UnloadModelIfNoSurfacesAttachedTask(const StreamType& stream_type);

  void StreamLoadComplete(LoadStreamTask::Result result);
  void LoadMoreComplete(LoadMoreTask::Result result);
  void BackgroundRefreshComplete(LoadStreamTask::Result result);
  void LoadTaskComplete(const LoadStreamTask::Result& result);
  void UploadActionsComplete(UploadActionsTask::Result result);
  void FetchResourceComplete(base::OnceCallback<void(NetworkResponse)> callback,
                             FeedNetwork::RawResponse response);
  void ClearAll();
  void ClearStream(const StreamType& stream_type, int sequence_number);

  bool IsFeedEnabledByEnterprisePolicy();
  bool IsFeedEnabled();
  bool IsFeedEnabledByDse();

  bool HasReachedConditionsToUploadActionsWithNoticeCard();

  void MaybeNotifyHasUnreadContent(const StreamType& stream_type);
  void EnabledPreferencesChanged();

  Stream& GetStream(const StreamType& type);
  Stream* FindStream(const StreamType& type);
  Stream* FindStream(SurfaceId surface_id);
  FeedStreamSurface* FindSurface(SurfaceId surface_id);

  const Stream* FindStream(const StreamType& type) const;
  void UpdateExperiments(Experiments experiments);

  RequestMetadata GetCommonRequestMetadata(bool signed_in_request,
                                           bool allow_expired_session_id) const;

  // Schedule a feed-close refresh when the user has taken some kind of action
  // on the feed.
  void ScheduleFeedCloseRefresh(const StreamType& type);

  void CheckDuplicatedContentsOnRefresh();
  void AddViewedContentHashes(const feedstore::Content& content);

  feedwire::DefaultSearchEngine::SearchEngine GetDefaultSearchEngine() const;

  // Unowned.

  raw_ptr<RefreshTaskScheduler> refresh_task_scheduler_;
  raw_ptr<MetricsReporter> metrics_reporter_;
  raw_ptr<Delegate> delegate_;
  raw_ptr<PrefService> profile_prefs_;  // May be null.
  raw_ptr<FeedNetwork> feed_network_;
  raw_ptr<ImageFetcher> image_fetcher_;
  raw_ptr<FeedStore, DanglingUntriaged> store_;
  raw_ptr<PersistentKeyValueStoreImpl, DanglingUntriaged>
      persistent_key_value_store_;
  raw_ptr<const WireResponseTranslator> wire_response_translator_;
  raw_ptr<TemplateURLService> template_url_service_;

  StreamModel::Context stream_model_context_;
  // For Xsurface datastore data which applies to all `StreamType`s.
  XsurfaceDatastoreSlice global_datastore_slice_;

  ChromeInfo chrome_info_;

  offline_pages::TaskQueue task_queue_;

  std::map<StreamType, Stream> streams_;

  // FeedStreamSurface handling:
  // We want to keep FeedStreamSurface instances available even after the actual
  // feed surface is no longer present, so that we can handle latent calls to
  // reporting functions (e.g. web page load on navigate) or calls in response
  // to command handlers (e.g. ephemeral operations) which do not have lifetime
  // bound to the feed surface.
  // Use deque so that references to surfaces are safer.
  std::deque<FeedStreamSurface> all_surfaces_;
  // Destroyed surfaces are kept at least this long.
  static constexpr base::TimeDelta kSurfaceDestroyDelay = base::Minutes(10);
  // The list of IDs for surfaces that were destroyed.
  std::vector<SurfaceId> destroyed_surfaces_;
  // Time of the last destroyed surface.
  base::TimeTicks surface_destroy_time_;

  std::unique_ptr<WebFeedSubscriptionCoordinator>
      web_feed_subscription_coordinator_;

  // Mutable state.
  RequestThrottler request_throttler_;

  BooleanPrefMember has_stored_data_;
  BooleanPrefMember snippets_enabled_by_policy_;
  BooleanPrefMember articles_list_visible_;
  BooleanPrefMember snippets_enabled_by_dse_;
  BooleanPrefMember signin_allowed_;

  // State loaded at startup:
  feedstore::Metadata metadata_;
  bool metadata_populated_ = false;

  base::ObserverList<UnreadContentObserver> unread_content_observers_;

  // To allow tests to wait on task queue idle.
  base::RepeatingClosure idle_callback_;

  // Stream update forced to use for new surfaces. This is provided in feed
  // internals page for debugging purpose.
  feedui::StreamUpdate forced_stream_update_for_debugging_;

  PrivacyNoticeCardTracker privacy_notice_card_tracker_;

  InfoCardTracker info_card_tracker_;

  bool clear_all_in_progress_ = false;

  std::vector<GURL> recent_feed_navigations_;
  UserActionsCollector user_actions_collector_;

  base::TimeTicks last_refresh_scheduled_on_interaction_time_{};

  bool chained_web_feed_refresh_enabled_ = true;

  // True if the stream with any stream type has been loaded at least once since
  // the start.
  bool loaded_after_start_ = false;

  base::WeakPtrFactory<FeedStream> weak_ptr_factory_{this};
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_FEED_STREAM_H_
