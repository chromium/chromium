// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_DATABASE_MAINTENANCE_IMPL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_DATABASE_MAINTENANCE_IMPL_H_

#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/database/database_maintenance.h"
#include "components/segmentation_platform/internal/execution/default_model_manager.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "components/segmentation_platform/public/proto/types.pb.h"

class PrefService;

namespace base {
class Clock;
class Time;
}  // namespace base

namespace segmentation_platform {
using proto::SegmentId;

class DefaultModelManager;
class SegmentInfoDatabase;
class SignalDatabase;
class SignalStorageConfig;

// DatabaseMaintenanceImpl is the main implementation of the DatabaseMaintenance
// functionality.
class DatabaseMaintenanceImpl : public DatabaseMaintenance {
 public:
  using SignalIdentifier = std::pair<uint64_t, proto::SignalType>;
  using CleanupItem = std::tuple<uint64_t, proto::SignalType, base::Time>;

  explicit DatabaseMaintenanceImpl(const base::flat_set<SegmentId>& segment_ids,
                                   base::Clock* clock,
                                   SegmentInfoDatabase* segment_info_database,
                                   SignalDatabase* signal_database,
                                   SignalStorageConfig* signal_storage_config,
                                   DefaultModelManager* default_model_manager,
                                   PrefService* profile_prefs);
  ~DatabaseMaintenanceImpl() override;

  // DatabaseMaintenance overrides.
  void ExecuteMaintenanceTasks() override;

 private:
  // Internal struct for tracking the remaining work when cleaning up the
  // signal storage.
  struct CleanupState;

  // All tasks currently need information about various segments, so this is
  // the callback after the initial database lookup for this data.
  void OnSegmentInfoCallback(
      DefaultModelManager::SegmentInfoList segment_infos);

  // Returns an ordered vector of all the tasks we are supposed to perform.
  // These are unfinished and also need to be linked to the next task to be
  // valid.
  std::vector<base::OnceCallback<void(base::OnceClosure)>> GetAllTasks(
      std::set<SignalIdentifier> signal_ids);

  // Signal storage cleanup.
  void CleanupSignalStorage(std::set<SignalIdentifier> signal_ids,
                            base::OnceClosure next_action);
  void CleanupSignalStorageProcessNext(
      std::unique_ptr<CleanupState> cleanup_state,
      base::OnceCallback<void(std::vector<CleanupItem>)> on_done_callback,
      CleanupItem previous_item,
      bool previous_item_deleted);
  void CleanupSignalStorageDone(base::OnceClosure next_action,
                                std::vector<CleanupItem> cleaned_up_signals);

  // Sample compaction.
  void CompactSamples(std::set<SignalIdentifier> signal_ids,
                      base::OnceClosure next_action);
  void RecordCompactionResult(proto::SignalType signal_type,
                              uint64_t name_hash,
                              bool success);
  void CompactSamplesDone(base::OnceClosure next_action,
                          base::Time last_compation_time);

  // Input.
  base::flat_set<SegmentId> segment_ids_;
  raw_ptr<base::Clock> clock_;

  // Databases.
  raw_ptr<SegmentInfoDatabase> segment_info_database_;
  raw_ptr<SignalDatabase> signal_database_;
  raw_ptr<SignalStorageConfig> signal_storage_config_;

  // Default model provider.
  raw_ptr<DefaultModelManager> default_model_manager_;

  // PrefService from profile.
  raw_ptr<PrefService> profile_prefs_;

  base::WeakPtrFactory<DatabaseMaintenanceImpl> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_DATABASE_MAINTENANCE_IMPL_H_
