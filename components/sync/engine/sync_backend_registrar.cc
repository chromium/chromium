// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/sync_backend_registrar.h"

#include <algorithm>
#include <cstddef>
#include <utility>

#include "base/logging.h"
#include "components/sync/model/change_processor.h"
#include "components/sync/syncable/user_share.h"

namespace syncer {

SyncBackendRegistrar::SyncBackendRegistrar(
    const std::string& name,
    ModelSafeWorkerFactory worker_factory)
    : name_(name) {
  DCHECK(!worker_factory.is_null());
  MaybeAddWorker(worker_factory, GROUP_UI);
  MaybeAddWorker(worker_factory, GROUP_PASSIVE);
  MaybeAddWorker(worker_factory, GROUP_PASSWORD);
}

void SyncBackendRegistrar::RegisterNonBlockingType(ModelType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AutoLock lock(lock_);
  // There may have been a previously successful sync of a type when passive,
  // which is now NonBlocking. We're not sure what order these two sets of types
  // are being registered in, so guard against SetInitialTypes(...) having been
  // already called by undoing everything to these types.
  if (routing_info_.find(type) != routing_info_.end() &&
      routing_info_[type] != GROUP_NON_BLOCKING) {
    routing_info_.erase(type);
    last_configured_types_.Remove(type);
  }
  non_blocking_types_.Put(type);
}

void SyncBackendRegistrar::SetInitialTypes(ModelTypeSet initial_types) {
  base::AutoLock lock(lock_);

  // This function should be called only once, shortly after construction. The
  // routing info at that point is expected to be empty.
  DCHECK(routing_info_.empty());

  // Set our initial state to reflect the current status of the sync directory.
  // This will ensure that our calculations in ConfigureDataTypes() will always
  // return correct results.
  for (ModelType type : initial_types) {
    // If this type is also registered as NonBlocking, assume that it shouldn't
    // be registered as passive. The NonBlocking path will eventually take care
    // of adding to routing_info_ later on.
    if (!non_blocking_types_.Has(type)) {
      routing_info_[type] = GROUP_PASSIVE;
    }
  }

  if (!workers_.count(GROUP_PASSWORD)) {
    LOG_IF(WARNING, initial_types.Has(PASSWORDS))
        << "Password store not initialized, cannot sync passwords";
    routing_info_.erase(PASSWORDS);
  }

  // Although this can re-set NonBlocking types, this should be idempotent.
  last_configured_types_ = GetRoutingInfoTypes(routing_info_);
}

void SyncBackendRegistrar::AddRestoredNonBlockingType(ModelType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AutoLock lock(lock_);
  DCHECK(non_blocking_types_.Has(type)) << syncer::ModelTypeToString(type);
  DCHECK(routing_info_.find(type) == routing_info_.end() ||
         routing_info_[type] == GROUP_NON_BLOCKING)
      << syncer::ModelTypeToString(type);
  routing_info_[type] = GROUP_NON_BLOCKING;
  last_configured_types_.Put(type);
}

bool SyncBackendRegistrar::IsNigoriEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AutoLock lock(lock_);
  return routing_info_.find(NIGORI) != routing_info_.end();
}

ModelTypeSet SyncBackendRegistrar::ConfigureDataTypes(
    ModelTypeSet types_to_add,
    ModelTypeSet types_to_remove) {
  DCHECK(Intersection(types_to_add, types_to_remove).Empty());
  ModelTypeSet filtered_types_to_add = types_to_add;
  if (workers_.count(GROUP_PASSWORD) == 0) {
    LOG(WARNING) << "No password worker -- removing PASSWORDS";
    filtered_types_to_add.Remove(PASSWORDS);
  }

  base::AutoLock lock(lock_);
  ModelTypeSet newly_added_types;
  for (ModelType type : filtered_types_to_add) {
    // Add a newly specified data type corresponding initial group into the
    // routing_info, if it does not already exist.
    if (routing_info_.count(type) == 0) {
      routing_info_[type] = GetInitialGroupForType(type);
      newly_added_types.Put(type);
    }
  }
  for (ModelType type : types_to_remove) {
    routing_info_.erase(type);
  }

  // TODO(akalin): Use SVLOG/SLOG if we add any more logging.
  DVLOG(1) << name_ << ": Adding types " << ModelTypeSetToString(types_to_add)
           << " (with newly-added types "
           << ModelTypeSetToString(newly_added_types) << ") and removing types "
           << ModelTypeSetToString(types_to_remove)
           << " to get new routing info "
           << ModelSafeRoutingInfoToString(routing_info_);
  last_configured_types_ = GetRoutingInfoTypes(routing_info_);

  return newly_added_types;
}

ModelTypeSet SyncBackendRegistrar::GetLastConfiguredTypes() const {
  return last_configured_types_;
}

