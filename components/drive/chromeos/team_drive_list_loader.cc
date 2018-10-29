// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/drive/chromeos/team_drive_list_loader.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/sequenced_task_runner.h"
#include "base/synchronization/cancellation_flag.h"
#include "components/drive/chromeos/change_list_loader.h"
#include "components/drive/chromeos/change_list_processor.h"
#include "components/drive/chromeos/loader_controller.h"
#include "components/drive/drive.pb.h"
#include "components/drive/drive_api_util.h"
#include "components/drive/event_logger.h"
#include "components/drive/file_change.h"
#include "components/drive/file_system_core_util.h"
#include "components/drive/job_scheduler.h"

namespace drive {
namespace internal {

namespace {

// Add any new team drives, or update existing team drives in resource
// metadata.
FileError AddOrUpdateTeamDrives(ResourceEntryVector team_drives,
                                ResourceMetadata* metadata,
                                base::CancellationFlag* in_shutdown) {
  DCHECK(metadata);
  DCHECK(in_shutdown);
  for (const auto& entry : team_drives) {
    if (in_shutdown->IsSet()) {
      return FILE_ERROR_ABORT;
    }
    DCHECK_EQ(entry.parent_local_id(), util::kDriveTeamDrivesDirLocalId);

    std::string local_id;
    FileError error =
        metadata->GetIdByResourceId(entry.resource_id(), &local_id);

    ResourceEntry existing_entry;
    if (error == FILE_ERROR_OK) {
      error = metadata->GetResourceEntryById(local_id, &existing_entry);
    }

    switch (error) {
      // Existing entry in metadata, see if we need to update it
      case FILE_ERROR_OK:
        if (entry.base_name() != existing_entry.base_name()) {
          existing_entry.set_base_name(entry.base_name());
          error = metadata->RefreshEntry(existing_entry);
        }
        break;
      // Add a new entry to metadata
      case FILE_ERROR_NOT_FOUND: {
        std::string local_id;
        error = metadata->AddEntry(entry, &local_id);
        break;
      }
      default:
        return error;
    }
    if (error != FILE_ERROR_OK) {
      return error;
    }
  }
  return FILE_ERROR_OK;
}

// Remove the supplied list of team drives from the resource metadata.
FileError RemoveTeamDrives(ResourceEntryVector team_drives,
                           ResourceMetadata* metadata,
                           base::CancellationFlag* in_shutdown) {
  DCHECK(metadata);
  DCHECK(in_shutdown);
  FileError result = FILE_ERROR_OK;
  for (const auto& entry : team_drives) {
    if (in_shutdown->IsSet()) {
      return FILE_ERROR_ABORT;
    }
    result = metadata->RemoveEntry(entry.local_id());
    if (result != FILE_ERROR_OK) {
      return result;
    }
  }
  return result;
}

}  // namespace

// Used to notify observers of the result of loading the team drives.
struct TeamDriveListLoader::TeamDriveUpdateData {
  std::vector<TeamDrive> all_team_drives;
  std::vector<TeamDrive> added_team_drives;
  std::vector<TeamDrive> removed_team_drives;
};

TeamDriveListLoader::TeamDriveListLoader(
    EventLogger* logger,
    base::SequencedTaskRunner* blocking_task_runner,
    ResourceMetadata* resource_metadata,
    JobScheduler* scheduler,
    LoaderController* apply_task_controller)
    : logger_(logger),
      blocking_task_runner_(blocking_task_runner),
      in_shutdown_(new base::CancellationFlag),
      pending_load_callbacks_(),
      resource_metadata_(resource_metadata),
      scheduler_(scheduler),
      loader_controller_(apply_task_controller),
      weak_ptr_factory_(this) {}

TeamDriveListLoader::~TeamDriveListLoader() {
  in_shutdown_->Set();
  // Delete |in_shutdown_| with the blocking task runner so that it gets deleted
  // after all active ChangeListProcessors.
  blocking_task_runner_->DeleteSoon(FROM_HERE, in_shutdown_.release());
}

void TeamDriveListLoader::AddObserver(TeamDriveListObserver* observer) {
  observers_.AddObserver(observer);
}

void TeamDriveListLoader::RemoveObserver(TeamDriveListObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool TeamDriveListLoader::IsRefreshing() const {
  return !pending_load_callbacks_.empty();
}

void TeamDriveListLoader::CheckForUpdates(
    const FileOperationCallback& callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(callback);

  if (IsRefreshing()) {
    pending_load_callbacks_.emplace_back(callback);
    return;
  }

  pending_load_callbacks_.emplace_back(callback);
  scheduler_->GetAllTeamDriveList(
      base::BindRepeating(&TeamDriveListLoader::OnTeamDriveListLoaded,
                          weak_ptr_factory_.GetWeakPtr()));
}

void TeamDriveListLoader::LoadIfNeeded(const FileOperationCallback& callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(callback);

  if (!loaded_ && !IsRefreshing()) {
    CheckForUpdates(callback);
  } else {
    callback.Run(FILE_ERROR_OK);
  }
}

void TeamDriveListLoader::OnTeamDriveListLoaded(
    google_apis::DriveApiErrorCode status,
    std::unique_ptr<google_apis::TeamDriveList> team_drives) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  FileError error = GDataToFileError(status);
  if (error != FILE_ERROR_OK) {
    logger_->Log(logging::LOG_ERROR,
                 "Failed to retrieve the list of team drives: %s",
                 google_apis::DriveApiErrorCodeToString(status).c_str());
    OnTeamDriveListLoadComplete(error);
    return;
  }

  change_lists_.push_back(std::make_unique<ChangeList>(*team_drives));

  if (!team_drives->next_page_token().empty()) {
    scheduler_->GetRemainingTeamDriveList(
        team_drives->next_page_token(),
        base::BindRepeating(&TeamDriveListLoader::OnTeamDriveListLoaded,
                            weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  ResourceEntryVector team_drive_resource_vector;
  for (auto& change_list : change_lists_) {
    std::move(change_list->entries().begin(), change_list->entries().end(),
              std::back_inserter(team_drive_resource_vector));
  }

  // We will store the current list of team drives in local_resources.
  std::unique_ptr<ResourceEntryVector> local_resources =
      std::make_unique<ResourceEntryVector>();

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE,
      base::BindRepeating(&ResourceMetadata::ReadDirectoryByPath,
                          base::Unretained(resource_metadata_),
                          util::GetDriveTeamDrivesRootPath(),
                          local_resources.get()),
      base::BindRepeating(&TeamDriveListLoader::OnReadDirectoryByPath,
                          weak_ptr_factory_.GetWeakPtr(),
                          base::Owned(local_resources.release()),
                          base::Passed(std::move(team_drive_resource_vector))));
}

void TeamDriveListLoader::OnReadDirectoryByPath(
    ResourceEntryVector* local_resources,
    ResourceEntryVector remote_resources,
    FileError error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(local_resources);

  // We need to sort vectors to use std::set_difference
  std::sort(local_resources->begin(), local_resources->end(),
            [](const ResourceEntry& lhs, const ResourceEntry& rhs) {
              return lhs.resource_id() < rhs.resource_id();
            });
  std::sort(remote_resources.begin(), remote_resources.end(),
            [](const ResourceEntry& lhs, const ResourceEntry& rhs) {
              return lhs.resource_id() < rhs.resource_id();
            });

  // Removed team drives are the ResourceEntry's that are present in
  // local_resources but missing from remote_resources.
  ResourceEntryVector removed_team_drives;
  std::set_difference(local_resources->begin(), local_resources->end(),
                      remote_resources.begin(), remote_resources.end(),
                      std::back_inserter(removed_team_drives),
                      [](const ResourceEntry& lhs, const ResourceEntry& rhs) {
                        return lhs.resource_id() < rhs.resource_id();
                      });

  // Added team drives are the ResourceEntry's that are present in
  // remote_resources but missing from local_resources.
  ResourceEntryVector added_team_drives;
  std::set_difference(remote_resources.begin(), remote_resources.end(),
                      local_resources->begin(), local_resources->end(),
                      std::back_inserter(added_team_drives),
                      [](const ResourceEntry& lhs, const ResourceEntry& rhs) {
                        return lhs.resource_id() < rhs.resource_id();
                      });

  // We need to store the list of team drives, the added drives and the
  // removed drives so we can notify observers on completion.
  TeamDriveUpdateData team_drive_updates;
  std::transform(remote_resources.begin(), remote_resources.end(),
                 std::back_inserter(team_drive_updates.all_team_drives),
                 [](const ResourceEntry& entry) -> TeamDrive {
                   return {entry.resource_id(), entry.base_name(),
                           drive::util::GetDriveTeamDrivesRootPath().Append(
                               util::NormalizeFileName(entry.base_name()))};
                 });

  // Create a copy of the added team drives list to notify observers.
  std::transform(added_team_drives.begin(), added_team_drives.end(),
                 std::back_inserter(team_drive_updates.added_team_drives),
                 [](const ResourceEntry& entry) -> TeamDrive {
                   return {entry.resource_id(), entry.base_name(),
                           drive::util::GetDriveTeamDrivesRootPath().Append(
                               util::NormalizeFileName(entry.base_name()))};
                 });

  // Create a copy of the removed team drives list to notify observers.
  std::transform(removed_team_drives.begin(), removed_team_drives.end(),
                 std::back_inserter(team_drive_updates.removed_team_drives),
                 [](const ResourceEntry& entry) -> TeamDrive {
                   return {entry.resource_id()};
                 });

  // Remove team drives that have been deleted.
  loader_controller_->ScheduleRun(base::BindOnce(
      &drive::util::RunAsyncTask, base::RetainedRef(blocking_task_runner_),
      FROM_HERE,
      base::BindOnce(&RemoveTeamDrives, std::move(removed_team_drives),
                     base::Unretained(resource_metadata_),
                     base::Unretained(in_shutdown_.get())),
      base::BindOnce(&TeamDriveListLoader::OnTeamDrivesRemoved,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(remote_resources),
                     std::move(team_drive_updates))));
}

void TeamDriveListLoader::OnTeamDrivesRemoved(
    ResourceEntryVector remote_resources,
    TeamDriveUpdateData team_drive_updates,
    FileError error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (error != FILE_ERROR_OK) {
    logger_->Log(logging::LOG_ERROR, "Failed to remove team drives: %s",
                 drive::FileErrorToString(error).c_str());
    OnTeamDriveListLoadComplete(error);
    return;
  }

  loader_controller_->ScheduleRun(base::BindOnce(
      &drive::util::RunAsyncTask, base::RetainedRef(blocking_task_runner_),
      FROM_HERE,
      base::BindOnce(&AddOrUpdateTeamDrives, std::move(remote_resources),
                     base::Unretained(resource_metadata_),
                     base::Unretained(in_shutdown_.get())),
      base::BindOnce(&TeamDriveListLoader::OnAddOrUpdateTeamDrives,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(team_drive_updates))));
}

void TeamDriveListLoader::OnAddOrUpdateTeamDrives(
    TeamDriveUpdateData team_drive_updates,
    FileError error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (error != FILE_ERROR_OK) {
    logger_->Log(logging::LOG_ERROR, "Failed to add or update team drives: %s",
                 drive::FileErrorToString(error).c_str());
    OnTeamDriveListLoadComplete(error);
    return;
  }

  for (auto& observer : observers_) {
    observer.OnTeamDriveListLoaded(team_drive_updates.all_team_drives,
                                   team_drive_updates.added_team_drives,
                                   team_drive_updates.removed_team_drives);
  }

  OnTeamDriveListLoadComplete(FILE_ERROR_OK);
}

void TeamDriveListLoader::OnTeamDriveListLoadComplete(FileError error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (error == FILE_ERROR_OK) {
    loaded_ = true;
  }

  for (auto& callback : pending_load_callbacks_) {
    callback.Run(error);
  }
  pending_load_callbacks_.clear();
}

}  // namespace internal
}  // namespace drive
