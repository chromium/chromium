// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/feed_stream.h"

#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/ui.pb.h"
#include "components/feed/core/proto/v2/wire/there_and_back_again_data.pb.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/feed_store.h"
#include "components/feed/core/v2/image_fetcher.h"
#include "components/feed/core/v2/metrics_reporter.h"
#include "components/feed/core/v2/offline_page_spy.h"
#include "components/feed/core/v2/prefs.h"
#include "components/feed/core/v2/protocol_translator.h"
#include "components/feed/core/v2/refresh_task_scheduler.h"
#include "components/feed/core/v2/scheduling.h"
#include "components/feed/core/v2/stream_model.h"
#include "components/feed/core/v2/surface_updater.h"
#include "components/feed/core/v2/tasks/clear_all_task.h"
#include "components/feed/core/v2/tasks/get_prefetch_suggestions_task.h"
#include "components/feed/core/v2/tasks/load_stream_task.h"
#include "components/feed/core/v2/tasks/upload_actions_task.h"
#include "components/feed/core/v2/tasks/wait_for_store_initialize_task.h"
#include "components/feed/feed_feature_list.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "components/offline_pages/task/closure_task.h"
#include "components/prefs/pref_service.h"

namespace feed {
namespace {

void UpdateDebugStreamData(
    const UploadActionsTask::Result& upload_actions_result,
    DebugStreamData& debug_data) {
  if (upload_actions_result.last_network_response_info) {
    debug_data.upload_info = upload_actions_result.last_network_response_info;
  }
}

void PopulateDebugStreamData(const LoadStreamTask::Result& load_result,
                             PrefService& profile_prefs) {
  DebugStreamData debug_data = ::feed::prefs::GetDebugStreamData(profile_prefs);
  std::stringstream ss;
  ss << "Code: " << load_result.final_status;
  debug_data.load_stream_status = ss.str();
  debug_data.fetch_info = load_result.network_response_info;
  if (load_result.upload_actions_result) {
    UpdateDebugStreamData(*load_result.upload_actions_result, debug_data);
  }
  ::feed::prefs::SetDebugStreamData(debug_data, profile_prefs);
}

void PopulateDebugStreamData(
    const UploadActionsTask::Result& upload_actions_result,
    PrefService& profile_prefs) {
  DebugStreamData debug_data = ::feed::prefs::GetDebugStreamData(profile_prefs);
  UpdateDebugStreamData(upload_actions_result, debug_data);
  ::feed::prefs::SetDebugStreamData(debug_data, profile_prefs);
}

}  // namespace

// offline_pages::SuggestionsProvider.
class FeedStream::OfflineSuggestionsProvider
    : public offline_pages::SuggestionsProvider {
 public:
  explicit OfflineSuggestionsProvider(FeedStream* stream) : stream_(stream) {}
  virtual ~OfflineSuggestionsProvider() = default;
  OfflineSuggestionsProvider(const OfflineSuggestionsProvider&) = delete;
  OfflineSuggestionsProvider& operator=(const OfflineSuggestionsProvider&) =
      delete;
  void GetCurrentArticleSuggestions(
      SuggestionCallback suggestions_callback) override {
    stream_->GetPrefetchSuggestions(std::move(suggestions_callback));
  }

  // These signals aren't used for v2.
  void ReportArticleListViewed() override {}
  void ReportArticleViewed(GURL article_url) override {}

 private:
  FeedStream* stream_;
};

RefreshResponseData FeedStream::WireResponseTranslator::TranslateWireResponse(
    feedwire::Response response,
    StreamModelUpdateRequest::Source source,
    bool was_signed_in_request,
    base::Time current_time) const {
  return ::feed::TranslateWireResponse(std::move(response), source,
                                       was_signed_in_request, current_time);
}

FeedStream::Metadata::Metadata(FeedStore* store) : store_(store) {}
FeedStream::Metadata::~Metadata() = default;

void FeedStream::Metadata::Populate(feedstore::Metadata metadata) {
  metadata_ = std::move(metadata);
}

std::string FeedStream::Metadata::GetConsistencyToken() const {
  return metadata_.consistency_token();
}

void FeedStream::Metadata::SetConsistencyToken(std::string consistency_token) {
  metadata_.set_consistency_token(std::move(consistency_token));
  store_->WriteMetadata(metadata_, base::DoNothing());
}

LocalActionId FeedStream::Metadata::GetNextActionId() {
  uint32_t id = metadata_.next_action_id();
  // Never use 0, as that's an invalid LocalActionId.
  if (id == 0)
    ++id;
  metadata_.set_next_action_id(id + 1);
  store_->WriteMetadata(metadata_, base::DoNothing());
  return LocalActionId(id);
}

FeedStream::FeedStream(RefreshTaskScheduler* refresh_task_scheduler,
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
                       const ChromeInfo& chrome_info)
    : prefetch_service_(prefetch_service),
      refresh_task_scheduler_(refresh_task_scheduler),
      metrics_reporter_(metrics_reporter),
      delegate_(delegate),
      profile_prefs_(profile_prefs),
      feed_network_(feed_network),
      image_fetcher_(image_fetcher),
      store_(feed_store),
      clock_(clock),
      tick_clock_(tick_clock),
      chrome_info_(chrome_info),
      task_queue_(this),
      request_throttler_(profile_prefs, clock),
      metadata_(feed_store) {
  static WireResponseTranslator default_translator;
  wire_response_translator_ = &default_translator;

  surface_updater_ = std::make_unique<SurfaceUpdater>(metrics_reporter_);
  offline_page_spy_ = std::make_unique<OfflinePageSpy>(surface_updater_.get(),
                                                       offline_page_model);

  if (prefetch_service_) {
    offline_suggestions_provider_ =
        std::make_unique<OfflineSuggestionsProvider>(this);
    prefetch_service_->SetSuggestionProvider(
        offline_suggestions_provider_.get());
  }

  // Inserting this task first ensures that |store_| is initialized before
  // it is used.
  task_queue_.AddTask(std::make_unique<WaitForStoreInitializeTask>(this));

  UpdateCanUploadActionsWithNoticeCard();
}

void FeedStream::InitializeScheduling() {
  if (!IsArticlesListVisible()) {
    refresh_task_scheduler_->Cancel();
    return;
  }
}

FeedStream::~FeedStream() = default;

void FeedStream::TriggerStreamLoad() {
  if (model_ || model_loading_in_progress_)
    return;

  // If we should not load the stream, abort and send a zero-state update.
  LoadStreamStatus do_not_attempt_reason = ShouldAttemptLoad();
  if (do_not_attempt_reason != LoadStreamStatus::kNoStatus) {
    InitialStreamLoadComplete(LoadStreamTask::Result(do_not_attempt_reason));
    return;
  }

  model_loading_in_progress_ = true;
  surface_updater_->LoadStreamStarted();
  task_queue_.AddTask(std::make_unique<LoadStreamTask>(
      LoadStreamTask::LoadType::kInitialLoad, this,
      base::BindOnce(&FeedStream::InitialStreamLoadComplete,
                     base::Unretained(this))));
}

void FeedStream::InitialStreamLoadComplete(LoadStreamTask::Result result) {
  PopulateDebugStreamData(result, *profile_prefs_);
  metrics_reporter_->OnLoadStream(result.load_from_store_status,
                                  result.final_status,
                                  std::move(result.latencies));
  UpdateIsActivityLoggingEnabled();

  model_loading_in_progress_ = false;

  surface_updater_->LoadStreamComplete(model_ != nullptr, result.final_status);

  if (result.loaded_new_content_from_network && prefetch_service_)
    prefetch_service_->NewSuggestionsAvailable();
}

void FeedStream::OnEnterBackground() {
  UpdateCanUploadActionsWithNoticeCard();
  metrics_reporter_->OnEnterBackground();
  if (GetFeedConfig().upload_actions_on_enter_background) {
    task_queue_.AddTask(std::make_unique<UploadActionsTask>(
        this, base::BindOnce(&FeedStream::UploadActionsComplete,
                             base::Unretained(this))));
  }
}

bool FeedStream::IsActivityLoggingEnabled() const {
  return is_activity_logging_enabled_ && CanUploadActions();
}

void FeedStream::UpdateIsActivityLoggingEnabled() {
  is_activity_logging_enabled_ =
      model_ && model_->signed_in() && model_->logging_enabled();
}

void FeedStream::AttachSurface(SurfaceInterface* surface) {
  metrics_reporter_->SurfaceOpened(surface->GetSurfaceId());
  TriggerStreamLoad();
  surface_updater_->SurfaceAdded(surface);
  // Cancel any scheduled model unload task.
  ++unload_on_detach_sequence_number_;
  UpdateCanUploadActionsWithNoticeCard();
}

void FeedStream::DetachSurface(SurfaceInterface* surface) {
  metrics_reporter_->SurfaceClosed(surface->GetSurfaceId());
  surface_updater_->SurfaceRemoved(surface);
  UpdateCanUploadActionsWithNoticeCard();
  ScheduleModelUnloadIfNoSurfacesAttached();
}

void FeedStream::ScheduleModelUnloadIfNoSurfacesAttached() {
  if (surface_updater_->HasSurfaceAttached())
    return;

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FeedStream::AddUnloadModelIfNoSurfacesAttachedTask,
                     GetWeakPtr(), unload_on_detach_sequence_number_),
      GetFeedConfig().model_unload_timeout);
}

