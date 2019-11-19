// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_TYPE_PROCESSOR_H_
#define COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_TYPE_PROCESSOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/sync/engine/model_type_processor.h"
#include "components/sync/model/model_type_controller_delegate.h"
#include "components/sync_bookmarks/synced_bookmark_tracker.h"

class BookmarkUndoService;

namespace bookmarks {
class BookmarkModel;
}

namespace favicon {
class FaviconService;
}

namespace sync_bookmarks {

class BookmarkModelObserverImpl;

class BookmarkModelTypeProcessor : public syncer::ModelTypeProcessor,
                                   public syncer::ModelTypeControllerDelegate {
 public:
  // |bookmark_undo_service| must not be nullptr and must outlive this object.
  explicit BookmarkModelTypeProcessor(
      BookmarkUndoService* bookmark_undo_service);
  ~BookmarkModelTypeProcessor() override;

  // ModelTypeProcessor implementation.
  void ConnectSync(std::unique_ptr<syncer::CommitQueue> worker) override;
  void DisconnectSync() override;
  void GetLocalChanges(size_t max_entries,
                       GetLocalChangesCallback callback) override;
  void OnCommitCompleted(
      const sync_pb::ModelTypeState& type_state,
      const syncer::CommitResponseDataList& response_list) override;
  void OnUpdateReceived(const sync_pb::ModelTypeState& type_state,
                        syncer::UpdateResponseDataList updates) override;

  // ModelTypeControllerDelegate implementation.
  void OnSyncStarting(const syncer::DataTypeActivationRequest& request,
                      StartCallback start_callback) override;
  void OnSyncStopping(syncer::SyncStopMetadataFate metadata_fate) override;
  void GetAllNodesForDebugging(AllNodesCallback callback) override;
  void GetStatusCountersForDebugging(StatusCountersCallback callback) override;
  void RecordMemoryUsageAndCountsHistograms() override;

  // Encodes all sync metadata into a string, representing a state that can be
  // restored via ModelReadyToSync() below.
  std::string EncodeSyncMetadata() const;

  // It mainly decodes a BookmarkModelMetadata proto serialized in
  // |metadata_str|, and uses it to fill in the tracker and the model type state
  // objects. |model| must not be null and must outlive this object. It is used
  // to the retrieve the local node ids, and is stored in the processor to be
  // used for further model operations. |schedule_save_closure| is a repeating
  // closure used to schedule a save of the bookmark model together with the
  // metadata.
  void ModelReadyToSync(const std::string& metadata_str,
                        const base::RepeatingClosure& schedule_save_closure,
                        bookmarks::BookmarkModel* model);

  // Sets the favicon service used when processing remote updates. It must be
  // called before the processor is ready to receive remote updates, and hence
  // before OnSyncStarting() is called. |favicon_service| must not be null.
  void SetFaviconService(favicon::FaviconService* favicon_service);

  // Returns the estimate of dynamically allocated memory in bytes.
  size_t EstimateMemoryUsage() const;

  const SyncedBookmarkTracker* GetTrackerForTest() const;
  bool IsConnectedForTest() const;

  base::WeakPtr<syncer::ModelTypeControllerDelegate> GetWeakPtr();

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // If preconditions are met, inform sync that we are ready to connect.
  void ConnectIfReady();

  // Nudges worker if there are any local entities to be committed. Should only
  // be called after initial sync is done and processor is tracking sync
  // entities.
  void NudgeForCommitIfNeeded();

  // Performs the required clean up when bookmark model is being deleted.
  void OnBookmarkModelBeingDeleted();

  // Process specifically calls to OnUpdateReceived() that correspond to the
  // initial merge of bookmarks (e.g. was just enabled).
  void OnInitialUpdateReceived(const sync_pb::ModelTypeState& type_state,
                               syncer::UpdateResponseDataList updates);

  // Instantiates the required objects to track metadata and starts observing
  // changes from the bookmark model.
  void StartTrackingMetadata(
      std::vector<NodeMetadataPair> nodes_metadata,
      std::unique_ptr<sync_pb::ModelTypeState> model_type_state);
  void StopTrackingMetadata();

  // Creates a DictionaryValue for local and remote debugging information about
  // |node| and appends it to |all_nodes|. It does the same for child nodes
  // recursively. |index| is the index of |node| within its parent. |index|
  // could computed from |node|, however it's much cheaper to pass from outside
  // since we iterate over child nodes already in the calling sites.
  void AppendNodeAndChildrenForDebugging(const bookmarks::BookmarkNode* node,
                                         int index,
                                         base::ListValue* all_nodes) const;

  // Stores the start callback in between OnSyncStarting() and
  // ModelReadyToSync().
  StartCallback start_callback_;

  // The bookmark model we are processing local changes from and forwarding
  // remote changes to. It is set during ModelReadyToSync(), which is called
  // during startup, as part of the bookmark-loading process.
  bookmarks::BookmarkModel* bookmark_model_ = nullptr;

  // Used to when processing remote updates to apply favicon information. It's
  // not set at start up because it's only avialable after the bookmark model
  // has been loaded.
  favicon::FaviconService* favicon_service_ = nullptr;

  // Used to suspend bookmark undo when processing remote changes.
  BookmarkUndoService* const bookmark_undo_service_;

  // The callback used to schedule the persistence of bookmark model as well as
  // the metadata to a file during which latest metadata should also be pulled
  // via EncodeSyncMetadata. Processor should invoke it upon changes in the
  // metadata that don't imply changes in the model itself. Persisting updates
  // that imply model changes is the model's responsibility.
  base::RepeatingClosure schedule_save_closure_;

  // Reference to the CommitQueue.
  //
  // The interface hides the posting of tasks across threads as well as the
  // CommitQueue's implementation.  Both of these features are
  // useful in tests.
  std::unique_ptr<syncer::CommitQueue> worker_;

  // Keeps the mapping between server ids and bookmarks nodes together with sync
  // metadata. It is constructed and set during ModelReadyToSync(), if the
  // loaded bookmarks JSON contained previous sync metadata, or upon completion
  // of initial sync, which is called during startup, as part of the
  // bookmark-loading process.
  std::unique_ptr<SyncedBookmarkTracker> bookmark_tracker_;

  // GUID string that identifies the sync client and is received from the sync
  // engine.
  std::string cache_guid_;

  syncer::ModelErrorHandler error_handler_;

  std::unique_ptr<BookmarkModelObserverImpl> bookmark_model_observer_;

  // WeakPtrFactory for this processor for ModelTypeController.
  base::WeakPtrFactory<BookmarkModelTypeProcessor>
      weak_ptr_factory_for_controller_{this};

  // WeakPtrFactory for this processor which will be sent to sync thread.
  base::WeakPtrFactory<BookmarkModelTypeProcessor> weak_ptr_factory_for_worker_{
      this};

  DISALLOW_COPY_AND_ASSIGN(BookmarkModelTypeProcessor);
};

}  // namespace sync_bookmarks

#endif  // COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_TYPE_PROCESSOR_H_
