// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/manage_mirrorsync/manage_mirrorsync_page_handler.h"

#include <string_view>
#include <utility>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/ranges/algorithm.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_manager/path_util.h"

namespace ash {

namespace {

// Do not surface any dotfiles in the UI to enable syncing. They can be
// synced as part of an existing directory but don't allow them to be
// selectable.
bool PathHasDotFile(const base::FilePath& path) {
  std::vector<base::FilePath::StringType> components = path.GetComponents();
  for (const auto& part : components) {
    if (part.front() == '.') {
      return true;
    }
  }
  return false;
}

std::vector<base::FilePath> GetChildFoldersBlocking(
    const base::FilePath& absolute_path,
    int remove_prefix_size) {
  std::vector<base::FilePath> child_folders;
  base::FileEnumerator enumerator(absolute_path, /*recursive=*/false,
                                  base::FileEnumerator::DIRECTORIES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    std::string_view child_path(path.value());

    // Paths are absolute in the form /home/chronos/u-HASH/MyFiles/..., to avoid
    // exposing all the unnecessary parts to the end user remove the prefix that
    // corresponds to the ~/MyFiles directory.
    child_path.remove_prefix(remove_prefix_size);
    base::FilePath stripped_path(child_path);
    if (PathHasDotFile(stripped_path)) {
      continue;
    }
    child_folders.push_back(std::move(stripped_path));
  }
  base::ranges::sort(child_folders);
  return child_folders;
}

}  // namespace

ManageMirrorSyncPageHandler::ManageMirrorSyncPageHandler(
    mojo::PendingReceiver<manage_mirrorsync::mojom::PageHandler>
        pending_page_handler,
    Profile* profile)
    : profile_(profile),
      my_files_dir_(file_manager::util::GetMyFilesFolderForProfile(profile)),
      receiver_{this, std::move(pending_page_handler)} {}

ManageMirrorSyncPageHandler::~ManageMirrorSyncPageHandler() = default;

void ManageMirrorSyncPageHandler::GetChildFolders(
    const base::FilePath& path,
    GetChildFoldersCallback callback) {
  if (path.empty() || !path.IsAbsolute() || path.ReferencesParent()) {
    LOG(ERROR) << "Supplied path is invalid";
    std::move(callback).Run({});
    return;
  }

  std::string_view path_piece(path.value());
  if (path_piece[0] != '/') {
    LOG(ERROR) << "Supplied directory doesn't have leading slash";
    std::move(callback).Run({});
    return;
  }

  path_piece.remove_prefix(1);
  base::FilePath absolute_path = my_files_dir_.Append(path_piece);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&base::DirectoryExists, absolute_path),
      base::BindOnce(&ManageMirrorSyncPageHandler::OnDirectoryExists,
                     weak_ptr_factory_.GetWeakPtr(), absolute_path,
                     std::move(callback)));
}

void ManageMirrorSyncPageHandler::OnDirectoryExists(
    const base::FilePath& absolute_path,
    GetChildFoldersCallback callback,
    bool exists) {
  if (!exists) {
    LOG(ERROR) << "Directory doesn't exist";
    std::move(callback).Run({});
    return;
  }

  const int dir_prefix_size =
      my_files_dir_.StripTrailingSeparators().value().size();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&GetChildFoldersBlocking, absolute_path, dir_prefix_size),
      base::BindOnce(&ManageMirrorSyncPageHandler::OnGetChildFolders,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ManageMirrorSyncPageHandler::OnGetChildFolders(
    GetChildFoldersCallback callback,
    std::vector<base::FilePath> child_folders) {
  std::move(callback).Run(std::move(child_folders));
}

using manage_mirrorsync::mojom::PageHandler;

void ManageMirrorSyncPageHandler::GetSyncingPaths(
    GetSyncingPathsCallback callback) {
  drive::DriveIntegrationService* drive_service =
      drive::DriveIntegrationServiceFactory::GetForProfile(profile_);
  if (drive_service == nullptr) {
    LOG(ERROR) << "Drive service is not available";
    std::move(callback).Run(PageHandler::GetSyncPathError::kServiceUnavailable,
                            {});
    return;
  }

  drive_service->GetSyncingPaths(
      base::BindOnce(&ManageMirrorSyncPageHandler::OnGetSyncingPaths,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ManageMirrorSyncPageHandler::OnGetSyncingPaths(
    GetSyncingPathsCallback callback,
    drive::FileError status,
    const std::vector<base::FilePath>& syncing_paths) {
  if (status != drive::FileError::FILE_ERROR_OK) {
    LOG(ERROR) << "Failed retrieving sync paths: " << status;
    const auto error =
        (status == drive::FileError::FILE_ERROR_SERVICE_UNAVAILABLE)
            ? PageHandler::GetSyncPathError::kServiceUnavailable
            : PageHandler::GetSyncPathError::kFailed;
    std::move(callback).Run(std::move(error), {});
    return;
  }

  std::vector<base::FilePath> remapped_paths;
  for (const base::FilePath& path : syncing_paths) {
    if (!my_files_dir_.IsParent(path)) {
      LOG(ERROR) << "Syncing path is not parented at MyFiles";
      continue;
    }
    base::FilePath remapped_path("/");
    my_files_dir_.AppendRelativePath(path, &remapped_path);
    remapped_paths.push_back(std::move(remapped_path));
  }
  std::move(callback).Run(PageHandler::GetSyncPathError::kSuccess,
                          std::move(remapped_paths));
}

}  // namespace ash
