// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_FEED_STREAM_H_
#define COMPONENTS_FEED_CORE_V2_FEED_STREAM_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner_util.h"
#include "base/version.h"
#include "components/feed/core/common/enums.h"
#include "components/feed/core/common/user_classifier.h"
#include "components/feed/core/proto/v2/wire/response.pb.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/protocol_translator.h"
#include "components/feed/core/v2/public/feed_stream_api.h"
#include "components/feed/core/v2/request_throttler.h"
#include "components/feed/core/v2/scheduling.h"
#include "components/feed/core/v2/stream_model.h"
#include "components/feed/core/v2/tasks/load_more_task.h"
#include "components/feed/core/v2/tasks/load_stream_task.h"
#include "components/offline_pages/core/prefetch/suggestions_provider.h"
#include "components/offline_pages/task/task_queue.h"

class PrefService;

namespace base {
class Clock;
class TickClock;
}  // namespace base

namespace offline_pages {
class OfflinePageModel;
class PrefetchService;
}  // namespace offline_pages

namespace feed {
class FeedNetwork;
class FeedStore;
class ImageFetcher;
class MetricsReporter;
class OfflinePageSpy;
class RefreshTaskScheduler;
class StreamModel;
class SurfaceUpdater;
struct StreamModelUpdateRequest;

// Implements FeedStreamApi. |FeedStream| additionally exposes functionality
// needed by other classes within the Feed component.
class FeedStream : public FeedStreamApi,
                   public offline_pages::TaskQueue::Delegate,
                   public StreamModel::StoreObserver {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    // Returns true if Chrome's EULA has been accepted.
    virtual bool IsEulaAccepted() = 0;
    // Returns true if the device is offline.
    virtual bool IsOffline() = 0;
    virtual DisplayMetrics GetDisplayMetrics() = 0;
    virtual std::string GetLanguageTag() = 0;
    virtual void ClearAll() = 0;
    virtual bool IsSignedIn() = 0;
  };

  // Forwards to |feed::TranslateWireResponse()| by default. Can be overridden
  // for testing.
  class WireResponseTranslator {
   public:
    WireResponseTranslator() = default;
    ~WireResponseTranslator() = default;
    virtual RefreshResponseData TranslateWireResponse(
        feedwire::Response response,
        StreamModelUpdateRequest::Source source,
        bool was_signed_in_request,
        base::Time current_time) const;
  };

  class Metadata {
   public:
    explicit Metadata(FeedStore* store);
    ~Metadata();

    void Populate(feedstore::Metadata metadata);

    std::string GetConsistencyToken() const;
    void SetConsistencyToken(std::string consistency_token);

    LocalActionId GetNextActionId();

   private:
    FeedStore* store_;
    feedstore::Metadata metadata_;
  };

  FeedStream(RefreshTaskScheduler* refresh_task_scheduler,
             MetricsReporter* metrics_reporter,
             Delegate* delegate,
             PrefService* profile_prefs,
             FeedNetwork* feed_network,
             ImageFetcher* image_fetcher,
             FeedStore* feed_store,
             offline_pages::PrefetchService* prefetch_service,
             offline_pages::OfflinePageModel* offline_page_model,
             const base::Clock* clock,
             const base::TickClock* tick_clock,
             const ChromeInfo& chrome_info);
  ~FeedStream() override;

  FeedStream(const FeedStream&) = delete;
  FeedStream& operator=(const FeedStream&) = delete;

  // Initializes scheduling. This should be called at startup.
  void InitializeScheduling();

  // FeedStreamApi.

