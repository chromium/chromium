// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_file_system_delegate.h"

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/files/file_util.h"
#include "base/sequence_checker.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/chromeos/fileapi/file_change_service_factory.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace ash {

// HoldingSpaceFileSystemDelegate::FileSystemWatcher ---------------------------

class HoldingSpaceFileSystemDelegate::FileSystemWatcher {
 public:
  explicit FileSystemWatcher(base::FilePathWatcher::Callback callback)
      : callback_(callback) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }

  FileSystemWatcher(const FileSystemWatcher&) = delete;
  FileSystemWatcher& operator=(const FileSystemWatcher&) = delete;
  ~FileSystemWatcher() { DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_); }

  void AddWatchForParent(const base::FilePath& file_path) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Observe the file path parent directory for changes - this reduces the
    // number of inotify requests, and works well enough for detecting file
    // deletion.
    const base::FilePath path_to_watch = file_path.DirName();

    if (!base::Contains(watchers_, path_to_watch)) {
      watchers_[path_to_watch] = std::make_unique<base::FilePathWatcher>();
      watchers_[path_to_watch]->Watch(
          path_to_watch, base::FilePathWatcher::Type::kNonRecursive,
          base::Bind(&FileSystemWatcher::OnFilePathChanged,
                     weak_factory_.GetWeakPtr()));
    }

    // If the target path got deleted while request to add a watcher was in
    // flight, notify observers of path change immediately.
    if (!base::PathExists(file_path))
      OnFilePathChanged(path_to_watch, /*error=*/false);
  }

  void RemoveWatch(const base::FilePath& file_path) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    watchers_.erase(file_path);
  }

  base::WeakPtr<FileSystemWatcher> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  void OnFilePathChanged(const base::FilePath& file_path, bool error) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(callback_, file_path, error));
  }

  SEQUENCE_CHECKER(sequence_checker_);
  base::FilePathWatcher::Callback callback_;
  std::map<base::FilePath, std::unique_ptr<base::FilePathWatcher>> watchers_;
  base::WeakPtrFactory<FileSystemWatcher> weak_factory_{this};
};

// HoldingSpaceFileSystemDelegate ----------------------------------------------

HoldingSpaceFileSystemDelegate::HoldingSpaceFileSystemDelegate(
    Profile* profile,
    HoldingSpaceModel* model)
    : HoldingSpaceKeyedServiceDelegate(profile, model),
      file_system_watcher_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT})) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

HoldingSpaceFileSystemDelegate::~HoldingSpaceFileSystemDelegate() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  weak_factory_.InvalidateWeakPtrs();
  file_system_watcher_runner_->DeleteSoon(FROM_HERE,
                                          file_system_watcher_.release());
}

void HoldingSpaceFileSystemDelegate::Init() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  file_system_watcher_ = std::make_unique<FileSystemWatcher>(
      base::Bind(&HoldingSpaceFileSystemDelegate::OnFilePathChanged,
                 weak_factory_.GetWeakPtr()));

  file_change_service_observer_.Observe(
      chromeos::FileChangeServiceFactory::GetInstance()->GetService(profile()));

  if (file_manager::VolumeManager::Get(profile())) {
    volume_manager_observer_.Observe(
        file_manager::VolumeManager::Get(profile()));
  }

  // Schedule a task to clean up items that belong to volumes that haven't been
  // mounted in a reasonable amount of time.
  // The primary goal of handling the delayed volume mount is to support volumes
  // that are mounted asynchronously during the startup.
  clear_non_finalized_items_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMinutes(1),
      base::BindOnce(&HoldingSpaceFileSystemDelegate::ClearNonFinalizedItems,
                     base::Unretained(this)));
}

