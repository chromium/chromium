// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/chrome_pwa_launcher/launcher_update.h"

#include "base/check.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "chrome/browser/web_applications/chrome_pwa_launcher/chrome_pwa_launcher_util.h"
#include "chrome/installer/util/callback_work_item.h"
#include "chrome/installer/util/delete_tree_work_item.h"
#include "chrome/installer/util/work_item_list.h"

namespace {

constexpr base::FilePath::StringPieceType kOldLauncherSuffix =
    FILE_PATH_LITERAL("_old");

// A callback invoked by |work_item| that tries to create a hardlink to
// |latest_version_path| at |launcher_path|. If it fails, tries to create a copy
// of |latest_version_path| at |launcher_path| instead. Returns true if either a
// hardlink or copy were created, or false otherwise.
bool CreateHardLinkOrCopyCallback(const base::FilePath& launcher_path,
                                  const base::FilePath& latest_version_path,
                                  const CallbackWorkItem& work_item) {
  return base::CreateWinHardLink(launcher_path, latest_version_path) ||
         base::CopyFile(latest_version_path, launcher_path);
}

// A callback invoked by |work_item| that deletes the file at |launcher_path|.
void DeleteHardLinkOrCopyCallback(const base::FilePath& launcher_path,
                                  const CallbackWorkItem& work_item) {
  base::DeleteFile(launcher_path);
}

// Replaces |launcher_path| with the one at |latest_version_path|. This is done
// by atomically renaming |launcher_path| to |old_path| and creating a hardlink
// to or copy of |latest_version_path| at |launcher_path|. Makes a best-effort
// attempt to delete |old_path|. Aside from the best-effort deletion, all
// changes are rolled back if any step fails.
void ReplaceLauncherWithLatestVersion(const base::FilePath& launcher_path,
                                      const base::FilePath& latest_version_path,
                                      const base::FilePath& old_path) {
  if (!base::PathExists(latest_version_path))
    return;

  // Create a temporary backup directory for use while moving in-use files.
  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDirUnderPath(launcher_path.DirName()))
    return;

  // Move |launcher_path| to |old_path|.
  std::unique_ptr<WorkItemList> change_list(WorkItem::CreateWorkItemList());
  change_list->AddMoveTreeWorkItem(launcher_path, old_path, temp_dir.GetPath(),
                                   WorkItem::ALWAYS_MOVE);

  // Create a hardlink or copy of |latest_version_path| at |launcher_path|.
  change_list->AddCallbackWorkItem(
      base::BindOnce(&CreateHardLinkOrCopyCallback, launcher_path,
                     latest_version_path),
      base::BindOnce(&DeleteHardLinkOrCopyCallback, launcher_path));

  // Make a best-effort, no-rollback attempt to delete |old_path|; deletion
  // will fail when |old_path| is still in use.
  std::unique_ptr<DeleteTreeWorkItem> delete_old_version_work_item(
      WorkItem::CreateDeleteTreeWorkItem(old_path, temp_dir.GetPath()));
  delete_old_version_work_item->set_best_effort(true);
  delete_old_version_work_item->set_rollback_enabled(false);
  change_list->AddWorkItem(delete_old_version_work_item.release());

  if (!change_list->Do())
    change_list->Rollback();
}

// Deletes |old_path| and any variations on it (e.g., |old_path| (1), |old_path|
// (2), etc.) if they exist. |old_path| is renamed to a unique name before
// deletion to ensure its filename is available for use immediately (as
// Windows' file deletion returns success before the deleted file's name
// actually becomes available).
void CleanUpOldLauncherVersions(const base::FilePath& old_path) {
  // If |old_path| exists, rename it to |unique_path| and delete it.
  const base::FilePath unique_path = base::GetUniquePath(old_path);
  if (!unique_path.empty() && unique_path != old_path) {
    base::Move(old_path, unique_path);
    base::DeleteFile(unique_path);
  }

  // Delete any old versions of |unique_path| that may exist from failed delete
  // attempts (e.g., if antivirus software prevented deletion of |unique_path|).
  base::FileEnumerator files(
      old_path.DirName(), /*recursive=*/false, base::FileEnumerator::FILES,
      old_path.BaseName()
          .InsertBeforeExtension(FILE_PATH_LITERAL(" (*)"))
          .value());
  for (base::FilePath file = files.Next(); !file.empty(); file = files.Next()) {
    base::DeleteFile(file);
  }
}

}  // namespace

namespace web_app {

void UpdatePwaLaunchers(std::vector<base::FilePath> launcher_paths) {
  const base::FilePath latest_version_path = GetChromePwaLauncherPath();

  for (const auto& path : launcher_paths) {
    DCHECK(!path.empty());

    const base::FilePath old_path =
        path.InsertBeforeExtension(kOldLauncherSuffix);
    CleanUpOldLauncherVersions(old_path);

    // Make a hardlink or copy of |latest_version_path|, and replace the current
    // launcher with it.
    if (base::PathExists(path))
      ReplaceLauncherWithLatestVersion(path, latest_version_path, old_path);
  }
}

}  // namespace web_app
