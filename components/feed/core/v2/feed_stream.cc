// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/feed_stream.h"

#include <algorithm>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/buildflag.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/ui.pb.h"
#include "components/feed/core/proto/v2/wire/content_id.pb.h"
#include "components/feed/core/proto/v2/wire/reliability_logging_enums.pb.h"
#include "components/feed/core/proto/v2/wire/there_and_back_again_data.pb.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/feed_store.h"
#include "components/feed/core/v2/feed_stream_surface.h"
#include "components/feed/core/v2/feedstore_util.h"
#include "components/feed/core/v2/image_fetcher.h"
#include "components/feed/core/v2/ios_shared_prefs.h"
#include "components/feed/core/v2/metrics_reporter.h"
#include "components/feed/core/v2/prefs.h"
#include "components/feed/core/v2/protocol_translator.h"
#include "components/feed/core/v2/public/common_enums.h"
#include "components/feed/core/v2/public/feed_api.h"
#include "components/feed/core/v2/public/feed_service.h"
#include "components/feed/core/v2/public/logging_parameters.h"
#include "components/feed/core/v2/public/refresh_task_scheduler.h"
#include "components/feed/core/v2/public/reliability_logging_bridge.h"
#include "components/feed/core/v2/public/stream_type.h"
#include "components/feed/core/v2/public/types.h"
#include "components/feed/core/v2/public/unread_content_observer.h"
#include "components/feed/core/v2/scheduling.h"
#include "components/feed/core/v2/stream/unread_content_notifier.h"
#include "components/feed/core/v2/stream_model.h"
#include "components/feed/core/v2/surface_updater.h"
#include "components/feed/core/v2/tasks/clear_all_task.h"
#include "components/feed/core/v2/tasks/clear_stream_task.h"
#include "components/feed/core/v2/tasks/load_stream_task.h"
#include "components/feed/core/v2/tasks/prefetch_images_task.h"
#include "components/feed/core/v2/tasks/upload_actions_task.h"
#include "components/feed/core/v2/tasks/wait_for_store_initialize_task.h"
#include "components/feed/core/v2/types.h"
#include "components/feed/core/v2/web_feed_subscription_coordinator.h"
#include "components/feed/feed_feature_list.h"
#include "components/offline_pages/task/closure_task.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"

