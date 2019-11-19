// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_ASYNC_DIRECTORY_TYPE_CONTROLLER_H_
#define COMPONENTS_SYNC_DRIVER_ASYNC_DIRECTORY_TYPE_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "components/sync/driver/directory_data_type_controller.h"
#include "components/sync/driver/shared_change_processor.h"

namespace syncer {

class SyncClient;
struct UserShare;

// Implementation for directory based datatypes that interact with their
// syncable services by posting to model thread. All interaction with datatype
// controller happens on UI thread.
class AsyncDirectoryTypeController : public DirectoryDataTypeController {
 public:
  // |dump_stack| is called when an unrecoverable error occurs.
  AsyncDirectoryTypeController(
      ModelType type,
      const base::Closure& dump_stack,
      SyncService* sync_service,
      SyncClient* sync_client,
      ModelSafeGroup model_safe_group,
      scoped_refptr<base::SequencedTaskRunner> model_thread);
  ~AsyncDirectoryTypeController() override;

  // DataTypeController interface.
  void LoadModels(const ConfigureContext& configure_context,
                  const ModelLoadCallback& model_load_callback) override;
  void StartAssociating(StartCallback start_callback) override;
  void Stop(ShutdownReason shutdown_reason) override;
  ChangeProcessor* GetChangeProcessor() const override;
  State state() const override;

  // Used by tests to override the factory used to create
  // GenericChangeProcessors.
  void SetGenericChangeProcessorFactoryForTest(
      std::unique_ptr<GenericChangeProcessorFactory> factory);

 protected:
  // For testing only.
  AsyncDirectoryTypeController();

  // Start any dependent services that need to be running before we can
  // associate models. The default implementation is a no-op.
  // Return value:
  //   True - if models are ready and association can proceed.
  //   False - if models are not ready. StartAssociationAsync should be called
  //           when the models are ready.
  // Note: this is performed on the UI thread.
  virtual bool StartModels();

  // Perform any DataType controller specific state cleanup before stopping
  // the datatype controller. The default implementation is a no-op.
  // Note: this is performed on the UI thread.
  virtual void StopModels();

  // Posts the given task to the model thread, i.e. the thread the datatype
  // lives on. Return value: True if task posted successfully, false otherwise.
  // Default implementation posts task to model_thread_. Types that don't use
  // TaskRunner need to override this method.
  virtual bool PostTaskOnModelThread(const base::Location& from_here,
                                     const base::Closure& task);

  // Start up complete, update the state and invoke the callback.
  virtual void StartDone(DataTypeController::ConfigureResult start_result,
                         const SyncMergeResult& local_merge_result,
                         const SyncMergeResult& syncer_merge_result);

  // Kick off the association process.
  virtual bool StartAssociationAsync();

  // Record causes of start failure.
  virtual void RecordStartFailure(ConfigureResult result);

  // To allow unit tests to control thread interaction during non-ui startup
  // and shutdown, use a factory method to create the SharedChangeProcessor.
  virtual SharedChangeProcessor* CreateSharedChangeProcessor();

  // If the DTC is waiting for models to load, once the models are
  // loaded the datatype service will call this function on DTC to let
  // us know that it is safe to start associating.
  void OnModelLoaded();

  std::unique_ptr<DataTypeErrorHandler> CreateErrorHandler() override;

 private:
  // Calls Disconnect() on |shared_change_processor_|, then sets it to
  // null.  Must be called only by StartDoneImpl() or Stop() (on the
  // UI thread) and only after a call to Start() (i.e.,
  // |shared_change_processor_| must be non-null).
  void DisconnectSharedChangeProcessor();

  // Posts StopLocalService() to the processor on the model type thread.
  void StopSyncableService();

  // Disable this type with the sync service. Should only be invoked in case of
  // an unrecoverable error.
  // Note: this is performed on the UI thread.
  void DisableImpl(const SyncError& error);

  SyncClient* const sync_client_;

  // UserShare is stored in StartAssociating while on UI thread and
  // passed to SharedChangeProcessor::Connect on the model thread.
  UserShare* user_share_;

  // Factory is used by tests to inject custom implementation of
  // GenericChangeProcessor.
  std::unique_ptr<GenericChangeProcessorFactory> processor_factory_;

  // State of this datatype controller.
  State state_;

  // Callbacks for use when starting the datatype.
  StartCallback start_callback_;
  ModelLoadCallback model_load_callback_;

  // Task runner of the model thread. Can be nullptr in which case datatype
  // controller needs to override PostTaskOnModelThread().
  scoped_refptr<base::SequencedTaskRunner> model_thread_;

  // The shared change processor is the thread-safe interface to the
  // datatype.  We hold a reference to it from the UI thread so that
  // we can call Disconnect() on it from Stop()/StartDoneImpl().  Most
  // of the work is done on the backend thread.
  //
  // Lifetime: The SharedChangeProcessor object is created on the UI
  // thread and passed on to the backend thread.  This reference is
  // released on the UI thread in Stop()/StartDoneImpl(), but the
  // backend thread may still have references to it (which is okay,
  // since we call Disconnect() before releasing the UI thread
  // reference).
  scoped_refptr<SharedChangeProcessor> shared_change_processor_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_ASYNC_DIRECTORY_TYPE_CONTROLLER_H_