void FeedStream::AddUnloadModelIfNoSurfacesAttachedTask(int sequence_number) {
  // Don't continue if unload_on_detach_sequence_number_ has changed.
  if (unload_on_detach_sequence_number_ != sequence_number)
    return;

  task_queue_.AddTask(std::make_unique<offline_pages::ClosureTask>(
      base::BindOnce(&FeedStream::UnloadModelIfNoSurfacesAttachedTask,
                     base::Unretained(this))));
}

void FeedStream::UnloadModelIfNoSurfacesAttachedTask() {
  if (surface_updater_->HasSurfaceAttached())
    return;
  UnloadModel();
}

void FeedStream::SetArticlesListVisible(bool is_visible) {
  profile_prefs_->SetBoolean(prefs::kArticlesListVisible, is_visible);
}

bool FeedStream::IsArticlesListVisible() {
  return profile_prefs_->GetBoolean(prefs::kArticlesListVisible);
}

std::string FeedStream::GetClientInstanceId() {
  return prefs::GetClientInstanceId(*profile_prefs_);
}

bool FeedStream::IsFeedEnabledByEnterprisePolicy() {
  return profile_prefs_->GetBoolean(prefs::kEnableSnippets);
}

void FeedStream::LoadMore(SurfaceId surface_id,
                          base::OnceCallback<void(bool)> callback) {
  if (!model_) {
    DLOG(ERROR) << "Ignoring LoadMore() before the model is loaded";
    return std::move(callback).Run(false);
  }
  // We want to abort early to avoid showing a loading spinner if it's not
  // necessary.
  if (ShouldMakeFeedQueryRequest(/*is_load_more=*/true,
                                 /*consume_quota=*/false) !=
      LoadStreamStatus::kNoStatus) {
    return std::move(callback).Run(false);
  }

  metrics_reporter_->OnLoadMoreBegin(surface_id);
  surface_updater_->SetLoadingMore(true);

  // Have at most one in-flight LoadMore() request. Send the result to all
  // requestors.
  load_more_complete_callbacks_.push_back(std::move(callback));
  if (load_more_complete_callbacks_.size() == 1) {
    task_queue_.AddTask(std::make_unique<LoadMoreTask>(
        this,
        base::BindOnce(&FeedStream::LoadMoreComplete, base::Unretained(this))));
  }
}