void HoldingSpaceFileSystemDelegate::OnHoldingSpaceItemsAdded(
    const std::vector<const HoldingSpaceItem*>& items) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  for (const HoldingSpaceItem* item : items) {
    if (item->IsFinalized()) {
      // Watch the directory containing `item`'s backing file. If the directory
      // is already being watched, this will no-op.
      AddWatchForParent(item->file_path());
      continue;
    }

    // If the item has not yet been finalized, check whether it's path can be
    // resolved to a file system URL - failure to do so may indicate that the
    // volume to which it path belongs is not mounted there. If the file system
    // URL cannot be resolved, leave the item in partially initialized state.
    // Validity will be checked when the associated mount point is mounted.
    // NOTE: Items will not be kept in partially initialized state indefinitely
    // - see `clear_non_finalized_items_timer_`.
    // NOTE: This does not work well for removable devices, as all removable
    // devices have the same top level "external" mount point (media/removable),
    // so a removable device path will be successfully resolved even if the
    // device is not currently mounted. The logic will have to be updated if
    // support for restoring items across removable device mounts becomes a
    // requirement.
    const GURL file_system_url =
        holding_space_util::ResolveFileSystemUrl(profile(), item->file_path());
    if (file_system_url.is_empty())
      continue;

    holding_space_util::ValidityRequirement requirements;
    if (item->type() != HoldingSpaceItem::Type::kPinnedFile)
      requirements.must_be_newer_than = kMaxFileAge;

    ScheduleFilePathValidityCheck({item->file_path(), requirements});
  }
}

void HoldingSpaceFileSystemDelegate::OnHoldingSpaceItemsRemoved(
    const std::vector<const HoldingSpaceItem*>& items) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  for (const HoldingSpaceItem* item : items)
    MaybeRemoveWatch(item->file_path().DirName());
}

void HoldingSpaceFileSystemDelegate::OnHoldingSpaceItemUpdated(
    const HoldingSpaceItem* item) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  AddWatchForParent(item->file_path());
}

void HoldingSpaceFileSystemDelegate::OnHoldingSpaceItemFinalized(
    const HoldingSpaceItem* item) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  AddWatchForParent(item->file_path());
}

void HoldingSpaceFileSystemDelegate::OnVolumeMounted(
    chromeos::MountError error_code,
    const file_manager::Volume& volume) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  holding_space_util::FilePathsWithValidityRequirements
      file_paths_with_requirements;
  // Check validity of partially initialized items under the volume's mount
  // path.
  for (auto& item : model()->items()) {
    if (item->IsFinalized())
      continue;
    if (!volume.mount_path().IsParent(item->file_path()))
      continue;

    holding_space_util::ValidityRequirement requirements;
    if (item->type() != HoldingSpaceItem::Type::kPinnedFile)
      requirements.must_be_newer_than = kMaxFileAge;
    ScheduleFilePathValidityCheck({item->file_path(), requirements});
  }
}

void HoldingSpaceFileSystemDelegate::OnVolumeUnmounted(
    chromeos::MountError error_code,
    const file_manager::Volume& volume) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Schedule task to remove items under the unmounted file path from the model.
  // During suspend, some volumes get unmounted - for example, drive FS. The
  // file system delegate gets shutdown to avoid removing items from unmounted
  // volumes, but depending on the order in which observers are added to power
  // manager dbus client, the file system delegate may get shutdown after
  // unmounting a volume. To avoid observer ordering issues, schedule
  // asynchronous task to remove unmounted items from the model.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&HoldingSpaceFileSystemDelegate::RemoveItemsParentedByPath,
                     weak_factory_.GetWeakPtr(), volume.mount_path()));
}

void HoldingSpaceFileSystemDelegate::OnFileMoved(
    const storage::FileSystemURL& src,
    const storage::FileSystemURL& dst) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Update backing files for any holding space `item` associated with `src`.
  bool did_update_backing_file = false;
  for (auto& item : model()->items()) {
    base::FilePath new_file_path;
    if (src.path() == item->file_path()) {
      // The file backing `item` has moved to `dst`.
      new_file_path = dst.path();
    } else if (src.path().IsParent(item->file_path())) {
      // A parent directory of the file backing `item` has moved to `dst` so
      // the file backing `item` needs to be re-parented.
      new_file_path = dst.path();
      if (!src.path().AppendRelativePath(item->file_path(), &new_file_path))
        NOTREACHED();
    }
    if (!new_file_path.empty()) {
      model()->UpdateBackingFileForItem(
          item->id(), new_file_path,
          holding_space_util::ResolveFileSystemUrl(profile(), new_file_path));
      did_update_backing_file = true;
    }
  }
  // If a backing file update occurred, it's possible that there are no longer
  // any holding space items associated with `src`. When that is the case, `src`
  // no longer needs to be watched.
  if (did_update_backing_file)
    MaybeRemoveWatch(src.path());
}

