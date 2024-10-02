// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_data_type_processor.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/data_type_histogram.h"
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
#include "components/undo/bookmark_undo_utils.h"
#include "ui/base/models/tree_node_iterator.h"

namespace sync_bookmarks {

namespace {

constexpr size_t kDefaultMaxBookmarksTillSyncEnabled = 100000;

class ScopedRemoteUpdateBookmarks {
 public:
  // `bookmark_model`, `bookmark_undo_service` and `observer` must not be null
  // and must outlive this object.
  ScopedRemoteUpdateBookmarks(BookmarkModelView* bookmark_model,
                              BookmarkUndoService* bookmark_undo_service,
                              bookmarks::BookmarkModelObserver* observer)
      : bookmark_model_(bookmark_model),
        suspend_undo_(bookmark_undo_service),
        observer_(observer) {
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

  // Changes made to the bookmark model due to sync should not be undoable.
  ScopedSuspendBookmarkUndo suspend_undo_;

  const raw_ptr<bookmarks::BookmarkModelObserver> observer_;
};

std::string ComputeServerDefinedUniqueTagForDebugging(
    const bookmarks::BookmarkNode* node,
    BookmarkModelView* model) {
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

size_t CountSyncableBookmarksFromModel(BookmarkModelView* model) {
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
  size_t num_unsynced_entities = 0;
  for (const auto* entity : tracker.GetAllEntities()) {
    if (entity->IsUnsynced()) {
      num_unsynced_entities++;
    }
  }
  syncer::SyncRecordDataTypeNumUnsyncedEntitiesOnModelReady(
      syncer::BOOKMARKS, num_unsynced_entities);
}

}  // namespace

BookmarkDataTypeProcessor::BookmarkDataTypeProcessor(
    BookmarkUndoService* bookmark_undo_service,
    syncer::WipeModelUponSyncDisabledBehavior
        wipe_model_upon_sync_disabled_behavior)
    : bookmark_undo_service_(bookmark_undo_service),
      wipe_model_upon_sync_disabled_behavior_(
          wipe_model_upon_sync_disabled_behavior),
      max_bookmarks_till_sync_enabled_(kDefaultMaxBookmarksTillSyncEnabled) {}

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
  // `last_initial_merge_remote_updates_exceeded_limit_` is set.
  DCHECK(!last_initial_merge_remote_updates_exceeded_limit_);
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
  // `last_initial_merge_remote_updates_exceeded_limit_` is set.
  DCHECK(!last_initial_merge_remote_updates_exceeded_limit_);

  // TODO(crbug.com/40860698): validate incoming updates, e.g. `gc_directive`
  // must be empty for Bookmarks.

  // Clients before M94 did not populate the parent UUID in specifics.
  PopulateParentGuidInSpecifics(bookmark_tracker_.get(), &updates);

  if (!bookmark_tracker_) {
    OnInitialUpdateReceived(data_type_state, std::move(updates));
    return;
  }

  // Incremental updates.
  {
    ScopedRemoteUpdateBookmarks update_bookmarks(
        bookmark_model_, bookmark_undo_service_,
        bookmark_model_observer_.get());
    BookmarkRemoteUpdatesHandler updates_handler(
        bookmark_model_, favicon_service_, bookmark_tracker_.get());
    const bool got_new_encryption_requirements =
        bookmark_tracker_->data_type_state().encryption_key_name() !=
        data_type_state.encryption_key_name();
    bookmark_tracker_->set_data_type_state(data_type_state);
    updates_handler.Process(updates, got_new_encryption_requirements);
  }

  // Issue error and stop sync if bookmarks count exceeds limit.
  if (bookmark_tracker_->TrackedBookmarksCount() >
      max_bookmarks_till_sync_enabled_) {
    // Local changes continue to be tracked in order to allow users to delete
    // bookmarks and recover upon restart.
    DisconnectSync();
    activation_request_.error_handler.Run(
        syncer::ModelError(FROM_HERE, "Local bookmarks count exceed limit."));
    return;
  }

  if (bookmark_tracker_->ReuploadBookmarksOnLoadIfNeeded()) {
    NudgeForCommitIfNeeded();
  }

