// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_data_type_processor.h"

#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/data_type_histogram.h"
#include "components/sync/base/features.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/commit_queue.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/engine/data_type_processor_metrics.h"
#include "components/sync/engine/data_type_processor_proxy.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/model/type_entities_count.h"
#include "components/sync/protocol/bookmark_model_metadata.pb.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/data_type_state_helper.h"
#include "components/sync/protocol/proto_value_conversions.h"
#include "components/sync_bookmarks/bookmark_local_changes_builder.h"
#include "components/sync_bookmarks/bookmark_model_merger.h"
#include "components/sync_bookmarks/bookmark_model_observer_impl.h"
#include "components/sync_bookmarks/bookmark_model_view.h"
#include "components/sync_bookmarks/bookmark_remote_updates_handler.h"
#include "components/sync_bookmarks/bookmark_specifics_conversions.h"
#include "components/sync_bookmarks/parent_guid_preprocessing.h"
#include "components/sync_bookmarks/synced_bookmark_tracker.h"
#include "components/sync_bookmarks/synced_bookmark_tracker_entity.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "ui/base/models/tree_node_iterator.h"

namespace sync_bookmarks {

namespace {

// Expiration period for the error state when the initial download of remote
// bookmarks exceeds the limit. After this period, a new attempt to download all
// bookmarks is made.
constexpr base::TimeDelta kInitialMergeRemoteUpdatesExceededLimitErrorTtl =
    base::Days(30);

// Jitter to be subtracted from
// `kInitialMergeRemoteUpdatesExceededLimitErrorTtl` to add randomness and avoid
// that all clients attempt to redownload bookmarks at the same time.
constexpr base::TimeDelta
    kInitialMergeRemoteUpdatesExceededLimitErrorTtlJitter = base::Days(7);

class ScopedRemoteUpdateBookmarks {
 public:
  // `bookmark_model` and `observer` must not be null and must outlive this
  // object.
  ScopedRemoteUpdateBookmarks(BookmarkModelView* bookmark_model,
                              bookmarks::BookmarkModelObserver* observer)
      : bookmark_model_(bookmark_model), observer_(observer) {
    // Notify UI intensive observers of BookmarkModel that we are about to make
    // potentially significant changes to it, so the updates may be batched. For
    // example, on Mac, the bookmarks bar displays animations when bookmark
    // items are added or deleted.
    DCHECK(bookmark_model_);
    bookmark_model_->BeginExtensiveChanges();
    // Shouldn't be notified upon changes due to sync.
    bookmark_model_->RemoveObserver(observer_);
  }

  ScopedRemoteUpdateBookmarks(const ScopedRemoteUpdateBookmarks&) = delete;
  ScopedRemoteUpdateBookmarks& operator=(const ScopedRemoteUpdateBookmarks&) =
      delete;

  ~ScopedRemoteUpdateBookmarks() {
    // Notify UI intensive observers of BookmarkModel that all updates have been
    // applied, and that they may now be consumed. This prevents issues like the
    // one described in https://crbug.com/281562, where old and new items on the
    // bookmarks bar would overlap.
    bookmark_model_->EndExtensiveChanges();
    bookmark_model_->AddObserver(observer_);
  }

 private:
  const raw_ptr<BookmarkModelView> bookmark_model_;
  const raw_ptr<bookmarks::BookmarkModelObserver> observer_;
};

std::string ComputeServerDefinedUniqueTagForDebugging(
    const bookmarks::BookmarkNode* node,
    const BookmarkModelView* model) {
  if (node == model->bookmark_bar_node()) {
    return "bookmark_bar";
  }
  if (node == model->other_node()) {
    return "other_bookmarks";
  }
  if (node == model->mobile_node()) {
    return "synced_bookmarks";
  }
  return "";
}

size_t CountSyncableBookmarksFromModel(const BookmarkModelView* model) {
  size_t count = 0;
  ui::TreeNodeIterator<const bookmarks::BookmarkNode> iterator(
      model->root_node());
  // Does not count the root node.
  while (iterator.has_next()) {
    const bookmarks::BookmarkNode* node = iterator.Next();
    if (model->IsNodeSyncable(node)) {
      ++count;
    }
  }
  return count;
}

void RecordDataTypeNumUnsyncedEntitiesOnModelReadyForBookmarks(
    const SyncedBookmarkTracker& tracker) {
  syncer::SyncRecordDataTypeNumUnsyncedEntitiesFromDataCounts(
      syncer::UnsyncedDataRecordingEvent::kOnModelReady,
      {{syncer::BOOKMARKS, tracker.GetUnsyncedDataCount()}});
}

// Returns whether `gc_directive` has a version_watermark based GC directive,
// which indicates to clear all sync data that's stored locally.
bool HasClearAllDirective(
    const std::optional<sync_pb::GarbageCollectionDirective>& gc_directive) {
  return gc_directive.has_value() && gc_directive->has_version_watermark();
}

}  // namespace

BookmarkDataTypeProcessor::BookmarkDataTypeProcessor(
    syncer::WipeModelUponSyncDisabledBehavior
        wipe_model_upon_sync_disabled_behavior)
    : wipe_model_upon_sync_disabled_behavior_(
          wipe_model_upon_sync_disabled_behavior) {}

BookmarkDataTypeProcessor::~BookmarkDataTypeProcessor() {
  if (bookmark_model_ && bookmark_model_observer_) {
    bookmark_model_->RemoveObserver(bookmark_model_observer_.get());
  }
}

void BookmarkDataTypeProcessor::ConnectSync(
    std::unique_ptr<syncer::CommitQueue> worker) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!worker_);
  DCHECK(bookmark_model_);

  worker_ = std::move(worker);

  // `bookmark_tracker_` is instantiated only after initial sync is done.
  if (bookmark_tracker_) {
    NudgeForCommitIfNeeded();
  }
}

