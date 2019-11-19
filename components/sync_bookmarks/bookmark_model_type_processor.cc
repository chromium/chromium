// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_model_type_processor.h"

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/trace_event/trace_event.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/sync/base/data_type_histogram.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/commit_queue.h"
#include "components/sync/engine/cycle/status_counters.h"
#include "components/sync/engine/model_type_processor_proxy.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/protocol/bookmark_model_metadata.pb.h"
#include "components/sync/protocol/proto_value_conversions.h"
#include "components/sync_bookmarks/bookmark_local_changes_builder.h"
#include "components/sync_bookmarks/bookmark_model_merger.h"
#include "components/sync_bookmarks/bookmark_model_observer_impl.h"
#include "components/sync_bookmarks/bookmark_remote_updates_handler.h"
#include "components/sync_bookmarks/bookmark_specifics_conversions.h"
#include "components/undo/bookmark_undo_utils.h"
#include "ui/base/models/tree_node_iterator.h"

namespace sync_bookmarks {

namespace {

// Metrics: "Sync.MissingBookmarkPermanentNodes"
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class MissingPermanentNodes {
  kBookmarkBar = 0,
  kOtherBookmarks = 1,
  kMobileBookmarks = 2,
  kBookmarkBarAndOtherBookmarks = 3,
  kBookmarkBarAndMobileBookmarks = 4,
  kOtherBookmarksAndMobileBookmarks = 5,
  kBookmarkBarAndOtherBookmarksAndMobileBookmarks = 6,

  kMaxValue = kBookmarkBarAndOtherBookmarksAndMobileBookmarks,
};

void LogMissingPermanentNodes(
    const SyncedBookmarkTracker::Entity* bookmark_bar,
    const SyncedBookmarkTracker::Entity* other_bookmarks,
    const SyncedBookmarkTracker::Entity* mobile_bookmarks) {
  MissingPermanentNodes missing_nodes;
  if (!bookmark_bar && other_bookmarks && mobile_bookmarks) {
    missing_nodes = MissingPermanentNodes::kBookmarkBar;
  } else if (bookmark_bar && !other_bookmarks && mobile_bookmarks) {
    missing_nodes = MissingPermanentNodes::kOtherBookmarks;
  } else if (bookmark_bar && other_bookmarks && !mobile_bookmarks) {
    missing_nodes = MissingPermanentNodes::kMobileBookmarks;
  } else if (!bookmark_bar && !other_bookmarks && mobile_bookmarks) {
    missing_nodes = MissingPermanentNodes::kBookmarkBarAndOtherBookmarks;
  } else if (!bookmark_bar && other_bookmarks && !mobile_bookmarks) {
    missing_nodes = MissingPermanentNodes::kBookmarkBarAndMobileBookmarks;
  } else if (bookmark_bar && !other_bookmarks && !mobile_bookmarks) {
    missing_nodes = MissingPermanentNodes::kOtherBookmarksAndMobileBookmarks;
  } else {
    // All must be missing.
    missing_nodes =
        MissingPermanentNodes::kBookmarkBarAndOtherBookmarksAndMobileBookmarks;
  }
  UMA_HISTOGRAM_ENUMERATION("Sync.MissingBookmarkPermanentNodes",
                            missing_nodes);
}

// Enables scheduling bookmark model saving only upon changes in entity sync
// metadata. This would stop persisting changes to the model type state that
// doesn't involve changes to the entity metadata as well.
// TODO(crbug.com/945820): This should be removed in M80 if not issues are
// observed.
const base::Feature kSyncScheduleForEntityMetadataChangesOnly{
    "SyncScheduleForEntityMetadataChangesOnly",
    base::FEATURE_ENABLED_BY_DEFAULT};

class ScopedRemoteUpdateBookmarks {
 public:
  // |bookmark_model|, |bookmark_undo_service| and |observer| must not be null
  // and must outlive this object.
  ScopedRemoteUpdateBookmarks(bookmarks::BookmarkModel* bookmark_model,
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

  ~ScopedRemoteUpdateBookmarks() {
    // Notify UI intensive observers of BookmarkModel that all updates have been
    // applied, and that they may now be consumed. This prevents issues like the
    // one described in https://crbug.com/281562, where old and new items on the
    // bookmarks bar would overlap.
    bookmark_model_->EndExtensiveChanges();
    bookmark_model_->AddObserver(observer_);
  }

 private:
  bookmarks::BookmarkModel* const bookmark_model_;

  // Changes made to the bookmark model due to sync should not be undoable.
  ScopedSuspendBookmarkUndo suspend_undo_;

  bookmarks::BookmarkModelObserver* const observer_;

  DISALLOW_COPY_AND_ASSIGN(ScopedRemoteUpdateBookmarks);
};

std::string ComputeServerDefinedUniqueTagForDebugging(
    const bookmarks::BookmarkNode* node,
    bookmarks::BookmarkModel* model) {
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

// Returns a map from id to node for all nodes in |model|.
std::map<int64_t, const bookmarks::BookmarkNode*> BuildIdToBookmarkNodeMap(
    const bookmarks::BookmarkModel* model) {
  std::map<int64_t, const bookmarks::BookmarkNode*> id_to_bookmark_node_map;

  // The TreeNodeIterator used below doesn't include the node itself, and hence
  // add the root node separately.
  id_to_bookmark_node_map[model->root_node()->id()] = model->root_node();

  ui::TreeNodeIterator<const bookmarks::BookmarkNode> iterator(
      model->root_node());
  while (iterator.has_next()) {
    const bookmarks::BookmarkNode* node = iterator.Next();
    id_to_bookmark_node_map[node->id()] = node;
  }
  return id_to_bookmark_node_map;
}

}  // namespace

BookmarkModelTypeProcessor::BookmarkModelTypeProcessor(
    BookmarkUndoService* bookmark_undo_service)
    : bookmark_undo_service_(bookmark_undo_service) {}

BookmarkModelTypeProcessor::~BookmarkModelTypeProcessor() {
  if (bookmark_model_ && bookmark_model_observer_) {
    bookmark_model_->RemoveObserver(bookmark_model_observer_.get());
  }
}

void BookmarkModelTypeProcessor::ConnectSync(
    std::unique_ptr<syncer::CommitQueue> worker) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!worker_);
  DCHECK(bookmark_model_);