void SyncBackendRegistrar::RequestWorkerStopOnUIThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AutoLock lock(lock_);
  for (const auto& kv : workers_) {
    kv.second->RequestStop();
  }
}

void SyncBackendRegistrar::ActivateDataType(ModelType type,
                                            ModelSafeGroup group,
                                            ChangeProcessor* change_processor,
                                            UserShare* user_share) {
  DVLOG(1) << "Activate: " << ModelTypeToString(type);

  base::AutoLock lock(lock_);
  // Ensure that the given data type is in the PASSIVE group.
  auto i = routing_info_.find(type);
  DCHECK(i != routing_info_.end());
  DCHECK_EQ(i->second, GROUP_PASSIVE);
  routing_info_[type] = group;

  // Add the data type's change processor to the list of change
  // processors so it can receive updates.
  DCHECK_EQ(processors_.count(type), 0U);
  processors_[type] = change_processor;

  // Start the change processor.
  change_processor->Start(user_share);
  DCHECK(GetProcessorUnsafe(type));
}

void SyncBackendRegistrar::DeactivateDataType(ModelType type) {
  DVLOG(1) << "Deactivate: " << ModelTypeToString(type);

  if (!IsControlType(type)) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }
  base::AutoLock lock(lock_);

  routing_info_.erase(type);
  ignore_result(processors_.erase(type));
  DCHECK(!GetProcessorUnsafe(type));
}

bool SyncBackendRegistrar::IsTypeActivatedForTest(ModelType type) const {
  return GetProcessor(type) != nullptr;
}

void SyncBackendRegistrar::OnChangesApplied(
    ModelType model_type,
    int64_t model_version,
    const BaseTransaction* trans,
    const ImmutableChangeRecordList& changes) {
  ChangeProcessor* processor = GetProcessor(model_type);
  if (!processor)
    return;

  processor->ApplyChangesFromSyncModel(trans, model_version, changes);
}

void SyncBackendRegistrar::OnChangesComplete(ModelType model_type) {
  ChangeProcessor* processor = GetProcessor(model_type);
  if (!processor)
    return;

  // This call just notifies the processor that it can commit; it
  // already buffered any changes it plans to makes so needs no
  // further information.
  processor->CommitChangesFromSyncModel();
}

void SyncBackendRegistrar::GetWorkers(
    std::vector<scoped_refptr<ModelSafeWorker>>* out) {
  base::AutoLock lock(lock_);
  out->clear();
  for (const auto& kv : workers_) {
    out->push_back(kv.second.get());
  }
}

void SyncBackendRegistrar::GetModelSafeRoutingInfo(ModelSafeRoutingInfo* out) {
  base::AutoLock lock(lock_);
  ModelSafeRoutingInfo copy(routing_info_);
  out->swap(copy);
}

ChangeProcessor* SyncBackendRegistrar::GetProcessor(ModelType type) const {
  base::AutoLock lock(lock_);
  ChangeProcessor* processor = GetProcessorUnsafe(type);
  if (!processor)
    return nullptr;

  // We can only check if |processor| exists, as otherwise the type is
  // mapped to GROUP_PASSIVE.
  DCHECK(IsCurrentThreadSafeForModel(type));
  return processor;
}

ChangeProcessor* SyncBackendRegistrar::GetProcessorUnsafe(
    ModelType type) const {
  lock_.AssertAcquired();
  auto it = processors_.find(type);

  // Until model association happens for a datatype, it will not
  // appear in the processors list.  During this time, it is OK to
  // drop changes on the floor (since model association has not
  // happened yet).  When the data type is activated, model
  // association takes place then the change processor is added to the
  // |processors_| list.
  if (it == processors_.end())
    return nullptr;

  return it->second;
}

bool SyncBackendRegistrar::IsCurrentThreadSafeForModel(
    ModelType model_type) const {
  lock_.AssertAcquired();
  ModelSafeGroup group = GetGroupForModelType(model_type, routing_info_);
  DCHECK_NE(GROUP_NON_BLOCKING, group);

  if (group == GROUP_PASSIVE) {
    return IsControlType(model_type);
  }

  auto it = workers_.find(group);
  DCHECK(it != workers_.end());
  return it->second->IsOnModelSequence();
}

SyncBackendRegistrar::~SyncBackendRegistrar() {
  // All data types should have been deactivated by now.
  DCHECK(processors_.empty());
}

void SyncBackendRegistrar::MaybeAddWorker(ModelSafeWorkerFactory worker_factory,
                                          ModelSafeGroup group) {
  scoped_refptr<ModelSafeWorker> worker = worker_factory.Run(group);
  if (worker) {
    DCHECK(workers_.find(group) == workers_.end());
    workers_[group] = worker;
  }
}

ModelSafeGroup SyncBackendRegistrar::GetInitialGroupForType(
    ModelType type) const {
  return non_blocking_types_.Has(type) ? GROUP_NON_BLOCKING : GROUP_PASSIVE;
}

}  // namespace syncer
