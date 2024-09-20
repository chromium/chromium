// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/database_maintenance_impl.h"

#include <deque>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/check_is_test.h"
#include "base/containers/adapters.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/database/signal_database.h"
#include "components/segmentation_platform/internal/database/ukm_database.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/stats.h"
#include "components/segmentation_platform/internal/ukm_data_manager.h"
#include "components/segmentation_platform/public/proto/types.pb.h"

namespace {
constexpr uint64_t kFirstCompactionDay = 2;
constexpr uint64_t kMaxSignalStorageDays = 60;
}  // namespace

namespace segmentation_platform {
using SignalIdentifier = DatabaseMaintenanceImpl::SignalIdentifier;

namespace {
// Gets the end of the UTC day time for `current_time`.
base::Time GetEndOfDayTime(base::Time current_time) {
  base::Time day_start_time = current_time.UTCMidnight();
  return day_start_time + base::Days(1) - base::Seconds(1);
}

std::set<SignalIdentifier> CollectAllSignalIdentifiers(
    std::unique_ptr<SegmentInfoDatabase::SegmentInfoList> segment_infos) {
  std::set<SignalIdentifier> signal_ids;
  for (const auto& info : *segment_infos) {
    const proto::SegmentInfo& segment_info = *info.second;
    const auto& metadata = segment_info.model_metadata();
    auto features =
        metadata_utils::GetAllUmaFeatures(metadata, /*include_outputs=*/true);
    for (auto const& feature : features) {
      if (feature.name_hash() != 0 &&
          feature.type() != proto::SignalType::UNKNOWN_SIGNAL_TYPE) {
        signal_ids.insert(std::make_pair(feature.name_hash(), feature.type()));
      }
    }
  }
  return signal_ids;
}

// Takes in the list of tasks and creates a link between each of them, and
// returns the first task which points to the next one, which points to the next
// one, etc., until the last task points to a callback that does nothing.
base::OnceClosure LinkTasks(
    std::vector<base::OnceCallback<void(base::OnceClosure)>> tasks) {
  // Iterate in reverse order over the list of tasks and put them into a type
  // of linked list, where the last task refers to a callback that does
  // nothing.
  base::OnceClosure first_task = base::DoNothing();
  for (base::OnceCallback<void(base::OnceClosure)>& curr_task :
       base::Reversed(tasks)) {
    // We need to first perform the current task, and then move on to the next
    // task which was previously stored in first_task.
    first_task = base::BindOnce(std::move(curr_task), std::move(first_task));
  }
  // All tasks can now be found following from the first task.
  return first_task;
}
}  // namespace

struct DatabaseMaintenanceImpl::CleanupState {
  CleanupState() = default;
  ~CleanupState() = default;

  // Disallow copy and assign.
  CleanupState(const CleanupState&) = delete;
  CleanupState& operator=(const CleanupState&) = delete;