  worker_ = std::move(worker);

  // |bookmark_tracker_| is instantiated only after initial sync is done.
  if (bookmark_tracker_) {
    NudgeForCommitIfNeeded();
  }
}

void BookmarkModelTypeProcessor::DisconnectSync() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  weak_ptr_factory_for_worker_.InvalidateWeakPtrs();
  if (!worker_) {
    return;
  }

  DVLOG(1) << "Disconnecting sync for Bookmarks";
  worker_.reset();
}

void BookmarkModelTypeProcessor::GetLocalChanges(
    size_t max_entries,
    GetLocalChangesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BookmarkLocalChangesBuilder builder(bookmark_tracker_.get(), bookmark_model_);
  syncer::CommitRequestDataList local_changes =
      builder.BuildCommitRequests(max_entries);
  for (const std::unique_ptr<syncer::CommitRequestData>& local_change :
       local_changes) {
    bookmark_tracker_->MarkCommitMayHaveStarted(local_change->entity->id);
  }
  std::move(callback).Run(std::move(local_changes));
}

void BookmarkModelTypeProcessor::OnCommitCompleted(
    const sync_pb::ModelTypeState& type_state,
    const syncer::CommitResponseDataList& response_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const syncer::CommitResponseData& response : response_list) {
    // In order to save space, |response.id_in_request| is written when it's
    // different from |response.id|. If it's empty, then there was no id change
    // during the commit, and |response.id| carries both the old and new ids.
    const std::string& old_sync_id =
        response.id_in_request.empty() ? response.id : response.id_in_request;
    bookmark_tracker_->UpdateUponCommitResponse(old_sync_id, response.id,
                                                response.sequence_number,
                                                response.response_version);
  }
  bookmark_tracker_->set_model_type_state(
      std::make_unique<sync_pb::ModelTypeState>(type_state));
  schedule_save_closure_.Run();
}

