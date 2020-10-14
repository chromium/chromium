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
#include "components/sync/engine/sync_manager.h"

namespace syncer {

// A class that keep track of the routing info for the enabled sync types.
// TODO(crbug.com/1138132): This class is a remainder from the old Directory
// implementation and should be removed.
class SyncBackendRegistrar {
 public:
  // |name| is used for debugging. Must be created on the UI thread.
  explicit SyncBackendRegistrar(const std::string& name);

  // A SyncBackendRegistrar is owned by a SyncEngineImpl. It is destroyed by
  // SyncEngineImpl::Shutdown() which performs the following operations on the
  // UI thread:
  //
  //   1) Post a SyncEngineBackend::DoShutdown() task to the sync thread. This
  //      task destroys SyncManager which holds a SyncBackendRegistrar pointer.
  //   2) Take ownership of the sync thread.
  //   3) Post a task to delete the SyncBackendRegistrar on the sync thread.
  //      When this task runs, there are no remaining pointers to the
  //      SyncBackendRegistrar.
  ~SyncBackendRegistrar();

  // Adds |type| to the set of registered types.
  void RegisterDataType(ModelType type);

  // Informs the SyncBackendRegistrar of the currently enabled set of types.
  // These types will be placed in the passive group.  This function should be
  // called exactly once during startup.
  void SetInitialTypes(ModelTypeSet initial_types);

  // Informs SyncBackendRegistrar about a type loaded from local storage.
  // Initial sync was already performed for this type, therefore its data
  // shouldn't be downloaded as part of configuration.
  void AddRestoredDataType(ModelType type);

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

  // Returns the set of currently enabled types.
  ModelTypeSet GetTypesWithRoutingInfo() const;

 private:
  // Legacy concept of model-safe groups, no longer relevant as of 2020.
  // TODO(crbug.com/1138132): Delete this enum.
  enum ModelSafeGroup { GROUP_PASSIVE, GROUP_NON_BLOCKING };

  // Same as GetTypesWithRoutingInfo() but callers are responsible for holding
  // |lock_|.
  ModelTypeSet GetTypesWithRoutingInfoNoLock() const;

  // Returns model safe group that should be assigned to type when it is first
  // configured (before activation).
  ModelSafeGroup GetInitialGroupForType(ModelType type) const;

  // Name used for debugging.
  const std::string name_;

  // Checker for the UI thread (where this object is constructed).
  SEQUENCE_CHECKER(sequence_checker_);

  // Protects all variables below.
  mutable base::Lock lock_;

  // Maps ModelType to ModelSafeGroup.
  std::map<ModelType, ModelSafeGroup> routing_info_;

  // The types that were enabled as of the last configuration. Updated on each
  // call to ConfigureDataTypes as well as SetInitialTypes.
  ModelTypeSet last_configured_types_;

  // Set of registered types.
  ModelTypeSet registered_types_;

  DISALLOW_COPY_AND_ASSIGN(SyncBackendRegistrar);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_SYNC_BACKEND_REGISTRAR_H_