void BookmarkDataTypeProcessor::DisconnectSync() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  weak_ptr_factory_for_worker_.InvalidateWeakPtrs();
  if (!worker_) {
    return;
  }

  DVLOG(1) << "Disconnecting sync for Bookmarks";
  worker_.reset();
}

void BookmarkDataTypeProcessor::GetLocalChanges(
    size_t max_entries,
    GetLocalChangesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Processor should never connect if
  // `initial_merge_remote_updates_exceeded_limit_timestamp_` is set.
  DCHECK(!initial_merge_remote_updates_exceeded_limit_timestamp_);
  BookmarkLocalChangesBuilder builder(bookmark_tracker_.get(), bookmark_model_);
  std::move(callback).Run(builder.BuildCommitRequests(max_entries));
}

void BookmarkDataTypeProcessor::OnCommitCompleted(
    const sync_pb::DataTypeState& type_state,
    const syncer::CommitResponseDataList& committed_response_list,
    const syncer::FailedCommitResponseDataList& error_response_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // `error_response_list` is ignored, because all errors are treated as
  // transient and the processor with eventually retry.
  for (const syncer::CommitResponseData& response : committed_response_list) {
    const SyncedBookmarkTrackerEntity* entity =
        bookmark_tracker_->GetEntityForClientTagHash(response.client_tag_hash);
    if (!entity) {
      DLOG(WARNING) << "Received a commit response for an unknown entity.";
      continue;
    }

    bookmark_tracker_->UpdateUponCommitResponse(entity, response.id,
                                                response.response_version,
                                                response.sequence_number);
  }

  bookmark_tracker_->set_data_type_state(type_state);
  schedule_save_closure_.Run();
}

void BookmarkDataTypeProcessor::OnUpdateReceived(
    const sync_pb::DataTypeState& data_type_state,
    syncer::UpdateResponseDataList updates,
    std::optional<sync_pb::GarbageCollectionDirective> gc_directive) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!data_type_state.cache_guid().empty());
  CHECK_EQ(data_type_state.cache_guid(), activation_request_.cache_guid);
  DCHECK(syncer::IsInitialSyncDone(data_type_state.initial_sync_state()));
  DCHECK(start_callback_.is_null());
  // Processor should never connect if
  // `initial_merge_remote_updates_exceeded_limit_timestamp_` is set.
  DCHECK(!initial_merge_remote_updates_exceeded_limit_timestamp_);

  // Clients before M94 did not populate the parent UUID in specifics.
  PopulateParentGuidInSpecifics(bookmark_tracker_.get(), &updates);

  if (!bookmark_tracker_) {
    OnInitialUpdateReceived(data_type_state, std::move(updates));
  } else if (HasClearAllDirective(gc_directive)) {
    ApplyFullUpdateAsIncrementalUpdate(data_type_state, std::move(updates));
  } else {
    OnIncrementalUpdateReceived(data_type_state, std::move(updates));
  }
}

void BookmarkDataTypeProcessor::StorePendingInvalidations(
    std::vector<sync_pb::DataTypeState::Invalidation> invalidations_to_store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!bookmark_tracker_) {
    // It's possible to receive invalidations while bookmarks are not syncing,
    // e.g. if invalidation system is initialized earlier than bookmark model.
    return;
  }
  sync_pb::DataTypeState data_type_state = bookmark_tracker_->data_type_state();
  data_type_state.mutable_invalidations()->Assign(
      invalidations_to_store.begin(), invalidations_to_store.end());
  bookmark_tracker_->set_data_type_state(data_type_state);
  schedule_save_closure_.Run();
}

bool BookmarkDataTypeProcessor::IsTrackingMetadata() const {
  return bookmark_tracker_.get() != nullptr;
}

const SyncedBookmarkTracker* BookmarkDataTypeProcessor::GetTrackerForTest()
    const {
  return bookmark_tracker_.get();
}

bool BookmarkDataTypeProcessor::IsConnectedForTest() const {
  return worker_ != nullptr;
}