void BookmarkModelTypeProcessor::OnUpdateReceived(
    const sync_pb::ModelTypeState& model_type_state,
    syncer::UpdateResponseDataList updates) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!model_type_state.cache_guid().empty());
  DCHECK_EQ(model_type_state.cache_guid(), cache_guid_);
  DCHECK(model_type_state.initial_sync_done());

  if (!bookmark_tracker_) {
    OnInitialUpdateReceived(model_type_state, std::move(updates));
    return;
  }

  // Incremental updates.
  ScopedRemoteUpdateBookmarks update_bookmarks(
      bookmark_model_, bookmark_undo_service_, bookmark_model_observer_.get());
  BookmarkRemoteUpdatesHandler updates_handler(
      bookmark_model_, favicon_service_, bookmark_tracker_.get());
  const bool got_new_encryption_requirements =
      bookmark_tracker_->model_type_state().encryption_key_name() !=
      model_type_state.encryption_key_name();
  bookmark_tracker_->set_model_type_state(
      std::make_unique<sync_pb::ModelTypeState>(model_type_state));
  updates_handler.Process(updates, got_new_encryption_requirements);
  // There are cases when we receive non-empty updates that don't result in
  // model changes (e.g. reflections). In that case, issue a write to persit the
  // progress marker in order to avoid downloading those updates again.
  if (!updates.empty() || !base::FeatureList::IsEnabled(
                              kSyncScheduleForEntityMetadataChangesOnly)) {
    // Schedule save just in case one is needed.
    schedule_save_closure_.Run();
  }
}

const SyncedBookmarkTracker* BookmarkModelTypeProcessor::GetTrackerForTest()
    const {
  return bookmark_tracker_.get();
}

bool BookmarkModelTypeProcessor::IsConnectedForTest() const {
  return worker_ != nullptr;
}

std::string BookmarkModelTypeProcessor::EncodeSyncMetadata() const {
  std::string metadata_str;
  if (bookmark_tracker_) {
    sync_pb::BookmarkModelMetadata model_metadata =
        bookmark_tracker_->BuildBookmarkModelMetadata();
    model_metadata.SerializeToString(&metadata_str);
  }
  return metadata_str;
}

void BookmarkModelTypeProcessor::ModelReadyToSync(
    const std::string& metadata_str,
    const base::RepeatingClosure& schedule_save_closure,
    bookmarks::BookmarkModel* model) {
  DCHECK(model);
  DCHECK(!bookmark_model_);
  DCHECK(!bookmark_tracker_);
  DCHECK(!bookmark_model_observer_);

  // TODO(crbug.com/950869): Remove after investigations are completed.
  TRACE_EVENT0("browser", "BookmarkModelTypeProcessor::ModelReadyToSync");

  bookmark_model_ = model;
  schedule_save_closure_ = schedule_save_closure;

  base::TimeTicks start_time = base::TimeTicks::Now();
  sync_pb::BookmarkModelMetadata model_metadata;
  model_metadata.ParseFromString(metadata_str);

  if (model_metadata.model_type_state().initial_sync_done() &&
      SyncedBookmarkTracker::BookmarkModelMatchesMetadata(model,
                                                          model_metadata)) {
    std::map<int64_t, const bookmarks::BookmarkNode*> id_to_bookmark_node_map =
        BuildIdToBookmarkNodeMap(bookmark_model_);
    std::vector<NodeMetadataPair> nodes_metadata;
    for (sync_pb::BookmarkMetadata& bookmark_metadata :
         *model_metadata.mutable_bookmarks_metadata()) {
      const bookmarks::BookmarkNode* node = nullptr;
      if (!bookmark_metadata.metadata().is_deleted()) {
        node = id_to_bookmark_node_map[bookmark_metadata.id()];
        DCHECK(node);
      }
      auto metadata = std::make_unique<sync_pb::EntityMetadata>();
      metadata->Swap(bookmark_metadata.mutable_metadata());
      nodes_metadata.emplace_back(node, std::move(metadata));
    }
    auto model_type_state = std::make_unique<sync_pb::ModelTypeState>();
    model_type_state->Swap(model_metadata.mutable_model_type_state());
    StartTrackingMetadata(std::move(nodes_metadata),
                          std::move(model_type_state));
    bookmark_tracker_->CheckAllNodesTracked(bookmark_model_);
    UMA_HISTOGRAM_TIMES("Sync.BookmarksModelReadyToSyncTime",
                        base::TimeTicks::Now() - start_time);
  } else if (!model_metadata.model_type_state().initial_sync_done() &&
             !model_metadata.bookmarks_metadata().empty()) {
    DLOG(ERROR)
        << "Persisted Metadata not empty while initial sync is not done.";
  }
  ConnectIfReady();
}

void BookmarkModelTypeProcessor::SetFaviconService(
    favicon::FaviconService* favicon_service) {
  DCHECK(favicon_service);
  favicon_service_ = favicon_service;
}