  // There are cases when we receive non-empty updates that don't result in
  // model changes (e.g. reflections). In that case, issue a write to persit the
  // progress marker in order to avoid downloading those updates again.
  if (!updates.empty()) {
    // Schedule save just in case one is needed.
    schedule_save_closure_.Run();
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
    // `last_initial_merge_remote_updates_exceeded_limit_` is only set in error
    // cases where the tracker would not be initialized.
    DCHECK(!last_initial_merge_remote_updates_exceeded_limit_);

    sync_pb::BookmarkModelMetadata model_metadata =
        bookmark_tracker_->BuildBookmarkModelMetadata();
    // Ensure that BuildBookmarkModelMetadata() never populates this field.
    DCHECK(
        !model_metadata.has_last_initial_merge_remote_updates_exceeded_limit());
    model_metadata.SerializeToString(&metadata_str);
  } else if (last_initial_merge_remote_updates_exceeded_limit_) {
    sync_pb::BookmarkModelMetadata model_metadata;
    // Setting the field only when true guarantees that the empty-string case
    // is interpreted as no-metadata-to-clear.
    model_metadata.set_last_initial_merge_remote_updates_exceeded_limit(true);
    model_metadata.SerializeToString(&metadata_str);
  }
  return metadata_str;
}

void BookmarkDataTypeProcessor::ModelReadyToSync(
    const std::string& metadata_str,
    const base::RepeatingClosure& schedule_save_closure,
    BookmarkModelView* model) {
  DCHECK(model);
  DCHECK(model->loaded());
  DCHECK(!bookmark_model_);
  DCHECK(!bookmark_tracker_);
  DCHECK(!bookmark_model_observer_);

  TRACE_EVENT0("sync", "BookmarkDataTypeProcessor::ModelReadyToSync");

  bookmark_model_ = model;
  schedule_save_closure_ = schedule_save_closure;

  sync_pb::BookmarkModelMetadata model_metadata;
  model_metadata.ParseFromString(metadata_str);

  if (pending_clear_metadata_) {
    pending_clear_metadata_ = false;
    // Schedule save empty metadata, if not already empty.
    if (!metadata_str.empty()) {
      LogClearMetadataWhileStoppedHistogram(syncer::BOOKMARKS,
                                            /*is_delayed_call=*/true);
      if (syncer::IsInitialSyncDone(
              model_metadata.data_type_state().initial_sync_state())) {
        // There used to be a tracker, which is dropped now due to
        // `pending_clear_metadata_`. This isn't very different to
        // ClearMetadataIfStopped(), in the sense that the need to wipe the
        // local model needs to be considered.
        TriggerWipeModelUponSyncDisabledBehavior();
      }
      schedule_save_closure_.Run();
    }
  } else if (model_metadata
                 .last_initial_merge_remote_updates_exceeded_limit()) {
    // Report error if remote updates fetched last time during initial merge
    // exceeded limit. Note that here we are only setting
    // `last_initial_merge_remote_updates_exceeded_limit_`, the actual error
    // would be reported in ConnectIfReady().
    last_initial_merge_remote_updates_exceeded_limit_ = true;
  } else {
    bookmark_tracker_ =
        SyncedBookmarkTracker::CreateFromBookmarkModelAndMetadata(
            model, std::move(model_metadata));

    if (bookmark_tracker_) {
      StartTrackingMetadata();
      RecordDataTypeNumUnsyncedEntitiesOnModelReadyForBookmarks(
          *bookmark_tracker_);
    } else if (!metadata_str.empty()) {
      DLOG(WARNING)
          << "Persisted bookmark sync metadata invalidated when loading.";
      // Schedule a save to make sure the corrupt metadata is deleted from disk
      // as soon as possible, to avoid reporting again after restart if nothing
      // else schedules a save meanwhile (which is common if sync is not running
      // properly, e.g. auth error).
      schedule_save_closure_.Run();
    }
  }

  if (!bookmark_tracker_) {
    switch (wipe_model_upon_sync_disabled_behavior_) {
      case syncer::WipeModelUponSyncDisabledBehavior::kNever:
        // Nothing to do.
        break;
      case syncer::WipeModelUponSyncDisabledBehavior::kOnceIfTrackingMetadata:
        // Since the model isn't initially tracking metadata, move away from
        // kOnceIfTrackingMetadata so the behavior doesn't kick in, in case sync
        // is turned on later and back to off. This should be practically
        // unreachable because usually ClearMetadataIfStopped() would be invoked
        // earlier, but let's be extra safe and avoid relying on this behavior.
        wipe_model_upon_sync_disabled_behavior_ =
            syncer::WipeModelUponSyncDisabledBehavior::kNever;
        break;
      case syncer::WipeModelUponSyncDisabledBehavior::kAlways:
        // Remove any previous data that may exist, if its lifetime is strongly
        // coupled with the tracker's (sync metadata's).
        bookmark_model_->RemoveAllSyncableNodes();
        break;
    }
  }

  ConnectIfReady();
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
  // Return if Sync didn't start yet.
  if (!start_callback_) {
    return;
  }

  DCHECK(activation_request_.error_handler);
  // ConnectSync() should not have been called by now.
  DCHECK(!worker_);

  // Report error if remote updates fetched last time during initial merge
  // exceeded limit.
  if (last_initial_merge_remote_updates_exceeded_limit_) {
    // `last_initial_merge_remote_updates_exceeded_limit_` is only set in error
    // case and thus tracker should be empty.
    DCHECK(!bookmark_tracker_);
    start_callback_.Reset();
    activation_request_.error_handler.Run(
        syncer::ModelError(FROM_HERE,
                           "Latest remote bookmarks count exceeded limit. Turn "
                           "off and turn on sync to retry."));
    return;
  }

  // Issue error and stop sync if bookmarks exceed limit.
  // TODO(crbug.com/40854724): Think about adding two different limits: one for
  // when sync just starts, the other (larger one) as hard limit, incl.
  // incremental changes.
  const size_t count = bookmark_tracker_
                           ? bookmark_tracker_->TrackedBookmarksCount()
                           : CountSyncableBookmarksFromModel(bookmark_model_);
  if (count > max_bookmarks_till_sync_enabled_) {
    // For the case where a tracker already exists, local changes will continue
    // to be tracked in order order to allow users to delete bookmarks and
    // recover upon restart.
    start_callback_.Reset();
    activation_request_.error_handler.Run(
        syncer::ModelError(FROM_HERE, "Local bookmarks count exceed limit."));
    return;
  }

  DCHECK(!activation_request_.cache_guid.empty());

  if (bookmark_tracker_ && bookmark_tracker_->data_type_state().cache_guid() !=
                               activation_request_.cache_guid) {
    // In case of a cache uuid mismatch, treat it as a corrupted metadata and
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
      last_initial_merge_remote_updates_exceeded_limit_ = false;
      schedule_save_closure_.Run();
      break;
    }
  }