  std::deque<CleanupItem> signals_to_cleanup_;
  std::vector<CleanupItem> cleaned_up_signals_;
};

DatabaseMaintenanceImpl::DatabaseMaintenanceImpl(
    const base::flat_set<SegmentId>& segment_ids,
    base::Clock* clock,
    StorageService* storage_service,
    PrefService* profile_prefs)
    : segment_ids_(segment_ids),
      clock_(clock),
      storage_service_(storage_service),
      profile_prefs_(profile_prefs) {}

DatabaseMaintenanceImpl::~DatabaseMaintenanceImpl() = default;

void DatabaseMaintenanceImpl::ExecuteMaintenanceTasks() {
  auto available_segments =
      storage_service_->segment_info_database()->GetSegmentInfoForBothModels(
          segment_ids_);
  OnSegmentInfoCallback(std::move(available_segments));
}

void DatabaseMaintenanceImpl::OnSegmentInfoCallback(
    std::unique_ptr<SegmentInfoDatabase::SegmentInfoList> segment_infos) {
  std::set<SignalIdentifier> signal_ids =
      CollectAllSignalIdentifiers(std::move(segment_infos));
  stats::RecordMaintenanceSignalIdentifierCount(signal_ids.size());

  auto all_tasks = GetAllTasks(signal_ids);
  auto first_task = LinkTasks(std::move(all_tasks));
  std::move(first_task).Run();
}

std::vector<base::OnceCallback<void(base::OnceClosure)>>
DatabaseMaintenanceImpl::GetAllTasks(std::set<SignalIdentifier> signal_ids) {
  // Create an ordered vector of tasks. These are not yet OnceClosures, since
  // they are missing their reference to the next task. This will be added later
  // using LinkTasks(...).
  std::vector<base::OnceCallback<void(base::OnceClosure)>> tasks;

  // 1) Clean up unnecessary signals.
  tasks.emplace_back(
      base::BindOnce(&DatabaseMaintenanceImpl::CleanupSignalStorage,
                     weak_ptr_factory_.GetWeakPtr(), signal_ids));
  // 2) Compact anything still left in the database.
  tasks.emplace_back(base::BindOnce(&DatabaseMaintenanceImpl::CompactSamples,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    signal_ids));

  return tasks;
}

void DatabaseMaintenanceImpl::CleanupSignalStorage(
    std::set<SignalIdentifier> signal_ids,
    base::OnceClosure next_action) {
  std::vector<CleanupItem> signals_to_cleanup_copy;
  storage_service_->signal_storage_config()->GetSignalsForCleanup(
      signal_ids, signals_to_cleanup_copy);

  if (storage_service_->ukm_data_manager()->HasUkmDatabase()) {
    std::vector<CleanupItem> signals_to_cleanup = signals_to_cleanup_copy;
    storage_service_->ukm_data_manager()->GetUkmDatabase()->CleanupItems(
        storage_service_->profile_id(), std::move(signals_to_cleanup));
  }

  auto cleanup_state = std::make_unique<CleanupState>();
  // Convert the vector of cleanup items to a deque so we can easily handle
  // the state by popping the first one until it is empty.
  for (auto& signal : signals_to_cleanup_copy) {
    // If UKM signal, skip deleting it as it is not present in signal database.
    if (signal.event_hash != CleanupItem::kNonUkmEventHash) {
      continue;
    }
    cleanup_state->signals_to_cleanup_.emplace_back(signal);
  }

  CleanupSignalStorageProcessNext(
      std::move(cleanup_state),
      base::BindOnce(&DatabaseMaintenanceImpl::CleanupSignalStorageDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(next_action)),
      CleanupItem(), false);
}

void DatabaseMaintenanceImpl::CleanupSignalStorageProcessNext(
    std::unique_ptr<CleanupState> cleanup_state,
    base::OnceCallback<void(std::vector<CleanupItem>)> on_done_callback,
    CleanupItem previous_item,
    bool previous_item_deleted) {
  if (previous_item_deleted)
    cleanup_state->cleaned_up_signals_.emplace_back(previous_item);

  if (cleanup_state->signals_to_cleanup_.empty()) {
    std::move(on_done_callback)
        .Run(std::move(cleanup_state->cleaned_up_signals_));
    return;
  }

  CleanupItem cleanup_item = cleanup_state->signals_to_cleanup_.front();
  cleanup_state->signals_to_cleanup_.pop_front();

  proto::SignalType signal_type = cleanup_item.signal_type;
  uint64_t name_hash = cleanup_item.name_hash;
  base::Time end_time = cleanup_item.timestamp;

  storage_service_->signal_database()->DeleteSamples(
      signal_type, name_hash, end_time,
      base::BindOnce(&DatabaseMaintenanceImpl::CleanupSignalStorageProcessNext,
                     weak_ptr_factory_.GetWeakPtr(), std::move(cleanup_state),
                     std::move(on_done_callback), std::move(cleanup_item)));
}

void DatabaseMaintenanceImpl::CleanupSignalStorageDone(
    base::OnceClosure next_action,
    std::vector<CleanupItem> cleaned_up_signals) {
  stats::RecordMaintenanceCleanupSignalSuccessCount(cleaned_up_signals.size());
  storage_service_->signal_storage_config()->UpdateSignalsForCleanup(
      cleaned_up_signals);
  std::move(next_action).Run();
}

void DatabaseMaintenanceImpl::CompactSamples(
    std::set<SignalIdentifier> signal_ids,
    base::OnceClosure next_action) {
  base::Time last_compation_time = base::Time::Min();
  if (profile_prefs_) {
    last_compation_time =
        profile_prefs_->GetTime(kSegmentationLastDBCompactionTimePref);
  } else {
    CHECK_IS_TEST();
  }
  base::Time end_of_day = GetEndOfDayTime(clock_->Now());
  base::Time most_recent_day_to_compact =
      end_of_day - base::Days(kFirstCompactionDay);
  base::Time ealiest_day_to_compact =
      end_of_day - base::Days(kMaxSignalStorageDays);
  base::Time compaction_day = most_recent_day_to_compact;

  while (compaction_day >= ealiest_day_to_compact &&
         compaction_day > last_compation_time) {
    for (auto signal_id : signal_ids) {
      storage_service_->signal_database()->CompactSamplesForDay(
          signal_id.second, signal_id.first, compaction_day,
          base::BindOnce(&DatabaseMaintenanceImpl::RecordCompactionResult,
                         weak_ptr_factory_.GetWeakPtr(), signal_id.second,
                         signal_id.first));
    }
    compaction_day -= base::Days(1);
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&DatabaseMaintenanceImpl::CompactSamplesDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(next_action),
                     most_recent_day_to_compact));
}

void DatabaseMaintenanceImpl::RecordCompactionResult(
    proto::SignalType signal_type,
    uint64_t name_hash,
    bool success) {
  stats::RecordMaintenanceCompactionResult(signal_type, success);
}

void DatabaseMaintenanceImpl::CompactSamplesDone(
    base::OnceClosure next_action,
    base::Time last_compation_time) {
  if (profile_prefs_)
    profile_prefs_->SetTime(kSegmentationLastDBCompactionTimePref,
                            last_compation_time);
  std::move(next_action).Run();
}

}  // namespace segmentation_platform
