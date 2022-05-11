// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/projector/pending_screencast_manager.h"

#include <map>
#include <vector>

#include "ash/components/drivefs/mojom/drivefs.mojom.h"
#include "ash/projector/projector_metrics.h"
#include "ash/public/cpp/projector/projector_controller.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/projector/projector_utils.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

constexpr base::FilePath::CharType kMediaExtension[] =
    FILE_PATH_LITERAL(".webm");

const std::string GetMetadataFileExtension() {
  return base::StrCat({".", ash::kProjectorMetadataFileExtension});
}

bool IsWebmOrProjectorFile(const base::FilePath& path) {
  return path.MatchesExtension(kMediaExtension) ||
         path.MatchesExtension(GetMetadataFileExtension());
}

drivefs::DriveFsHost* GetDriveFsHostForActiveProfile() {
  auto* drivefs_integration = GetDriveIntegrationServiceForActiveProfile();
  return drivefs_integration ? drivefs_integration->GetDriveFsHost() : nullptr;
}

// Returns a valid pending screencast from `container_absolute_path`.  A valid
// screencast should have 1 media file and 1 metadata file.
absl::optional<ash::PendingScreencast> GetPendingScreencast(
    const base::FilePath& container_dir,
    const base::FilePath& drivefs_mounted_point,
    bool upload_failed) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  base::FilePath root("/");
  // `container_absolute_path` is the DriveFS absolute path of
  // `container_dir`, for example: container_absolute_path =
  // "/{$drivefs_mounted_point}/root/{$container_dir}";
  base::FilePath container_absolute_path(drivefs_mounted_point);
  root.AppendRelativePath(container_dir, &container_absolute_path);
  if (!base::PathExists(container_absolute_path))
    return absl::nullopt;

  int64_t total_size_in_bytes = 0;
  int media_file_count = 0;
  int metadata_file_count = 0;

  base::Time created_time;
  std::string media_name;

  base::FileEnumerator files(container_absolute_path, /*recursive=*/false,
                             base::FileEnumerator::FILES);

  // Calculates the size of media file and metadata file, and the created time
  // of media.
  const std::string metadata_extension = GetMetadataFileExtension();
  for (base::FilePath path = files.Next(); !path.empty(); path = files.Next()) {
    if (path.MatchesExtension(metadata_extension)) {
      total_size_in_bytes += files.GetInfo().GetSize();
      media_file_count++;
    } else if (path.MatchesExtension(kMediaExtension)) {
      base::File::Info info;
      if (!base::GetFileInfo(path, &info))
        continue;
      created_time = info.creation_time;
      total_size_in_bytes += files.GetInfo().GetSize();
      media_name = path.BaseName().RemoveExtension().value();
      metadata_file_count++;
    }

    // Return null if the screencast is not valid.
    if (media_file_count > 1 || metadata_file_count > 1)
      return absl::nullopt;
  }

  // Return null if the screencast is not valid.
  if (media_file_count != 1 || metadata_file_count != 1)
    return absl::nullopt;

  ash::PendingScreencast pending_screencast{container_dir};
  pending_screencast.created_time = created_time;
  pending_screencast.name = media_name;
  pending_screencast.total_size_in_bytes = total_size_in_bytes;
  pending_screencast.upload_failed = upload_failed;
  return pending_screencast;
}