void HoldingSpaceFileSystemDelegate::OnFilePathChanged(
    const base::FilePath& file_path,
    bool error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!error);

  // The `file_path` that changed is a directory containing backing files for
  // one or more holding space items. Changes to this directory may indicate
  // that some, all, or none of these backing files have been removed. We need
  // to verify the existence of these backing files and remove any holding space
  // items that no longer exist.
  for (const auto& item : model()->items()) {
    if (file_path.IsParent(item->file_path()))
      ScheduleFilePathValidityCheck({item->file_path(), /*requirements=*/{}});
  }
}

void HoldingSpaceFileSystemDelegate::ScheduleFilePathValidityCheck(
    holding_space_util::FilePathWithValidityRequirement requirement) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  pending_file_path_validity_checks_.push_back(std::move(requirement));
  if (file_path_validity_checks_scheduled_)
    return;

  file_path_validity_checks_scheduled_ = true;

  // Schedule file validity check for pending items. The check is scheduled
  // asynchronously so path checks added in quick succession are handled in a
  // single batch.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &HoldingSpaceFileSystemDelegate::RunPendingFilePathValidityChecks,
          weak_factory_.GetWeakPtr()));
}

void HoldingSpaceFileSystemDelegate::RunPendingFilePathValidityChecks() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  holding_space_util::FilePathsWithValidityRequirements requirements;
  requirements.swap(pending_file_path_validity_checks_);

  file_path_validity_checks_scheduled_ = false;
  holding_space_util::PartitionFilePathsByValidity(
      profile(), std::move(requirements),
      base::BindOnce(
          &HoldingSpaceFileSystemDelegate::OnFilePathValidityChecksComplete,
          weak_factory_.GetWeakPtr()));
}

void HoldingSpaceFileSystemDelegate::OnFilePathValidityChecksComplete(
    std::vector<base::FilePath> valid_paths,
    std::vector<base::FilePath> invalid_paths) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Remove items with invalid paths.
  // When `file_path` is removed, we need to remove any associated items.
  model()->RemoveIf(base::BindRepeating(
      [](const std::vector<base::FilePath>* invalid_paths,
         const HoldingSpaceItem* item) {
        return base::Contains(*invalid_paths, item->file_path());
      },
      &invalid_paths));

  std::vector<const HoldingSpaceItem*> items_to_finalize;
  for (auto& item : model()->items()) {
    if (!item->IsFinalized() &&
        base::Contains(valid_paths, item->file_path())) {
      items_to_finalize.push_back(item.get());
    }
  }

  for (auto* item : items_to_finalize) {
    model()->FinalizeOrRemoveItem(
        item->id(),
        holding_space_util::ResolveFileSystemUrl(profile(), item->file_path()));
  }
}

void HoldingSpaceFileSystemDelegate::AddWatchForParent(
    const base::FilePath& file_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  file_system_watcher_runner_->PostTask(
      FROM_HERE, base::BindOnce(&FileSystemWatcher::AddWatchForParent,
                                file_system_watcher_->GetWeakPtr(), file_path));
}

void HoldingSpaceFileSystemDelegate::MaybeRemoveWatch(
    const base::FilePath& file_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // The watch for `file_path` should only be removed if no holding space items
  // exist in the model which are backed by files it directly parents.
  const bool remove_watch = std::none_of(
      model()->items().begin(), model()->items().end(),
      [&file_path](const auto& item) {
        return item->IsFinalized() && item->file_path().DirName() == file_path;
      });

  if (!remove_watch)
    return;

  file_system_watcher_runner_->PostTask(
      FROM_HERE, base::BindOnce(&FileSystemWatcher::RemoveWatch,
                                file_system_watcher_->GetWeakPtr(), file_path));
}

void HoldingSpaceFileSystemDelegate::RemoveItemsParentedByPath(
    const base::FilePath& parent_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  model()->RemoveIf(base::BindRepeating(
      [](const base::FilePath& parent_path, const HoldingSpaceItem* item) {
        return parent_path.IsParent(item->file_path());
      },
      parent_path));
}

void HoldingSpaceFileSystemDelegate::ClearNonFinalizedItems() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  model()->RemoveIf(base::BindRepeating(
      [](const HoldingSpaceItem* item) { return !item->IsFinalized(); }));
}

}  // namespace ash