void FeedStream::LoadMoreComplete(LoadMoreTask::Result result) {
  UpdateIsActivityLoggingEnabled();
  metrics_reporter_->OnLoadMore(result.final_status);
  surface_updater_->SetLoadingMore(false);
  std::vector<base::OnceCallback<void(bool)>> moved_callbacks =
      std::move(load_more_complete_callbacks_);
  bool success = result.final_status == LoadStreamStatus::kLoadedFromNetwork;
  for (auto& callback : moved_callbacks) {
    std::move(callback).Run(success);
  }

  if (result.loaded_new_content_from_network)
    prefetch_service_->NewSuggestionsAvailable();
}

void FeedStream::ExecuteOperations(
    std::vector<feedstore::DataOperation> operations) {
  if (!model_) {
    DLOG(ERROR) << "Calling ExecuteOperations before the model is loaded";
    return;
  }
  return model_->ExecuteOperations(std::move(operations));
}

EphemeralChangeId FeedStream::CreateEphemeralChange(
    std::vector<feedstore::DataOperation> operations) {
  if (!model_) {
    DLOG(ERROR) << "Calling CreateEphemeralChange before the model is loaded";
    return {};
  }
  metrics_reporter_->EphemeralStreamChange();
  return model_->CreateEphemeralChange(std::move(operations));
}

