// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_FRONTEND_DATA_TYPE_CONTROLLER_H__
#define COMPONENTS_SYNC_DRIVER_FRONTEND_DATA_TYPE_CONTROLLER_H__

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/sync/driver/directory_data_type_controller.h"
#include "components/sync/model/data_type_error_handler.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace syncer {

class AssociatorInterface;
class ChangeProcessor;
class SyncClient;
class SyncError;

// Implementation for datatypes that reside on the frontend thread
// (UI thread). This is the same thread we perform initialization on, so we
// don't have to worry about thread safety. The main start/stop funtionality is
// implemented by default.
// Derived classes must implement (at least):
//    ModelType type() const
//    void CreateSyncComponents();
// NOTE: This class is deprecated! New sync datatypes should be using the
// SyncableService API and the UIDataTypeController instead.
// TODO(zea): Delete this once all types are on the new API.
class FrontendDataTypeController : public DirectoryDataTypeController {
 public:
  // |dump_stack| is called when an unrecoverable error occurs.
  FrontendDataTypeController(ModelType type,
                             const base::Closure& dump_stack,
                             SyncClient* sync_client);
  ~FrontendDataTypeController() override;

  // DataTypeController interface.
  void LoadModels(const ConfigureContext& configure_context,
                  const ModelLoadCallback& model_load_callback) override;
  void StartAssociating(StartCallback start_callback) override;
  void Stop(ShutdownReason shutdown_reason) override;
  State state() const override;

 protected:
  friend class FrontendDataTypeControllerMock;

  // For testing only.
  FrontendDataTypeController();

  // Kick off any dependent services that need to be running before we can
  // associate models. The default implementation is a no-op.
  // Return value:
  //   True - if models are ready and association can proceed.
  //   False - if models are not ready. Associate() should be called when the
  //           models are ready. Refer to Start(_) implementation.
  virtual bool StartModels();

  // Datatype specific creation of sync components.
  virtual void CreateSyncComponents() = 0;

  // Perform any DataType controller specific state cleanup before stopping
  // the datatype controller. The default implementation is a no-op.
  virtual void CleanUpState();

  // Helper method for cleaning up state and running the start callback.
  virtual void StartDone(ConfigureResult start_result,
                         const SyncMergeResult& local_merge_result,
                         const SyncMergeResult& syncer_merge_result);

  // Record association time.
  virtual void RecordAssociationTime(base::TimeDelta time);
  // Record causes of start failure.
  virtual void RecordStartFailure(ConfigureResult result);

  virtual AssociatorInterface* model_associator() const;
  virtual void set_model_associator(
      std::unique_ptr<AssociatorInterface> associator);
  ChangeProcessor* GetChangeProcessor() const override;
  virtual void set_change_processor(std::unique_ptr<ChangeProcessor> processor);

  // If the DTC is waiting for models to load, once the models are
  // loaded the datatype service will call this function on DTC to let
  // us know that it is safe to start associating.
  void OnModelLoaded();

  std::unique_ptr<DataTypeErrorHandler> CreateErrorHandler() override;

  State state_;

  StartCallback start_callback_;
  ModelLoadCallback model_load_callback_;

  // TODO(sync): transition all datatypes to SyncableService and deprecate
  // AssociatorInterface.
  std::unique_ptr<AssociatorInterface> model_associator_;
  std::unique_ptr<ChangeProcessor> change_processor_;

 private:
  // Build sync components and associate models.
  virtual void Associate();

  void AbortModelLoad();

  // Clean up our state and state variables. Called in response
  // to a failure or abort or stop.
  void CleanUp();

  // Handle an unrecoverable error.
  void OnUnrecoverableError(const SyncError& error);

  DISALLOW_COPY_AND_ASSIGN(FrontendDataTypeController);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_FRONTEND_DATA_TYPE_CONTROLLER_H__