namespace feed {
namespace {
constexpr size_t kMaxRecentFeedNavigations = 10;
constexpr base::TimeDelta kFeedCloseRefreshDelay = base::Minutes(30);

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

// Will check all sources of ordering setting and always return a valid result.
ContentOrder GetValidWebFeedContentOrder(const PrefService& pref_service) {
  // First priority is the prefs stored order choice.
  ContentOrder pref_order = prefs::GetWebFeedContentOrder(pref_service);
  if (pref_order != ContentOrder::kUnspecified)
    return pref_order;
  // Defaults to grouped, encompassing finch_order == "grouped".
  return ContentOrder::kGrouped;
}

LoadType RequestScheduleTypeToLoadType(RequestSchedule::Type type) {
  switch (type) {
    case RequestSchedule::Type::kFeedCloseRefresh:
      return LoadType::kFeedCloseBackgroundRefresh;
    case RequestSchedule::Type::kScheduledRefresh:
    default:
      return LoadType::kBackgroundRefresh;
  }
}

}  // namespace

FeedStream::Stream::Stream(const StreamType& stream_type)
    : type(stream_type), surfaces(stream_type) {}
FeedStream::Stream::~Stream() = default;

FeedStream::FeedStream(RefreshTaskScheduler* refresh_task_scheduler,
                       MetricsReporter* metrics_reporter,
                       Delegate* delegate,
                       PrefService* profile_prefs,
                       FeedNetwork* feed_network,
                       ImageFetcher* image_fetcher,
                       FeedStore* feed_store,
                       PersistentKeyValueStoreImpl* persistent_key_value_store,
                       TemplateURLService* template_url_service,
                       const ChromeInfo& chrome_info)
    : refresh_task_scheduler_(refresh_task_scheduler),
      metrics_reporter_(metrics_reporter),
      delegate_(delegate),
      profile_prefs_(profile_prefs),
      feed_network_(feed_network),
      image_fetcher_(image_fetcher),
      store_(feed_store),
      persistent_key_value_store_(persistent_key_value_store),
      template_url_service_(template_url_service),
      chrome_info_(chrome_info),
      task_queue_(this),
      request_throttler_(profile_prefs),
      privacy_notice_card_tracker_(profile_prefs),
      info_card_tracker_(profile_prefs),
      user_actions_collector_(profile_prefs) {
  DCHECK(persistent_key_value_store_);
  DCHECK(feed_network_);
  DCHECK(profile_prefs_);
  DCHECK(metrics_reporter);
  DCHECK(image_fetcher_);

  static WireResponseTranslator default_translator;
  wire_response_translator_ = &default_translator;
  metrics_reporter_->Initialize(this);

  base::RepeatingClosure preference_change_callback =
      base::BindRepeating(&FeedStream::EnabledPreferencesChanged, GetWeakPtr());
  snippets_enabled_by_policy_.Init(prefs::kEnableSnippets, profile_prefs,
                                   preference_change_callback);
  articles_list_visible_.Init(prefs::kArticlesListVisible, profile_prefs,
                              preference_change_callback);
  snippets_enabled_by_dse_.Init(prefs::kEnableSnippetsByDse, profile_prefs,
                                preference_change_callback);
  has_stored_data_.Init(feed::prefs::kHasStoredData, profile_prefs);
  signin_allowed_.Init(
      ::prefs::kSigninAllowed, profile_prefs,
      base::BindRepeating(&FeedStream::ClearAll, GetWeakPtr()));
  web_feed_subscription_coordinator_ =
      std::make_unique<WebFeedSubscriptionCoordinator>(delegate, this);

  // Inserting this task first ensures that |store_| is initialized before
  // it is used.
  task_queue_.AddTask(FROM_HERE,
                      std::make_unique<WaitForStoreInitializeTask>(
                          store_, this,
                          base::BindOnce(&FeedStream::InitializeComplete,
                                         base::Unretained(this))));
  EnabledPreferencesChanged();
}

FeedStream::~FeedStream() = default;

WebFeedSubscriptionCoordinator& FeedStream::subscriptions() {
  return *web_feed_subscription_coordinator_;
}

FeedStream::Stream* FeedStream::FindStream(const StreamType& stream_type) {
  auto iter = streams_.find(stream_type);
  return (iter != streams_.end()) ? &iter->second : nullptr;
}

const FeedStream::Stream* FeedStream::FindStream(
    const StreamType& stream_type) const {
  return const_cast<FeedStream*>(this)->FindStream(stream_type);
}

FeedStream::Stream* FeedStream::FindStream(SurfaceId surface_id) {
  FeedStreamSurface* surface = FindSurface(surface_id);
  if (surface) {
    return FindStream(surface->GetStreamType());
  }
  return nullptr;
}

FeedStreamSurface* FeedStream::FindSurface(SurfaceId surface_id) {
  // There should only ever be a handful of surfaces at once, so linear search
  // is fine.
  for (FeedStreamSurface& surface : all_surfaces_) {
    if (surface.GetSurfaceId() == surface_id) {
      return &surface;
    }
  }
  return nullptr;
}

FeedStream::Stream& FeedStream::GetStream(const StreamType& stream_type) {
  CHECK(stream_type.IsValid());
  auto iter = streams_.find(stream_type);
  if (iter != streams_.end())
    return iter->second;
  FeedStream::Stream& new_stream =
      streams_.emplace(stream_type, stream_type).first->second;
  new_stream.surface_updater = std::make_unique<SurfaceUpdater>(
      metrics_reporter_, &global_datastore_slice_, &new_stream.surfaces);
  new_stream.surfaces.AddObserver(new_stream.surface_updater.get());
  return new_stream;
}

StreamModel* FeedStream::GetModel(const StreamType& stream_type) {
  Stream* stream = FindStream(stream_type);
  return stream ? stream->model.get() : nullptr;
}

StreamModel* FeedStream::GetModel(SurfaceId surface_id) {
  Stream* stream = FindStream(surface_id);
  return stream ? stream->model.get() : nullptr;
}

feedwire::DiscoverLaunchResult FeedStream::TriggerStreamLoad(
    const StreamType& stream_type,
    SingleWebFeedEntryPoint entry_point) {
  Stream& stream = GetStream(stream_type);
  if (stream.model || stream.model_loading_in_progress)
    return feedwire::DiscoverLaunchResult::CARDS_UNSPECIFIED;

  // If we should not load the stream, abort and send a zero-state update.
  LaunchResult do_not_attempt_reason =
      ShouldAttemptLoad(stream_type, LoadType::kInitialLoad);
  if (do_not_attempt_reason.load_stream_status != LoadStreamStatus::kNoStatus) {
    LoadStreamTask::Result result(stream_type,
                                  do_not_attempt_reason.load_stream_status);
    result.launch_result = do_not_attempt_reason.launch_result;
    StreamLoadComplete(std::move(result));
    return do_not_attempt_reason.launch_result;
  }

  stream.model_loading_in_progress = true;

  stream.surface_updater->LoadStreamStarted(/*manual_refreshing=*/false);
  LoadStreamTask::Options options;
  options.stream_type = stream_type;
  options.single_feed_entry_point = entry_point;
  if (!loaded_after_start_ &&
      base::FeatureList::IsEnabled(kRefreshFeedOnRestart)) {
    options.refresh_even_when_not_stale = true;
  }
  task_queue_.AddTask(FROM_HERE,
                      std::make_unique<LoadStreamTask>(
                          options, this,
                          base::BindOnce(&FeedStream::StreamLoadComplete,
                                         base::Unretained(this))));
  return feedwire::DiscoverLaunchResult::CARDS_UNSPECIFIED;
}

void FeedStream::InitializeComplete(WaitForStoreInitializeTask::Result result) {
  metadata_ = *std::move(result.startup_data.metadata);
  for (const feedstore::StreamData& stream_data :
       result.startup_data.stream_data) {
    StreamType stream_type =
        feedstore::StreamTypeFromKey(stream_data.stream_key());
    if (stream_type.IsValid()) {
      GetStream(stream_type).content_ids =
          feedstore::GetContentIds(stream_data);
    }
    if (stream_type.IsForYou()) {
      GetStream(stream_type).viewed_content_hashes =
          feedstore::GetViewedContentHashes(metadata_, stream_type);
    }
  }

  metadata_populated_ = true;
  metrics_reporter_->OnMetadataInitialized(
      IsFeedEnabledByEnterprisePolicy(), IsArticlesListVisible(), IsSignedIn(),
      IsFeedEnabled(), metadata_);

  web_feed_subscription_coordinator_->Populate(result.web_feed_startup_data);

  for (const feedstore::StreamData& stream_data :
       result.startup_data.stream_data) {
    StreamType stream_type =
        feedstore::StreamTypeFromKey(stream_data.stream_key());
    if (stream_type.IsValid())
      MaybeNotifyHasUnreadContent(stream_type);
  }

  if (!IsEnabledAndVisible() && has_stored_data_.GetValue()) {
    ClearAll();
  }
}

void FeedStream::StreamLoadComplete(LoadStreamTask::Result result) {
  DCHECK(result.load_type == LoadType::kInitialLoad ||
         result.load_type == LoadType::kManualRefresh);

  loaded_after_start_ = true;

  Stream& stream = GetStream(result.stream_type);
  if (result.load_type == LoadType::kManualRefresh)
    UnloadModel(result.stream_type);

  if (result.update_request) {
    auto model = std::make_unique<StreamModel>(
        &stream_model_context_,
        feed::MakeLoggingParameters(prefs::GetClientInstanceId(*profile_prefs_),

                                    *result.update_request));
    model->Update(std::move(result.update_request));

    if (!model->HasVisibleContent() &&
        result.launch_result ==
            feedwire::DiscoverLaunchResult::CARDS_UNSPECIFIED) {
      result.launch_result =
          feedwire::DiscoverLaunchResult::NO_CARDS_RESPONSE_ERROR_ZERO_CARDS;
    }

    LoadModel(result.stream_type, std::move(model));
  }

  if (result.request_schedule)
    SetRequestSchedule(stream.type, *result.request_schedule);

  ContentStats content_stats;
  if (stream.model)
    content_stats = stream.model->GetContentStats();

  // stream_metadata is an optional because in some scenarios StreamLoadComplete
  // is called bypassing the task queue. In this case WaitForStoreInitialize
  // task is not completed first and the metadata is not populated. Thus, we
  // dont need to track content_lifetime.
  std::optional<feedstore::Metadata::StreamMetadata> stream_metadata;
  if (metadata_populated_) {
    feedstore::Metadata metadata = GetMetadata();
    stream_metadata =
        feedstore::MetadataForStream(metadata, result.stream_type);
  }

  MetricsReporter::LoadStreamResultSummary result_summary;
  result_summary.load_from_store_status = result.load_from_store_status;
  result_summary.final_status = result.final_status;
  result_summary.is_initial_load = result.load_type == LoadType::kInitialLoad;
  result_summary.loaded_new_content_from_network =
      result.loaded_new_content_from_network;
  result_summary.stored_content_age = result.stored_content_age;
  result_summary.content_order = GetContentOrder(result.stream_type);
  result_summary.stream_metadata = stream_metadata;

  metrics_reporter_->OnLoadStream(stream.type, result_summary, content_stats,
                                  std::move(result.latencies));

  stream.model_loading_in_progress = false;
  stream.surface_updater->LoadStreamComplete(
      stream.model != nullptr, result.final_status, result.launch_result);

  LoadTaskComplete(result);

  // When done loading the for-you feed, try to refresh the web-feed if there's
  // no unread content.
  if (IsWebFeedEnabled() && IsSignedIn() &&
      result.load_type != LoadType::kManualRefresh &&
      result.stream_type.IsForYou() && chained_web_feed_refresh_enabled_) {
    // Checking for users without follows.
    // TODO(b/229143375) - We should rate limit fetches if the server side is
    // turned off for this locale, and continually fails.
    StreamType following_type = StreamType(StreamKind::kFollowing);
    if (!HasUnreadContent(following_type)) {
      LoadStreamTask::Options options;
      options.load_type = LoadType::kBackgroundRefresh;
      options.stream_type = following_type;
      options.abort_if_unread_content = true;
      task_queue_.AddTask(
          FROM_HERE, std::make_unique<LoadStreamTask>(
                         options, this,
                         base::BindOnce(&FeedStream::BackgroundRefreshComplete,
                                        base::Unretained(this))));
    }
  }

  if (result.load_type == LoadType::kManualRefresh) {
    std::vector<base::OnceCallback<void(bool)>> moved_callbacks =
        std::move(stream.refresh_complete_callbacks);
    for (auto& callback : moved_callbacks) {
      std::move(callback).Run(result.loaded_new_content_from_network);
    }
  }
}

void FeedStream::OnEnterBackground() {
  metrics_reporter_->OnEnterBackground();
  if (GetFeedConfig().upload_actions_on_enter_background) {
    task_queue_.AddTask(
        FROM_HERE,
        std::make_unique<UploadActionsTask>(
            // Pass empty list to read pending actions from the store.
            std::vector<feedstore::StoredAction>(),
            /*from_load_more=*/false,
            // Pass unknown stream type to skip logging upload actions events.
            StreamType(), this,
            base::BindOnce(&FeedStream::UploadActionsComplete,
                           base::Unretained(this))));
  }
}

std::string FeedStream::GetSessionId() const {
  return metadata_.session_id().token();
}

const feedstore::Metadata& FeedStream::GetMetadata() const {
  DCHECK(metadata_populated_)
      << "Metadata is not yet populated. This function should only be called "
         "after the WaitForStoreInitialize task is complete.";
  return metadata_;
}

void FeedStream::SetMetadata(feedstore::Metadata metadata) {
  metadata_ = std::move(metadata);
  store_->WriteMetadata(metadata_, base::DoNothing());
}

void FeedStream::SetStreamStale(const StreamType& stream_type, bool is_stale) {
  feedstore::Metadata metadata = GetMetadata();
  feedstore::Metadata::StreamMetadata& stream_metadata =
      feedstore::MetadataForStream(metadata, stream_type);
  if (stream_metadata.is_known_stale() != is_stale) {
    stream_metadata.set_is_known_stale(is_stale);
    if (is_stale) {
      SetStreamViewContentHashes(metadata_, stream_type, {});
    }
    SetMetadata(metadata);
  }
}

bool FeedStream::SetMetadata(std::optional<feedstore::Metadata> metadata) {
  if (metadata) {
    SetMetadata(std::move(*metadata));
    return true;
  }
  return false;
}

void FeedStream::PrefetchImage(const GURL& url) {
  delegate_->PrefetchImage(url);
}

void FeedStream::UpdateExperiments(Experiments experiments) {
  delegate_->RegisterExperiments(experiments);
  prefs::SetExperiments(experiments, *profile_prefs_);
}

SurfaceId FeedStream::CreateSurface(const StreamType& type,
                                    SingleWebFeedEntryPoint entry_point) {
  if (base::TimeTicks::Now() - surface_destroy_time_ > kSurfaceDestroyDelay) {
    CleanupDestroyedSurfaces();
  }

  return all_surfaces_.emplace_back(type, entry_point).GetSurfaceId();
}

void FeedStream::DestroySurface(SurfaceId surface) {
  destroyed_surfaces_.push_back(surface);
  surface_destroy_time_ = base::TimeTicks::Now();
}

void FeedStream::CleanupDestroyedSurfaces() {
  all_surfaces_.erase(base::ranges::remove_if(
                          all_surfaces_,
                          [&](const FeedStreamSurface& surface) {
                            return base::ranges::find(destroyed_surfaces_,
                                                      surface.GetSurfaceId()) !=
                                   destroyed_surfaces_.end();
                          }),
                      all_surfaces_.end());
  destroyed_surfaces_.clear();
}

void FeedStream::AttachSurface(SurfaceId surface_id,
                               SurfaceRenderer* renderer) {
  FeedStreamSurface* surface = FindSurface(surface_id);
  CHECK(surface);
  metrics_reporter_->SurfaceOpened(surface->GetStreamType(),
                                   surface->GetSurfaceId(),
                                   surface->GetSingleWebFeedEntryPoint());
  Stream& stream = GetStream(surface->GetStreamType());
  // Skip normal processing when overriding stream data from the internals page.
  if (forced_stream_update_for_debugging_.updated_slices_size() > 0) {
    stream.surfaces.SurfaceAdded(
        surface_id, renderer,
        /*loading_not_allowed_reason=*/
        feedwire::DiscoverLaunchResult::CARDS_UNSPECIFIED);
    renderer->StreamUpdate(forced_stream_update_for_debugging_);
    return;
  }

  stream.surfaces.SurfaceAdded(
      surface_id, renderer,
      TriggerStreamLoad(surface->GetStreamType(),
                        surface->GetSingleWebFeedEntryPoint()));

  // Cancel any scheduled model unload task.
  ++stream.unload_on_detach_sequence_number;
}

void FeedStream::DetachSurface(SurfaceId surface_id) {
  Stream* stream = FindStream(surface_id);
  if (!stream) {
    return;
  }
  // Ignore subsequent DetachSurface calls.
  if (!stream->surfaces.SurfacePresent(surface_id)) {
    return;
  }

  metrics_reporter_->SurfaceClosed(surface_id);
  stream->surfaces.SurfaceRemoved(surface_id);
  ScheduleModelUnloadIfNoSurfacesAttached(stream->type);
}

void FeedStream::AddUnreadContentObserver(const StreamType& stream_type,
                                          UnreadContentObserver* observer) {
  GetStream(stream_type)
      .unread_content_notifiers.emplace_back(observer->GetWeakPtr());
  MaybeNotifyHasUnreadContent(stream_type);
}

void FeedStream::RemoveUnreadContentObserver(const StreamType& stream_type,
                                             UnreadContentObserver* observer) {
  Stream& stream = GetStream(stream_type);
  auto predicate = [&](const UnreadContentNotifier& notifier) {
    UnreadContentObserver* ptr = notifier.observer().get();
    return ptr == nullptr || observer == ptr;
  };
  std::erase_if(stream.unread_content_notifiers, predicate);
}

void FeedStream::ScheduleModelUnloadIfNoSurfacesAttached(
    const StreamType& stream_type) {
  Stream& stream = GetStream(stream_type);
  if (!stream.surfaces.empty())
    return;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
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
  task_queue_.AddTask(
      FROM_HERE, std::make_unique<offline_pages::ClosureTask>(base::BindOnce(
                     &FeedStream::UnloadModelIfNoSurfacesAttachedTask,
                     base::Unretained(this), stream_type)));
  // If this is a SingleWebFeed stream, remove it and delete stream data on a
  // delay.
  if (stream_type.IsSingleWebFeed()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FeedStream::ClearStream, GetWeakPtr(), stream_type,
                       sequence_number),
        GetFeedConfig().single_web_feed_stream_clear_timeout);
  }
}