EphemeralChangeId FeedStream::CreateEphemeralChangeFromPackedData(
    base::StringPiece data) {
  feedpacking::DismissData msg;
  msg.ParseFromArray(data.data(), data.size());
  return CreateEphemeralChange(TranslateDismissData(clock_->Now(), msg));
}

bool FeedStream::CommitEphemeralChange(EphemeralChangeId id) {
  if (!model_)
    return false;
  return model_->CommitEphemeralChange(id);
}

bool FeedStream::RejectEphemeralChange(EphemeralChangeId id) {
  if (!model_)
    return false;
  metrics_reporter_->EphemeralStreamChangeRejected();
  return model_->RejectEphemeralChange(id);
}

void FeedStream::ProcessThereAndBackAgain(base::StringPiece data) {
  feedwire::ThereAndBackAgainData msg;
  msg.ParseFromArray(data.data(), data.size());
  if (msg.has_action_payload()) {
    feedwire::FeedAction action_msg;
    *action_msg.mutable_action_payload() = std::move(msg.action_payload());
    UploadAction(std::move(action_msg), /*upload_now=*/true,
                 base::BindOnce(&FeedStream::UploadActionsComplete,
                                base::Unretained(this)));
  }
}

void FeedStream::ProcessViewAction(base::StringPiece data) {
  if (!CanLogViews()) {
    return;
  }

  feedwire::FeedAction msg;
  msg.ParseFromArray(data.data(), data.size());
  UploadAction(std::move(msg), /*upload_now=*/false,
               base::BindOnce(&FeedStream::UploadActionsComplete,
                              base::Unretained(this)));
}

void FeedStream::UploadActionsComplete(UploadActionsTask::Result result) {
  PopulateDebugStreamData(result, *profile_prefs_);
}

void FeedStream::GetPrefetchSuggestions(
    base::OnceCallback<void(std::vector<offline_pages::PrefetchSuggestion>)>
        suggestions_callback) {
  task_queue_.AddTask(std::make_unique<GetPrefetchSuggestionsTask>(
      this, std::move(suggestions_callback)));
}

DebugStreamData FeedStream::GetDebugStreamData() {
  return ::feed::prefs::GetDebugStreamData(*profile_prefs_);
}

void FeedStream::ForceRefreshForDebugging() {
  // Avoid request throttling for debug refreshes.
  feed::prefs::SetThrottlerRequestCounts({}, *profile_prefs_);
  task_queue_.AddTask(
      std::make_unique<offline_pages::ClosureTask>(base::BindOnce(
          &FeedStream::ForceRefreshForDebuggingTask, base::Unretained(this))));
}

void FeedStream::ForceRefreshForDebuggingTask() {
  UnloadModel();
  store_->ClearStreamData(base::DoNothing());
  TriggerStreamLoad();
}

std::string FeedStream::DumpStateForDebugging() {
  std::stringstream ss;
  if (model_) {
    ss << "model loaded, " << model_->GetContentList().size() << " contents, "
       << "signed_in=" << model_->signed_in()
       << ", logging_enabled=" << model_->logging_enabled()
       << ", privacy_notice_fulfilled=" << model_->privacy_notice_fulfilled();
  }
  RequestSchedule schedule = prefs::GetRequestSchedule(*profile_prefs_);
  if (schedule.refresh_offsets.empty()) {
    ss << "No request schedule\n";
  } else {
    ss << "Request schedule reference " << schedule.anchor_time << '\n';
    for (base::TimeDelta entry : schedule.refresh_offsets) {
      ss << " fetch at " << entry << '\n';
    }
  }

  return ss.str();
}

base::Time FeedStream::GetLastFetchTime() {
  const base::Time fetch_time =
      profile_prefs_->GetTime(feed::prefs::kLastFetchAttemptTime);
  // Ignore impossible time values.
  if (fetch_time > clock_->Now())
    return base::Time();
  return fetch_time;
}

bool FeedStream::HasSurfaceAttached() const {
  return surface_updater_->HasSurfaceAttached();
}

