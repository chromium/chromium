// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_GENERIC_CHANGE_PROCESSOR_H_
#define COMPONENTS_SYNC_DRIVER_GENERIC_CHANGE_PROCESSOR_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "components/sync/model/change_processor.h"
#include "components/sync/model/data_type_error_handler.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/sync_merge_result.h"

namespace syncer {

class SyncData;
class SyncableService;
class WriteNode;
class WriteTransaction;

namespace syncable {
class Entry;
}  // namespace syncable

using SyncDataList = std::vector<SyncData>;

// Datatype agnostic change processor. One instance of GenericChangeProcessor
// is created for each datatype and lives on the datatype's sequence. It then
// handles all interaction with the sync api, both translating pushes from the
// local service into transactions and receiving changes from the sync model,
// which then get converted into SyncChange's and sent to the local service.
//
// As a rule, the GenericChangeProcessor is not thread safe, and should only
// be used on the same sequence in which it was created.
class GenericChangeProcessor : public ChangeProcessor,
                               public SyncChangeProcessor {
 public:
  // Create a change processor for |type| and connect it to the syncer.
  GenericChangeProcessor(ModelType type,
                         std::unique_ptr<DataTypeErrorHandler> error_handler,
                         const base::WeakPtr<SyncableService>& local_service,
                         const base::WeakPtr<SyncMergeResult>& merge_result,
                         UserShare* user_share);
  ~GenericChangeProcessor() override;

  // ChangeProcessor interface.
  // Build and store a list of all changes into |syncer_changes_|.
  void ApplyChangesFromSyncModel(
      const BaseTransaction* trans,
      int64_t version,
      const ImmutableChangeRecordList& changes) override;
  // Passes |syncer_changes_|, built in ApplyChangesFromSyncModel, onto
  // |local_service_| by way of its ProcessSyncChanges method.
  void CommitChangesFromSyncModel() override;

  // SyncChangeProcessor implementation.
  SyncError ProcessSyncChanges(const base::Location& from_here,
                               const SyncChangeList& change_list) override;
  SyncDataList GetAllSyncData(ModelType type) const override;
  SyncError UpdateDataTypeContext(
      ModelType type,
      SyncChangeProcessor::ContextRefreshStatus refresh_status,
      const std::string& context) override;
  void AddLocalChangeObserver(LocalChangeObserver* observer) override;
  void RemoveLocalChangeObserver(LocalChangeObserver* observer) override;

  // Similar to above, but returns a SyncError for use by direct clients
  // of GenericChangeProcessor that may need more error visibility.
  virtual SyncError GetAllSyncDataReturnError(SyncDataList* data) const;

  // If a datatype context associated with this GenericChangeProcessor's type
  // exists, fills |context| and returns true. Otheriwse, if there has not been
  // a context set, returns false.
  virtual bool GetDataTypeContext(std::string* context) const;

  // Returns the number of items for this type.
  virtual int GetSyncCount();

  // Generic versions of AssociatorInterface methods. Called by
  // SyncableServiceAdapter or the DataTypeController.
  virtual bool SyncModelHasUserCreatedNodes(bool* has_nodes);
  virtual bool CryptoReadyIfNecessary();

 protected:
  // ChangeProcessor interface.
  void StartImpl() override;  // Does nothing.
  UserShare* share_handle() const override;

 private:
  SyncError AttemptDelete(const SyncChange& change,
                          ModelType type,
                          const std::string& type_str,
                          WriteNode* node,
                          DataTypeErrorHandler* error_handler);
  // Logically part of ProcessSyncChanges.
  SyncError HandleActionAdd(const SyncChange& change,
                            const std::string& type_str,
                            const WriteTransaction& trans,
                            WriteNode* sync_node);

  // Logically part of ProcessSyncChanges.
  SyncError HandleActionUpdate(const SyncChange& change,
                               const std::string& type_str,
                               const WriteTransaction& trans,
                               WriteNode* sync_node);

  // Notify every registered local change observer that |change| is about to be
  // applied to |current_entry|.
  void NotifyLocalChangeObservers(const syncable::Entry* current_entry,
                                  const SyncChange& change);

  base::SequenceChecker sequence_checker_;

  const ModelType type_;

  // The SyncableService this change processor will forward changes on to.
  const base::WeakPtr<SyncableService> local_service_;

  // A SyncMergeResult used to track the changes made during association. The
  // owner will invalidate the weak pointer when association is complete. While
  // the pointer is valid though, we increment it with any changes received
  // via ProcessSyncChanges.
  const base::WeakPtr<SyncMergeResult> merge_result_;

  // The current list of changes received from the syncer. We buffer because
  // we must ensure no syncapi transaction is held when we pass it on to
  // |local_service_|.
  // Set in ApplyChangesFromSyncModel, consumed in CommitChangesFromSyncModel.
  SyncChangeList syncer_changes_;

  // Our handle to the sync model. Unlike normal ChangeProcessors, we need to
  // be able to access the sync model before the change processor begins
  // listening to changes (the local_service_ will be interacting with us
  // when it starts up). As such we can't wait until Start(_) has been called,
  // and have to keep a local pointer to the user_share.
  UserShare* const share_handle_;

  // List of observers that want to be notified of local changes being written.
  base::ObserverList<LocalChangeObserver>::Unchecked local_change_observers_;

  base::WeakPtrFactory<GenericChangeProcessor> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GenericChangeProcessor);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_GENERIC_CHANGE_PROCESSOR_H_
