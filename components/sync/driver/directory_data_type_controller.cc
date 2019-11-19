// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/directory_data_type_controller.h"

#include <utility>
#include <vector>

#include "base/threading/thread_task_runner_handle.h"
#include "components/sync/base/data_type_histogram.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/engine/model_type_configurer.h"
#include "components/sync/syncable/syncable_read_transaction.h"
#include "components/sync/syncable/user_share.h"

namespace syncer {

DirectoryDataTypeController::DirectoryDataTypeController(
    ModelType type,
    const base::Closure& dump_stack,
    SyncService* sync_service,
    ModelSafeGroup model_safe_group)
    : DataTypeController(type),
      dump_stack_(dump_stack),
      sync_service_(sync_service),
      model_safe_group_(model_safe_group) {}

DirectoryDataTypeController::~DirectoryDataTypeController() {}

bool DirectoryDataTypeController::ShouldLoadModelBeforeConfigure() const {
  // Directory datatypes don't require loading models before configure. Their
  // progress markers are stored in directory and can be extracted without
  // datatype participation.
  return false;
}

void DirectoryDataTypeController::BeforeLoadModels(
    ModelTypeConfigurer* configurer) {
  configurer->RegisterDirectoryDataType(type(), model_safe_group_);
}

DataTypeController::RegisterWithBackendResult
DirectoryDataTypeController::RegisterWithBackend(
    ModelTypeConfigurer* configurer) {
  return REGISTRATION_IGNORED;
}

void DirectoryDataTypeController::ActivateDataType(
    ModelTypeConfigurer* configurer) {
  DCHECK(CalledOnValidThread());
  // Tell the backend about the change processor for this type so it can
  // begin routing changes to it.
  configurer->ActivateDirectoryDataType(type(), model_safe_group_,
                                        GetChangeProcessor());
}

void DirectoryDataTypeController::DeactivateDataType(
    ModelTypeConfigurer* configurer) {
  DCHECK(CalledOnValidThread());
  configurer->DeactivateDirectoryDataType(type());
  configurer->UnregisterDirectoryDataType(type());
}

void DirectoryDataTypeController::Stop(ShutdownReason shutdown_reason,
                                       StopCallback callback) {
  DCHECK(CalledOnValidThread());
  Stop(shutdown_reason);
  std::move(callback).Run();
}

void DirectoryDataTypeController::GetAllNodes(AllNodesCallback callback) {
  std::unique_ptr<base::ListValue> node_list = GetAllNodesForTypeFromDirectory(
      type(), sync_service_->GetUserShare()->directory.get());
  std::move(callback).Run(type(), std::move(node_list));
}

void DirectoryDataTypeController::GetStatusCounters(
    StatusCountersCallback callback) {
  std::vector<int> num_entries_by_type(syncer::ModelType::NUM_ENTRIES, 0);
  std::vector<int> num_to_delete_entries_by_type(syncer::ModelType::NUM_ENTRIES,
                                                 0);
  sync_service_->GetUserShare()->directory->CollectMetaHandleCounts(
      &num_entries_by_type, &num_to_delete_entries_by_type);
  syncer::StatusCounters counters;
  counters.num_entries_and_tombstones = num_entries_by_type[type()];
  counters.num_entries =
      num_entries_by_type[type()] - num_to_delete_entries_by_type[type()];

  std::move(callback).Run(type(), counters);
}

void DirectoryDataTypeController::RecordMemoryUsageAndCountsHistograms() {
  syncer::syncable::Directory* directory =
      sync_service_->GetUserShare()->directory.get();
  SyncRecordModelTypeMemoryHistogram(
      type(), directory->EstimateMemoryUsageByType(type()));
  int count_excl_root_node = directory->CountEntriesByType(type()) - 1;
  SyncRecordModelTypeCountHistogram(type(), count_excl_root_node);
}

// static
std::unique_ptr<base::ListValue>
DirectoryDataTypeController::GetAllNodesForTypeFromDirectory(
    ModelType type,
    syncable::Directory* directory) {
  DCHECK(directory);
  syncable::ReadTransaction trans(FROM_HERE, directory);
  return directory->GetNodeDetailsForType(&trans, type);
}

}  // namespace syncer