void FeedStream::UnloadModelIfNoSurfacesAttachedTask(
    const StreamType& stream_type) {
  Stream& stream = GetStream(stream_type);
  if (!stream.surfaces.empty())
    return;
  UnloadModel(stream_type);
}

bool FeedStream::IsArticlesListVisible() {
  return articles_list_visible_.GetValue();
}

bool FeedStream::IsFeedEnabledByEnterprisePolicy() {
  return snippets_enabled_by_policy_.GetValue();
}

bool FeedStream::IsFeedEnabled() {
  return FeedService::IsEnabled(*profile_prefs());
}

bool FeedStream::IsEnabledAndVisible() {
  return IsArticlesListVisible() && IsFeedEnabled() && IsFeedEnabledByDse();
}

bool FeedStream::IsFeedEnabledByDse() {
#if BUILDFLAG(IS_ANDROID)
  if (chrome_info_.is_new_tab_search_engine_url_android_enabled) {
    return snippets_enabled_by_dse_.GetValue();
  }
#endif  // BUILDFLAG(IS_ANDROID)
  return true;
}

bool FeedStream::IsWebFeedEnabled() {
  return feed::IsWebFeedEnabledForLocale(delegate_->GetCountry()) &&
         !delegate_->IsSupervisedAccount() &&
         !base::FeatureList::IsEnabled(kWebFeedKillSwitch);
}

void FeedStream::EnabledPreferencesChanged() {
  // Assume there might be stored data if the Feed is ever enabled.
  if (IsEnabledAndVisible())
    has_stored_data_.SetValue(true);
}

void FeedStream::LoadMore(SurfaceId surface_id,
                          base::OnceCallback<void(bool)> callback) {
  FeedStreamSurface* surface = FindSurface(surface_id);
  CHECK(surface);
  Stream& stream = GetStream(surface->GetStreamType());
  if (!stream.model) {
    DLOG(ERROR) << "Ignoring LoadMore() before the model is loaded";
    return std::move(callback).Run(false);
  }

  // We want to abort early to avoid showing a loading spinner if it's not
  // necessary.
  if (ShouldMakeFeedQueryRequest(surface->GetStreamType(), LoadType::kLoadMore,
                                 /*consume_quota=*/false)
          .load_stream_status != LoadStreamStatus::kNoStatus) {
    return std::move(callback).Run(false);
  }

  stream.surface_updater->launch_reliability_logger().LogLoadMoreStarted();

  metrics_reporter_->OnLoadMoreBegin(surface->GetStreamType(), surface_id);
  stream.surface_updater->SetLoadingMore(true);

  // Have at most one in-flight LoadMore() request per stream. Send the result
  // to all requestors.
  stream.load_more_complete_callbacks.push_back(std::move(callback));
  if (stream.load_more_complete_callbacks.size() == 1) {
    task_queue_.AddTask(FROM_HERE,
                        std::make_unique<LoadMoreTask>(
                            surface->GetStreamType(), this,
                            base::BindOnce(&FeedStream::LoadMoreComplete,
                                           base::Unretained(this))));
  }
}

void FeedStream::LoadMoreComplete(LoadMoreTask::Result result) {
  Stream& stream = GetStream(result.stream_type);
  if (stream.model && result.model_update_request)
    stream.model->Update(std::move(result.model_update_request));

  if (result.request_schedule)
    SetRequestSchedule(stream.type, *result.request_schedule);

  metrics_reporter_->OnLoadMore(
      result.stream_type, result.final_status,
      stream.model ? stream.model->GetContentStats() : ContentStats());
  stream.surface_updater->SetLoadingMore(false);
  std::vector<base::OnceCallback<void(bool)>> moved_callbacks =
      std::move(stream.load_more_complete_callbacks);
  bool success = result.final_status == LoadStreamStatus::kLoadedFromNetwork;
  for (auto& callback : moved_callbacks) {
    std::move(callback).Run(success);
  }
}

void FeedStream::ManualRefresh(SurfaceId surface_id,
                               base::OnceCallback<void(bool)> callback) {
  FeedStreamSurface* surface = FindSurface(surface_id);
  if (!surface) {
    return;
  }
  Stream& stream = GetStream(surface->GetStreamType());

  // Bail out immediately if loading in progress, or if no surfaces are
  // attached.
  if (stream.model_loading_in_progress || stream.surfaces.empty()) {
    return std::move(callback).Run(false);
  }

  // The user has manually refreshed. In this case we allow resetting
  // the request throttler. Without this, it's likely the user will hit a
  // request limit.
  feed::prefs::SetThrottlerRequestCounts({}, *profile_prefs_);

  stream.model_loading_in_progress = true;

  stream.surface_updater->LoadStreamStarted(/*manual_refreshing=*/true);

  // Have at most one in-flight refresh request per stream.
  stream.refresh_complete_callbacks.push_back(std::move(callback));
  if (stream.refresh_complete_callbacks.size() == 1) {
    LoadStreamTask::Options options;
    options.stream_type = surface->GetStreamType();
    options.load_type = LoadType::kManualRefresh;
    task_queue_.AddTask(FROM_HERE,
                        std::make_unique<LoadStreamTask>(
                            options, this,
                            base::BindOnce(&FeedStream::StreamLoadComplete,
                                           base::Unretained(this))));
  }

  last_refresh_scheduled_on_interaction_time_ = base::TimeTicks();

  metrics_reporter_->OnManualRefresh(surface->GetStreamType(), metadata_,
                                     stream.content_ids);
}

