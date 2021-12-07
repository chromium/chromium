// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/projector/pending_screencast_manager.h"

#include "ash/components/drivefs/mojom/drivefs.mojom.h"
#include "ash/public/cpp/projector/projector_controller.h"
#include "ash/webui/projector_app/projector_app_client.h"
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

// Verifies whether `relative_path` is a valid screencast container. A valid
// screencast should have at least 1 media file and 1 metadata file. The
// 'relative_path' looks like
// "/{drivefs mounted point}/root/{folder path in drive}"
bool IsScreencastContainer(const base::FilePath& relative_path) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!base::PathExists(relative_path))
    return false;

  int number_of_metadata_files = 0;
  int number_of_media_files = 0;
  base::FileEnumerator files(relative_path, /* recursive */ false,
                             base::FileEnumerator::FILES);
  const std::string metadata_extension = GetMetadataFileExtension();
  for (base::FilePath path = files.Next(); !path.empty(); path = files.Next()) {
    if (path.MatchesExtension(metadata_extension))
      number_of_metadata_files++;

    if (path.MatchesExtension(kMediaExtension))
      number_of_media_files++;

    if (number_of_media_files > 0 && number_of_metadata_files > 0)
      return true;
  }

  return number_of_media_files > 0 && number_of_metadata_files > 0;
}

// The `pending_webm_or_projector_files` are new pending ".webm" or ".projector"
// files. Checks whether these files are valid screencast files and returns
// valid pending screencasts.
std::set<ash::PendingScreencast> ProcessAndGenerateNewScreencasts(
    const std::vector<base::FilePath>& pending_webm_or_projector_files,
    const base::FilePath drivefs_mounted_point) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // The valid screencasts set.
  std::set<ash::PendingScreencast> screencasts;
  if (!base::PathExists(drivefs_mounted_point))
    return screencasts;

  // `pending_file` is a relative path to DriveFs mounted folder. It looks like
  // "/root/{folder path in drive}/{file name}".
  for (const auto& pending_file : pending_webm_or_projector_files) {
    // `container_dir` is the parent folder of `pending_file` in drive. It looks
    // like "/root/{folder path in drive}".
    const base::FilePath container_dir = pending_file.DirName();
    ash::PendingScreencast screencast;
    screencast.container_dir = container_dir;
    // The display name of the a pending screencast is the name of the container
    // folder name of this screencast.
    screencast.name = container_dir.BaseName().value();

    // During this loop, items of multiple events might be under the same
    // folder. Skips folders that have been validated before.
    if (screencasts.find(screencast) != screencasts.end())
      continue;

    base::FilePath root("/");
    base::FilePath container_absolute_dir(drivefs_mounted_point);
    root.AppendRelativePath(container_dir, &container_absolute_dir);
    if (IsScreencastContainer(container_absolute_dir))
      screencasts.emplace(screencast);
  }

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
  std::vector<base::FilePath> pending_webm_or_projector_files;

  for (const auto& event : status.item_events) {
    base::FilePath pending_file = base::FilePath(event->path);
    bool pending =
        event->state == drivefs::mojom::ItemEvent::State::kQueued ||
        event->state == drivefs::mojom::ItemEvent::State::kInProgress;
    // Filters pending ".webm" or ".projector".
    if (!pending || !IsWebmOrProjectorFile(pending_file)) {
      continue;
    }

    pending_webm_or_projector_files.push_back(pending_file);
  }

  // The `task` is a blocking I/O operation while `reply` runs on current
  // thread.
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          ProcessAndGenerateNewScreencasts,
          std::move(pending_webm_or_projector_files),
          GetDriveIntegrationServiceForActiveProfile()->GetMountPointPath()),
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

const std::set<ash::PendingScreencast>&
PendingSreencastManager::GetPendingScreencasts() const {
  return pending_screencast_cache_;
}

void PendingSreencastManager::OnProcessAndGenerateNewScreencastsFinished(
    const std::set<ash::PendingScreencast>& screencasts) {
  // Return if pending screencasts didn't change.
  if (screencasts == pending_screencast_cache_)
    return;
  pending_screencast_cache_ = screencasts;

  // Notifies pending screencast status changed.
  pending_screencast_change_callback_.Run(pending_screencast_cache_);
}
