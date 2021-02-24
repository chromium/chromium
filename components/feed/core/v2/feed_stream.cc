// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/feed_stream.h"

#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
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
#include "components/feed/core/v2/public/feed_stream_api.h"
#include "components/feed/core/v2/refresh_task_scheduler.h"
#include "components/feed/core/v2/scheduling.h"
#include "components/feed/core/v2/stream_model.h"
#include "components/feed/core/v2/surface_updater.h"
#include "components/feed/core/v2/tasks/clear_all_task.h"
#include "components/feed/core/v2/tasks/get_prefetch_suggestions_task.h"
#include "components/feed/core/v2/tasks/load_stream_task.h"
#include "components/feed/core/v2/tasks/prefetch_images_task.h"
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
  if (load_result.network_response_info) {
    debug_data.fetch_info = load_result.network_response_info;
  }
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

const std::string& FeedStream::Metadata::GetConsistencyToken() const {
  return metadata_.consistency_token();
}

void FeedStream::Metadata::SetConsistencyToken(std::string consistency_token) {
  metadata_.set_consistency_token(std::move(consistency_token));
  store_->WriteMetadata(metadata_, base::DoNothing());
}

const std::string& FeedStream::Metadata::GetSessionIdToken() const {
  return metadata_.session_id().token();
}

base::Time FeedStream::Metadata::GetSessionIdExpiryTime() const {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMilliseconds(
          metadata_.session_id().expiry_time_ms()));
}

void FeedStream::Metadata::SetSessionId(std::string token,
                                        base::Time expiry_time) {
  feedstore::Metadata::SessionID* session_id = metadata_.mutable_session_id();
  session_id->set_token(std::move(token));
  session_id->set_expiry_time_ms(
      expiry_time.ToDeltaSinceWindowsEpoch().InMilliseconds());
  store_->WriteMetadata(metadata_, base::DoNothing());
}