size_t BookmarkModelTypeProcessor::EstimateMemoryUsage() const {
  using base::trace_event::EstimateMemoryUsage;
  size_t memory_usage = 0;
  if (bookmark_tracker_) {
    memory_usage += bookmark_tracker_->EstimateMemoryUsage();
  }
  memory_usage += EstimateMemoryUsage(cache_guid_);
  return memory_usage;
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
BookmarkModelTypeProcessor::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_ptr_factory_for_controller_.GetWeakPtr();
}

void BookmarkModelTypeProcessor::OnSyncStarting(
    const syncer::DataTypeActivationRequest& request,
    StartCallback start_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(start_callback);
  // |favicon_service_| should have been set by now.
  DCHECK(favicon_service_);
  DVLOG(1) << "Sync is starting for Bookmarks";

  cache_guid_ = request.cache_guid;
  start_callback_ = std::move(start_callback);
  error_handler_ = request.error_handler;

  DCHECK(!cache_guid_.empty());
  ConnectIfReady();
}

void BookmarkModelTypeProcessor::ConnectIfReady() {
  // Return if the model isn't ready.
  if (!bookmark_model_) {
    return;
  }
  // Return if Sync didn't start yet.
  if (!start_callback_) {
    return;
  }

  DCHECK(!cache_guid_.empty());

  if (bookmark_tracker_ &&
      bookmark_tracker_->model_type_state().cache_guid() != cache_guid_) {
    // TODO(crbug.com/820049): Add basic unit testing  consider using
    // StopTrackingMetadata().
    // In case of a cache guid mismatch, treat it as a corrupted metadata and
    // start clean.
    bookmark_model_->RemoveObserver(bookmark_model_observer_.get());
    bookmark_model_observer_.reset();
    bookmark_tracker_.reset();
  }

  auto activation_context =
      std::make_unique<syncer::DataTypeActivationResponse>();
  if (bookmark_tracker_) {
    activation_context->model_type_state =
        bookmark_tracker_->model_type_state();
  } else {
    sync_pb::ModelTypeState model_type_state;
    model_type_state.mutable_progress_marker()->set_data_type_id(
        GetSpecificsFieldNumberFromModelType(syncer::BOOKMARKS));
    model_type_state.set_cache_guid(cache_guid_);
    activation_context->model_type_state = model_type_state;
  }
  activation_context->type_processor =
      std::make_unique<syncer::ModelTypeProcessorProxy>(
          weak_ptr_factory_for_worker_.GetWeakPtr(),
          base::SequencedTaskRunnerHandle::Get());
  std::move(start_callback_).Run(std::move(activation_context));
}

void BookmarkModelTypeProcessor::OnSyncStopping(
    syncer::SyncStopMetadataFate metadata_fate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Disabling sync for a type shouldn't happen before the model is loaded
  // because OnSyncStopping() is not allowed to be called before
  // OnSyncStarting() has completed..
  DCHECK(bookmark_model_);
  DCHECK(!start_callback_);

  cache_guid_.clear();
  worker_.reset();

  switch (metadata_fate) {
    case syncer::KEEP_METADATA: {
      break;
    }

    case syncer::CLEAR_METADATA: {
      // Stop observing local changes. We'll start observing local changes again
      // when Sync is (re)started in StartTrackingMetadata().
      if (bookmark_tracker_) {
        DCHECK(bookmark_model_observer_);
        bookmark_model_->RemoveObserver(bookmark_model_observer_.get());
        bookmark_model_observer_.reset();
        bookmark_tracker_.reset();
      }
      schedule_save_closure_.Run();
      break;
    }
  }

  // Do not let any delayed callbacks to be called.
  weak_ptr_factory_for_controller_.InvalidateWeakPtrs();
  weak_ptr_factory_for_worker_.InvalidateWeakPtrs();
}

void BookmarkModelTypeProcessor::NudgeForCommitIfNeeded() {
  DCHECK(bookmark_tracker_);
  // Don't bother sending anything if there's no one to send to.
  if (!worker_) {
    return;
  }

  // Nudge worker if there are any entities with local changes.
  if (bookmark_tracker_->HasLocalChanges()) {
    worker_->NudgeForCommit();
  }
}

void BookmarkModelTypeProcessor::OnBookmarkModelBeingDeleted() {
  DCHECK(bookmark_model_);
  DCHECK(bookmark_model_observer_);
  StopTrackingMetadata();
}

