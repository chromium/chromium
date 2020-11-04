// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_file_system_delegate.h"

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/sequence_checker.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
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

  void AddWatch(const base::FilePath& file_path) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (base::Contains(watchers_, file_path))
      return;
    watchers_[file_path] = std::make_unique<base::FilePathWatcher>();
    watchers_[file_path]->Watch(
        file_path, /*recursive=*/false,
        base::Bind(&FileSystemWatcher::OnFilePathChanged,
                   weak_factory_.GetWeakPtr()));
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

  if (file_manager::VolumeManager::Get(profile()))
    volume_manager_observer_.Add(file_manager::VolumeManager::Get(profile()));

  // Schedule a task to clean up items that belong to volumes that haven't been
  // mounted in a reasonable amount of time.
  // The primary goal of handling the delayed volume mount is to support volumes
  // that are mounted asynchronously during the startup.
  clear_non_finalized_items_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMinutes(1),
      base::BindOnce(&HoldingSpaceFileSystemDelegate::ClearNonFinalizedItems,
                     base::Unretained(this)));
}

void HoldingSpaceFileSystemDelegate::Shutdown() {
  volume_manager_observer_.RemoveAll();
}

void HoldingSpaceFileSystemDelegate::OnHoldingSpaceItemAdded(
    const HoldingSpaceItem* item) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (item->IsFinalized()) {
    // Watch the directory containing `items`'s backing file. If the directory
    // is already being watched, this will no-op.
    AddWatch(item->file_path().DirName());
    return;
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
    return;

  holding_space_util::ValidityRequirement requirements;
  if (item->type() != HoldingSpaceItem::Type::kPinnedFile)
    requirements.must_be_newer_than = kMaxFileAge;

  ScheduleFilePathValidityCheck({item->file_path(), requirements});
}

void HoldingSpaceFileSystemDelegate::OnHoldingSpaceItemRemoved(
    const HoldingSpaceItem* item) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Since we were watching the directory containing `item`'s backing file and
  // not the backing file itself, we only need to remove the associated watch if
  // there are no other holding space items backed by the same directory.
  const bool remove_watch = std::none_of(
      model()->items().begin(), model()->items().end(),
      [removed_item = item](const auto& item) {
        return item->IsFinalized() && item->file_path().DirName() ==
                                          removed_item->file_path().DirName();
      });

  if (remove_watch)
    RemoveWatch(item->file_path().DirName());
}

void HoldingSpaceFileSystemDelegate::OnHoldingSpaceItemFinalized(
    const HoldingSpaceItem* item) {
  AddWatch(item->file_path().DirName());
}

void HoldingSpaceFileSystemDelegate::OnVolumeMounted(
    chromeos::MountError error_code,
    const file_manager::Volume& volume) {
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
  model()->RemoveIf(base::BindRepeating(
      [](const base::FilePath& volume_path, const HoldingSpaceItem* item) {
        return volume_path.IsParent(item->file_path());
      },
      volume.mount_path()));
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

void HoldingSpaceFileSystemDelegate::AddWatch(const base::FilePath& file_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  file_system_watcher_runner_->PostTask(
      FROM_HERE, base::BindOnce(&FileSystemWatcher::AddWatch,
                                file_system_watcher_->GetWeakPtr(), file_path));
}

void HoldingSpaceFileSystemDelegate::RemoveWatch(
    const base::FilePath& file_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  file_system_watcher_runner_->PostTask(
      FROM_HERE, base::BindOnce(&FileSystemWatcher::RemoveWatch,
                                file_system_watcher_->GetWeakPtr(), file_path));
}

void HoldingSpaceFileSystemDelegate::ClearNonFinalizedItems() {
  model()->RemoveIf(base::BindRepeating(
      [](const HoldingSpaceItem* item) { return !item->IsFinalized(); }));
}

}  // namespace ash