void FeedStream::LoadModelForTesting(std::unique_ptr<StreamModel> model) {
  LoadModel(std::move(model));
}
offline_pages::TaskQueue* FeedStream::GetTaskQueueForTesting() {
  return &task_queue_;
}

void FeedStream::OnTaskQueueIsIdle() {
  if (idle_callback_)
    idle_callback_.Run();
}

void FeedStream::SetIdleCallbackForTesting(
    base::RepeatingClosure idle_callback) {
  idle_callback_ = idle_callback;
}

void FeedStream::OnStoreChange(StreamModel::StoreUpdate update) {
  if (!update.operations.empty()) {
    DCHECK(!update.update_request);
    store_->WriteOperations(update.sequence_number, update.operations);
  } else {
    DCHECK(update.update_request);
    if (update.overwrite_stream_data) {
      DCHECK_EQ(update.sequence_number, 0);
      store_->OverwriteStream(std::move(update.update_request),
                              base::DoNothing());
    } else {
      store_->SaveStreamUpdate(update.sequence_number,
                               std::move(update.update_request),
                               base::DoNothing());
    }
  }
}

LoadStreamStatus FeedStream::ShouldAttemptLoad(bool model_loading) {
  // Don't try to load the model if it's already loaded, or in the process of
  // being loaded. Because |ShouldAttemptLoad()| is used both before and during
  // the load process, we need to ignore this check when |model_loading| is
  // true.
  if (model_ || (!model_loading && model_loading_in_progress_))
    return LoadStreamStatus::kModelAlreadyLoaded;

  if (!IsArticlesListVisible())
    return LoadStreamStatus::kLoadNotAllowedArticlesListHidden;

  if (!IsFeedEnabledByEnterprisePolicy())
    return LoadStreamStatus::kLoadNotAllowedDisabledByEnterprisePolicy;

  if (!delegate_->IsEulaAccepted())
    return LoadStreamStatus::kLoadNotAllowedEulaNotAccepted;

  return LoadStreamStatus::kNoStatus;
}

LoadStreamStatus FeedStream::ShouldMakeFeedQueryRequest(bool is_load_more,
                                                        bool consume_quota) {
  if (!is_load_more) {
    // Time has passed since calling |ShouldAttemptLoad()|, call it again to
    // confirm we should still attempt loading.
    const LoadStreamStatus should_not_attempt_reason =
        ShouldAttemptLoad(/*model_loading=*/true);
    if (should_not_attempt_reason != LoadStreamStatus::kNoStatus) {
      return should_not_attempt_reason;
    }
  } else {
    // LoadMore requires a next page token.
    if (!model_ || model_->GetNextPageToken().empty()) {
      return LoadStreamStatus::kCannotLoadMoreNoNextPageToken;
    }
  }

  if (delegate_->IsOffline()) {
    return LoadStreamStatus::kCannotLoadFromNetworkOffline;
  }

  if (consume_quota &&
      !request_throttler_.RequestQuota(NetworkRequestType::kFeedQuery)) {
    return LoadStreamStatus::kCannotLoadFromNetworkThrottled;
  }

  return LoadStreamStatus::kNoStatus;
}

bool FeedStream::ShouldForceSignedOutFeedQueryRequest() const {
  return base::TimeTicks::Now() < signed_out_refreshes_until_;
}

RequestMetadata FeedStream::GetRequestMetadata() {
  RequestMetadata result;
  result.chrome_info = chrome_info_;
  result.display_metrics = delegate_->GetDisplayMetrics();
  result.language_tag = delegate_->GetLanguageTag();
  result.client_instance_id = GetClientInstanceId();
  return result;
}

void FeedStream::OnEulaAccepted() {
  if (surface_updater_->HasSurfaceAttached())
    TriggerStreamLoad();
}

void FeedStream::OnAllHistoryDeleted() {
  // Give sync the time to propagate the changes in history to the server.
  // In the interim, only send signed-out FeedQuery requests.
  signed_out_refreshes_until_ =
      tick_clock_->NowTicks() + kSuppressRefreshDuration;
  ClearAll();
}

void FeedStream::OnCacheDataCleared() {
  ClearAll();
}

