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
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

// Returned by GetScreencastContainerSize(const base::FilePath& relative_path)
// when the given `relative_path` is an invalid screencast container.
constexpr int64_t kScreencastSizeUnavailable = -1;

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

// Return the size in bytes of screencast if `absolute_path` is a valid
// screencast container, otherwise return kScreencastSizeUnavailable. A valid
// screencast should have at least 1 media file and 1 metadata file. The
// 'absolute_path' looks like
// "/{drivefs mounted point}/root/{folder path in drive}".
int64_t GetScreencastContainerSize(const base::FilePath& absolute_path) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!base::PathExists(absolute_path))
    return kScreencastSizeUnavailable;

  int size_of_metadata_files = 0;
  int size_of_media_files = 0;
  base::FileEnumerator files(absolute_path, /*recursive=*/false,
                             base::FileEnumerator::FILES);
  const std::string metadata_extension = GetMetadataFileExtension();
  for (base::FilePath path = files.Next(); !path.empty(); path = files.Next()) {
    if (path.MatchesExtension(GetMetadataFileExtension()))
      size_of_metadata_files += files.GetInfo().GetSize();
    else if (path.MatchesExtension(kMediaExtension))
      size_of_media_files += files.GetInfo().GetSize();
  }

  return size_of_media_files > 0 && size_of_metadata_files > 0
             ? size_of_media_files + size_of_metadata_files
             : kScreencastSizeUnavailable;
}

// The `pending_webm_or_projector_events` are new pending ".webm" or
// ".projector" files' events. Checks whether these files are valid screencast
// files, calculate the upload progress, and returns valid pending screencasts.
ash::PendingScreencastSet ProcessAndGenerateNewScreencasts(
    const std::vector<drivefs::mojom::ItemEvent>&
        pending_webm_or_projector_events,
    drive::DriveIntegrationService* drivefs_integration) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  const base::FilePath drivefs_mounted_point =
      drivefs_integration->GetMountPointPath();
  // The valid screencasts set.
  ash::PendingScreencastSet screencasts;
  if (!drivefs_integration->IsMounted() ||
      !base::PathExists(drivefs_mounted_point)) {
    return screencasts;
  }

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
    ash::PendingScreencast screencast;
    screencast.container_dir = container_dir;
    // The display name of the a pending screencast is the name of the container
    // folder name of this screencast.
    screencast.name = container_dir.BaseName().value();

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
    const int64_t total_size_in_bytes =
        GetScreencastContainerSize(container_absolute_dir);
    if (total_size_in_bytes != -1) {
      screencast.total_size_in_bytes = total_size_in_bytes;
      screencast.bytes_transferred = pending_event.bytes_transferred;
      container_to_screencasts[container_dir] = screencast;
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
    session_manager->AddObserver(this);
}

PendingSreencastManager::~PendingSreencastManager() {
  session_manager::SessionManager* session_manager =
      session_manager::SessionManager::Get();
  if (session_manager) {
    session_manager->RemoveObserver(this);
    auto* drivefs_host = GetDriveFsHostForActiveProfile();
    if (drivefs_host)
      drivefs_host->RemoveObserver(this);
  }
}

void PendingSreencastManager::OnUnmounted() {
  if (pending_screencast_cache_.empty()) {
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
                     GetDriveIntegrationServiceForActiveProfile()),
      base::BindOnce(
          &PendingSreencastManager::OnProcessAndGenerateNewScreencastsFinished,
          weak_ptr_factory_.GetWeakPtr()));
}

// TODO(b/200179137): Handle drive full cannot upload error. Maybe send
// Notification and show failed file on gallery?
void PendingSreencastManager::OnError(const drivefs::mojom::DriveError& error) {
}

void PendingSreencastManager::OnUserSessionStarted(bool is_primary_user) {
  auto* drivefs_host = GetDriveFsHostForActiveProfile();
  if (drivefs_host)
    GetDriveFsHostForActiveProfile()->AddObserver(this);
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