std::string BookmarkDataTypeProcessor::EncodeSyncMetadata() const {
  std::string metadata_str;
  if (bookmark_tracker_) {
    // `initial_merge_remote_updates_exceeded_limit_timestamp_` is only set
    // in error cases where the tracker would not be initialized.
    DCHECK(!initial_merge_remote_updates_exceeded_limit_timestamp_);

    sync_pb::BookmarkModelMetadata model_metadata =
        bookmark_tracker_->BuildBookmarkModelMetadata();
    // Ensure that BuildBookmarkModelMetadata() never populates this field.
    DCHECK(
        !model_metadata.has_last_initial_merge_remote_updates_exceeded_limit());
    DCHECK(
        !model_metadata
             .has_initial_merge_remote_updates_exceeded_limit_timestamp_windows_epoch_micros());
    model_metadata.SerializeToString(&metadata_str);
  } else if (initial_merge_remote_updates_exceeded_limit_timestamp_) {
    sync_pb::BookmarkModelMetadata model_metadata;

    model_metadata
        .set_initial_merge_remote_updates_exceeded_limit_timestamp_windows_epoch_micros(
            initial_merge_remote_updates_exceeded_limit_timestamp_
                ->ToDeltaSinceWindowsEpoch()
                .InMicroseconds());
    model_metadata.SerializeToString(&metadata_str);
  }
  return metadata_str;
}

void BookmarkDataTypeProcessor::MigrateLegacyExceededLimitError(
    sync_pb::BookmarkModelMetadata* model_metadata) {
  if (!model_metadata->last_initial_merge_remote_updates_exceeded_limit() ||
      model_metadata
          ->has_initial_merge_remote_updates_exceeded_limit_timestamp_windows_epoch_micros()) {
    model_metadata->clear_last_initial_merge_remote_updates_exceeded_limit();
    return;
  }

  // For legacy clients, set a random timestamp from 23-30 days ago to
  // represent the error state. This is to preserve the error across restarts.
  // This will also be used to decide whether to reset the error.
  const base::Time limit_set_time =
      base::Time::Now() - kInitialMergeRemoteUpdatesExceededLimitErrorTtl +
      base::RandTimeDeltaUpTo(
          kInitialMergeRemoteUpdatesExceededLimitErrorTtlJitter);
  model_metadata
      ->set_initial_merge_remote_updates_exceeded_limit_timestamp_windows_epoch_micros(
          limit_set_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  // Clear the legacy field as it is no longer needed.
  model_metadata->clear_last_initial_merge_remote_updates_exceeded_limit();
  schedule_save_closure_.Run();
}

void BookmarkDataTypeProcessor::MaybeResetExceededLimitError(
    sync_pb::BookmarkModelMetadata* model_metadata) {
  if (!base::FeatureList::IsEnabled(
          syncer::kSyncResetBookmarksInitialMergeLimitExceededError)) {
    return;
  }
  if (!model_metadata
           ->has_initial_merge_remote_updates_exceeded_limit_timestamp_windows_epoch_micros()) {
    return;
  }

  const base::Time limit_set_time =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(
          model_metadata
              ->initial_merge_remote_updates_exceeded_limit_timestamp_windows_epoch_micros()));

  // For users who have a timestamp, reset the error
  // after 30 days to give users a chance to recover.
  if (base::Time::Now() - limit_set_time >
      kInitialMergeRemoteUpdatesExceededLimitErrorTtl) {
    model_metadata->clear_last_initial_merge_remote_updates_exceeded_limit();
    model_metadata
        ->clear_initial_merge_remote_updates_exceeded_limit_timestamp_windows_epoch_micros();
    schedule_save_closure_.Run();
  }
}

bool BookmarkDataTypeProcessor::HandlePreviousErrorState(
    const sync_pb::BookmarkModelMetadata& model_metadata) {
  if (!model_metadata
           .has_initial_merge_remote_updates_exceeded_limit_timestamp_windows_epoch_micros()) {
    return false;
  }
  // Report error if remote updates fetched last time during initial merge
  // exceeded limit. Note that here we are only setting
  // `last_initial_merge_remote_updates_exceeded_limit_timestamp_`, the
  // actual error would be reported in ConnectIfReady().
  initial_merge_remote_updates_exceeded_limit_timestamp_ =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(
          model_metadata
              .initial_merge_remote_updates_exceeded_limit_timestamp_windows_epoch_micros()));
  return true;
}

std::optional<sync_pb::BookmarkModelMetadata>
BookmarkDataTypeProcessor::ParseAndValidateMetadata(
    const std::string& metadata_str) {
  if (HandlePendingClearMetadata(metadata_str)) {
    return std::nullopt;
  }

  sync_pb::BookmarkModelMetadata model_metadata;
  model_metadata.ParseFromString(metadata_str);

  MigrateLegacyExceededLimitError(&model_metadata);
  // Ensure that the legacy field is not set, as it should have been migrated.
  CHECK(!model_metadata.last_initial_merge_remote_updates_exceeded_limit());
  MaybeResetExceededLimitError(&model_metadata);

  if (HandlePreviousErrorState(model_metadata)) {
    return std::nullopt;
  }
  return model_metadata;
}