void FeedStream::FetchResource(
    const GURL& url,
    const std::string& method,
    const std::vector<std::string>& header_names_and_values,
    const std::string& post_data,
    base::OnceCallback<void(NetworkResponse)> callback) {
  net::HttpRequestHeaders headers;
  for (size_t i = 0; i + 1 < header_names_and_values.size(); i += 2) {
    headers.SetHeader(header_names_and_values[i],
                      header_names_and_values[i + 1]);
  }
  feed_network_->SendAsyncDataRequest(
      url, method, headers, post_data, GetAccountInfo(),
      base::BindOnce(&FeedStream::FetchResourceComplete, base::Unretained(this),
                     std::move(callback)));
}

void FeedStream::FetchResourceComplete(
    base::OnceCallback<void(NetworkResponse)> callback,
    FeedNetwork::RawResponse response) {
  MetricsReporter::OnResourceFetched(response.response_info.status_code);
  NetworkResponse network_response;
  network_response.status_code = response.response_info.status_code;
  network_response.response_bytes = std::move(response.response_bytes);
  network_response.response_header_names_and_values =
      std::move(response.response_info.response_header_names_and_values);
  std::move(callback).Run(std::move(network_response));
}

void FeedStream::ExecuteOperations(
    SurfaceId surface_id,
    std::vector<feedstore::DataOperation> operations) {
  Stream* stream = FindStream(surface_id);
  if (!stream) {
    return;
  }

  StreamModel* model = GetModel(stream->type);
  if (!model) {
    DLOG(ERROR) << "Calling ExecuteOperations before the model is loaded";
    return;
  }
  // TODO(crbug.com/40777338): Convert this to a task.
  return model->ExecuteOperations(std::move(operations));
}

EphemeralChangeId FeedStream::CreateEphemeralChange(
    SurfaceId surface_id,
    std::vector<feedstore::DataOperation> operations) {
  FeedStreamSurface* surface = FindSurface(surface_id);
  if (!surface) {
    return {};
  }
  StreamModel* model = GetModel(surface->GetStreamType());
  if (!model) {
    DLOG(ERROR) << "Calling CreateEphemeralChange before the model is loaded";
    return {};
  }
  metrics_reporter_->OtherUserAction(surface->GetStreamType(),
                                     FeedUserActionType::kEphemeralChange);
  return model->CreateEphemeralChange(std::move(operations));
}

EphemeralChangeId FeedStream::CreateEphemeralChangeFromPackedData(
    SurfaceId surface_id,
    std::string_view data) {
  feedpacking::DismissData msg;
  msg.ParseFromArray(data.data(), data.size());
  return CreateEphemeralChange(surface_id,
                               TranslateDismissData(base::Time::Now(), msg));
}

bool FeedStream::CommitEphemeralChange(SurfaceId surface_id,
                                       EphemeralChangeId id) {
  StreamModel* model = GetModel(surface_id);
  if (!model)
    return false;
  metrics_reporter_->OtherUserAction(
      model->GetStreamType(), FeedUserActionType::kEphemeralChangeCommited);
  return model->CommitEphemeralChange(id);
}

bool FeedStream::RejectEphemeralChange(SurfaceId surface_id,
                                       EphemeralChangeId id) {
  StreamModel* model = GetModel(surface_id);
  if (!model)
    return false;
  metrics_reporter_->OtherUserAction(
      model->GetStreamType(), FeedUserActionType::kEphemeralChangeRejected);
  return model->RejectEphemeralChange(id);
}

void FeedStream::ProcessThereAndBackAgain(
    std::string_view data,
    const LoggingParameters& logging_parameters) {
  feedwire::ThereAndBackAgainData msg;
  msg.ParseFromArray(data.data(), data.size());
  if (msg.has_action_payload()) {
    feedwire::FeedAction action_msg;
    *action_msg.mutable_action_payload() = std::move(msg.action_payload());
    UploadAction(std::move(action_msg), logging_parameters,
                 /*upload_now=*/true,
                 base::BindOnce(&FeedStream::UploadActionsComplete,
                                base::Unretained(this)));
  }
}

void FeedStream::ProcessViewAction(
    std::string_view data,
    const LoggingParameters& logging_parameters) {
  if (!logging_parameters.view_actions_enabled)
    return;

  feedwire::FeedAction msg;
  msg.ParseFromArray(data.data(), data.size());
  UploadAction(std::move(msg), logging_parameters,
               /*upload_now=*/false,
               base::BindOnce(&FeedStream::UploadActionsComplete,
                              base::Unretained(this)));
}

void FeedStream::UploadActionsComplete(UploadActionsTask::Result result) {
  PopulateDebugStreamData(result, *profile_prefs_);
}

bool FeedStream::WasUrlRecentlyNavigatedFromFeed(const GURL& url) {
  return base::Contains(recent_feed_navigations_, url);
}

void FeedStream::InvalidateContentCacheFor(StreamKind stream_kind) {
  if (stream_kind != StreamKind::kUnknown)
    SetStreamStale(StreamType(stream_kind), true);
}
void FeedStream::RecordContentViewed(SurfaceId /*surface_id*/, uint64_t docid) {
  if (!store_) {
    return;
  }
  WriteDocViewIfEnabled(*this, docid);
}

DebugStreamData FeedStream::GetDebugStreamData() {
  return ::feed::prefs::GetDebugStreamData(*profile_prefs_);
}

void FeedStream::ForceRefreshForDebugging(const StreamType& stream_type) {
  // Avoid request throttling for debug refreshes.
  feed::prefs::SetThrottlerRequestCounts({}, *profile_prefs_);
  task_queue_.AddTask(
      FROM_HERE, std::make_unique<offline_pages::ClosureTask>(
                     base::BindOnce(&FeedStream::ForceRefreshForDebuggingTask,
                                    base::Unretained(this), stream_type)));
}

void FeedStream::ForceRefreshTask(const StreamType& stream_type) {
  UnloadModel(stream_type);
  store_->ClearStreamData(stream_type, base::DoNothing());
  GetStream(stream_type)
      .surface_updater->launch_reliability_logger()
      .LogFeedLaunchOtherStart();
  if (!GetStream(stream_type).surfaces.empty())
    TriggerStreamLoad(stream_type);
}

void FeedStream::ForceRefreshForDebuggingTask(const StreamType& stream_type) {
  UnloadModel(stream_type);
  store_->ClearStreamData(stream_type, base::DoNothing());
  GetStream(stream_type)
      .surface_updater->launch_reliability_logger()
      .LogFeedLaunchOtherStart();
  TriggerStreamLoad(stream_type);
}

std::string FeedStream::DumpStateForDebugging() {
  Stream& stream = GetStream(StreamType(StreamKind::kForYou));
  std::stringstream ss;
  if (stream.model) {
    ss << "model loaded, " << stream.model->GetContentList().size()
       << " contents, "
       << "signed_in=" << stream.model->signed_in()
       << ", logging_enabled=" << stream.model->logging_enabled()
       << ", privacy_notice_fulfilled="
       << stream.model->privacy_notice_fulfilled();
  }

  auto print_refresh_schedule = [&](RefreshTaskId task_id) {
    RequestSchedule schedule =
        prefs::GetRequestSchedule(task_id, *profile_prefs_);
    if (schedule.refresh_offsets.empty()) {
      ss << "No request schedule\n";
    } else {
      ss << "Request schedule reference " << schedule.anchor_time << '\n';
      for (base::TimeDelta entry : schedule.refresh_offsets) {
        ss << " fetch at " << entry << '\n';
      }
    }
  };
  ss << "For You: ";
  print_refresh_schedule(RefreshTaskId::kRefreshForYouFeed);
  ss << "WebFeeds: ";
  print_refresh_schedule(RefreshTaskId::kRefreshWebFeed);
  ss << "WebFeedSubscriptions:\n";
  subscriptions().DumpStateForDebugging(ss);
  return ss.str();
}

void FeedStream::SetForcedStreamUpdateForDebugging(
    const feedui::StreamUpdate& stream_update) {
  forced_stream_update_for_debugging_ = stream_update;
}

