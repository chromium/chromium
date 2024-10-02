// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_DATA_TYPE_CONTROLLER_H_
#define COMPONENTS_SYNC_SERVICE_DATA_TYPE_CONTROLLER_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/sync_mode.h"
#include "components/sync/base/sync_stop_metadata_fate.h"
#include "components/sync/model/data_type_controller_delegate.h"
#include "components/sync/model/model_error.h"
#include "components/sync/service/configure_context.h"
#include "components/sync/service/data_type_local_data_batch_uploader.h"

namespace syncer {

struct ConfigureContext;
struct DataTypeActivationResponse;
struct TypeEntitiesCount;

// DataTypeController are responsible for managing the state of a single data
// type. They are not thread safe and should only be used on the UI thread.
class DataTypeController {
 public:
  // TODO(crbug.com/41461370): Should MODEL_LOADED be renamed to
  // MODEL_READY_TO_CONNECT?
  enum State {
    NOT_RUNNING,     // The controller has never been started or has previously
                     // been stopped.  Must be in this state to start.
    MODEL_STARTING,  // The model is loading.
    MODEL_LOADED,    // The model has finished loading and is ready to connect.
    RUNNING,         // The controller is running and the data type is
                     // in sync with the cloud.
    STOPPING,        // The controller is in the process of stopping
                     // and is waiting for dependent services to stop.
    FAILED           // The controller was started but encountered an error.
  };
  static std::string StateToString(State state);

  // Note: This seems like it should be a OnceCallback, but it can actually be
  // called multiple times in the case of errors.
  using ModelLoadCallback =
      base::RepeatingCallback<void(const std::optional<ModelError>&)>;

  using StopCallback = base::OnceClosure;

  using AllNodesCallback = base::OnceCallback<void(base::Value::List)>;

  using TypeMap = std::map<DataType, std::unique_ptr<DataTypeController>>;
  using TypeVector = std::vector<std::unique_ptr<DataTypeController>>;

  // For legacy data types that do not support transport mode,
  // `delegate_for_transport_mode` may be null.
  // THIS IS NOT SUPPORTED for new data types. When introducing a new data type,
  // you must consider how it should work in transport mode.
  // For types having both "local" and "account" storages, `batch_uploader`
  // can be passed and will be exposed via GetLocalDataBatchUploader()
  // to allow moving local data to the account.
  DataTypeController(
      DataType type,
      std::unique_ptr<DataTypeControllerDelegate> delegate_for_full_sync_mode,
      std::unique_ptr<DataTypeControllerDelegate> delegate_for_transport_mode,
      std::unique_ptr<DataTypeLocalDataBatchUploader> batch_uploader = nullptr);

  DataTypeController(const DataTypeController&) = delete;
  DataTypeController& operator=(const DataTypeController&) = delete;

  virtual ~DataTypeController();

  // Unique data type for this data type controller.
  DataType type() const { return type_; }

  // Name of this data type.  For logging purposes only.
  std::string name() const { return DataTypeToDebugString(type()); }

  // Returns whether the datatype knows how to, and wants to, run in
  // transport-only mode (see syncer::SyncMode enum).
  bool ShouldRunInTransportOnlyMode() const;

  // Begins asynchronous operation of loading the model to get it ready for
  // activation. Once the models are loaded the callback will be invoked with
  // the result. If the models are already loaded it is safe to call the
  // callback right away. Else the callback needs to be stored and called when
  // the models are ready.
  virtual void LoadModels(const ConfigureContext& configure_context,
                          const ModelLoadCallback& model_load_callback);

  // Called by DataTypeManager once the local model has loaded (MODEL_LOADED),
  // in order to enable the sync engine's propagation of sync changes between
  // the server and the local processor. Upon return, the controller assumes
  // that the caller will take care of actually instrumenting the sync engine.
  virtual std::unique_ptr<DataTypeActivationResponse> Connect();

  // Stops the data type. If LoadModels() has not completed it will enter
  // STOPPING state first and eventually STOPPED. Once stopped, |callback| will
  // be run. |callback| must not be null.
  //
  // NOTE: Stop() should be called after sync backend machinery has stopped
  // routing changes to this data type. Stop() should ensure the data type
  // logic shuts down gracefully by flushing remaining changes and calling
  // StopSyncing on the SyncableService. This assumes no changes will ever
  // propagate from sync again from point where Stop() is called.
  virtual void Stop(SyncStopMetadataFate fate, StopCallback callback);

  // Current state of the data type controller.
  virtual State state() const;