void BookmarkDataTypeProcessor::InitTracker(
    sync_pb::BookmarkModelMetadata model_metadata,
    const std::string& metadata_str) {
  bookmark_tracker_ = SyncedBookmarkTracker::CreateFromBookmarkModelAndMetadata(
      bookmark_model_, std::move(model_metadata));

  if (bookmark_tracker_) {
    StartTrackingMetadata();
    RecordDataTypeNumUnsyncedEntitiesOnModelReadyForBookmarks(
        *bookmark_tracker_);
  } else if (!metadata_str.empty()) {
    DLOG(WARNING)
        << "Persisted bookmark sync metadata invalidated when loading.";
    // Schedule a save to make sure the corrupt metadata is deleted from
    // disk as soon as possible, to avoid reporting again after restart if
    // nothing else schedules a save meanwhile (which is common if sync is
    // not running properly, e.g. auth error).
    schedule_save_closure_.Run();
  }
}

bool BookmarkDataTypeProcessor::HandlePendingClearMetadata(
    const std::string& metadata_str) {
  if (!pending_clear_metadata_) {
    return false;
  }

  pending_clear_metadata_ = false;
  // Schedule save empty metadata, if not already empty.
  if (!metadata_str.empty()) {
    LogClearMetadataWhileStoppedHistogram(syncer::BOOKMARKS,
                                          /*is_delayed_call=*/true);
    schedule_save_closure_.Run();
  }
  return true;
}

void BookmarkDataTypeProcessor::ModelReadyToSync(
    const std::string& metadata_str,
    const base::RepeatingClosure& schedule_save_closure,
    BookmarkModelView* model) {
  DCHECK(model);
  DCHECK(!bookmark_model_);
  DCHECK(!bookmark_tracker_);
  DCHECK(!bookmark_model_observer_);

  TRACE_EVENT0("sync", "BookmarkDataTypeProcessor::ModelReadyToSync");

  bookmark_model_ = model;
  schedule_save_closure_ = schedule_save_closure;

  std::optional<sync_pb::BookmarkModelMetadata> model_metadata =
      ParseAndValidateMetadata(metadata_str);

  if (model_metadata) {
    InitTracker(std::move(*model_metadata), metadata_str);
  }

  // Post a task instead of invoking ConnectIfReady() immediately to avoid
  // sophisticated operations while BookmarkModel is being loaded. In
  // particular, cache GUID mismatches (edge case) lead to deleting account
  // bookmarks.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&BookmarkDataTypeProcessor::ConnectIfReady,
                                weak_ptr_factory_for_controller_.GetWeakPtr()));
}

void BookmarkDataTypeProcessor::SetFaviconService(
    favicon::FaviconService* favicon_service) {
  DCHECK(favicon_service);
  favicon_service_ = favicon_service;
}

size_t BookmarkDataTypeProcessor::EstimateMemoryUsage() const {
  using base::trace_event::EstimateMemoryUsage;
  size_t memory_usage = 0;
  if (bookmark_tracker_) {
    memory_usage += bookmark_tracker_->EstimateMemoryUsage();
  }
  memory_usage += EstimateMemoryUsage(activation_request_.cache_guid);
  return memory_usage;
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
BookmarkDataTypeProcessor::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_ptr_factory_for_controller_.GetWeakPtr();
}

void BookmarkDataTypeProcessor::OnSyncStarting(
    const syncer::DataTypeActivationRequest& request,
    StartCallback start_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(start_callback);
  CHECK(request.IsValid());
  CHECK(!request.cache_guid.empty());
  // `favicon_service_` should have been set by now.
  CHECK(favicon_service_);
  DVLOG(1) << "Sync is starting for Bookmarks";

  start_callback_ = std::move(start_callback);
  activation_request_ = request;

  ConnectIfReady();
}

void BookmarkDataTypeProcessor::ConnectIfReady() {
  // Return if the model isn't ready.
  if (!bookmark_model_) {
    return;
  }
  // Return if Sync didn't start yet, or ConnectIfReady() already succeeded
  // before.
  if (!start_callback_) {
    return;
  }

  DCHECK(activation_request_.error_handler);
  // ConnectSync() should not have been called by now.
  DCHECK(!worker_);

  // Report error if remote updates fetched last time during initial merge
  // exceeded limit.
  if (initial_merge_remote_updates_exceeded_limit_timestamp_) {
    // `initial_merge_remote_updates_exceeded_limit_timestamp_` is only set
    // in error case and thus tracker should be empty.
    DCHECK(!bookmark_tracker_);
    start_callback_.Reset();
    activation_request_.error_handler.Run(syncer::ModelError(
        FROM_HERE, syncer::ModelError::Type::
                       kBookmarksRemoteCountExceededLimitLastInitialMerge));
    return;
  }

  if (MaybeReportBookmarkCountLimitExceededError(
          syncer::ModelError::Type::
              kBookmarksLocalCountExceededLimitOnSyncStart)) {
    return;
  }

  DCHECK(!activation_request_.cache_guid.empty());

  if (bookmark_tracker_ && bookmark_tracker_->data_type_state().cache_guid() !=
                               activation_request_.cache_guid) {
    // In case of a cache guid mismatch, treat it as a corrupted metadata and
    // start clean.
    StopTrackingMetadataAndResetTracker();
  }

  auto activation_context =
      std::make_unique<syncer::DataTypeActivationResponse>();
  if (bookmark_tracker_) {
    activation_context->data_type_state = bookmark_tracker_->data_type_state();
  } else {
    sync_pb::DataTypeState data_type_state;
    data_type_state.mutable_progress_marker()->set_data_type_id(
        GetSpecificsFieldNumberFromDataType(syncer::BOOKMARKS));
    data_type_state.set_cache_guid(activation_request_.cache_guid);
    activation_context->data_type_state = data_type_state;
  }
  activation_context->type_processor =
      std::make_unique<syncer::DataTypeProcessorProxy>(
          weak_ptr_factory_for_worker_.GetWeakPtr(),
          base::SequencedTaskRunner::GetCurrentDefault());
  std::move(start_callback_).Run(std::move(activation_context));
}

