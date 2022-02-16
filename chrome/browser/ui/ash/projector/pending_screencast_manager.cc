// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/projector/pending_screencast_manager.h"

#include <map>
#include <vector>

#include "ash/components/drivefs/mojom/drivefs.mojom.h"
#include "ash/public/cpp/projector/projector_controller.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
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

drive::DriveIntegrationService* GetDriveIntegrationServiceForActiveProfile() {
  return drive::DriveIntegrationServiceFactory::FindForProfile(
      ProfileManager::GetActiveUserProfile());
}

drivefs::DriveFsHost* GetDriveFsHostForActiveProfile() {
  auto* drivefs_integration = GetDriveIntegrationServiceForActiveProfile();
  return drivefs_integration ? drivefs_integration->GetDriveFsHost() : nullptr;
}

// Returns a valid pending screencast from `container_absolute_path`.  A valid
// screencast should have 1 media file and 1 metadata file. The
// `container_absolute_path` is the DriveFS absolute path of `container_dir`,
// for example: container_absolute_path = "/{drivefs mounted
// point}/root/{$container_dir}";
absl::optional<ash::PendingScreencast> GetPendingScreencast(
    const base::FilePath& container_dir,
    const base::FilePath& container_absolute_path) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
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
  return pending_screencast;
}

// The `pending_webm_or_projector_events` are new pending ".webm" or
// ".projector" files' events. Checks whether these files are valid screencast
// files, calculate the upload progress, and returns valid pending screencasts.
ash::PendingScreencastSet ProcessAndGenerateNewScreencasts(
    const std::vector<drivefs::mojom::ItemEvent>&
        pending_webm_or_projector_events,
    const base::FilePath drivefs_mounted_point) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // The valid screencasts set.
  ash::PendingScreencastSet screencasts;
  if (!base::PathExists(drivefs_mounted_point))
    return screencasts;

  // A map of container directory path to pending screencast. Each screencast
  // has a unique container directory path in DriveFS.
  std::map<base::FilePath, ash::PendingScreencast> container_to_screencasts;

  // The `pending_event.path` is the file path in drive. It looks like
  // "/root/{folder path in drive}/{file name}".
  for (const auto& pending_event : pending_webm_or_projector_events) {
    // `container_dir` is the parent folder of `pending_event.path` in drive. It
    // looks like "/root/{folder path in drive}".
    const base::FilePath container_dir =
        base::FilePath(pending_event.path).DirName();

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
      iter->second.bytes_transferred += pending_event.bytes_transferred;

      // Skips getting the size of a folder if it has been validated before.
      continue;
    }

    base::FilePath root("/");
    base::FilePath container_absolute_dir(drivefs_mounted_point);
    root.AppendRelativePath(container_dir, &container_absolute_dir);

    auto new_screencast =
        GetPendingScreencast(container_dir, container_absolute_dir);

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

PendingSreencastManager::PendingSreencastManager(
    PendingScreencastChangeCallback pending_screencast_change_callback)
    : pending_screencast_change_callback_(pending_screencast_change_callback),
      blocking_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
  session_manager::SessionManager* session_manager =
      session_manager::SessionManager::Get();
  if (session_manager)
    session_observation_.Observe(session_manager);
}

PendingSreencastManager::~PendingSreencastManager() = default;

void PendingSreencastManager::OnUnmounted() {
  if (!pending_screencast_cache_.empty()) {
    pending_screencast_cache_.clear();
    // Since DriveFS is unmounted, screencasts stop uploading. Notifies pending
    // screencast status has changed.
    pending_screencast_change_callback_.Run(pending_screencast_cache_);
  }
}

// TODO(b/200343894): OnSyncingStatusUpdate() gets called for both upload and
// download event. Find a way to filter out the upload event.
void PendingSreencastManager::OnSyncingStatusUpdate(
    const drivefs::mojom::SyncingStatus& status) {
  drive::DriveIntegrationService* drivefs_integration =
      GetDriveIntegrationServiceForActiveProfile();
  if (!drivefs_integration->IsMounted())
    return;
  std::vector<drivefs::mojom::ItemEvent> pending_webm_or_projector_events;

  for (const auto& event : status.item_events) {
    base::FilePath pending_file = base::FilePath(event->path);
    bool pending =
        event->state == drivefs::mojom::ItemEvent::State::kQueued ||
        event->state == drivefs::mojom::ItemEvent::State::kInProgress;
    // Filters pending ".webm" or ".projector".
    if (!pending || !IsWebmOrProjectorFile(pending_file))
      continue;

    pending_webm_or_projector_events.push_back(
        drivefs::mojom::ItemEvent(*event.get()));
  }

  // The `task` is a blocking I/O operation while `reply` runs on current
  // thread.
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(ProcessAndGenerateNewScreencasts,
                     std::move(pending_webm_or_projector_events),
                     drivefs_integration->GetMountPointPath()),
      base::BindOnce(
          &PendingSreencastManager::OnProcessAndGenerateNewScreencastsFinished,
          weak_ptr_factory_.GetWeakPtr()));
}

// TODO(b/200179137): Handle drive full cannot upload error. Maybe send
// Notification and show failed file on gallery?
void PendingSreencastManager::OnError(const drivefs::mojom::DriveError& error) {
}

const ash::PendingScreencastSet&
PendingSreencastManager::GetPendingScreencasts() const {
  return pending_screencast_cache_;
}

void PendingSreencastManager::OnProcessAndGenerateNewScreencastsFinished(
    const ash::PendingScreencastSet& screencasts) {
  // Return if pending screencasts didn't change.
  if (screencasts == pending_screencast_cache_)
    return;
  pending_screencast_cache_ = screencasts;

  // Notifies pending screencast status changed.
  pending_screencast_change_callback_.Run(pending_screencast_cache_);
}

void PendingSreencastManager::OnUserProfileLoaded(const AccountId& account_id) {
  auto* profile = ProfileManager::GetActiveUserProfile();
  if (!IsProjectorAllowedForProfile(profile))
    return;
  auto* drivefs_host = GetDriveFsHostForActiveProfile();
  // DriveFs could be mounted for different profiles.
  // TODO(b/215199269): Observe ActiveUserChanged for switching between
  // different profiles.
  if (drivefs_host && !drivefs_observation_.IsObserving())
    drivefs_observation_.Observe(drivefs_host);
}