  bool IsActivityLoggingEnabled() const override;
  void AttachSurface(SurfaceInterface*) override;
  void DetachSurface(SurfaceInterface*) override;
  void SetArticlesListVisible(bool is_visible) override;
  bool IsArticlesListVisible() override;
  std::string GetClientInstanceId() override;
  void ExecuteRefreshTask() override;
  ImageFetchId FetchImage(
      const GURL& url,
      base::OnceCallback<void(NetworkResponse)> callback) override;
  void CancelImageFetch(ImageFetchId id) override;
  void LoadMore(SurfaceId surface_id,
                base::OnceCallback<void(bool)> callback) override;
  void ExecuteOperations(
      std::vector<feedstore::DataOperation> operations) override;
  EphemeralChangeId CreateEphemeralChange(
      std::vector<feedstore::DataOperation> operations) override;
  EphemeralChangeId CreateEphemeralChangeFromPackedData(
      base::StringPiece data) override;
  bool CommitEphemeralChange(EphemeralChangeId id) override;
  bool RejectEphemeralChange(EphemeralChangeId id) override;
  void ProcessThereAndBackAgain(base::StringPiece data) override;
  void ProcessViewAction(base::StringPiece data) override;
  DebugStreamData GetDebugStreamData() override;
  void ForceRefreshForDebugging() override;
  std::string DumpStateForDebugging() override;

  void ReportSliceViewed(SurfaceId surface_id,
                         const std::string& slice_id) override;
  void ReportFeedViewed(SurfaceId surface_id) override;
  void ReportNavigationStarted() override;
  void ReportPageLoaded() override;
  void ReportOpenAction(const std::string& slice_id) override;
  void ReportOpenVisitComplete(base::TimeDelta visit_time) override;
  void ReportOpenInNewTabAction(const std::string& slice_id) override;
  void ReportOpenInNewIncognitoTabAction() override;
  void ReportSendFeedbackAction() override;
  void ReportLearnMoreAction() override;
  void ReportDownloadAction() override;
  void ReportRemoveAction() override;
  void ReportNotInterestedInAction() override;
  void ReportManageInterestsAction() override;
  void ReportContextMenuOpened() override;
  void ReportStreamScrolled(int distance_dp) override;
  void ReportStreamScrollStart() override;
  void ReportTurnOnAction() override;
  void ReportTurnOffAction() override;

  // offline_pages::TaskQueue::Delegate.
  void OnTaskQueueIsIdle() override;

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

  void LoadModel(std::unique_ptr<StreamModel> model);

  void SetRequestSchedule(RequestSchedule schedule);

  // Store/upload an action and update the consistency token. |callback| is
  // called with |true| if the consistency token was written to the store.
  void UploadAction(
      feedwire::FeedAction action,
      bool upload_now,
      base::OnceCallback<void(UploadActionsTask::Result)> callback);

  FeedNetwork* GetNetwork() { return feed_network_; }
  FeedStore* GetStore() { return store_; }
  RequestThrottler* GetRequestThrottler() { return &request_throttler_; }
  Metadata* GetMetadata() { return &metadata_; }
  MetricsReporter* GetMetricsReporter() const { return metrics_reporter_; }

  // Returns the time of the last content fetch.
  base::Time GetLastFetchTime();

  bool HasSurfaceAttached() const;
  bool IsSignedIn() const { return delegate_->IsSignedIn(); }

  // Determines if we should attempt loading the stream or refreshing at all.
  // Returns |LoadStreamStatus::kNoStatus| if loading may be attempted.
  LoadStreamStatus ShouldAttemptLoad(bool model_loading = false);

  // Determines if a FeedQuery request can be made. If successful,
  // returns |LoadStreamStatus::kNoStatus| and acquires throttler quota.
  // Otherwise returns the reason. If |consume_quota| is false, no quota is
  // consumed. This can be used to predict the likely result on a subsequent
  // call.
  LoadStreamStatus ShouldMakeFeedQueryRequest(bool is_load_more = false,
                                              bool consume_quota = true);

  // Returns true if a FeedQuery request made right now should be made without
  // user credentials.
  bool ShouldForceSignedOutFeedQueryRequest() const;

  // Unloads the model. Surfaces are not updated, and will remain frozen until a
  // model load is requested.
  void UnloadModel();

  // Triggers a stream load. The load will be aborted if |ShouldAttemptLoad()|
  // is not true.
  void TriggerStreamLoad();

  // Only to be called by ClearAllTask. This clears other stream data stored in
  // memory.
  void FinishClearAll();

  // Returns the model if it is loaded, or null otherwise.
  StreamModel* GetModel() { return model_.get(); }