  // Whether preconditions are met for the datatype to start. This is useful for
  // example if the datatype depends on certain user preferences other than the
  // ones for sync settings themselves.
  enum class PreconditionState {
    kPreconditionsMet,
    kMustStopAndClearData,
    kMustStopAndKeepData,
  };
  virtual PreconditionState GetPreconditionState() const;

  // Returns whether this data type has any unsynced changes, i.e. any local
  // changes that are waiting to be committed.
  // May be invoked at any time; if the model isn't loaded yet or is in an error
  // state, this should typically return "false".
  virtual void HasUnsyncedData(base::OnceCallback<void(bool)> callback);

  // Returns a Value::List representing all nodes for this data type through
  // |callback| on this thread. Can only be called if state() != NOT_RUNNING.
  // Used for populating nodes in Sync Node Browser of chrome://sync-internals.
  virtual void GetAllNodes(AllNodesCallback callback);

  // Collects TypeEntitiesCount for this datatype and passes them to |callback|.
  // Used to display entity counts in chrome://sync-internals.
  virtual void GetTypeEntitiesCount(
      base::OnceCallback<void(const TypeEntitiesCount&)> callback) const;

  // Records entities count and estimated memory usage of the type into
  // histograms. May do nothing if state() is NOT_RUNNING or FAILED.
  virtual void RecordMemoryUsageAndCountsHistograms();

  // Returns the uploader passed on construction.
  DataTypeLocalDataBatchUploader* GetLocalDataBatchUploader();

  // Reports data type error to simulate the error reported by the bridge.
  virtual void ReportBridgeErrorForTest();

  DataTypeControllerDelegate* GetDelegateForTesting(SyncMode sync_mode);

 protected:
  // Subclasses that use this constructor must call InitDataTypeController().
  explicit DataTypeController(
      DataType type,
      std::unique_ptr<DataTypeLocalDataBatchUploader> batch_uploader = nullptr);

  // |delegate_for_transport_mode| may be null if the type does not run in
  // transport mode.
  void InitDataTypeController(
      std::unique_ptr<DataTypeControllerDelegate> delegate_for_full_sync_mode,
      std::unique_ptr<DataTypeControllerDelegate> delegate_for_transport_mode);

  void ReportModelError(const ModelError& error);

  // Allows subclasses to DCHECK that they're on the correct sequence.
  // TODO(crbug.com/41390876): Rename this to CalledOnValidSequence.
  bool CalledOnValidThread() const;

  // Clears `delegate_map_`. This is useful for derived classes during
  // destruction, as the delegates might reference state from the derived class
  // and otherwise become dangling.
  void ClearDelegateMap();

 private:
  void RecordStartFailure() const;
  void RecordRunFailure() const;
  void OnDelegateStarted(
      std::unique_ptr<DataTypeActivationResponse> activation_response);
  void TriggerCompletionCallbacks(const std::optional<ModelError>& error);
  void ClearMetadataIfStopped();

  // The type this object is responsible for controlling.
  const DataType type_;

  // Null if the DataType does not support batch upload.
  const std::unique_ptr<DataTypeLocalDataBatchUploader> batch_uploader_;

  // Used to check that functions are called on the correct sequence.
  base::SequenceChecker sequence_checker_;

  base::flat_map<SyncMode, std::unique_ptr<DataTypeControllerDelegate>>
      delegate_map_;

  // State of this datatype controller.
  State state_ = NOT_RUNNING;

  // Owned by |delegate_map_|. Null while NOT_RUNNING.
  raw_ptr<DataTypeControllerDelegate> delegate_ = nullptr;

  // Callback for use when starting the datatype (usually MODEL_STARTING, but
  // STOPPING if abort requested while starting).
  ModelLoadCallback model_load_callback_;

  // Callbacks for use when stopping the datatype (STOPPING), which also
  // includes aborting a start. This is important because STOPPING is a state
  // used to make sure we don't request two starts in parallel to the delegate,
  // which is hard to support, most notably in ClientTagBasedDataTypeProcessor.
  // We use a vector because it's allowed to call Stop() multiple times (i.e.
  // while STOPPING).
  std::vector<StopCallback> model_stop_callbacks_;
  SyncStopMetadataFate model_stop_metadata_fate_ = KEEP_METADATA;

  // Controller receives |activation_response_| from
  // ClientTagBasedDataTypeProcessor callback and must temporarily own it until
  // Connect is called.
  std::unique_ptr<DataTypeActivationResponse> activation_response_;

  // This factory must only be used to bind weak pointers to non-virtual member
  // functions that don't call any virtual ones.
  base::WeakPtrFactory<DataTypeController> weak_ptr_factory_{this};
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_DATA_TYPE_CONTROLLER_H_
