// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_SYNC_BACKEND_REGISTRAR_H_
#define COMPONENTS_SYNC_ENGINE_SYNC_BACKEND_REGISTRAR_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "components/sync/base/model_type.h"
#include "components/sync/engine/model_safe_worker.h"
#include "components/sync/engine/sync_manager.h"

namespace syncer {

class ChangeProcessor;
struct UserShare;

// A class that keep track of the workers, change processors, and
// routing info for the enabled sync types, and also routes change
// events to the right processors.
class SyncBackendRegistrar : public SyncManager::ChangeDelegate {
 public:
  using ModelSafeWorkerFactory =
      base::Callback<scoped_refptr<ModelSafeWorker>(ModelSafeGroup)>;

  // |name| is used for debugging. Must be created on the UI thread.
  SyncBackendRegistrar(const std::string& name,
                       ModelSafeWorkerFactory worker_factory);

  // A SyncBackendRegistrar is owned by a SyncEngineImpl. It is destroyed by
  // SyncEngineImpl::Shutdown() which performs the following operations on the
  // UI thread:
  //
  //   1) Call SyncBackendRegistrar::RequestWorkerStopOnUIThread().
  //   2) Post a SyncEngineBackend::DoShutdown() task to the sync thread. This
  //      task destroys SyncManager which holds a SyncBackendRegistrar pointer.
  //   3) Take ownership of the sync thread.
  //   4) Post a task to delete the SyncBackendRegistrar on the sync thread.
  //      When this task runs, there are no remaining pointers to the
  //      SyncBackendRegistrar.
  ~SyncBackendRegistrar() override;

  // Adds |type| to set of non-blocking types. These types are assigned to
  // GROUP_NON_BLOCKING model safe group and will be treated differently in
  // ModelTypeRegistry. Unlike directory types, non-blocking types always stay
  // assigned to GROUP_NON_BLOCKING group.
  void RegisterNonBlockingType(ModelType type);

  // Informs the SyncBackendRegistrar of the currently enabled set of types.
  // These types will be placed in the passive group.  This function should be
  // called exactly once during startup.
  void SetInitialTypes(ModelTypeSet initial_types);

  // Informs SyncBackendRegistrar about non-blocking type loaded from local
  // storage. Initial sync was already performed for this type, therefore its
  // data shouldn't be downloaded as part of configuration.
  void AddRestoredNonBlockingType(ModelType type);

  // Returns whether or not we are currently syncing encryption keys.
  // Must be called on the UI thread.
  bool IsNigoriEnabled() const;

  // Removes all types in |types_to_remove| from the routing info and
  // adds all the types in |types_to_add| to the routing info that are
  // not already there (initially put in the passive group).
  // |types_to_remove| and |types_to_add| must be disjoint.  Returns
  // the set of newly-added types.  Must be called on the UI thread.
  ModelTypeSet ConfigureDataTypes(ModelTypeSet types_to_add,
                                  ModelTypeSet types_to_remove);

  // Returns the set of enabled types as of the last configuration. Note that
  // this might be different from the current types in the routing info due
  // to DeactiveDataType being called separately from ConfigureDataTypes.
  ModelTypeSet GetLastConfiguredTypes() const;

  // Must be called from the UI thread. (See destructor comment.)
  void RequestWorkerStopOnUIThread();

  // Activates the given data type (which should belong to the given
  // group) and starts the given change processor.  Must be called
  // from |group|'s native thread.
  void ActivateDataType(ModelType type,
                        ModelSafeGroup group,
                        ChangeProcessor* change_processor,
                        UserShare* user_share);

  // Deactivates the given type if necessary.  Must be called from the
  // UI thread and not |type|'s native thread.  Yes, this is
  // surprising: see http://crbug.com/92804.
  void DeactivateDataType(ModelType type);

  // Returns true only between calls to ActivateDataType(type, ...)
  // and DeactivateDataType(type).  Used only by tests.
  bool IsTypeActivatedForTest(ModelType type) const;

  // SyncManager::ChangeDelegate implementation.  May be called from
  // any thread.
  void OnChangesApplied(ModelType model_type,
                        int64_t model_version,
                        const BaseTransaction* trans,
                        const ImmutableChangeRecordList& changes) override;
  void OnChangesComplete(ModelType model_type) override;

  void GetWorkers(std::vector<scoped_refptr<ModelSafeWorker>>* out);
  void GetModelSafeRoutingInfo(ModelSafeRoutingInfo* out);

 private:
  // Add a worker for |group| to the worker map if one is successfully created
  // by |worker_factory|.
  void MaybeAddWorker(ModelSafeWorkerFactory worker_factory,
                      ModelSafeGroup group);

  // Returns the change processor for the given model, or null if none
  // exists.  Must be called from |group|'s native thread.
  ChangeProcessor* GetProcessor(ModelType type) const;

  // Must be called with |lock_| held.  Simply returns the change
  // processor for the given type, if it exists.  May be called from
  // any thread.
  ChangeProcessor* GetProcessorUnsafe(ModelType type) const;

  // Return true if |model_type| lives on the current thread.  Must be
  // called with |lock_| held.  May be called on any thread.
  bool IsCurrentThreadSafeForModel(ModelType model_type) const;

  // Returns model safe group that should be assigned to type when it is first
  // configured (before activation). Returns GROUP_PASSIVE for directory types
  // and GROUP_NON_BLOCKING for non-blocking types.
  ModelSafeGroup GetInitialGroupForType(ModelType type) const;

  // Name used for debugging.
  const std::string name_;

  // Checker for the UI thread (where this object is constructed).
  SEQUENCE_CHECKER(sequence_checker_);

  // Protects all variables below.
  mutable base::Lock lock_;

  // Workers created by this SyncBackendRegistrar.
  std::map<ModelSafeGroup, scoped_refptr<ModelSafeWorker>> workers_;

  // The change processors that handle the different data types.
  std::map<ModelType, ChangeProcessor*> processors_;

  // Maps ModelType to ModelSafeGroup.
  ModelSafeRoutingInfo routing_info_;

  // The types that were enabled as of the last configuration. Updated on each
  // call to ConfigureDataTypes as well as SetInitialTypes.
  ModelTypeSet last_configured_types_;

  // Set of types with non-blocking implementation (as opposed to directory
  // based).
  ModelTypeSet non_blocking_types_;

  DISALLOW_COPY_AND_ASSIGN(SyncBackendRegistrar);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_SYNC_BACKEND_REGISTRAR_H_