void FeedStream::OnSignedIn() {
  // On sign-in, turn off activity logging. This avoids the possibility that we
  // send logs with the wrong user info attached, but may cause us to lose
  // buffered events.
  is_activity_logging_enabled_ = false;

  UpdateCanUploadActionsWithNoticeCard();

  ClearAll();
}

void FeedStream::OnSignedOut() {
  // On sign-out, turn off activity logging. This avoids the possibility that we
  // send logs with the wrong user info attached, but may cause us to lose
  // buffered events.
  is_activity_logging_enabled_ = false;

  UpdateCanUploadActionsWithNoticeCard();

  ClearAll();
}

void FeedStream::ExecuteRefreshTask() {
  // Schedule the next refresh attempt. If a new refresh schedule is returned
  // through this refresh, it will be overwritten.
  SetRequestSchedule(feed::prefs::GetRequestSchedule(*profile_prefs_));

  LoadStreamStatus do_not_attempt_reason = ShouldAttemptLoad();
  if (do_not_attempt_reason != LoadStreamStatus::kNoStatus) {
    BackgroundRefreshComplete(LoadStreamTask::Result(do_not_attempt_reason));
    return;
  }

  task_queue_.AddTask(std::make_unique<LoadStreamTask>(
      LoadStreamTask::LoadType::kBackgroundRefresh, this,
      base::BindOnce(&FeedStream::BackgroundRefreshComplete,
                     base::Unretained(this))));
}

void FeedStream::BackgroundRefreshComplete(LoadStreamTask::Result result) {
  metrics_reporter_->OnBackgroundRefresh(result.final_status);
  if (result.loaded_new_content_from_network && prefetch_service_)
    prefetch_service_->NewSuggestionsAvailable();

  refresh_task_scheduler_->RefreshTaskComplete();
}

void FeedStream::ClearAll() {
  metrics_reporter_->OnClearAll(clock_->Now() - GetLastFetchTime());

  task_queue_.AddTask(std::make_unique<ClearAllTask>(this));
}

void FeedStream::FinishClearAll() {
  prefs::ClearClientInstanceId(*profile_prefs_);
  metadata_.Populate(feedstore::Metadata());
  delegate_->ClearAll();
}

ImageFetchId FeedStream::FetchImage(
    const GURL& url,
    base::OnceCallback<void(NetworkResponse)> callback) {
  return image_fetcher_->Fetch(url, std::move(callback));
}

void FeedStream::CancelImageFetch(ImageFetchId id) {
  image_fetcher_->Cancel(id);
}

void FeedStream::UploadAction(
    feedwire::FeedAction action,
    bool upload_now,
    base::OnceCallback<void(UploadActionsTask::Result)> callback) {
  if (!delegate_->IsSignedIn()) {
    DLOG(WARNING)
        << "Called UploadActions while user is signed-out, dropping upload";
    return;
  }
  task_queue_.AddTask(std::make_unique<UploadActionsTask>(
      std::move(action), upload_now, this, std::move(callback)));
}

void FeedStream::LoadModel(std::unique_ptr<StreamModel> model) {
  DCHECK(!model_);
  model_ = std::move(model);
  model_->SetStoreObserver(this);
  surface_updater_->SetModel(model_.get());
  offline_page_spy_->SetModel(model_.get());
  ScheduleModelUnloadIfNoSurfacesAttached();
}

void FeedStream::SetRequestSchedule(RequestSchedule schedule) {
  const base::Time now = clock_->Now();
  base::Time run_time = NextScheduledRequestTime(now, &schedule);
  if (!run_time.is_null()) {
    refresh_task_scheduler_->EnsureScheduled(run_time - now);
  } else {
    refresh_task_scheduler_->Cancel();
  }
  feed::prefs::SetRequestSchedule(schedule, *profile_prefs_);
}

void FeedStream::UnloadModel() {
  // Note: This should only be called from a running Task, as some tasks assume
  // the model remains loaded.
  if (!model_)
    return;
  offline_page_spy_->SetModel(nullptr);
  surface_updater_->SetModel(nullptr);
  model_.reset();
}