base::Time FeedStream::GetLastFetchTime(SurfaceId surface_id) {
  FeedStreamSurface* surface = FindSurface(surface_id);
  if (!surface) {
    return {};
  }
  const base::Time fetch_time =
      feedstore::GetLastFetchTime(metadata_, surface->GetStreamType());
  // Ignore impossible time values.
  return (fetch_time > base::Time::Now()) ? base::Time() : fetch_time;
}

void FeedStream::LoadModelForTesting(const StreamType& stream_type,
                                     std::unique_ptr<StreamModel> model) {
  LoadModel(stream_type, std::move(model));
}
offline_pages::TaskQueue& FeedStream::GetTaskQueueForTesting() {
  return task_queue_;
}

void FeedStream::OnTaskQueueIsIdle() {
  if (idle_callback_)
    idle_callback_.Run();
}

void FeedStream::SubscribedWebFeedCount(
    base::OnceCallback<void(int)> callback) {
  subscriptions().SubscribedWebFeedCount(std::move(callback));
}
void FeedStream::RegisterFeedUserSettingsFieldTrial(std::string_view group) {
  delegate_->RegisterFeedUserSettingsFieldTrial(group);
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
  } else if (update.update_request) {
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

LaunchResult FeedStream::ShouldAttemptLoad(const StreamType& stream_type,
                                           LoadType load_type,
                                           bool model_loading) {
  Stream& stream = GetStream(stream_type);
  if (load_type == LoadType::kInitialLoad ||
      load_type == LoadType::kBackgroundRefresh) {
    // For initial load or background refresh, the model should not be loaded
    // or in the process of being loaded. Because |ShouldAttemptLoad()| is used
    // both before and during the load process, we need to ignore this check
    // when |model_loading| is true.
    if (stream.model || (!model_loading && stream.model_loading_in_progress)) {
      return {LoadStreamStatus::kModelAlreadyLoaded,
              feedwire::DiscoverLaunchResult::CARDS_UNSPECIFIED};
    }
  }

  if (!IsArticlesListVisible()) {
    return {LoadStreamStatus::kLoadNotAllowedArticlesListHidden,
            feedwire::DiscoverLaunchResult::FEED_HIDDEN};
  }

  if (!IsFeedEnabledByEnterprisePolicy()) {
    return {LoadStreamStatus::kLoadNotAllowedDisabledByEnterprisePolicy,
            feedwire::DiscoverLaunchResult::
                INELIGIBLE_DISCOVER_DISABLED_BY_ENTERPRISE_POLICY};
  }

  if (!IsFeedEnabled()) {
    return {LoadStreamStatus::kLoadNotAllowedDisabled,
            feedwire::DiscoverLaunchResult::INELIGIBLE_DISCOVER_DISABLED};
  }

  if (!IsFeedEnabledByDse()) {
    return {
        LoadStreamStatus::kLoadNotAllowedDisabledByDse,
        feedwire::DiscoverLaunchResult::INELIGIBLE_DISCOVER_DISABLED_BY_DSE};
  }

  if (!delegate_->IsEulaAccepted()) {
    return {LoadStreamStatus::kLoadNotAllowedEulaNotAccepted,
            feedwire::DiscoverLaunchResult::INELIGIBLE_EULA_NOT_ACCEPTED};
  }

  // Skip this check if metadata_ is not initialized. ShouldAttemptLoad() will
  // be called again from within the LoadStreamTask, and then the metadata
  // will be initialized.
  if (metadata_populated_ &&
      delegate_->GetAccountInfo().gaia != metadata_.gaia()) {
    return {LoadStreamStatus::kDataInStoreIsForAnotherUser,
            feedwire::DiscoverLaunchResult::DATA_IN_STORE_IS_FOR_ANOTHER_USER};
  }

  return {LoadStreamStatus::kNoStatus,
          feedwire::DiscoverLaunchResult::CARDS_UNSPECIFIED};
}

bool FeedStream::MissedLastRefresh(const StreamType& stream_type) {
  RefreshTaskId task_id;
  if (!stream_type.GetRefreshTaskId(task_id))
    return false;
  RequestSchedule schedule =
      feed::prefs::GetRequestSchedule(task_id, *profile_prefs_);
  if (schedule.refresh_offsets.empty())
    return false;
  base::Time scheduled_time =
      schedule.anchor_time + schedule.refresh_offsets[0];
  return scheduled_time < base::Time::Now();
}

LaunchResult FeedStream::ShouldMakeFeedQueryRequest(
    const StreamType& stream_type,
    LoadType load_type,
    bool consume_quota) {
  Stream& stream = GetStream(stream_type);
  if (load_type == LoadType::kLoadMore) {
    // LoadMore requires a next page token.
    if (!stream.model || stream.model->GetNextPageToken().empty()) {
      return {LoadStreamStatus::kCannotLoadMoreNoNextPageToken,
              feedwire::DiscoverLaunchResult::CARDS_UNSPECIFIED};
    }
  } else if (load_type != LoadType::kManualRefresh) {
    // Time has passed since calling |ShouldAttemptLoad()|, call it again to
    // confirm we should still attempt loading.
    const LaunchResult should_not_attempt_reason =
        ShouldAttemptLoad(stream_type, load_type, /*model_loading=*/true);
    if (should_not_attempt_reason.load_stream_status !=
        LoadStreamStatus::kNoStatus) {
      return should_not_attempt_reason;
    }
  }

  if (delegate_->IsOffline()) {
    return {LoadStreamStatus::kCannotLoadFromNetworkOffline,
            feedwire::DiscoverLaunchResult::NO_CARDS_REQUEST_ERROR_NO_INTERNET};
  }

  NetworkRequestType request_type;
  switch (stream_type.GetKind()) {
    case StreamKind::kUnknown:
      DLOG(ERROR) << "Unknown stream kind";
      [[fallthrough]];
    case StreamKind::kSupervisedUser:
      request_type = NetworkRequestType::kSupervisedFeed;
      break;
    case StreamKind::kForYou:
      request_type = (load_type != LoadType::kLoadMore)
                         ? NetworkRequestType::kFeedQuery
                         : NetworkRequestType::kNextPage;
      break;
    case StreamKind::kFollowing:
      request_type = NetworkRequestType::kWebFeedListContents;
      break;
    case StreamKind::kSingleWebFeed:
      request_type = NetworkRequestType::kSingleWebFeedListContents;
      break;
  }

  if (consume_quota && !request_throttler_.RequestQuota(request_type)) {
    return {LoadStreamStatus::kCannotLoadFromNetworkThrottled,
            feedwire::DiscoverLaunchResult::NO_CARDS_REQUEST_ERROR_OTHER};
  }

  return {LoadStreamStatus::kNoStatus,
          feedwire::DiscoverLaunchResult::CARDS_UNSPECIFIED};
}

feedwire::ChromeSignInStatus::SignInStatus FeedStream::GetSignInStatus() const {
  if (IsSignedIn()) {
    return feedwire::ChromeSignInStatus::SIGNED_IN;
  }
  if (!IsSigninAllowed()) {
    return feedwire::ChromeSignInStatus::SIGNIN_DISALLOWED_BY_CONFIG;
  }
  return feedwire::ChromeSignInStatus::NOT_SIGNED_IN;
}

feedwire::DefaultSearchEngine::SearchEngine FeedStream::GetDefaultSearchEngine()
    const {
  const TemplateURL* template_url =
      template_url_service_->GetDefaultSearchProvider();
  if (template_url) {
    SearchEngineType engine_type =
        template_url->GetEngineType(template_url_service_->search_terms_data());
    if (engine_type == SEARCH_ENGINE_GOOGLE) {
      return feedwire::DefaultSearchEngine::ENGINE_GOOGLE;
    }
  }
  return feedwire::DefaultSearchEngine::ENGINE_OTHER;
}

RequestMetadata FeedStream::GetCommonRequestMetadata(
    bool signed_in_request,
    bool allow_expired_session_id) const {
  RequestMetadata result;
  result.chrome_info = chrome_info_;
  result.display_metrics = delegate_->GetDisplayMetrics();
  result.language_tag = delegate_->GetLanguageTag();
  result.notice_card_acknowledged =
      privacy_notice_card_tracker_.HasAcknowledgedNoticeCard();
  result.tab_group_enabled_state = delegate_->GetTabGroupEnabledState();

  if (signed_in_request) {
    result.client_instance_id = prefs::GetClientInstanceId(*profile_prefs_);
  } else {
    std::string session_id = GetSessionId();
    if (!session_id.empty() &&
        (allow_expired_session_id ||
         feedstore::GetSessionIdExpiryTime(metadata_) > base::Time::Now())) {
      result.session_id = session_id;
    }
  }
  result.followed_from_web_page_menu_count =
      metadata_.followed_from_web_page_menu_count();

  DCHECK(result.session_id.empty() || result.client_instance_id.empty());
  return result;
}

RequestMetadata FeedStream::GetSignedInRequestMetadata() const {
  return GetCommonRequestMetadata(/*signed_in_request =*/true,
                                  /*allow_expired_session_id =*/true);
}

RequestMetadata FeedStream::GetRequestMetadata(const StreamType& stream_type,
                                               bool is_for_next_page) const {
  const Stream* stream = FindStream(stream_type);
  // TODO(crbug.com/40869569) handle null single web feed streams
  DCHECK(stream);
  RequestMetadata result;
  if (is_for_next_page) {
    // If we are continuing an existing feed, use whatever session continuity
    // mechanism is currently associated with the stream: client-instance-id
    // for signed-in feed, session_id token for signed-out.
    DCHECK(stream->model);
    result = GetCommonRequestMetadata(stream->model->signed_in(),
                                      /*allow_expired_session_id =*/true);
  } else {
    // The request is for the first page of the feed. Use client_instance_id
    // for signed in requests and session_id token (if any, and not expired)
    // for signed-out.
    result = GetCommonRequestMetadata(IsSignedIn(),
                                      /*allow_expired_session_id =*/false);
  }

  result.content_order = GetContentOrder(stream_type);

  const feedstore::Metadata::StreamMetadata* stream_metadata =
      FindMetadataForStream(GetMetadata(), stream_type);
  if (stream_metadata != nullptr) {
    result.info_card_tracking_states = info_card_tracker_.GetAllStates(
        stream_metadata->last_server_response_time_millis(),
        stream_metadata->last_fetch_time_millis());
  }
  // Set sign in status for request metadata
  result.sign_in_status = GetSignInStatus();

  result.default_search_engine = GetDefaultSearchEngine();

  result.country = delegate_->GetCountry();

  return result;
}

void FeedStream::OnEulaAccepted() {
  for (auto& item : streams_) {
    if (!item.second.surfaces.empty()) {
      item.second.surface_updater->launch_reliability_logger()
          .LogFeedLaunchOtherStart();
      TriggerStreamLoad(item.second.type);
    }
  }
}

void FeedStream::OnAllHistoryDeleted() {
  // We don't really need to delete StreamType(StreamKind::kFollowing) data
  // here, but clearing all data because it's easy.
  ClearAll();
}

void FeedStream::OnCacheDataCleared() {
  ClearAll();
}

void FeedStream::OnSignedIn() {
  // On sign-in, turn off activity logging. This avoids the possibility that we
  // send logs with the wrong user info attached, but may cause us to lose
  // buffered events.
  for (auto& item : streams_) {
    item.second.is_activity_logging_enabled = false;
  }

  ClearAll();
}

void FeedStream::OnSignedOut() {
  // On sign-out, turn off activity logging. This avoids the possibility that we
  // send logs with the wrong user info attached, but may cause us to lose
  // buffered events.
  for (auto& item : streams_) {
    item.second.is_activity_logging_enabled = false;
  }

  ClearAll();
}

void FeedStream::ExecuteRefreshTask(RefreshTaskId task_id) {
  StreamType stream_type = StreamType::ForTaskId(task_id);
  LoadStreamStatus do_not_attempt_reason =
      ShouldAttemptLoad(stream_type, LoadType::kBackgroundRefresh)
          .load_stream_status;

  RequestSchedule request_schedule =
      feed::prefs::GetRequestSchedule(task_id, *profile_prefs_);
  LoadType load_type = RequestScheduleTypeToLoadType(request_schedule.type);

  // If `do_not_attempt_reason` indicates the stream shouldn't be loaded, it's
  // unlikely that criteria will change, so we skip rescheduling.
  if (do_not_attempt_reason == LoadStreamStatus::kNoStatus ||
      do_not_attempt_reason == LoadStreamStatus::kModelAlreadyLoaded) {
    // Schedule the next refresh attempt. If a new refresh schedule is returned
    // through this refresh, it will be overwritten.
    SetRequestSchedule(task_id, std::move(request_schedule));
  }

  if (do_not_attempt_reason != LoadStreamStatus::kNoStatus) {
    BackgroundRefreshComplete(
        LoadStreamTask::Result(stream_type, do_not_attempt_reason));
    return;
  }

  LoadStreamTask::Options options;
  options.stream_type = stream_type;
  options.load_type = load_type;
  options.refresh_even_when_not_stale = true;
  task_queue_.AddTask(FROM_HERE,
                      std::make_unique<LoadStreamTask>(
                          options, this,
                          base::BindOnce(&FeedStream::BackgroundRefreshComplete,
                                         base::Unretained(this))));
}

void FeedStream::BackgroundRefreshComplete(LoadStreamTask::Result result) {
  metrics_reporter_->OnBackgroundRefresh(result.stream_type,
                                         result.final_status);

  LoadTaskComplete(result);

  // Add prefetch images to task queue without waiting to finish
  // since we treat them as best-effort.
  if (result.stream_type.IsForYou())
    task_queue_.AddTask(FROM_HERE, std::make_unique<PrefetchImagesTask>(this));

  RefreshTaskId task_id;
  if (result.stream_type.GetRefreshTaskId(task_id)) {
    refresh_task_scheduler_->RefreshTaskComplete(task_id);
  }
}

// Performs work that is necessary for both background and foreground load
// tasks.
void FeedStream::LoadTaskComplete(const LoadStreamTask::Result& result) {
  if (delegate_->GetAccountInfo().gaia != metadata_.gaia()) {
    ClearAll();
    return;
  }
  PopulateDebugStreamData(result, *profile_prefs_);
  if (!result.content_ids.IsEmpty()) {
    GetStream(result.stream_type).content_ids = result.content_ids;
  }
  if (result.loaded_new_content_from_network) {
    SetStreamStale(result.stream_type, false);
    if (result.stream_type.IsForYou()) {
      UpdateExperiments(result.experiments);
      CheckDuplicatedContentsOnRefresh();
    }
  }

  MaybeNotifyHasUnreadContent(result.stream_type);
}

bool FeedStream::HasUnreadContent(const StreamType& stream_type) {
  Stream& stream = GetStream(stream_type);
  if (stream.content_ids.IsEmpty())
    return false;
  if (feedstore::GetViewContentIds(metadata_, stream_type)
          .ContainsAllOf(stream.content_ids)) {
    return false;
  }

  // If there is currently a surface already viewing the content, update the
  // ViewContentIds to whatever the current set is. This can happen if the
  // surface already shown is refreshed.
  if (stream.model && stream.surfaces.HasSurfaceShowingContent()) {
    SetMetadata(SetStreamViewContentHashes(metadata_, stream_type,
                                           stream.model->GetContentIds()));
    return false;
  }
  return true;
}

void FeedStream::IncrementFollowedFromWebPageMenuCount() {
  feedstore::Metadata metadata = GetMetadata();
  metadata.set_followed_from_web_page_menu_count(
      metadata.followed_from_web_page_menu_count() + 1);
  SetMetadata(std::move(metadata));
}

void FeedStream::ClearAll() {
  clear_all_in_progress_ = true;
  task_queue_.AddTask(FROM_HERE, std::make_unique<ClearAllTask>(this));
}

void FeedStream::FinishClearAll() {
  // Clear any experiments stored.
  has_stored_data_.SetValue(false);
  feed::prefs::SetExperiments({}, *profile_prefs_);
  feed::prefs::ClearClientInstanceId(*profile_prefs_);
  SetMetadata(feedstore::MakeMetadata(delegate_->GetAccountInfo().gaia));

  delegate_->ClearAll();

  clear_all_in_progress_ = false;

  for (auto& item : streams_) {
    if (!item.second.surfaces.empty()) {
      item.second.surface_updater->launch_reliability_logger()
          .LogFeedLaunchOtherStart();
      TriggerStreamLoad(item.second.type);
    }
  }
  web_feed_subscription_coordinator_->ClearAllFinished();
}

void FeedStream::FinishClearStream(const StreamType& stream_type) {
  Stream* stream = FindStream(stream_type);
  if (stream && stream_type.IsSingleWebFeed()) {
    streams_.erase(stream_type);
  }
}

ImageFetchId FeedStream::FetchImage(
    const GURL& url,
    base::OnceCallback<void(NetworkResponse)> callback) {
  return image_fetcher_->Fetch(url, std::move(callback));
}

PersistentKeyValueStoreImpl& FeedStream::GetPersistentKeyValueStore() {
  return *persistent_key_value_store_;
}

void FeedStream::CancelImageFetch(ImageFetchId id) {
  image_fetcher_->Cancel(id);
}

void FeedStream::UploadAction(
    feedwire::FeedAction action,
    const LoggingParameters& logging_parameters,
    bool upload_now,
    base::OnceCallback<void(UploadActionsTask::Result)> callback) {
  UploadActionsTask::WireAction wire_action(action, logging_parameters,
                                            upload_now);
  task_queue_.AddTask(
      FROM_HERE,
      std::make_unique<UploadActionsTask>(
          std::move(wire_action),
          // Pass unknown string type to skip logging upload actions events.
          StreamType(), this, std::move(callback)));
}

void FeedStream::LoadModel(const StreamType& stream_type,
                           std::unique_ptr<StreamModel> model) {
  DCHECK(model);
  Stream& stream = GetStream(stream_type);
  DCHECK(!stream.model);
  stream.model = std::move(model);
  stream.model->SetStreamType(stream_type);
  stream.model->SetStoreObserver(this);

  stream.content_ids = stream.model->GetContentIds();
  stream.surface_updater->SetModel(stream.model.get());
  ScheduleModelUnloadIfNoSurfacesAttached(stream_type);
  MaybeNotifyHasUnreadContent(stream_type);
}

void FeedStream::SetRequestSchedule(const StreamType& stream_type,
                                    RequestSchedule schedule) {
  RefreshTaskId task_id;
  if (!stream_type.GetRefreshTaskId(task_id)) {
    DLOG(ERROR) << "Ignoring request schedule for this stream: " << stream_type;
    return;
  }
  SetRequestSchedule(task_id, std::move(schedule));
}

void FeedStream::SetRequestSchedule(RefreshTaskId task_id,
                                    RequestSchedule schedule) {
  const base::Time now = base::Time::Now();
  base::Time run_time = NextScheduledRequestTime(now, &schedule);
  if (!run_time.is_null()) {
    refresh_task_scheduler_->EnsureScheduled(task_id, run_time - now);
  } else {
    refresh_task_scheduler_->Cancel(task_id);
  }
  feed::prefs::SetRequestSchedule(task_id, schedule, *profile_prefs_);
}

void FeedStream::UnloadModel(const StreamType& stream_type) {
  // Note: This should only be called from a running Task, as some tasks assume
  // the model remains loaded.
  Stream* stream = FindStream(stream_type);
  if (!stream)
    return;

  if (stream->model) {
    stream->surface_updater->SetModel(nullptr);
    stream->model.reset();
  }
}

void FeedStream::ClearStream(const StreamType& stream_type,
                             int sequence_number) {
  Stream* stream = FindStream(stream_type);
  if (!stream || stream->unload_on_detach_sequence_number != sequence_number) {
    return;
  }
  task_queue_.AddTask(FROM_HERE,
                      std::make_unique<ClearStreamTask>(this, stream_type));
}

void FeedStream::UnloadModels() {
  for (auto& item : streams_) {
    UnloadModel(item.second.type);
  }
}

LaunchReliabilityLogger& FeedStream::GetLaunchReliabilityLogger(
    const StreamType& stream_type) {
  return GetStream(stream_type).surface_updater->launch_reliability_logger();
}

void FeedStream::UpdateUserProfileOnLinkClick(
    const GURL& url,
    const std::vector<int64_t>& entity_mids) {
  user_actions_collector_.UpdateUserProfileOnLinkClick(url, entity_mids);
}

void FeedStream::ReportOpenAction(const GURL& url,
                                  SurfaceId surface_id,
                                  const std::string& slice_id,
                                  OpenActionType action_type) {
  FeedStreamSurface* surface = FindSurface(surface_id);
  if (!surface) {
    return;
  }
  recent_feed_navigations_.insert(recent_feed_navigations_.begin(), url);
  recent_feed_navigations_.resize(
      std::min(kMaxRecentFeedNavigations, recent_feed_navigations_.size()));

  Stream& stream = GetStream(surface->GetStreamType());

  int index = stream.surface_updater->GetSliceIndexFromSliceId(slice_id);
  if (index < 0)
    index = MetricsReporter::kUnknownCardIndex;
  metrics_reporter_->OpenAction(surface->GetStreamType(), index, action_type);

  if (stream.model) {
    privacy_notice_card_tracker_.OnOpenAction(
        stream.model->FindContentId(ToContentRevision(slice_id)));
  }
  ScheduleFeedCloseRefresh(surface->GetStreamType());
}

void FeedStream::ReportOpenVisitComplete(SurfaceId /*surface_id*/,
                                         base::TimeDelta visit_time) {
  metrics_reporter_->OpenVisitComplete(visit_time);
}

void FeedStream::ReportSliceViewed(SurfaceId surface_id,
                                   const std::string& slice_id) {
  Stream* stream = FindStream(surface_id);
  if (!stream) {
    return;
  }
  int index = stream->surface_updater->GetSliceIndexFromSliceId(slice_id);
  if (index < 0)
    return;

  if (!stream->model) {
    return;
  }

  metrics_reporter_->ContentSliceViewed(stream->type, index,
                                        stream->model->GetContentList().size());

  ContentRevision content_revision = ToContentRevision(slice_id);

  privacy_notice_card_tracker_.OnCardViewed(
      stream->model->signed_in(),
      stream->model->FindContentId(content_revision));

  if (stream->type.IsForYou()) {
    const feedstore::Content* content =
        stream->model->FindContent(content_revision);
    if (content)
      AddViewedContentHashes(*content);
  }
}

// Notifies observers if 'HasUnreadContent' has changed for `stream_type`.
// Stream content has been seen if StreamData::content_hash ==
// Metadata::StreamMetadata::view_content_hash. This should be called:
// when initial metadata is loaded, when the model is loaded, when a refresh is
// attempted, and when content is viewed.
void FeedStream::MaybeNotifyHasUnreadContent(const StreamType& stream_type) {
  Stream& stream = GetStream(stream_type);
  if (!metadata_populated_ || stream.model_loading_in_progress)
    return;

  const bool has_new_content = HasUnreadContent(stream_type);
  for (auto& o : stream.unread_content_notifiers) {
    o.NotifyIfValueChanged(has_new_content);
  }
}

void FeedStream::ReportFeedViewed(SurfaceId surface_id) {
  metrics_reporter_->FeedViewed(surface_id);
  Stream* stream = FindStream(surface_id);
  if (!stream) {
    return;
  }

  stream->surfaces.FeedViewed(surface_id);
  MaybeNotifyHasUnreadContent(stream->type);
}

void FeedStream::ReportPageLoaded(SurfaceId /*surface_id*/) {
  metrics_reporter_->PageLoaded();
}
void FeedStream::ReportStreamScrolled(SurfaceId surface_id, int distance_dp) {
  FeedStreamSurface* surface = FindSurface(surface_id);
  if (!surface) {
    return;
  }
  metrics_reporter_->StreamScrolled(surface->GetStreamType(), distance_dp);
  if (GetStream(surface->GetStreamType()).surfaces.HasSurfaceShowingContent()) {
    ScheduleFeedCloseRefresh(surface->GetStreamType());
  }
}
void FeedStream::ReportStreamScrollStart(SurfaceId /*surface_id*/) {
  metrics_reporter_->StreamScrollStart();
}
void FeedStream::ReportOtherUserAction(SurfaceId surface_id,
                                       FeedUserActionType action_type) {
  FeedStreamSurface* surface = FindSurface(surface_id);
  if (!surface) {
    return;
  }
  metrics_reporter_->OtherUserAction(surface->GetStreamType(), action_type);
}

void FeedStream::ReportOtherUserAction(const StreamType& stream_type,
                                       FeedUserActionType action_type) {
  metrics_reporter_->OtherUserAction(stream_type, action_type);
}

void FeedStream::ReportInfoCardTrackViewStarted(SurfaceId surface_id,
                                                int info_card_type) {
  FeedStreamSurface* surface = FindSurface(surface_id);
  if (!surface) {
    return;
  }
  metrics_reporter_->OnInfoCardTrackViewStarted(surface->GetStreamType(),
                                                info_card_type);
}

void FeedStream::ReportInfoCardViewed(SurfaceId surface_id,
                                      int info_card_type,
                                      int minimum_view_interval_seconds) {
  FeedStreamSurface* surface = FindSurface(surface_id);
  if (!surface) {
    return;
  }
  metrics_reporter_->OnInfoCardViewed(surface->GetStreamType(), info_card_type);
  info_card_tracker_.OnViewed(info_card_type, minimum_view_interval_seconds);
}

void FeedStream::ReportInfoCardClicked(SurfaceId surface_id,
                                       int info_card_type) {
  FeedStreamSurface* surface = FindSurface(surface_id);
  if (!surface) {
    return;
  }
  metrics_reporter_->OnInfoCardClicked(surface->GetStreamType(),
                                       info_card_type);
  info_card_tracker_.OnClicked(info_card_type);
}

void FeedStream::ReportInfoCardDismissedExplicitly(SurfaceId surface_id,
                                                   int info_card_type) {
  FeedStreamSurface* surface = FindSurface(surface_id);
  if (!surface) {
    return;
  }
  metrics_reporter_->OnInfoCardDismissedExplicitly(surface->GetStreamType(),
                                                   info_card_type);
  info_card_tracker_.OnDismissed(info_card_type);
}

void FeedStream::ResetInfoCardStates(SurfaceId surface_id, int info_card_type) {
  FeedStreamSurface* surface = FindSurface(surface_id);
  if (!surface) {
    return;
  }
  metrics_reporter_->OnInfoCardStateReset(surface->GetStreamType(),
                                          info_card_type);
  info_card_tracker_.ResetState(info_card_type);
}

void FeedStream::ReportContentSliceVisibleTimeForGoodVisits(
    SurfaceId surface_id,
    base::TimeDelta elapsed) {
  metrics_reporter_->ReportStableContentSliceVisibilityTimeForGoodVisits(
      elapsed);
}

void FeedStream::SetContentOrder(const StreamType& stream_type,
                                 ContentOrder content_order) {
  if (!stream_type.IsWebFeed()) {
    DLOG(ERROR) << "SetContentOrder is not supported for this stream_type "
                << stream_type;
    return;
  }

  ContentOrder current_order = GetValidWebFeedContentOrder(*profile_prefs_);
  prefs::SetWebFeedContentOrder(*profile_prefs_, content_order);
  if (current_order == content_order)
    return;

  // Note that ForceRefreshTask clears stored content and forces a network
  // refresh. It is possible to instead cache each ordering of the Feed
  // separately, so that users who switch back and forth can do so more quickly
  // and efficiently. However, there are some reasons to avoid this
  // optimization:
  // * we want content to be fresh, so this optimization would have limited
  //   effect.
  // * interactions with the feed can modify content; in these cases we would
  //   want a full refresh.
  // * it will add quite a bit of complexity to do it right
  task_queue_.AddTask(
      FROM_HERE,
      std::make_unique<offline_pages::ClosureTask>(base::BindOnce(
          &FeedStream::ForceRefreshTask, base::Unretained(this), stream_type)));
}

ContentOrder FeedStream::GetContentOrder(const StreamType& stream_type) const {
  if (!stream_type.IsWebFeed())
    return ContentOrder::kUnspecified;
  return GetValidWebFeedContentOrder(*profile_prefs_);
}

ContentOrder FeedStream::GetContentOrderFromPrefs(
    const StreamType& stream_type) {
  if (!stream_type.IsWebFeed()) {
    NOTREACHED_IN_MIGRATION()
        << "GetContentOrderFromPrefs is not supported for this stream_type "
        << stream_type;
    return ContentOrder::kUnspecified;
  }
  return prefs::GetWebFeedContentOrder(*profile_prefs_);
}

void FeedStream::ScheduleFeedCloseRefresh(const StreamType& type) {
  // To avoid causing jank, only schedule the refresh once every several
  // minutes.
  base::TimeTicks now = base::TimeTicks::Now();
  if (now - last_refresh_scheduled_on_interaction_time_ < base::Minutes(5))
    return;

  last_refresh_scheduled_on_interaction_time_ = now;

  base::TimeDelta delay = kFeedCloseRefreshDelay;
  RequestSchedule schedule;
  schedule.anchor_time = base::Time::Now();
  schedule.refresh_offsets = {delay, delay * 2, delay * 3};
  schedule.type = RequestSchedule::Type::kFeedCloseRefresh;
  SetRequestSchedule(type, std::move(schedule));
}

void FeedStream::CheckDuplicatedContentsOnRefresh() {
  StreamType stream_type = StreamType(StreamKind::kForYou);
  Stream& stream = GetStream(stream_type);
  feedstore::Metadata metadata = GetMetadata();
  feedstore::Metadata::StreamMetadata& stream_metadata =
      MetadataForStream(metadata, stream_type);

  // Update the most recent list to include the currently view contents.
  // This is done in the following steps:
  // 1) If the currently viewed content hash is found in the most recent list,
  //    it means that the content was viewed in previous sessions. We need to
  //    remove the content hash from its old position and add it back later.
  // 2) Append the currently viewed content hashes.
  std::vector<uint32_t> most_recent_viewed_content_hashes(
      metadata.most_recent_viewed_content_hashes().begin(),
      metadata.most_recent_viewed_content_hashes().end());
  base::flat_set<uint32_t> viewed_content_hashes(
      stream_metadata.viewed_content_hashes().begin(),
      stream_metadata.viewed_content_hashes().end());
  std::erase_if(most_recent_viewed_content_hashes,
                [&viewed_content_hashes](uint32_t x) {
                  return viewed_content_hashes.contains(x);
                });
  most_recent_viewed_content_hashes.insert(
      most_recent_viewed_content_hashes.end(),
      stream_metadata.viewed_content_hashes().begin(),
      stream_metadata.viewed_content_hashes().end());

  // Check and report the content duplication.
  if (!stream.content_ids.IsEmpty()) {
    base::flat_set<uint32_t> most_recent_content_hashes(
        most_recent_viewed_content_hashes.begin(),
        most_recent_viewed_content_hashes.end());
    bool is_duplicated_at_pos_1 = false;
    bool is_duplicated_at_pos_2 = false;
    bool is_duplicated_at_pos_3 = false;
    int duplicate_count_for_top_10 = 0;
    int duplicate_count_for_all = 0;
    int total_count = 0;
    int pos = 0;
    for (const feedstore::StreamContentHashList& hash_list :
         stream.content_ids.original_hashes()) {
      if (hash_list.hashes_size() == 0)
        continue;

      // For position specific metrics, only the first item is checked for
      // duplication if there are more than one items in a row, like carousel,
      // collection or 2-column.
      if (pos < 10 &&
          most_recent_content_hashes.contains(hash_list.hashes(0))) {
        duplicate_count_for_top_10++;
        if (pos == 0) {
          is_duplicated_at_pos_1 = true;
        } else if (pos == 1) {
          is_duplicated_at_pos_2 = true;
        } else if (pos == 2) {
          is_duplicated_at_pos_3 = true;
        }
      }

      for (uint32_t hash : hash_list.hashes()) {
        total_count++;
        if (most_recent_content_hashes.contains(hash)) {
          duplicate_count_for_all++;
        }
      }

      pos++;
    }

    // Don't report the duplication metrics for the first time.
    if (most_recent_viewed_content_hashes.size() > 0 && total_count > 0) {
      metrics_reporter_->ReportContentDuplication(
          is_duplicated_at_pos_1, is_duplicated_at_pos_2,
          is_duplicated_at_pos_3,
          static_cast<int>(duplicate_count_for_top_10 * 10),
          static_cast<int>(100 * duplicate_count_for_all / total_count));
    }
  }

  // Reset `viewed_content_hashes` after each refresh.
  stream_metadata.clear_viewed_content_hashes();
  stream.viewed_content_hashes.clear();

  // Cap the most recent list.
  size_t max_size = static_cast<size_t>(
      GetFeedConfig().max_most_recent_viewed_content_hashes);
  if (most_recent_viewed_content_hashes.size() > max_size) {
    most_recent_viewed_content_hashes.erase(
        most_recent_viewed_content_hashes.begin(),
        most_recent_viewed_content_hashes.begin() +
            (most_recent_viewed_content_hashes.size() - max_size));
  }
  metadata.mutable_most_recent_viewed_content_hashes()->Assign(
      most_recent_viewed_content_hashes.begin(),
      most_recent_viewed_content_hashes.end());

  SetMetadata(metadata);
}

void FeedStream::AddViewedContentHashes(const feedstore::Content& content) {
  // Count only the 1st item in the collection.
  if (content.prefetch_metadata_size() == 0)
    return;
  StreamType stream_type = StreamType(StreamKind::kForYou);
  const auto& prefetch_metadata = content.prefetch_metadata(0);
  int32_t content_hash =
      feedstore::ContentHashFromPrefetchMetadata(prefetch_metadata);
  if (!content_hash)
    return;
  Stream& stream = GetStream(stream_type);
  if (!stream.viewed_content_hashes.contains(content_hash)) {
    feedstore::Metadata metadata = GetMetadata();
    stream.viewed_content_hashes.insert(content_hash);
    feedstore::Metadata::StreamMetadata& stream_metadata =
        MetadataForStream(metadata, stream_type);
    stream_metadata.add_viewed_content_hashes(content_hash);
    SetMetadata(metadata);
  }
}

}  // namespace feed