bool BookmarkDataTypeProcessor::DoesCountExceedBookmarksSyncLimit(
    size_t count,
    size_t offset) const {
  if (sync_bookmarks_limit_for_tests_.has_value()) {
    return count > sync_bookmarks_limit_for_tests_.value() + offset;
  }
  // Count is less than the default limit so should not bother checking against
  // `kSyncBookmarksLimitValue` which is bound to be >= the default limit.
  if (count <= syncer::kDefaultSyncBookmarksLimit + offset) {
    return false;
  }
  return count > syncer::kSyncBookmarksLimitValue.Get() + offset;
}

bool BookmarkDataTypeProcessor::MaybeReportBookmarkCountLimitExceededError(
    syncer::ModelError::Type error_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If `activation_request_.error_handler` is not set, the check is ignored
  // because this gets re-evaluated in ConnectIfReady().
  if (!activation_request_.error_handler) {
    return false;
  }

  const size_t count = bookmark_tracker_
                           ? bookmark_tracker_->TrackedBookmarksCount()
                           : CountSyncableBookmarksFromModel(bookmark_model_);
  if (DoesCountExceedBookmarksSyncLimit(count)) {
    // For the case where a tracker already
    // exists, local changes will continue
    // to be tracked in order order to allow users to delete bookmarks and
    // recover upon restart.
    DisconnectSync();
    start_callback_.Reset();

    activation_request_.error_handler.Run(
        syncer::ModelError(FROM_HERE, error_type));
    return true;
  }
  return false;
}

void BookmarkDataTypeProcessor::OnSyncStopping(
    syncer::SyncStopMetadataFate metadata_fate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Disabling sync for a type shouldn't happen before the model is loaded
  // because OnSyncStopping() is not allowed to be called before
  // OnSyncStarting() has completed.
  DCHECK(bookmark_model_);
  DCHECK(!start_callback_);

  activation_request_ = syncer::DataTypeActivationRequest{};

  DisconnectSync();

  switch (metadata_fate) {
    case syncer::KEEP_METADATA: {
      break;
    }

    case syncer::CLEAR_METADATA: {
      // Stop observing local changes. We'll start observing local changes again
      // when Sync is (re)started in StartTrackingMetadata(). This is only
      // necessary if a tracker exists, which also means local changes are being
      // tracked (see StartTrackingMetadata()).
      if (bookmark_tracker_) {
        StopTrackingMetadataAndResetTracker();
      }
      initial_merge_remote_updates_exceeded_limit_timestamp_.reset();
      schedule_save_closure_.Run();
      break;
    }
  }

  // Do not let any delayed callbacks be called.
  weak_ptr_factory_for_controller_.InvalidateWeakPtrs();
}

void BookmarkDataTypeProcessor::NudgeForCommitIfNeeded() {
  DCHECK(bookmark_tracker_);

  if (MaybeReportBookmarkCountLimitExceededError(
          syncer::ModelError::Type::
              kBookmarksLocalCountExceededLimitNudgeForCommit)) {
    return;
  }

  // Don't bother sending anything if there's no one to send to.
  if (!worker_) {
    return;
  }

  // Nudge worker if there are any entities with local changes.
  if (bookmark_tracker_->HasLocalChanges()) {
    worker_->NudgeForCommit();
  }
}

void BookmarkDataTypeProcessor::OnBookmarkModelBeingDeleted() {
  DCHECK(bookmark_model_);
  DCHECK(bookmark_model_observer_);

  bookmark_model_->RemoveObserver(bookmark_model_observer_.get());
  bookmark_model_ = nullptr;
  bookmark_model_observer_.reset();

  DisconnectSync();
}