void FeedStream::ReportOpenAction(const std::string& slice_id) {
  int index = surface_updater_->GetSliceIndexFromSliceId(slice_id);
  if (index >= 0)
    metrics_reporter_->OpenAction(index);
}
void FeedStream::ReportOpenVisitComplete(base::TimeDelta visit_time) {
  metrics_reporter_->OpenVisitComplete(visit_time);
}
void FeedStream::ReportOpenInNewTabAction(const std::string& slice_id) {
  int index = surface_updater_->GetSliceIndexFromSliceId(slice_id);
  if (index >= 0)
    metrics_reporter_->OpenInNewTabAction(index);
}
void FeedStream::ReportOpenInNewIncognitoTabAction() {
  metrics_reporter_->OpenInNewIncognitoTabAction();
}
void FeedStream::ReportSliceViewed(SurfaceId surface_id,
                                   const std::string& slice_id) {
  int index = surface_updater_->GetSliceIndexFromSliceId(slice_id);
  if (index >= 0) {
    UpdateShownSlicesUploadCondition(index);
    metrics_reporter_->ContentSliceViewed(surface_id, index);
  }
}
bool FeedStream::CanUploadActions() const {
  return can_upload_actions_with_notice_card_ ||
         !prefs::GetLastFetchHadNoticeCard(*profile_prefs_);
}
void FeedStream::SetLastStreamLoadHadNoticeCard(bool value) {
  prefs::SetLastFetchHadNoticeCard(*profile_prefs_, value);
}
bool FeedStream::HasReachedConditionsToUploadActionsWithNoticeCard() {
  if (base::FeatureList::IsEnabled(
          feed::kInterestFeedV2ClicksAndViewsConditionalUpload)) {
    return prefs::GetHasReachedClickAndViewActionsUploadConditions(
        *profile_prefs_);
  }
  // Consider the conditions as already reached to enable uploads when the
  // feature is disabled. This will also have the effect of not updating the
  // related pref.
  return true;
}
void FeedStream::DeclareHasReachedConditionsToUploadActionsWithNoticeCard() {
  if (base::FeatureList::IsEnabled(
          feed::kInterestFeedV2ClicksAndViewsConditionalUpload)) {
    prefs::SetHasReachedClickAndViewActionsUploadConditions(*profile_prefs_,
                                                            true);
  }
}
void FeedStream::UpdateShownSlicesUploadCondition(int viewed_slice_index) {
  constexpr int kShownSlicesThreshold = 2;

  // Don't take shown slices into consideration when the upload conditions has
  // already been reached.
  if (HasReachedConditionsToUploadActionsWithNoticeCard())
    return;

  if (viewed_slice_index + 1 >= kShownSlicesThreshold)
    DeclareHasReachedConditionsToUploadActionsWithNoticeCard();
}
bool FeedStream::CanLogViews() const {
  return CanUploadActions();
}
void FeedStream::UpdateCanUploadActionsWithNoticeCard() {
  can_upload_actions_with_notice_card_ =
      HasReachedConditionsToUploadActionsWithNoticeCard();
}
void FeedStream::ReportFeedViewed(SurfaceId surface_id) {
  metrics_reporter_->FeedViewed(surface_id);
}
void FeedStream::ReportSendFeedbackAction() {
  metrics_reporter_->SendFeedbackAction();
}
void FeedStream::ReportLearnMoreAction() {
  metrics_reporter_->LearnMoreAction();
}
void FeedStream::ReportDownloadAction() {
  metrics_reporter_->DownloadAction();
}
void FeedStream::ReportNavigationStarted() {
  metrics_reporter_->NavigationStarted();
}
void FeedStream::ReportPageLoaded() {
  metrics_reporter_->PageLoaded();
}
void FeedStream::ReportRemoveAction() {
  metrics_reporter_->RemoveAction();
}
void FeedStream::ReportNotInterestedInAction() {
  metrics_reporter_->NotInterestedInAction();
}
void FeedStream::ReportManageInterestsAction() {
  metrics_reporter_->ManageInterestsAction();
}
void FeedStream::ReportContextMenuOpened() {
  metrics_reporter_->ContextMenuOpened();
}
void FeedStream::ReportStreamScrolled(int distance_dp) {
  metrics_reporter_->StreamScrolled(distance_dp);
}
void FeedStream::ReportStreamScrollStart() {
  metrics_reporter_->StreamScrollStart();
}
void FeedStream::ReportTurnOnAction() {
  metrics_reporter_->TurnOnAction();
}
void FeedStream::ReportTurnOffAction() {
  metrics_reporter_->TurnOffAction();
}

}  // namespace feed