// The `pending_webm_or_projector_events` are new uploading ".webm" or
// ".projector" files' events. The `error_syncing_file` are ".webm" or
// ".projector" files which failed to upload. Checks whether these files are
// valid screencast files. Calculates the upload progress or error state and
// returns valid pending or error screencasts.
ash::PendingScreencastSet ProcessAndGenerateNewScreencasts(
    const std::vector<drivefs::mojom::ItemEvent>&
        pending_webm_or_projector_events,
    const std::set<base::FilePath>& error_syncing_file,
    const base::FilePath drivefs_mounted_point) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // The valid screencasts set.
  ash::PendingScreencastSet screencasts;

  if (!base::PathExists(drivefs_mounted_point) ||
      (pending_webm_or_projector_events.empty() &&
       error_syncing_file.empty())) {
    return screencasts;
  }

  // A map of container directory path to pending screencast. Each screencast
  // has a unique container directory path in DriveFS.
  std::map<base::FilePath, ash::PendingScreencast> container_to_screencasts;

  // Creates error screencasts from `error_syncing_file`:
  for (const auto& upload_failed_file : error_syncing_file) {
    const base::FilePath container_dir = upload_failed_file.DirName();
    auto new_screencast = GetPendingScreencast(
        container_dir, drivefs_mounted_point, /*upload_failed=*/true);
    if (new_screencast)
      container_to_screencasts[container_dir] = new_screencast.value();
  }

  // Creates uploading screencasts from `pending_webm_or_projector_events`:

  // The `pending_event.path` is the file path in drive. It looks like
  // "/root/{folder path in drive}/{file name}".
  for (const auto& pending_event : pending_webm_or_projector_events) {
    base::FilePath event_file = base::FilePath(pending_event.path);
    // `container_dir` is the parent folder of `pending_event.path` in drive. It
    // looks like "/root/{folder path in drive}".
    const base::FilePath container_dir = event_file.DirName();

    // During this loop, items of multiple events might be under the same
    // folder.
    auto iter = container_to_screencasts.find(container_dir);
    if (iter != container_to_screencasts.end()) {
      // Calculates remaining untranferred bytes of a screencast by adding up
      // its transferred bytes of its files. `pending_event.bytes_to_transfer`
      // is the total bytes of current file.
      // TODO(b/209854146) Not all files appear in
      // `pending_webm_or_projector_events.bytes_transferred`. The missing files
      // might be uploaded or not uploaded. To get an accurate
      // `bytes_transferred`, use DriveIntegrationService::GetMetadata().
      if (!iter->second.upload_failed)
        iter->second.bytes_transferred += pending_event.bytes_transferred;

      // Skips getting the size of a folder if it has been validated before.
      continue;
    }

    auto new_screencast = GetPendingScreencast(
        container_dir, drivefs_mounted_point, /*upload_failed=*/false);

    if (new_screencast) {
      new_screencast->bytes_transferred = pending_event.bytes_transferred;
      container_to_screencasts[container_dir] = new_screencast.value();
    }
  }

  for (const auto& pair : container_to_screencasts)
    screencasts.insert(pair.second);

  return screencasts;
}

}  // namespace

PendingScreencastManager::PendingScreencastManager(
    PendingScreencastChangeCallback pending_screencast_change_callback)
    : pending_screencast_change_callback_(pending_screencast_change_callback),
      blocking_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
  session_manager::SessionManager* session_manager =
      session_manager::SessionManager::Get();
  if (session_manager)
    session_observation_.Observe(session_manager);
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (user_manager)
    session_state_observation_.Observe(user_manager);
}

PendingScreencastManager::~PendingScreencastManager() = default;

bool PendingScreencastManager::IsDriveFsObservationObservingSource(
    drivefs::DriveFsHost* source) const {
  return drivefs_observation_.IsObservingSource(source);
}

void PendingScreencastManager::OnUnmounted() {
  if (!pending_screencast_cache_.empty()) {
    pending_screencast_cache_.clear();
    // Since DriveFS is unmounted, screencasts stop uploading. Notifies pending
    // screencast status has changed.
    pending_screencast_change_callback_.Run(pending_screencast_cache_);
    last_pending_screencast_change_tick_ = base::TimeTicks();
  }
  error_syncing_files_.clear();
}