void BookmarkDataTypeProcessor::OnInitialUpdateReceived(
    const sync_pb::DataTypeState& data_type_state,
    syncer::UpdateResponseDataList updates) {
  DCHECK(!bookmark_tracker_);
  DCHECK(activation_request_.error_handler);

  TRACE_EVENT0("sync", "BookmarkDataTypeProcessor::OnInitialUpdateReceived");

  // Report error if count of remote updates is more than the limit.
  // `updates` can contain an additional root folder. The server may or may not
  // deliver a root node - it is not guaranteed, but this works as an
  // approximated safeguard.
  // Note that we are not having this check for incremental updates as it is
  // very unlikely that there will be many updates downloaded.
  if (DoesCountExceedBookmarksSyncLimit(updates.size(), /*offset=*/1)) {
    DisconnectSync();
    initial_merge_remote_updates_exceeded_limit_timestamp_ = base::Time::Now();
    activation_request_.error_handler.Run(syncer::ModelError(
        FROM_HERE, syncer::ModelError::Type::
                       kBookmarksRemoteCountExceededLimitInitialMerge));
    schedule_save_closure_.Run();
    return;
  }

  bookmark_tracker_ = SyncedBookmarkTracker::CreateEmpty(data_type_state);
  StartTrackingMetadata();

  {
    ScopedRemoteUpdateBookmarks update_bookmarks(
        bookmark_model_, bookmark_model_observer_.get());

    bookmark_model_->EnsurePermanentNodesExist();
    BookmarkModelMerger model_merger(std::move(updates), bookmark_model_,
                                     favicon_service_, bookmark_tracker_.get());
    model_merger.Merge();
  }

  // If any of the permanent nodes is missing, we treat it as failure.
  if (!bookmark_tracker_->GetEntityForBookmarkNode(
          bookmark_model_->bookmark_bar_node()) ||
      !bookmark_tracker_->GetEntityForBookmarkNode(
          bookmark_model_->other_node()) ||
      !bookmark_tracker_->GetEntityForBookmarkNode(
          bookmark_model_->mobile_node())) {
    DisconnectSync();
    StopTrackingMetadataAndResetTracker();
    activation_request_.error_handler.Run(syncer::ModelError(
        FROM_HERE, syncer::ModelError::Type::
                       kBookmarksInitialMergePermanentEntitiesMissing));
    return;
  }

  bookmark_tracker_->CheckAllNodesTracked(bookmark_model_);

  if (base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    bookmark_model_->MaybeRemoveUnderlyingModelDuplicatesUponInitialSync();
  }

  LogDataTypeConfigurationTime(syncer::BOOKMARKS, activation_request_.sync_mode,
                               activation_request_.configuration_start_time);

  schedule_save_closure_.Run();
  NudgeForCommitIfNeeded();
}

void BookmarkDataTypeProcessor::OnIncrementalUpdateReceived(
    const sync_pb::DataTypeState& type_state,
    syncer::UpdateResponseDataList updates) {
  {
    ScopedRemoteUpdateBookmarks update_bookmarks(
        bookmark_model_, bookmark_model_observer_.get());
    BookmarkRemoteUpdatesHandler updates_handler(
        bookmark_model_, favicon_service_, bookmark_tracker_.get());

    const bool got_new_encryption_requirements =
        bookmark_tracker_->data_type_state().encryption_key_name() !=
        type_state.encryption_key_name();
    bookmark_tracker_->set_data_type_state(type_state);
    updates_handler.Process(updates, got_new_encryption_requirements);
  }

  if (MaybeReportBookmarkCountLimitExceededError(
          syncer::ModelError::Type::
              kBookmarksLocalCountExceededLimitOnUpdateReceived)) {
    return;
  }

  if (bookmark_tracker_->ReuploadBookmarksOnLoadIfNeeded()) {
    NudgeForCommitIfNeeded();
  }

  // There are cases when we receive non-empty updates that don't result in
  // model changes (e.g. reflections). In that case, issue a write to persist
  // the progress marker in order to avoid downloading those updates again.
  if (!updates.empty()) {
    // Schedule save just in case one is needed.
    schedule_save_closure_.Run();
  }
}

void BookmarkDataTypeProcessor::ApplyFullUpdateAsIncrementalUpdate(
    const sync_pb::DataTypeState& type_state,
    syncer::UpdateResponseDataList updates) {
  absl::flat_hash_set<const SyncedBookmarkTrackerEntity*> updated_entities;
  for (const syncer::UpdateResponseData& update : updates) {
    bool should_ignore_update = false;
    const SyncedBookmarkTrackerEntity* tracked_entity =
        BookmarkRemoteUpdatesHandler::DetermineLocalTrackedEntityToUpdate(
            bookmark_tracker_.get(), update.entity, &should_ignore_update);
    if (tracked_entity) {
      // If the update is invalid and should be ignored, there should be no
      // `tracked_entity`.
      CHECK(!should_ignore_update);
      updated_entities.insert(tracked_entity);
    }
  }

  // Simulate the deletion of all entities that are not in the update (and
  // synced).
  for (const SyncedBookmarkTrackerEntity* entity :
       bookmark_tracker_->GetAllEntities()) {
    // Don't create deletions for permanent nodes.
    if (entity->bookmark_node()->is_permanent_node()) {
      continue;
    }
    if (entity->IsUnsyncedLocalCreation()) {
      // Special case a local creation to avoid generating a deletion.
      // Otherwise, it would result in a conflict with a remote deletion
      // which is not real, polluting UMA metrics. This would still result
      // in keeping the local creation but it'd be fragile and non-obvious.
      continue;
    }

    // Do not handle local updates and deletions explicitly. Consider the
    // following scenarios:
    // 1. Local update, remote entity still exists. A deletion won't be
    //    generated in this case, so it's a normal conflict.
    // 2. Local update, remote entity deleted. A deletion will be generated
    //    but the local update will be preferred during conflict resolution.
    // 3. Local deletion, remote entity deleted. A deletion will be
    //    generated in this case, so it's a normal conflict resulting in a
    //    no-op for the bridge.
    // 4. Local deletion, remote entity still exists. This case will result
    //    in restoring the entity during conflict resolution. It's not ideal
    //    but safer than data loss.
    // TODO(crbug.com/40668179): Improve handling of local deletions during
    // full updates.
    if (updated_entities.contains(entity)) {
      // Consider this as a normal incremental update. Note that this update
      // might be dropped due to the version having been seen before.
      continue;
    }

    syncer::UpdateResponseData deletion;
    deletion.entity.id = entity->metadata().server_id();
    deletion.entity.client_tag_hash = entity->GetClientTagHash();
    deletion.entity.creation_time =
        syncer::ProtoTimeToTime(entity->metadata().creation_time());
    deletion.entity.modification_time =
        syncer::ProtoTimeToTime(entity->metadata().modification_time());
    deletion.entity.name = "tombstone";

    // Increment the version to ensure that the deletion is not immediately
    // ignored.
    deletion.response_version = entity->metadata().server_version() + 1;
    updates.push_back(std::move(deletion));
  }

  OnIncrementalUpdateReceived(type_state, std::move(updates));
}