void BookmarkModelTypeProcessor::OnInitialUpdateReceived(
    const sync_pb::ModelTypeState& model_type_state,
    syncer::UpdateResponseDataList updates) {
  DCHECK(!bookmark_tracker_);

  StartTrackingMetadata(
      std::vector<NodeMetadataPair>(),
      std::make_unique<sync_pb::ModelTypeState>(model_type_state));

  {
    ScopedRemoteUpdateBookmarks update_bookmarks(
        bookmark_model_, bookmark_undo_service_,
        bookmark_model_observer_.get());

    BookmarkModelMerger(std::move(updates), bookmark_model_, favicon_service_,
                        bookmark_tracker_.get())
        .Merge();
  }

  // If any of the permanent nodes is missing, we treat it as failure.
  if (!bookmark_tracker_->GetEntityForBookmarkNode(
          bookmark_model_->bookmark_bar_node()) ||
      !bookmark_tracker_->GetEntityForBookmarkNode(
          bookmark_model_->other_node()) ||
      !bookmark_tracker_->GetEntityForBookmarkNode(
          bookmark_model_->mobile_node())) {
    LogMissingPermanentNodes(bookmark_tracker_->GetEntityForBookmarkNode(
                                 bookmark_model_->bookmark_bar_node()),
                             bookmark_tracker_->GetEntityForBookmarkNode(
                                 bookmark_model_->other_node()),
                             bookmark_tracker_->GetEntityForBookmarkNode(
                                 bookmark_model_->mobile_node()));
    StopTrackingMetadata();
    bookmark_tracker_.reset();
    error_handler_.Run(
        syncer::ModelError(FROM_HERE, "Permanent bookmark entities missing"));
    return;
  }

  bookmark_tracker_->CheckAllNodesTracked(bookmark_model_);

  schedule_save_closure_.Run();
  NudgeForCommitIfNeeded();
}

void BookmarkModelTypeProcessor::StartTrackingMetadata(
    std::vector<NodeMetadataPair> nodes_metadata,
    std::unique_ptr<sync_pb::ModelTypeState> model_type_state) {
  bookmark_tracker_ = std::make_unique<SyncedBookmarkTracker>(
      std::move(nodes_metadata), std::move(model_type_state));

  bookmark_model_observer_ = std::make_unique<BookmarkModelObserverImpl>(
      base::BindRepeating(&BookmarkModelTypeProcessor::NudgeForCommitIfNeeded,
                          base::Unretained(this)),
      base::BindOnce(&BookmarkModelTypeProcessor::OnBookmarkModelBeingDeleted,
                     base::Unretained(this)),
      bookmark_tracker_.get());
  bookmark_model_->AddObserver(bookmark_model_observer_.get());
}

void BookmarkModelTypeProcessor::StopTrackingMetadata() {
  DCHECK(bookmark_model_observer_);

  bookmark_model_->RemoveObserver(bookmark_model_observer_.get());
  bookmark_model_ = nullptr;
  bookmark_model_observer_.reset();

  DisconnectSync();
}

void BookmarkModelTypeProcessor::GetAllNodesForDebugging(
    AllNodesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto all_nodes = std::make_unique<base::ListValue>();
  // Create a permanent folder since sync server no longer create root folders,
  // and USS won't migrate root folders from directory, we create root folders.
  auto root_node = std::make_unique<base::DictionaryValue>();
  // Function isTypeRootNode in sync_node_browser.js use PARENT_ID and
  // UNIQUE_SERVER_TAG to check if the node is root node. isChildOf in
  // sync_node_browser.js uses modelType to check if root node is parent of real
  // data node. NON_UNIQUE_NAME will be the name of node to display.
  root_node->SetString("ID", "BOOKMARKS_ROOT");
  root_node->SetString("PARENT_ID", "r");
  root_node->SetString("UNIQUE_SERVER_TAG", "Bookmarks");
  root_node->SetBoolean("IS_DIR", true);
  root_node->SetString("modelType", "Bookmarks");
  root_node->SetString("NON_UNIQUE_NAME", "Bookmarks");
  all_nodes->Append(std::move(root_node));

  const bookmarks::BookmarkNode* model_root_node = bookmark_model_->root_node();
  int i = 0;
  for (const auto& child : model_root_node->children())
    AppendNodeAndChildrenForDebugging(child.get(), i++, all_nodes.get());

  std::move(callback).Run(syncer::BOOKMARKS, std::move(all_nodes));
}