// Generates new pending upload screencasts list base on `error_syncing_files_`
// and files from drivefs::mojom::SyncingStatus.
//
// When file in error_syncing_files_ complete uploading, remove from
// `error_syncing_files_` so failed screencasts will be removed from pending
// screencast list.
// TODO(b/200343894): OnSyncingStatusUpdate() gets called for both upload and
// download event. Find a way to filter out the upload event.
void PendingScreencastManager::OnSyncingStatusUpdate(
    const drivefs::mojom::SyncingStatus& status) {
  drive::DriveIntegrationService* drivefs_integration =
      GetDriveIntegrationServiceForActiveProfile();
  if (!drivefs_integration->IsMounted())
    return;
  std::vector<drivefs::mojom::ItemEvent> pending_webm_or_projector_events;
  for (const auto& event : status.item_events) {
    base::FilePath event_file = base::FilePath(event->path);
    // If observe a error uploaded file is now successfully uploaded, remove it
    // from `error_syncing_files_`.
    if (event->state == drivefs::mojom::ItemEvent::State::kCompleted)
      error_syncing_files_.erase(event_file);

    bool pending =
        event->state == drivefs::mojom::ItemEvent::State::kQueued ||
        event->state == drivefs::mojom::ItemEvent::State::kInProgress;
    // Filters pending ".webm" or ".projector".
    if (!pending || !IsWebmOrProjectorFile(event_file))
      continue;

    pending_webm_or_projector_events.push_back(
        drivefs::mojom::ItemEvent(*event.get()));
  }

  // If the `pending_webm_or_projector_events`, `error_syncing_files_` and
  // `pending_screencast_cache_` are empty, return early because the syncing may
  // be triggered by files that are not related to Projector.
  if (pending_webm_or_projector_events.empty() &&
      error_syncing_files_.empty() && pending_screencast_cache_.empty()) {
    return;
  }

  // The `task` is a blocking I/O operation while `reply` runs on current
  // thread.
  // TODO(b/223668878) OnSyncingStatusUpdate might get called multiple times
  // within 1s. Add a repeat timer to trigger this task for less frequency.
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(ProcessAndGenerateNewScreencasts,
                     std::move(pending_webm_or_projector_events),
                     error_syncing_files_,
                     drivefs_integration->GetMountPointPath()),
      base::BindOnce(
          &PendingScreencastManager::OnProcessAndGenerateNewScreencastsFinished,
          weak_ptr_factory_.GetWeakPtr(),
          /*task_start_tick=*/base::TimeTicks::Now()));
}

// Observes the Drive OnError event and add the related files to
// `error_syncing_files_`. The validation of a screencast happens in
// OnSyncingStatusUpdate because the drivefs::mojom::SyncingStatus contains the
// info about the file completed uploaded or not and other files status for the
// same screencast.
void PendingScreencastManager::OnError(
    const drivefs::mojom::DriveError& error) {
  base::FilePath error_file = base::FilePath(error.path);
  // mojom::DriveError::Type has 2 types: kCantUploadStorageFull and
  // kPinningFailedDiskFull. Only handle kCantUploadStorageFull so far.
  if (error.type != drivefs::mojom::DriveError::Type::kCantUploadStorageFull ||
      !IsWebmOrProjectorFile(error_file)) {
    return;
  }
  error_syncing_files_.insert(error_file);
}

const ash::PendingScreencastSet&
PendingScreencastManager::GetPendingScreencasts() const {
  return pending_screencast_cache_;
}

void PendingScreencastManager::OnProcessAndGenerateNewScreencastsFinished(
    const base::TimeTicks task_start_tick,
    const ash::PendingScreencastSet& screencasts) {
  const base::TimeTicks now = base::TimeTicks::Now();
  ash::RecordPendingScreencastBatchIOTaskDuration(now - task_start_tick);

  // Returns if pending screencasts didn't change.
  if (screencasts == pending_screencast_cache_)
    return;
  pending_screencast_cache_ = screencasts;

  // Notifies pending screencast status changed.
  pending_screencast_change_callback_.Run(pending_screencast_cache_);
  if (!last_pending_screencast_change_tick_.is_null()) {
    ash::RecordPendingScreencastChangeInterval(
        now - last_pending_screencast_change_tick_);
  }
  // Resets `last_pending_screencast_change_tick_` to null. We don't track time
  // delta between finish uploading and new uploading started.
  last_pending_screencast_change_tick_ =
      pending_screencast_cache_.empty() ? base::TimeTicks() : now;
}

void PendingScreencastManager::OnUserProfileLoaded(
    const AccountId& account_id) {
  MaybeSwitchDriveFsObservation();
}

void PendingScreencastManager::ActiveUserChanged(
    user_manager::User* active_user) {
  // After user login, the first ActiveUserChanged() might be called before
  // profile is loaded.
  if (!active_user->is_profile_created())
    return;

  MaybeSwitchDriveFsObservation();
}

void PendingScreencastManager::MaybeSwitchDriveFsObservation() {
  auto* profile = ProfileManager::GetActiveUserProfile();

  if (!IsProjectorAllowedForProfile(profile))
    return;

  auto* drivefs_host = GetDriveFsHostForActiveProfile();
  if (!drivefs_host || drivefs_observation_.IsObservingSource(drivefs_host))
    return;

  pending_screencast_cache_.clear();
  error_syncing_files_.clear();

  // Reset if observing DriveFsHost of other profile.
  if (drivefs_observation_.IsObserving())
    drivefs_observation_.Reset();

  drivefs_observation_.Observe(drivefs_host);
}