void BookmarkDataTypeProcessor::StartTrackingMetadata() {
  DCHECK(bookmark_tracker_);
  DCHECK(!bookmark_model_observer_);

  bookmark_model_observer_ = std::make_unique<BookmarkModelObserverImpl>(
      bookmark_model_,
      base::BindRepeating(&BookmarkDataTypeProcessor::NudgeForCommitIfNeeded,
                          base::Unretained(this)),
      base::BindOnce(&BookmarkDataTypeProcessor::OnBookmarkModelBeingDeleted,
                     base::Unretained(this)),
      bookmark_tracker_.get());
  bookmark_model_->AddObserver(bookmark_model_observer_.get());
}

void BookmarkDataTypeProcessor::GetUnsyncedDataCount(
    base::OnceCallback<void(size_t)> callback) {
  std::move(callback).Run(
      bookmark_tracker_ ? bookmark_tracker_->GetUnsyncedDataCount() : 0);
}

void BookmarkDataTypeProcessor::GetAllNodesForDebugging(
    AllNodesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(bookmark_model_);

  base::Value::List all_nodes;

  // Create a permanent folder since sync server no longer create root folders,
  // and USS won't migrate root folders from directory, we create root folders.

  // Function isTypeRootNode in sync_node_browser.js use PARENT_ID and
  // UNIQUE_SERVER_TAG to check if the node is root node. isChildOf in
  // sync_node_browser.js uses dataType to check if root node is parent of real
  // data node. NON_UNIQUE_NAME will be the name of node to display.
  auto root_node = base::Value::Dict()
                       .Set("ID", "BOOKMARKS_ROOT")
                       .Set("PARENT_ID", "r")
                       .Set("UNIQUE_SERVER_TAG", "Bookmarks")
                       .Set("IS_DIR", true)
                       .Set("dataType", "Bookmarks")
                       .Set("NON_UNIQUE_NAME", "Bookmarks");
  all_nodes.Append(std::move(root_node));

  const bookmarks::BookmarkNode* model_root_node = bookmark_model_->root_node();
  int i = 0;
  for (const auto& child : model_root_node->children()) {
    if (bookmark_model_->IsNodeSyncable(child.get())) {
      AppendNodeAndChildrenForDebugging(child.get(), i++, &all_nodes);
    }
  }

  std::move(callback).Run(std::move(all_nodes));
}

void BookmarkDataTypeProcessor::AppendNodeAndChildrenForDebugging(
    const bookmarks::BookmarkNode* node,
    int index,
    base::Value::List* all_nodes) const {
  const SyncedBookmarkTrackerEntity* entity =
      bookmark_tracker_->GetEntityForBookmarkNode(node);
  // Include only tracked nodes. Newly added nodes are tracked even before being
  // sent to the server. Managed bookmarks (that are installed by a policy)
  // aren't syncable and hence not tracked.
  if (!entity) {
    return;
  }
  const sync_pb::EntityMetadata& metadata = entity->metadata();
  // Copy data to an EntityData object to reuse its conversion
  // ToDictionaryValue() methods.
  syncer::EntityData data;
  data.id = metadata.server_id();
  data.creation_time = node->date_added();
  data.modification_time =
      syncer::ProtoTimeToTime(metadata.modification_time());
  data.name = base::UTF16ToUTF8(node->GetTitle());
  data.specifics = CreateSpecificsFromBookmarkNode(
      node, bookmark_model_, metadata.unique_position(),
      /*force_favicon_load=*/false);

  if (node->is_permanent_node()) {
    data.server_defined_unique_tag =
        ComputeServerDefinedUniqueTagForDebugging(node, bookmark_model_);
    // Set the parent to empty string to indicate it's parent of the root node
    // for bookmarks. The code in sync_node_browser.js links nodes with the
    // "dataType" when they are lacking a parent id.
    data.legacy_parent_id = "";
  } else {
    const bookmarks::BookmarkNode* parent = node->parent();
    const SyncedBookmarkTrackerEntity* parent_entity =
        bookmark_tracker_->GetEntityForBookmarkNode(parent);
    DCHECK(parent_entity);
    data.legacy_parent_id = parent_entity->metadata().server_id();
  }

  base::Value::Dict data_dictionary = data.ToDictionaryValue();
  // Set ID value as in legacy directory-based implementation, "s" means server.
  data_dictionary.Set("ID", "s" + metadata.server_id());
  if (node->is_permanent_node()) {
    // Hardcode the parent of permanent nodes.
    data_dictionary.Set("PARENT_ID", "BOOKMARKS_ROOT");
    data_dictionary.Set("UNIQUE_SERVER_TAG", data.server_defined_unique_tag);
  } else {
    data_dictionary.Set("PARENT_ID", "s" + data.legacy_parent_id);
  }
  data_dictionary.Set("LOCAL_EXTERNAL_ID", static_cast<int>(node->id()));
  data_dictionary.Set("positionIndex", index);
  data_dictionary.Set("metadata", syncer::EntityMetadataToValue(metadata));
  data_dictionary.Set("dataType", "Bookmarks");
  data_dictionary.Set("IS_DIR", node->is_folder());
  all_nodes->Append(std::move(data_dictionary));

  int i = 0;
  for (const auto& child : node->children()) {
    AppendNodeAndChildrenForDebugging(child.get(), i++, all_nodes);
  }
}

