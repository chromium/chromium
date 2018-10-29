// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SHARED_CHANGE_PROCESSOR_H_
#define COMPONENTS_SYNC_DRIVER_SHARED_CHANGE_PROCESSOR_H_

#include <memory>
#include <string>

#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/lock.h"
#include "components/sync/driver/data_type_controller.h"
#include "components/sync/engine/model_safe_worker.h"
#include "components/sync/model/data_type_error_handler.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/model/sync_error.h"
#include "components/sync/model/sync_error_factory.h"
#include "components/sync/model/sync_merge_result.h"

namespace syncer {

class ChangeProcessor;
class GenericChangeProcessor;
class GenericChangeProcessorFactory;
class SyncClient;
class SyncableService;
struct UserShare;

// A ref-counted wrapper around a GenericChangeProcessor for use with datatypes
// that don't live on the UI thread.
//
// We need to make it refcounted as the ownership transfer from the
// DataTypeController is dependent on threading, and hence racy. The
// SharedChangeProcessor should be created on the UI thread, but should only be
// connected and used on the same thread as the datatype it interacts with.
//
// The only thread-safe method is Disconnect, which will disconnect from the
// generic change processor, letting us shut down the syncer/datatype without
// waiting for non-UI threads.
//
// Note: since we control the work being done while holding the lock, we ensure
// no I/O or other intensive work is done while blocking the UI thread (all
// the work is in-memory sync interactions).
//
// We use virtual methods so that we can use mock's in testing.
class SharedChangeProcessor
    : public base::RefCountedThreadSafe<SharedChangeProcessor> {
 public:
  using StartDoneCallback =
      base::Callback<void(DataTypeController::ConfigureResult start_result,
                          const SyncMergeResult& local_merge_result,
                          const SyncMergeResult& syncer_merge_result)>;

  // Create an uninitialized SharedChangeProcessor.
  explicit SharedChangeProcessor(ModelType type);

  void StartAssociation(StartDoneCallback start_done,
                        SyncClient* const sync_client,
                        GenericChangeProcessorFactory* processor_factory,
                        UserShare* user_share,
                        std::unique_ptr<DataTypeErrorHandler> error_handler);

  // Connect to the Syncer and prepare to handle changes for |type|. Will
  // create and store a new GenericChangeProcessor and return a weak pointer to
  // the SyncableService associated with |type|.
  // Note: If this SharedChangeProcessor has been disconnected, or the
  // SyncableService was not alive, will return a null weak pointer.
  virtual base::WeakPtr<SyncableService> Connect(
      SyncClient* sync_client,
      GenericChangeProcessorFactory* processor_factory,
      UserShare* user_share,
      std::unique_ptr<DataTypeErrorHandler> error_handler,
      const base::WeakPtr<SyncMergeResult>& merge_result);

  // Disconnects from the generic change processor. This method is thread-safe.
  // After this, all attempts to interact with the change processor by
  // |local_service_| are dropped and return errors. The syncer will be safe to
  // shut down from the point of view of this datatype.
  // Note: Once disconnected, you cannot reconnect without creating a new
  // SharedChangeProcessor.
  // Returns: true if we were previously succesfully connected, false if we were
  // already disconnected.
  virtual bool Disconnect();

  // GenericChangeProcessor stubs (with disconnect support).
  // Should only be called on the same sequence the datatype resides.
  virtual int GetSyncCount();
  virtual SyncError ProcessSyncChanges(const base::Location& from_here,
                                       const SyncChangeList& change_list);
  virtual SyncDataList GetAllSyncData(ModelType type) const;
  virtual SyncError GetAllSyncDataReturnError(ModelType type,
                                              SyncDataList* data) const;
  virtual SyncError UpdateDataTypeContext(
      ModelType type,
      SyncChangeProcessor::ContextRefreshStatus refresh_status,
      const std::string& context);
  virtual void AddLocalChangeObserver(LocalChangeObserver* observer);
  virtual void RemoveLocalChangeObserver(LocalChangeObserver* observer);
  virtual bool SyncModelHasUserCreatedNodes(bool* has_nodes);
  virtual bool CryptoReadyIfNecessary();

  // If a datatype context associated with the current type exists, fills
  // |context| and returns true. Otheriwse, if there has not been a context
  // set, returns false.
  virtual bool GetDataTypeContext(std::string* context) const;

  virtual SyncError CreateAndUploadError(const base::Location& location,
                                         const std::string& message);

  // Calls local_service_->StopSyncing() and releases our reference to it.
  void StopLocalService();

  ChangeProcessor* generic_change_processor();

 protected:
  friend class base::RefCountedThreadSafe<SharedChangeProcessor>;
  virtual ~SharedChangeProcessor();

 private:
  // Record association time.
  virtual void RecordAssociationTime(base::TimeDelta time);

  // Monitor lock for this object. All methods that interact with the change
  // processor must aquire this lock and check whether we're disconnected or
  // not. Once disconnected, all attempted changes to or loads from the change
  // processor return errors. This enables us to shut down the syncer without
  // having to wait for possibly non-UI thread datatypes to complete work.
  mutable base::Lock monitor_lock_;
  bool disconnected_;

  // The sync datatype we process changes for.
  const ModelType type_;

  // The frontend / UI MessageLoop this object is constructed on. May also be
  // destructed and/or disconnected on this loop, see ~SharedChangeProcessor.
  const scoped_refptr<const base::SequencedTaskRunner> frontend_task_runner_;

  // The execution sequence that all methods except the constructor, destructor,
  // and Disconnect() should be called on. Set in Connect().
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  // Used only on |backend_task_runner_|.
  GenericChangeProcessor* generic_change_processor_;

  std::unique_ptr<DataTypeErrorHandler> error_handler_;

  // The local service for this type. Only set if the DTC for the type uses
  // SharedChangeProcessor::StartAssociation().
  base::WeakPtr<SyncableService> local_service_;

  DISALLOW_COPY_AND_ASSIGN(SharedChangeProcessor);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_SHARED_CHANGE_PROCESSOR_H_