  // Do not let any delayed callbacks be called.
  weak_ptr_factory_for_controller_.InvalidateWeakPtrs();
}

void BookmarkDataTypeProcessor::NudgeForCommitIfNeeded() {
  DCHECK(bookmark_tracker_);

  // Issue error and stop sync if the number of local bookmarks exceed limit.
  // If `activation_request_.error_handler` is not set, the check is ignored
  // because this gets re-evaluated in ConnectIfReady().
  if (activation_request_.error_handler &&
      bookmark_tracker_->TrackedBookmarksCount() >
          max_bookmarks_till_sync_enabled_) {
    // Local changes continue to be tracked in order to allow users to delete
    // bookmarks and recover upon restart.
    DisconnectSync();
    start_callback_.Reset();
    activation_request_.error_handler.Run(
        syncer::ModelError(FROM_HERE, "Local bookmarks count exceed limit."));
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

  // `updates` can contain an additional root folder. The server may or may not
  // deliver a root node - it is not guaranteed, but this works as an
  // approximated safeguard.
  const size_t max_initial_updates_count = max_bookmarks_till_sync_enabled_ + 1;

  // Report error if count of remote updates is more than the limit.
  // Note that we are not having this check for incremental updates as it is
  // very unlikely that there will be many updates downloaded.
  if (updates.size() > max_initial_updates_count) {
    DisconnectSync();
    last_initial_merge_remote_updates_exceeded_limit_ = true;
    activation_request_.error_handler.Run(
        syncer::ModelError(FROM_HERE, "Remote bookmarks count exceed limit."));
    schedule_save_closure_.Run();
    return;
  }

  bookmark_tracker_ = SyncedBookmarkTracker::CreateEmpty(data_type_state);
  StartTrackingMetadata();

  {
    ScopedRemoteUpdateBookmarks update_bookmarks(
        bookmark_model_, bookmark_undo_service_,
        bookmark_model_observer_.get());

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
    activation_request_.error_handler.Run(
        syncer::ModelError(FROM_HERE, "Permanent bookmark entities missing"));
    return;
  }

  bookmark_tracker_->CheckAllNodesTracked(bookmark_model_);

  LogDataTypeConfigurationTime(syncer::BOOKMARKS, activation_request_.sync_mode,
                               activation_request_.configuration_start_time);

  schedule_save_closure_.Run();
  NudgeForCommitIfNeeded();
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

void BookmarkDataTypeProcessor::HasUnsyncedData(
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(bookmark_tracker_ &&
                          bookmark_tracker_->HasLocalChanges());
}

void BookmarkDataTypeProcessor::GetAllNodesForDebugging(
    AllNodesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  max_bookmarks_till_sync_enabled_ = limit;
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
  } else if (last_initial_merge_remote_updates_exceeded_limit_) {
    LogClearMetadataWhileStoppedHistogram(syncer::BOOKMARKS,
                                          /*is_delayed_call=*/false);
    last_initial_merge_remote_updates_exceeded_limit_ = false;
    // Schedule save empty metadata.
    schedule_save_closure_.Run();
  }
}

void BookmarkDataTypeProcessor::ReportBridgeErrorForTest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DisconnectSync();
  activation_request_.error_handler.Run(
      syncer::ModelError(FROM_HERE, "Report error for test"));
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
    case syncer::WipeModelUponSyncDisabledBehavior::kOnceIfTrackingMetadata:
      // Do it this time, but switch to kNever so it doesn't trigger next
      // time.
      syncer::SyncRecordModelClearedOnceHistogram(syncer::BOOKMARKS);
      wipe_model_upon_sync_disabled_behavior_ =
          syncer::WipeModelUponSyncDisabledBehavior::kNever;
      [[fallthrough]];
    case syncer::WipeModelUponSyncDisabledBehavior::kAlways:
      bookmark_model_->RemoveAllSyncableNodes();
      break;
  }
}

}  // namespace sync_bookmarks