void BookmarkDataTypeProcessor::GetTypeEntitiesCountForDebugging(
    base::OnceCallback<void(const syncer::TypeEntitiesCount&)> callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  syncer::TypeEntitiesCount count(syncer::BOOKMARKS);
  if (bookmark_tracker_) {
    count.non_tombstone_entities = bookmark_tracker_->TrackedBookmarksCount();
    count.entities = count.non_tombstone_entities +
                     bookmark_tracker_->TrackedUncommittedTombstonesCount();
  }
  std::move(callback).Run(count);
}

void BookmarkDataTypeProcessor::RecordMemoryUsageAndCountsHistograms() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SyncRecordDataTypeMemoryHistogram(syncer::BOOKMARKS, EstimateMemoryUsage());
  if (bookmark_tracker_) {
    SyncRecordDataTypeCountHistogram(
        syncer::BOOKMARKS, bookmark_tracker_->TrackedBookmarksCount());
  } else {
    SyncRecordDataTypeCountHistogram(syncer::BOOKMARKS, 0);
  }
}

void BookmarkDataTypeProcessor::SetMaxBookmarksTillSyncEnabledForTest(
    size_t limit) {
  sync_bookmarks_limit_for_tests_ = limit;
}

void BookmarkDataTypeProcessor::ClearMetadataIfStopped() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If Sync is not actually stopped, ignore this call.
  if (!activation_request_.cache_guid.empty()) {
    return;
  }

  if (!bookmark_model_) {
    // Defer the clearing until ModelReadyToSync() is invoked.
    pending_clear_metadata_ = true;
    return;
  }
  if (bookmark_tracker_) {
    LogClearMetadataWhileStoppedHistogram(syncer::BOOKMARKS,
                                          /*is_delayed_call=*/false);
    StopTrackingMetadataAndResetTracker();
    // Schedule save empty metadata.
    schedule_save_closure_.Run();
  } else if (initial_merge_remote_updates_exceeded_limit_timestamp_) {
    LogClearMetadataWhileStoppedHistogram(syncer::BOOKMARKS,
                                          /*is_delayed_call=*/false);
    initial_merge_remote_updates_exceeded_limit_timestamp_.reset();
    // Schedule save empty metadata.
    schedule_save_closure_.Run();
  }
}

void BookmarkDataTypeProcessor::ReportBridgeErrorForTest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DisconnectSync();
  activation_request_.error_handler.Run(syncer::ModelError(
      FROM_HERE, syncer::ModelError::Type::
                     kBookmarksInitialMergePermanentEntitiesMissing));
}

void BookmarkDataTypeProcessor::StopTrackingMetadataAndResetTracker() {
  // DisconnectSync() should have been called by the caller.
  DCHECK(!worker_);
  DCHECK(bookmark_tracker_);
  DCHECK(bookmark_model_observer_);
  bookmark_model_->RemoveObserver(bookmark_model_observer_.get());
  bookmark_model_observer_.reset();
  bookmark_tracker_.reset();

  // Tracked sync metadata has just been thrown away. Depending on the current
  // selected behavior, bookmarks themselves may need clearing too.
  TriggerWipeModelUponSyncDisabledBehavior();
}

void BookmarkDataTypeProcessor::TriggerWipeModelUponSyncDisabledBehavior() {
  switch (wipe_model_upon_sync_disabled_behavior_) {
    case syncer::WipeModelUponSyncDisabledBehavior::kNever:
      // Nothing to do.
      break;
    case syncer::WipeModelUponSyncDisabledBehavior::kAlways:
      bookmark_model_->RemoveAllSyncableNodes();
      break;
  }
}

}  // namespace sync_bookmarks
