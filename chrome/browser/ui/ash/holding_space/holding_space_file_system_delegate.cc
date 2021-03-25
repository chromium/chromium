// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_file_system_delegate.h"

#include <set>
#include <string>

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
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/fileapi/file_change_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/session/arc_bridge_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace ash {

namespace {

arc::ConnectionHolder<arc::mojom::FileSystemInstance,
                      arc::mojom::FileSystemHost>*
GetArcFileSystem() {
  if (!arc::ArcServiceManager::Get())
    return nullptr;
  return arc::ArcServiceManager::Get()->arc_bridge_service()->file_system();
}

// Returns whether
// *   the ARC is enabled for `profile`, and
// *   connection to ARC file system service is *not* established at the time.
bool IsArcFileSystemDisconnected(Profile* profile) {
  return arc::IsArcPlayStoreEnabledForProfile(profile) && GetArcFileSystem() &&
         !GetArcFileSystem()->IsConnected();
}

// Returns whether the item is backed by an Android file. Can be used with
// non-finalized items.
bool ItemBackedByAndroidFile(const HoldingSpaceItem* item) {
  return file_manager::util::GetAndroidFilesPath().IsParent(item->file_path());
}

}  // namespace

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

    auto it = watchers_.lower_bound(path_to_watch);
    if (it == watchers_.end() || it->first != path_to_watch) {
      it = watchers_.emplace_hint(it, std::piecewise_construct,
                                  std::forward_as_tuple(path_to_watch),
                                  std::forward_as_tuple());
      it->second.Watch(
          path_to_watch, base::FilePathWatcher::Type::kNonRecursive,
          base::BindRepeating(&FileSystemWatcher::OnFilePathChanged,
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
  std::map<base::FilePath, base::FilePathWatcher> watchers_;
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

void HoldingSpaceFileSystemDelegate::OnConnectionReady() {
  // Schedule validity checks for android items.
  for (auto& item : model()->items()) {
    if (!ItemBackedByAndroidFile(item.get()))
      continue;

    holding_space_util::ValidityRequirement requirements;
    if (item->type() != HoldingSpaceItem::Type::kPinnedFile)
      requirements.must_be_newer_than = kMaxFileAge;
    ScheduleFilePathValidityCheck({item->file_path(), requirements});
  }
}

void HoldingSpaceFileSystemDelegate::Init() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  file_system_watcher_ = std::make_unique<FileSystemWatcher>(
      base::BindRepeating(&HoldingSpaceFileSystemDelegate::OnFilePathChanged,
                          weak_factory_.GetWeakPtr()));

  file_change_service_observer_.Observe(
      chromeos::FileChangeServiceFactory::GetInstance()->GetService(profile()));

  if (file_manager::VolumeManager::Get(profile())) {
    volume_manager_observer_.Observe(
        file_manager::VolumeManager::Get(profile()));
  }

  if (GetArcFileSystem())
    arc_file_system_observer_.Observe(GetArcFileSystem());

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

  const bool arc_file_system_disconnected =
      IsArcFileSystemDisconnected(profile());
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

    // Defer validity checks (and finalization) for android files if the
    // ARC file system connection is not yet ready.
    if (arc_file_system_disconnected && ItemBackedByAndroidFile(item))
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

void HoldingSpaceFileSystemDelegate::OnFileModified(
    const storage::FileSystemURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  model()->InvalidateItemImageIf(base::BindRepeating(
      [](const base::FilePath& path, const HoldingSpaceItem* item) {
        return item->file_path() == path;
      },
      url.path()));
}

void HoldingSpaceFileSystemDelegate::OnFileMoved(
    const storage::FileSystemURL& src,
    const storage::FileSystemURL& dst) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Collect items that should be moved to a new path. This includes:
  // *   Items whose path matches the source path.
  // *   Items parented by the source path.
  // Maps item ID to the item's new file path.
  std::vector<std::pair<std::string, base::FilePath>> items_to_move;
  for (auto& item : model()->items()) {
    if (src.path() == item->file_path()) {
      items_to_move.push_back(std::make_pair(item->id(), dst.path()));
      continue;
    }

    if (src.path().IsParent(item->file_path())) {
      base::FilePath target_path = dst.path();
      if (!src.path().AppendRelativePath(item->file_path(), &target_path)) {
        NOTREACHED();
        continue;
      }
      items_to_move.push_back(std::make_pair(item->id(), target_path));
    }
  }

  // Handle existing holding space items under the target file path.
  // Moving an existing item to a new path may create conflict within the
  // holding space if an item with the target path already exists within the
  // holding space. If this is the case, assume that the original item was
  // overwritten, and remove it from the holding space.

  // NOTE: Don't remove items at destination if no holding space items have to
  // be updated due to the file move. The reason for this is to:
  // *   Support use case where apps change files by moving temp file with
  //     modifications to the file path.
  // *   Handle duplicate file path move notifications.
  // Instead, update the items as if the target path was modified.
  if (items_to_move.empty()) {
    OnFileModified(dst);
    return;
  }

  // Resolve conflicts with existing items that arise from the move.
  std::set<std::string> item_ids_to_remove;
  for (auto& item : model()->items()) {
    if (dst.path() == item->file_path() ||
        dst.path().IsParent(item->file_path())) {
      item_ids_to_remove.insert(item->id());
    }
  }
  model()->RemoveItems(item_ids_to_remove);

  // Finally, update the files that have been moved.
  for (const auto& to_move : items_to_move) {
    if (item_ids_to_remove.count(to_move.first))
      continue;

    model()->UpdateBackingFileForItem(
        to_move.first, to_move.second,
        holding_space_util::ResolveFileSystemUrl(profile(), to_move.second));
  }

  // If a backing file update occurred, it's possible that there are no longer
  // any holding space items associated with `src`. When that is the case, `src`
  // no longer needs to be watched.
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

  const bool arc_file_system_disconnected =
      IsArcFileSystemDisconnected(profile());

  // Remove items with invalid paths.
  model()->RemoveIf(base::BindRepeating(
      [](bool arc_file_system_disconnected,
         const std::vector<base::FilePath>* invalid_paths,
         const HoldingSpaceItem* item) {
        // Avoid removing Android files if connection to ARC file system has
        // been lost (e.g. Android container might have crashed). Validity
        // checks will be re-run once the file system gets connected.
        if (arc_file_system_disconnected && ItemBackedByAndroidFile(item))
          return false;

        return base::Contains(*invalid_paths, item->file_path());
      },
      arc_file_system_disconnected, &invalid_paths));

  std::vector<const HoldingSpaceItem*> items_to_finalize;
  for (auto& item : model()->items()) {
    // Defer finalization of items backed by android files if the connection to
    // ARC file system service has been lost.
    if (arc_file_system_disconnected && ItemBackedByAndroidFile(item.get()))
      continue;

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
      [](Profile* profile, const HoldingSpaceItem* item) {
        if (item->IsFinalized())
          return false;

        // Do not remove items whose path can be resolved to a file system URL.
        // In this case, the associated mount point has been mounted, but the
        // finalization may have been delayed - for example due to issues/delays
        // with initializing ARC.
        const GURL url = holding_space_util::ResolveFileSystemUrl(
            profile, item->file_path());
        return url.is_empty();
      },
      profile()));
}

}  // namespace ash
