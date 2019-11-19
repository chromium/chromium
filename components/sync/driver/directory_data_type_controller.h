// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_DIRECTORY_DATA_TYPE_CONTROLLER_H__
#define COMPONENTS_SYNC_DRIVER_DIRECTORY_DATA_TYPE_CONTROLLER_H__

#include <memory>

#include "components/sync/driver/data_type_controller.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync/engine/model_safe_worker.h"
#include "components/sync/syncable/directory.h"

namespace syncer {

class ChangeProcessor;
class SyncService;

// Base class for Directory based Data type controllers.
class DirectoryDataTypeController : public DataTypeController {
 public:
  ~DirectoryDataTypeController() override;

  // DataTypeController implementation.
  bool ShouldLoadModelBeforeConfigure() const override;

  // Directory types need to register with sync engine before LoadModels because
  // downloading initial data happens in parallel with LoadModels.
  void BeforeLoadModels(ModelTypeConfigurer* configurer) override;

  // Directory based data types register with backend before LoadModels in
  // BeforeLoadModels. No need to do anything in RegisterWithBackend.
  RegisterWithBackendResult RegisterWithBackend(
      ModelTypeConfigurer* configurer) override;

  // Directory specific implementation of ActivateDataType with the
  // type specific ChangeProcessor and ModelSafeGroup.
  // Activates change processing on the controlled data type.
  // This is called by DataTypeManager, synchronously with data type's
  // model association.
  // See ModelTypeConfigurer::ActivateDataType for more details.
  void ActivateDataType(ModelTypeConfigurer* configurer) override;

  // Directory specific implementation of DeactivateDataType.
  // Deactivates change processing on the controlled data type (by removing
  // the data type's ChangeProcessor registration with the backend).
  // See ModelTypeConfigurer::DeactivateDataType for more details.
  void DeactivateDataType(ModelTypeConfigurer* configurer) override;
  void Stop(ShutdownReason shutdown_reason, StopCallback callback) final;
  void GetAllNodes(AllNodesCallback callback) override;
  void GetStatusCounters(StatusCountersCallback callback) override;
  void RecordMemoryUsageAndCountsHistograms() override;

  // Convenience overload with synchronous API, since directory types are always
  // capable of stopping immediately.
  virtual void Stop(ShutdownReason shutdown_reason) = 0;

  // Returns a ListValue representing all nodes for a specified type by querying
  // the directory.
  static std::unique_ptr<base::ListValue> GetAllNodesForTypeFromDirectory(
      ModelType type,
      syncable::Directory* directory);

 protected:
  // |dump_stack| is called when an unrecoverable error occurs.
  DirectoryDataTypeController(ModelType type,
                              const base::Closure& dump_stack,
                              SyncService* sync_service,
                              ModelSafeGroup model_safe_group);

  SyncService* sync_service() { return sync_service_; }
  const SyncService* sync_service() const { return sync_service_; }

  // Create an error handler that reports back to this controller.
  virtual std::unique_ptr<DataTypeErrorHandler> CreateErrorHandler() = 0;

  // Access to the ChangeProcessor for the type being controlled by |this|.
  // Returns null if the ChangeProcessor isn't created or connected.
  virtual ChangeProcessor* GetChangeProcessor() const = 0;

  // Function to capture and upload a stack trace when an error occurs.
  base::Closure dump_stack_;

 private:
  SyncService* const sync_service_;

  // The model safe group of this data type.  This should reflect the
  // thread that should be used to modify the data type's native
  // model.
  ModelSafeGroup model_safe_group_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_DIRECTORY_DATA_TYPE_CONTROLLER_H__