  const base::Clock* GetClock() const { return clock_; }
  const base::TickClock* GetTickClock() const { return tick_clock_; }
  RequestMetadata GetRequestMetadata();

  const WireResponseTranslator* GetWireResponseTranslator() const {
    return wire_response_translator_;
  }

  // Testing functionality.
  offline_pages::TaskQueue* GetTaskQueueForTesting();
  // Loads |model|. Should be used for testing in place of typical model
  // loading from network or storage.
  void LoadModelForTesting(std::unique_ptr<StreamModel> model);
  void SetWireResponseTranslatorForTesting(
      const WireResponseTranslator* wire_response_translator) {
    wire_response_translator_ = wire_response_translator;
  }
  void SetIdleCallbackForTesting(base::RepeatingClosure idle_callback);

  bool CanUploadActions() const;
  void SetLastStreamLoadHadNoticeCard(bool value);

 private:
  class OfflineSuggestionsProvider;

  base::WeakPtr<FeedStream> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Re-evaluate whether or not activity logging should currently be enabled.
  void UpdateIsActivityLoggingEnabled();

  void GetPrefetchSuggestions(
      base::OnceCallback<void(std::vector<offline_pages::PrefetchSuggestion>)>
          suggestions_callback);

  // A single function task to delete stored feed data and force a refresh.
  // To only be called from within a |Task|.
  void ForceRefreshForDebuggingTask();

  void ScheduleModelUnloadIfNoSurfacesAttached();
  void AddUnloadModelIfNoSurfacesAttachedTask(int sequence_number);
  void UnloadModelIfNoSurfacesAttachedTask();

  void InitialStreamLoadComplete(LoadStreamTask::Result result);
  void LoadMoreComplete(LoadMoreTask::Result result);
  void BackgroundRefreshComplete(LoadStreamTask::Result result);
  void UploadActionsComplete(UploadActionsTask::Result result);

  void ClearAll();

  bool IsFeedEnabledByEnterprisePolicy();

  bool HasReachedConditionsToUploadActionsWithNoticeCard();
  void DeclareHasReachedConditionsToUploadActionsWithNoticeCard();

  void UpdateShownSlicesUploadCondition(int index);

  bool CanLogViews() const;

  void UpdateCanUploadActionsWithNoticeCard();

  // Unowned.

  offline_pages::PrefetchService* prefetch_service_;
  RefreshTaskScheduler* refresh_task_scheduler_;
  MetricsReporter* metrics_reporter_;
  Delegate* delegate_;
  PrefService* profile_prefs_;  // May be null.
  FeedNetwork* feed_network_;
  ImageFetcher* image_fetcher_;
  FeedStore* store_;
  const base::Clock* clock_;
  const base::TickClock* tick_clock_;
  const WireResponseTranslator* wire_response_translator_;

  ChromeInfo chrome_info_;

  offline_pages::TaskQueue task_queue_;
  // Whether the model is being loaded. Used to prevent multiple simultaneous
  // attempts to load the model.
  bool model_loading_in_progress_ = false;
  std::unique_ptr<SurfaceUpdater> surface_updater_;
  std::unique_ptr<OfflineSuggestionsProvider> offline_suggestions_provider_;
  std::unique_ptr<OfflinePageSpy> offline_page_spy_;
  // The stream model. Null if not yet loaded.
  // Internally, this should only be changed by |LoadModel()| and
  // |UnloadModel()|.
  std::unique_ptr<StreamModel> model_;

  // Mutable state.
  RequestThrottler request_throttler_;
  base::TimeTicks signed_out_refreshes_until_;
  std::vector<base::OnceCallback<void(bool)>> load_more_complete_callbacks_;
  Metadata metadata_;
  int unload_on_detach_sequence_number_ = 0;
  bool is_activity_logging_enabled_ = false;
  // Whether the feed stream can upload actions with the notice card in the
  // feed.
  bool can_upload_actions_with_notice_card_ = false;

  // To allow tests to wait on task queue idle.
  base::RepeatingClosure idle_callback_;

  base::WeakPtrFactory<FeedStream> weak_ptr_factory_{this};
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_FEED_STREAM_H_