void BookmarkModelTypeProcessor::AppendNodeAndChildrenForDebugging(
    const bookmarks::BookmarkNode* node,
    int index,
    base::ListValue* all_nodes) const {
  const SyncedBookmarkTracker::Entity* entity =
      bookmark_tracker_->GetEntityForBookmarkNode(node);
  // Include only tracked nodes. Newly added nodes are tracked even before being
  // sent to the server. Managed bookmarks (that are installed by a policy)
  // aren't syncable and hence not tracked.
  if (!entity) {
    return;
  }
  const sync_pb::EntityMetadata* metadata = entity->metadata();
  // Copy data to an EntityData object to reuse its conversion
  // ToDictionaryValue() methods.
  syncer::EntityData data;
  data.id = metadata->server_id();
  data.creation_time = node->date_added();
  data.modification_time =
      syncer::ProtoTimeToTime(metadata->modification_time());
  data.name = base::UTF16ToUTF8(node->GetTitle());
  data.is_folder = node->is_folder();
  data.unique_position = metadata->unique_position();
  data.specifics = CreateSpecificsFromBookmarkNode(
      node, bookmark_model_, /*force_favicon_load=*/false);
  if (node->is_permanent_node()) {
    data.server_defined_unique_tag =
        ComputeServerDefinedUniqueTagForDebugging(node, bookmark_model_);
    // Set the parent to empty string to indicate it's parent of the root node
    // for bookmarks. The code in sync_node_browser.js links nodes with the
    // "modelType" when they are lacking a parent id.
    data.parent_id = "";
  } else {
    const bookmarks::BookmarkNode* parent = node->parent();
    const SyncedBookmarkTracker::Entity* parent_entity =
        bookmark_tracker_->GetEntityForBookmarkNode(parent);
    DCHECK(parent_entity);
    data.parent_id = parent_entity->metadata()->server_id();
  }

  std::unique_ptr<base::DictionaryValue> data_dictionary =
      data.ToDictionaryValue();
  // TODO(https://crbug.com/516866): Prepending the ID with an "s" is consistent
  // with the implementation in ClientTagBasedModelTypeProcessor. Double check
  // if this is actually needed and update both implementations if makes sense.
  // Set ID value as in legacy directory-based implementation, "s" means server.
  data_dictionary->SetString("ID", "s" + metadata->server_id());
  if (node->is_permanent_node()) {
    // Hardcode the parent of permanent nodes.
    data_dictionary->SetString("PARENT_ID", "BOOKMARKS_ROOT");
    data_dictionary->SetString("UNIQUE_SERVER_TAG",
                               data.server_defined_unique_tag);
  } else {
    data_dictionary->SetString("PARENT_ID", "s" + data.parent_id);
  }
  data_dictionary->SetInteger("LOCAL_EXTERNAL_ID", node->id());
  data_dictionary->SetInteger("positionIndex", index);
  data_dictionary->Set("metadata", syncer::EntityMetadataToValue(*metadata));
  data_dictionary->SetString("modelType", "Bookmarks");
  all_nodes->Append(std::move(data_dictionary));

  int i = 0;
  for (const auto& child : node->children())
    AppendNodeAndChildrenForDebugging(child.get(), i++, all_nodes);
}

void BookmarkModelTypeProcessor::GetStatusCountersForDebugging(
    StatusCountersCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  syncer::StatusCounters counters;
  if (bookmark_tracker_) {
    counters.num_entries =
        bookmark_tracker_->TrackedBookmarksCountForDebugging();
    counters.num_entries_and_tombstones =
        counters.num_entries +
        bookmark_tracker_->TrackedUncommittedTombstonesCountForDebugging();
  }
  std::move(callback).Run(syncer::BOOKMARKS, counters);
}

void BookmarkModelTypeProcessor::RecordMemoryUsageAndCountsHistograms() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SyncRecordModelTypeMemoryHistogram(syncer::BOOKMARKS, EstimateMemoryUsage());
  if (bookmark_tracker_) {
    SyncRecordModelTypeCountHistogram(
        syncer::BOOKMARKS,
        bookmark_tracker_->TrackedBookmarksCountForDebugging());
  } else {
    SyncRecordModelTypeCountHistogram(syncer::BOOKMARKS, 0);
  }
}

}  // namespace sync_bookmarks