void FeedStream::Metadata::MaybeUpdateSessionId(
    base::Optional<std::string> token) {
  if (token && metadata_.session_id().token() != *token) {
    base::Time expiry_time =
        token->empty() ? base::Time()
                       : base::Time::Now() + GetFeedConfig().session_id_max_age;
    SetSessionId(*token, expiry_time);
  }
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

FeedStream::Stream::Stream() = default;
FeedStream::Stream::~Stream() = default;

FeedStream::FeedStream(RefreshTaskScheduler* refresh_task_scheduler,
                       MetricsReporter* metrics_reporter,
                       Delegate* delegate,
                       PrefService* profile_prefs,
                       FeedNetwork* feed_network,
                       ImageFetcher* image_fetcher,
                       FeedStore* feed_store,
                       PersistentKeyValueStoreImpl* persistent_key_value_store,
                       offline_pages::PrefetchService* prefetch_service,
                       offline_pages::OfflinePageModel* offline_page_model,
                       const ChromeInfo& chrome_info)
    : prefetch_service_(prefetch_service),
      refresh_task_scheduler_(refresh_task_scheduler),
      metrics_reporter_(metrics_reporter),
      delegate_(delegate),
      profile_prefs_(profile_prefs),
      feed_network_(feed_network),
      image_fetcher_(image_fetcher),
      store_(feed_store),
      persistent_key_value_store_(persistent_key_value_store),
      chrome_info_(chrome_info),
      task_queue_(this),
      request_throttler_(profile_prefs),
      metadata_(feed_store),
      notice_card_tracker_(profile_prefs) {
  static WireResponseTranslator default_translator;
  wire_response_translator_ = &default_translator;

  Stream& stream = GetStream(kInterestStream);
  offline_page_spy_ = std::make_unique<OfflinePageSpy>(
      stream.surface_updater.get(), offline_page_model);

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

FeedStream::Stream* FeedStream::FindStream(const StreamType& stream_type) {
  auto iter = streams_.find(stream_type);
  return (iter != streams_.end()) ? &iter->second : nullptr;
}

const FeedStream::Stream* FeedStream::FindStream(
    const StreamType& stream_type) const {
  return const_cast<FeedStream*>(this)->FindStream(stream_type);
}

FeedStream::Stream& FeedStream::GetStream(const StreamType& stream_type) {
  auto iter = streams_.find(stream_type);
  if (iter != streams_.end())
    return iter->second;
  FeedStream::Stream& new_stream = streams_[stream_type];
  new_stream.type = stream_type;
  new_stream.surface_updater =
      std::make_unique<SurfaceUpdater>(metrics_reporter_);
  return new_stream;
}

StreamModel* FeedStream::GetModel(const StreamType& stream_type) {
  Stream* stream = FindStream(stream_type);
  return stream ? stream->model.get() : nullptr;
}

void FeedStream::TriggerStreamLoad(const StreamType& stream_type) {
  Stream& stream = GetStream(stream_type);
  if (stream.model || stream.model_loading_in_progress)
    return;

  // If we should not load the stream, abort and send a zero-state update.
  LoadStreamStatus do_not_attempt_reason = ShouldAttemptLoad(stream_type);
  if (do_not_attempt_reason != LoadStreamStatus::kNoStatus) {
    InitialStreamLoadComplete(LoadStreamTask::Result(do_not_attempt_reason));
    return;
  }

  stream.model_loading_in_progress = true;

  stream.surface_updater->LoadStreamStarted();
  task_queue_.AddTask(std::make_unique<LoadStreamTask>(
      LoadStreamTask::LoadType::kInitialLoad, stream_type, this,
      base::BindOnce(&FeedStream::InitialStreamLoadComplete,
                     base::Unretained(this))));
}

void FeedStream::InitialStreamLoadComplete(LoadStreamTask::Result result) {
  Stream& stream = GetStream(result.stream_type);
  PopulateDebugStreamData(result, *profile_prefs_);
  metrics_reporter_->OnLoadStream(
      result.load_from_store_status, result.final_status,
      result.loaded_new_content_from_network, result.stored_content_age,
      std::move(result.latencies));
  UpdateIsActivityLoggingEnabled(result.stream_type);

  stream.model_loading_in_progress = false;
  stream.surface_updater->LoadStreamComplete(stream.model != nullptr,
                                             result.final_status);

  if (result.loaded_new_content_from_network) {
    if (result.stream_type.IsInterest())
      UpdateExperiments(result.experiments);
    if (prefetch_service_)
      prefetch_service_->NewSuggestionsAvailable();
  }
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

void FeedStream::UpdateIsActivityLoggingEnabled(const StreamType& stream_type) {
  Stream& stream = GetStream(stream_type);
  is_activity_logging_enabled_ =
      stream.model &&
      ((stream.model->signed_in() && stream.model->logging_enabled()) ||
       (!stream.model->signed_in() &&
        GetFeedConfig().send_signed_out_session_logs));
}

std::string FeedStream::GetSessionId() const {
  return GetMetadata()->GetSessionIdToken();
}

void FeedStream::PrefetchImage(const GURL& url) {
  delegate_->PrefetchImage(url);
}

void FeedStream::UpdateExperiments(Experiments experiments) {
  delegate_->RegisterExperiments(experiments);
  prefs::SetExperiments(experiments, *profile_prefs_);
}

void FeedStream::AttachSurface(SurfaceInterface* surface) {
  metrics_reporter_->SurfaceOpened(surface->GetSurfaceId());
  Stream& stream = GetStream(surface->GetStreamType());
  // Skip normal processing when overriding stream data from the internals page.
  if (forced_stream_update_for_debugging_.updated_slices_size() > 0) {
    stream.surface_updater->SurfaceAdded(surface);
    surface->StreamUpdate(forced_stream_update_for_debugging_);
    return;
  }

  TriggerStreamLoad(surface->GetStreamType());
  stream.surface_updater->SurfaceAdded(surface);

  // Cancel any scheduled model unload task.
  ++stream.unload_on_detach_sequence_number;
  UpdateCanUploadActionsWithNoticeCard();
}

void FeedStream::DetachSurface(SurfaceInterface* surface) {
  Stream& stream = GetStream(surface->GetStreamType());
  metrics_reporter_->SurfaceClosed(surface->GetSurfaceId());
  stream.surface_updater->SurfaceRemoved(surface);
  UpdateCanUploadActionsWithNoticeCard();
  ScheduleModelUnloadIfNoSurfacesAttached(surface->GetStreamType());
}

void FeedStream::ScheduleModelUnloadIfNoSurfacesAttached(
    const StreamType& stream_type) {
  Stream& stream = GetStream(stream_type);
  if (stream.surface_updater->HasSurfaceAttached())
    return;

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FeedStream::AddUnloadModelIfNoSurfacesAttachedTask,
                     GetWeakPtr(), stream.type,
                     stream.unload_on_detach_sequence_number),
      GetFeedConfig().model_unload_timeout);
}

void FeedStream::AddUnloadModelIfNoSurfacesAttachedTask(
    const StreamType& stream_type,
    int sequence_number) {
  Stream& stream = GetStream(stream_type);
  // Don't continue if unload_on_detach_sequence_number_ has changed.
  if (stream.unload_on_detach_sequence_number != sequence_number)
    return;

  task_queue_.AddTask(std::make_unique<offline_pages::ClosureTask>(
      base::BindOnce(&FeedStream::UnloadModelIfNoSurfacesAttachedTask,
                     base::Unretained(this), stream_type)));
}

void FeedStream::UnloadModelIfNoSurfacesAttachedTask(
    const StreamType& stream_type) {
  Stream& stream = GetStream(stream_type);
  if (stream.surface_updater->HasSurfaceAttached())
    return;
  UnloadModel(stream_type);
}

bool FeedStream::IsArticlesListVisible() {
  return profile_prefs_->GetBoolean(prefs::kArticlesListVisible);
}

std::string FeedStream::GetClientInstanceId() const {
  return prefs::GetClientInstanceId(*profile_prefs_);
}

bool FeedStream::IsFeedEnabledByEnterprisePolicy() {
  return profile_prefs_->GetBoolean(prefs::kEnableSnippets);
}

void FeedStream::LoadMore(const SurfaceInterface& surface,
                          base::OnceCallback<void(bool)> callback) {
  Stream& stream = GetStream(surface.GetStreamType());
  if (!stream.model) {
    DLOG(ERROR) << "Ignoring LoadMore() before the model is loaded";
    return std::move(callback).Run(false);
  }
  // We want to abort early to avoid showing a loading spinner if it's not
  // necessary.
  if (ShouldMakeFeedQueryRequest(surface.GetStreamType(), /*is_load_more=*/true,
                                 /*consume_quota=*/false) !=
      LoadStreamStatus::kNoStatus) {
    return std::move(callback).Run(false);
  }

  metrics_reporter_->OnLoadMoreBegin(surface.GetSurfaceId());
  stream.surface_updater->SetLoadingMore(true);

  // Have at most one in-flight LoadMore() request. Send the result to all
  // requestors.
  load_more_complete_callbacks_.push_back(std::move(callback));
  if (load_more_complete_callbacks_.size() == 1) {
    task_queue_.AddTask(std::make_unique<LoadMoreTask>(
        surface.GetStreamType(), this,
        base::BindOnce(&FeedStream::LoadMoreComplete, base::Unretained(this))));
  }
}

void FeedStream::LoadMoreComplete(LoadMoreTask::Result result) {
  UpdateIsActivityLoggingEnabled(result.stream_type);
  Stream& stream = GetStream(result.stream_type);
  metrics_reporter_->OnLoadMore(result.final_status);
  stream.surface_updater->SetLoadingMore(false);
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
    const StreamType& stream_type,
    std::vector<feedstore::DataOperation> operations) {
  StreamModel* model = GetModel(stream_type);
  if (!model) {
    DLOG(ERROR) << "Calling ExecuteOperations before the model is loaded";
    return;
  }
  return model->ExecuteOperations(std::move(operations));
}

EphemeralChangeId FeedStream::CreateEphemeralChange(
    const StreamType& stream_type,
    std::vector<feedstore::DataOperation> operations) {
  StreamModel* model = GetModel(stream_type);
  if (!model) {
    DLOG(ERROR) << "Calling CreateEphemeralChange before the model is loaded";
    return {};
  }
  metrics_reporter_->OtherUserAction(FeedUserActionType::kEphemeralChange);
  return model->CreateEphemeralChange(std::move(operations));
}

EphemeralChangeId FeedStream::CreateEphemeralChangeFromPackedData(
    const StreamType& stream_type,
    base::StringPiece data) {
  feedpacking::DismissData msg;
  msg.ParseFromArray(data.data(), data.size());
  return CreateEphemeralChange(stream_type,
                               TranslateDismissData(base::Time::Now(), msg));
}

bool FeedStream::CommitEphemeralChange(const StreamType& stream_type,
                                       EphemeralChangeId id) {
  StreamModel* model = GetModel(stream_type);
  if (!model)
    return false;
  metrics_reporter_->OtherUserAction(
      FeedUserActionType::kEphemeralChangeCommited);
  return model->CommitEphemeralChange(id);
}

bool FeedStream::RejectEphemeralChange(const StreamType& stream_type,
                                       EphemeralChangeId id) {
  StreamModel* model = GetModel(stream_type);
  if (!model)
    return false;
  metrics_reporter_->OtherUserAction(
      FeedUserActionType::kEphemeralChangeRejected);
  return model->RejectEphemeralChange(id);
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
  UnloadModel(kInterestStream);
  store_->ClearStreamData(kInterestStream, base::DoNothing());
  TriggerStreamLoad(kInterestStream);

  if (base::FeatureList::IsEnabled(kWebFeed)) {
    UnloadModel(kWebFeedStream);
    store_->ClearStreamData(kWebFeedStream, base::DoNothing());
    TriggerStreamLoad(kWebFeedStream);
  }
}

std::string FeedStream::DumpStateForDebugging() {
  Stream& stream = GetStream(kInterestStream);
  std::stringstream ss;
  if (stream.model) {
    ss << "model loaded, " << stream.model->GetContentList().size()
       << " contents, "
       << "signed_in=" << stream.model->signed_in()
       << ", logging_enabled=" << stream.model->logging_enabled()
       << ", privacy_notice_fulfilled="
       << stream.model->privacy_notice_fulfilled();
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

void FeedStream::SetForcedStreamUpdateForDebugging(
    const feedui::StreamUpdate& stream_update) {
  forced_stream_update_for_debugging_ = stream_update;
}

base::Time FeedStream::GetLastFetchTime() {
  const base::Time fetch_time =
      profile_prefs_->GetTime(feed::prefs::kLastFetchAttemptTime);
  // Ignore impossible time values.
  if (fetch_time > base::Time::Now())
    return base::Time();
  return fetch_time;
}

void FeedStream::LoadModelForTesting(const StreamType& stream_type,
                                     std::unique_ptr<StreamModel> model) {
  LoadModel(stream_type, std::move(model));
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
    store_->WriteOperations(update.stream_type, update.sequence_number,
                            update.operations);
  } else {
    DCHECK(update.update_request);
    if (update.overwrite_stream_data) {
      DCHECK_EQ(update.sequence_number, 0);
      store_->OverwriteStream(update.stream_type,
                              std::move(update.update_request),
                              base::DoNothing());
    } else {
      store_->SaveStreamUpdate(update.stream_type, update.sequence_number,
                               std::move(update.update_request),
                               base::DoNothing());
    }
  }
}

LoadStreamStatus FeedStream::ShouldAttemptLoad(const StreamType& stream_type,
                                               bool model_loading) {
  // Don't try to load the model if it's already loaded, or in the process of
  // being loaded. Because |ShouldAttemptLoad()| is used both before and during
  // the load process, we need to ignore this check when |model_loading| is
  // true.
  Stream& stream = GetStream(stream_type);
  if (stream.model || (!model_loading && stream.model_loading_in_progress))
    return LoadStreamStatus::kModelAlreadyLoaded;

  if (!IsArticlesListVisible())
    return LoadStreamStatus::kLoadNotAllowedArticlesListHidden;

  if (!IsFeedEnabledByEnterprisePolicy())
    return LoadStreamStatus::kLoadNotAllowedDisabledByEnterprisePolicy;

  if (!delegate_->IsEulaAccepted())
    return LoadStreamStatus::kLoadNotAllowedEulaNotAccepted;

  return LoadStreamStatus::kNoStatus;
}

bool FeedStream::MissedLastRefresh() {
  RequestSchedule schedule = prefs::GetRequestSchedule(*profile_prefs_);
  if (schedule.refresh_offsets.empty())
    return false;
  base::Time scheduled_time =
      schedule.anchor_time + schedule.refresh_offsets[0];
  return scheduled_time < base::Time::Now();
}

LoadStreamStatus FeedStream::ShouldMakeFeedQueryRequest(
    const StreamType& stream_type,
    bool is_load_more,
    bool consume_quota) {
  Stream& stream = GetStream(stream_type);
  if (!is_load_more) {
    // Time has passed since calling |ShouldAttemptLoad()|, call it again to
    // confirm we should still attempt loading.
    const LoadStreamStatus should_not_attempt_reason =
        ShouldAttemptLoad(stream_type, /*model_loading=*/true);
    if (should_not_attempt_reason != LoadStreamStatus::kNoStatus) {
      return should_not_attempt_reason;
    }
  } else {
    // LoadMore requires a next page token.
    if (!stream.model || stream.model->GetNextPageToken().empty()) {
      return LoadStreamStatus::kCannotLoadMoreNoNextPageToken;
    }
  }

  if (delegate_->IsOffline()) {
    return LoadStreamStatus::kCannotLoadFromNetworkOffline;
  }

  if (consume_quota && !request_throttler_.RequestQuota(
                           !is_load_more ? NetworkRequestType::kFeedQuery
                                         : NetworkRequestType::kNextPage)) {
    return LoadStreamStatus::kCannotLoadFromNetworkThrottled;
  }

  return LoadStreamStatus::kNoStatus;
}

bool FeedStream::ShouldForceSignedOutFeedQueryRequest() const {
  return base::TimeTicks::Now() < signed_out_refreshes_until_;
}

RequestMetadata FeedStream::GetRequestMetadata(const StreamType& stream_type,
                                               bool is_for_next_page) const {
  const Stream* stream = FindStream(stream_type);
  DCHECK(stream);
  RequestMetadata result;
  result.chrome_info = chrome_info_;
  result.display_metrics = delegate_->GetDisplayMetrics();
  result.language_tag = delegate_->GetLanguageTag();
  result.notice_card_acknowledged =
      notice_card_tracker_.HasAcknowledgedNoticeCard();

  if (is_for_next_page) {
    // If we are continuing an existing feed, use whatever session continuity
    // mechanism is currently associated with the stream: client-instance-id
    // for signed-in feed, session_id token for signed-out.
    DCHECK(stream->model);
    if (stream->model->signed_in()) {
      result.client_instance_id = GetClientInstanceId();
    } else {
      result.session_id = GetMetadata()->GetSessionIdToken();
    }
  } else {
    // The request is for the first page of the feed. Use client_instance_id
    // for signed in requests and session_id token (if any, and not expired)
    // for signed-out.
    if (delegate_->IsSignedIn() && !ShouldForceSignedOutFeedQueryRequest()) {
      result.client_instance_id = GetClientInstanceId();
    } else if (!GetMetadata()->GetSessionIdToken().empty() &&
               GetMetadata()->GetSessionIdExpiryTime() > base::Time::Now()) {
      result.session_id = GetMetadata()->GetSessionIdToken();
    }
  }

  DCHECK(result.session_id.empty() || result.client_instance_id.empty());

  return result;
}

void FeedStream::OnEulaAccepted() {
  for (auto& item : streams_) {
    if (item.second.surface_updater->HasSurfaceAttached()) {
      TriggerStreamLoad(item.second.type);
    }
  }
}

void FeedStream::OnAllHistoryDeleted() {
  // Give sync the time to propagate the changes in history to the server.
  // In the interim, only send signed-out FeedQuery requests.
  signed_out_refreshes_until_ =
      base::TimeTicks::Now() + kSuppressRefreshDuration;
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

  LoadStreamStatus do_not_attempt_reason = ShouldAttemptLoad(kInterestStream);
  if (do_not_attempt_reason != LoadStreamStatus::kNoStatus) {
    BackgroundRefreshComplete(LoadStreamTask::Result(do_not_attempt_reason));
    return;
  }

  task_queue_.AddTask(std::make_unique<LoadStreamTask>(
      LoadStreamTask::LoadType::kBackgroundRefresh, kInterestStream, this,
      base::BindOnce(&FeedStream::BackgroundRefreshComplete,
                     base::Unretained(this))));
}

void FeedStream::BackgroundRefreshComplete(LoadStreamTask::Result result) {
  metrics_reporter_->OnBackgroundRefresh(result.final_status);
  if (result.loaded_new_content_from_network) {
    if (result.stream_type.IsInterest())
      UpdateExperiments(result.experiments);
    if (prefetch_service_)
      prefetch_service_->NewSuggestionsAvailable();
  }

  // Add prefetch images to task queue without waiting to finish
  // since we treat them as best-effort.
  task_queue_.AddTask(std::make_unique<PrefetchImagesTask>(this));

  refresh_task_scheduler_->RefreshTaskComplete();
}

void FeedStream::ClearAll() {
  metrics_reporter_->OnClearAll(base::Time::Now() - GetLastFetchTime());

  task_queue_.AddTask(std::make_unique<ClearAllTask>(this));
}

void FeedStream::FinishClearAll() {
  prefs::ClearClientInstanceId(*profile_prefs_);
  // Clear any experiments stored.
  prefs::SetExperiments({}, *profile_prefs_);
  metadata_.Populate(feedstore::Metadata());
  delegate_->ClearAll();

  for (auto& item : streams_) {
    if (item.second.surface_updater->HasSurfaceAttached()) {
      TriggerStreamLoad(item.second.type);
    }
  }
}

ImageFetchId FeedStream::FetchImage(
    const GURL& url,
    base::OnceCallback<void(NetworkResponse)> callback) {
  return image_fetcher_->Fetch(url, std::move(callback));
}

PersistentKeyValueStoreImpl* FeedStream::GetPersistentKeyValueStore() {
  return persistent_key_value_store_;
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

void FeedStream::LoadModel(const StreamType& stream_type,
                           std::unique_ptr<StreamModel> model) {
  Stream& stream = GetStream(stream_type);
  DCHECK(!stream.model);
  stream.model = std::move(model);
  stream.model->SetStreamType(stream_type);
  stream.model->SetStoreObserver(this);
  stream.surface_updater->SetModel(stream.model.get());
  if (stream.type.IsInterest()) {
    offline_page_spy_->SetModel(stream.model.get());
  }
  ScheduleModelUnloadIfNoSurfacesAttached(stream_type);
}

void FeedStream::SetRequestSchedule(RequestSchedule schedule) {
  const base::Time now = base::Time::Now();
  base::Time run_time = NextScheduledRequestTime(now, &schedule);
  if (!run_time.is_null()) {
    refresh_task_scheduler_->EnsureScheduled(run_time - now);
  } else {
    refresh_task_scheduler_->Cancel();
  }
  feed::prefs::SetRequestSchedule(schedule, *profile_prefs_);
}

void FeedStream::UnloadModel(const StreamType& stream_type) {
  // Note: This should only be called from a running Task, as some tasks assume
  // the model remains loaded.
  Stream* stream = FindStream(stream_type);
  if (!stream || !stream->model)
    return;
  if (stream_type.IsInterest()) {
    offline_page_spy_->SetModel(nullptr);
  }
  stream->surface_updater->SetModel(nullptr);
  stream->model.reset();
}

void FeedStream::UnloadModels() {
  for (auto& item : streams_) {
    UnloadModel(item.second.type);
  }
}

void FeedStream::ReportOpenAction(const StreamType& stream_type,
                                  const std::string& slice_id) {
  Stream& stream = GetStream(stream_type);

  int index = stream.surface_updater->GetSliceIndexFromSliceId(slice_id);
  if (index < 0)
    index = MetricsReporter::kUnknownCardIndex;
  metrics_reporter_->OpenAction(index);
  if (stream_type.IsInterest()) {
    notice_card_tracker_.OnOpenAction(index);
  }
}
void FeedStream::ReportOpenVisitComplete(base::TimeDelta visit_time) {
  metrics_reporter_->OpenVisitComplete(visit_time);
}
void FeedStream::ReportOpenInNewTabAction(const StreamType& stream_type,
                                          const std::string& slice_id) {
  Stream& stream = GetStream(stream_type);
  int index = stream.surface_updater->GetSliceIndexFromSliceId(slice_id);
  if (index < 0)
    index = MetricsReporter::kUnknownCardIndex;
  metrics_reporter_->OpenInNewTabAction(index);
  if (stream_type.IsInterest()) {
    notice_card_tracker_.OnOpenAction(index);
  }
}
void FeedStream::ReportSliceViewed(SurfaceId surface_id,
                                   const StreamType& stream_type,
                                   const std::string& slice_id) {
  Stream& stream = GetStream(stream_type);
  int index = stream.surface_updater->GetSliceIndexFromSliceId(slice_id);
  if (index >= 0) {
    if (stream_type.IsInterest()) {
      UpdateShownSlicesUploadCondition(index);
      notice_card_tracker_.OnSliceViewed(index);
    }
    metrics_reporter_->ContentSliceViewed(surface_id, index);
  }
}
// TODO(crbug/1147237): Rename this method and related members?
bool FeedStream::CanUploadActions() const {
  // TODO(crbug/1152592): Determine notice card behavior with web feeds.
  return can_upload_actions_with_notice_card_ ||
         !prefs::GetLastFetchHadNoticeCard(*profile_prefs_);
}
void FeedStream::SetLastStreamLoadHadNoticeCard(bool value) {
  // TODO(crbug/1152592): Determine notice card behavior with web feeds.
  prefs::SetLastFetchHadNoticeCard(*profile_prefs_, value);
}
bool FeedStream::HasReachedConditionsToUploadActionsWithNoticeCard() {
  // TODO(crbug/1152592): Determine notice card behavior with web feeds.
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
  // TODO(crbug/1152592): Determine notice card behavior with web feeds.
  if (base::FeatureList::IsEnabled(
          feed::kInterestFeedV2ClicksAndViewsConditionalUpload)) {
    prefs::SetHasReachedClickAndViewActionsUploadConditions(*profile_prefs_,
                                                            true);
  }
}
void FeedStream::UpdateShownSlicesUploadCondition(int viewed_slice_index) {
  constexpr int kShownSlicesThreshold = 2;

  // TODO(crbug/1152592): Determine notice card behavior with web feeds.
  Stream& stream = GetStream(kInterestStream);
  if (!stream.model) {
    DLOG(ERROR) << "Model was unloaded while handling a viewed slice";
    return;
  }

  // Don't take shown slices into consideration when the upload conditions has
  // already been reached.
  if (HasReachedConditionsToUploadActionsWithNoticeCard()) {
    return;
  }

  if (!stream.model->signed_in()) {
    return;
  }

  if (viewed_slice_index + 1 >= kShownSlicesThreshold)
    DeclareHasReachedConditionsToUploadActionsWithNoticeCard();
}
bool FeedStream::CanLogViews() const {
  // TODO(crbug/1152592): Determine notice card behavior with web feeds.
  return CanUploadActions();
}
void FeedStream::UpdateCanUploadActionsWithNoticeCard() {
  // TODO(crbug/1152592): Determine notice card behavior with web feeds.
  can_upload_actions_with_notice_card_ =
      HasReachedConditionsToUploadActionsWithNoticeCard();
}
void FeedStream::ReportFeedViewed(SurfaceId surface_id) {
  metrics_reporter_->FeedViewed(surface_id);
}
void FeedStream::ReportPageLoaded() {
  metrics_reporter_->PageLoaded();
}
void FeedStream::ReportStreamScrolled(int distance_dp) {
  metrics_reporter_->StreamScrolled(distance_dp);
}
void FeedStream::ReportStreamScrollStart() {
  metrics_reporter_->StreamScrollStart();
}
void FeedStream::ReportOtherUserAction(FeedUserActionType action_type) {
  metrics_reporter_->OtherUserAction(action_type);
}

}  // namespace feed
