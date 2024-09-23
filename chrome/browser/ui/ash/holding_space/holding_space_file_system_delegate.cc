// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_file_system_delegate.h"

#include <set>
#include <string>

#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_file.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/files/file_util.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/trash_common_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/fileapi/file_change_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace ash {

namespace {

// Returns the absolute file path for the specified `drive_path`.
// NOTE: This method requires that the `DriveIntegrationService` be mounted.
base::FilePath ConvertDrivePathToAbsoluteFilePath(
    Profile* profile,
    const base::FilePath& drive_path) {
  const auto* drive_integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);
  if (drive_integration_service) {
    base::FilePath absolute_file_path =
        drive_integration_service->GetMountPointPath();
    if (base::FilePath("/").AppendRelativePath(drive_path, &absolute_file_path))
      return absolute_file_path;
  }
  NOTREACHED_IN_MIGRATION();
  return base::FilePath();
}

// Returns a mojo connection to the ARC file system.
arc::ConnectionHolder<arc::mojom::FileSystemInstance,
                      arc::mojom::FileSystemHost>*
GetArcFileSystem() {
  if (!arc::ArcServiceManager::Get())
    return nullptr;
  return arc::ArcServiceManager::Get()->arc_bridge_service()->file_system();
}

// Returns whether:
// *   the ARC is enabled for `profile`, and
// *   connection to ARC file system service is *not* established at the time.
bool IsArcFileSystemDisconnected(Profile* profile) {
  return arc::IsArcPlayStoreEnabledForProfile(profile) && GetArcFileSystem() &&
         !GetArcFileSystem()->IsConnected();
}

// Returns whether the item is backed by an Android file. Can be used with
// non-initialized items.
bool ItemBackedByAndroidFile(const HoldingSpaceItem* item) {
  return file_manager::util::GetAndroidFilesPath().IsParent(
      item->file().file_path);
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
    // In tests `OnFilePathChanged()` events are sometimes propagated before
    // `OnFileMoved()` events. Delay propagation of `OnFilePathChanged()`
    // events to give `OnFileMoved()` events time to propagate.
    content::GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE, base::BindOnce(callback_, file_path, error),
        base::Milliseconds(1));
  }

  SEQUENCE_CHECKER(sequence_checker_);
  base::FilePathWatcher::Callback callback_;
  std::map<base::FilePath, base::FilePathWatcher> watchers_;
  base::WeakPtrFactory<FileSystemWatcher> weak_factory_{this};
};

// HoldingSpaceFileSystemDelegate ----------------------------------------------

HoldingSpaceFileSystemDelegate::HoldingSpaceFileSystemDelegate(
    HoldingSpaceKeyedService* service,
    HoldingSpaceModel* model)
    : HoldingSpaceKeyedServiceDelegate(service, model),
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
    if (item->type() != HoldingSpaceItem::Type::kPinnedFile) {
      requirements.must_be_newer_than = kMaxFileAge;
    }
    ScheduleFilePathValidityCheck({item->file().file_path, requirements});
  }
}

void HoldingSpaceFileSystemDelegate::OnFilesChanged(
    const std::vector<drivefs::mojom::FileChange>& changes) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // When a file is moved, a `kDelete` change will be followed by a `kCreate`
  // change with the same `stable_id` in the same set of `changes`. In some
  // versions of the `DriveFsHost`, `stable_id` is not supported and will always
  // be `0` for backwards compatibility.
  std::map<int64_t, base::FilePath> deleted_paths_by_stable_id;
  std::set<base::FilePath> deleted_paths;

  for (const auto& change : changes) {
    // Holding space requires absolute file paths.
    const base::FilePath absolute_file_path =
        ConvertDrivePathToAbsoluteFilePath(profile(), change.path);
    if (absolute_file_path.empty())
      continue;

    switch (change.type) {
      case drivefs::mojom::FileChange::Type::kCreate: {
        if (change.stable_id) {
          // If the `kCreate` change has an associated `stable_id`, it can
          // potentially be paired with a preceding `kDelete` change. In such
          // cases, the change sequence actually constitutes a move rather than
          // a delete of the underlying file.
          auto it = deleted_paths_by_stable_id.find(change.stable_id);
          if (it != deleted_paths_by_stable_id.end()) {
            OnFilePathMoved(/*src=*/it->second, /*dst=*/absolute_file_path);
            deleted_paths_by_stable_id.erase(it);
          }
        }
        break;
      }
      case drivefs::mojom::FileChange::Type::kDelete: {
        // If the change has a `stable_id` it can potentially constitute part of
        // a move rather than a delete of the underlying file. Whether the
        // change is move or a delete will not be known until a `kCreate` change
        // follows (or is confirmed *not* to follow). When `stable_id` is absent
        // it is not possible to detect a move.
        if (change.stable_id)
          deleted_paths_by_stable_id[change.stable_id] = absolute_file_path;
        else
          deleted_paths.insert(absolute_file_path);
        break;
      }
      case drivefs::mojom::FileChange::Type::kModify: {
        OnFilePathModified(absolute_file_path);
        break;
      }
    }
  }

  // At this point, all `kDelete` changes in `deleted_paths_by_stable_id` are
  // confirmed to be deletions, having already stripped out all changes that
  // were actually constituting moves.
  for (const auto& deleted_path_by_stable_id : deleted_paths_by_stable_id)
    deleted_paths.insert(deleted_path_by_stable_id.second);

  // Remove any holding space items backed by deleted file paths.
  model()->RemoveIf(base::BindRepeating(
      [](const std::set<base::FilePath>& deleted_paths,
         const HoldingSpaceItem* item) {
        return base::ranges::any_of(
            deleted_paths, [&](const base::FilePath& deleted_path) {
              return item->file().file_path == deleted_path ||
                     deleted_path.IsParent(item->file().file_path);
            });
      },
      std::cref(deleted_paths)));
}

void HoldingSpaceFileSystemDelegate::Init() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  file_system_watcher_ = std::make_unique<FileSystemWatcher>(
      base::BindRepeating(&HoldingSpaceFileSystemDelegate::OnFilePathChanged,
                          weak_factory_.GetWeakPtr()));

  // Arc file system.
  auto* const arc_file_system = GetArcFileSystem();
  if (arc_file_system)
    arc_file_system_observer_.Observe(arc_file_system);

  // Drive file system.
  if (drive::DriveIntegrationService* const service =
          drive::DriveIntegrationServiceFactory::FindForProfile(profile())) {
    Observe(service->GetDriveFsHost());
  }

  // Local file system.
  file_change_service_observer_.Observe(
      FileChangeServiceFactory::GetInstance()->GetService(profile()));

  // Volume manager.
  auto* const volume_manager = file_manager::VolumeManager::Get(profile());
  if (volume_manager)
    volume_manager_observer_.Observe(volume_manager);

  // Schedule a task to clean up items that belong to volumes that haven't been
  // mounted in a reasonable amount of time. The primary goal of handling the
  // delayed volume mount is to support volumes that are mounted asynchronously
  // during the startup.
  clear_non_initialized_items_timer_.Start(
      FROM_HERE, base::Minutes(1),
      base::BindOnce(&HoldingSpaceFileSystemDelegate::ClearNonInitializedItems,
                     base::Unretained(this)));
}

void HoldingSpaceFileSystemDelegate::OnHoldingSpaceItemsAdded(
    const std::vector<const HoldingSpaceItem*>& items) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const bool arc_file_system_disconnected =
      IsArcFileSystemDisconnected(profile());
  for (const HoldingSpaceItem* item : items) {
    if (item->IsInitialized() && item->progress().IsComplete()) {
      // Watch the directory containing `item`'s backing file. If the directory
      // is already being watched, this will no-op. Note that it is not
      // necessary to register a watch if the `item` is in-progress since
      // in-progress items are not subject to validity checks.
      AddWatchForParent(item->file().file_path);
      continue;
    }

    // In-progress items are not subject to validity checks.
    if (!item->progress().IsComplete())
      continue;

    // If the item has not yet been initialized, check whether it's path can be
    // resolved to a file system URL - failure to do so may indicate that the
    // volume to which it path belongs is not mounted there. If the file system
    // URL cannot be resolved, leave the item in partially initialized state.
    // Validity will be checked when the associated mount point is mounted.
    // NOTE: Items will not be kept in partially initialized state indefinitely
    // - see `clear_non_initialized_items_timer_`.
    // NOTE: This does not work well for removable devices, as all removable
    // devices have the same top level "external" mount point (media/removable),
    // so a removable device path will be successfully resolved even if the
    // device is not currently mounted. The logic will have to be updated if
    // support for restoring items across removable device mounts becomes a
    // requirement.
    const GURL file_system_url = holding_space_util::ResolveFileSystemUrl(
        profile(), item->file().file_path);
    if (file_system_url.is_empty())
      continue;

    // Defer validity checks (and initialization) for android files if the ARC
    // file system connection is not yet ready.
    if (arc_file_system_disconnected && ItemBackedByAndroidFile(item))
      continue;

    holding_space_util::ValidityRequirement requirements;
    if (item->type() != HoldingSpaceItem::Type::kPinnedFile) {
      requirements.must_be_newer_than = kMaxFileAge;
    }
    ScheduleFilePathValidityCheck({item->file().file_path, requirements});
  }
}

void HoldingSpaceFileSystemDelegate::OnHoldingSpaceItemsRemoved(
    const std::vector<const HoldingSpaceItem*>& items) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  for (const HoldingSpaceItem* item : items)
    MaybeRemoveWatch(item->file().file_path.DirName());
}

void HoldingSpaceFileSystemDelegate::OnHoldingSpaceItemUpdated(
    const HoldingSpaceItem* item,
    const HoldingSpaceItemUpdatedFields& updated_fields) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // In-progress items are not subject to validity checks.
  if (item->progress().IsComplete())
    AddWatchForParent(item->file().file_path);
}

void HoldingSpaceFileSystemDelegate::OnHoldingSpaceItemInitialized(
    const HoldingSpaceItem* item) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  AddWatchForParent(item->file().file_path);
}

void HoldingSpaceFileSystemDelegate::OnVolumeMounted(
    MountError error_code,
    const file_manager::Volume& volume) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  holding_space_util::FilePathsWithValidityRequirements
      file_paths_with_requirements;
  // Check validity of partially initialized items under the volume's mount
  // path.
  for (auto& item : model()->items()) {
    if (item->IsInitialized())
      continue;
    if (!volume.mount_path().IsParent(item->file().file_path)) {
      continue;
    }
    holding_space_util::ValidityRequirement requirements;
    if (item->type() != HoldingSpaceItem::Type::kPinnedFile) {
      requirements.must_be_newer_than = kMaxFileAge;
    }
    ScheduleFilePathValidityCheck({item->file().file_path, requirements});
  }
}

void HoldingSpaceFileSystemDelegate::OnVolumeUnmounted(
    MountError error_code,
    const file_manager::Volume& volume) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Drive FS may restart from time to time, so only remove items if drive is
  // disabled.
  if (volume.type() == file_manager::VOLUME_TYPE_GOOGLE_DRIVE) {
    const auto* drive_integration_service =
        drive::DriveIntegrationServiceFactory::FindForProfile(profile());
    if (drive_integration_service && drive_integration_service->is_enabled()) {
      return;
    }
  }
  // Schedule task to remove items under the unmounted file path from the model.
  // During suspend, some volumes get unmounted - for example, drive FS. The
  // file system delegate gets shutdown to avoid removing items from unmounted
  // volumes, but depending on the order in which observers are added to power
  // manager dbus client, the file system delegate may get shutdown after
  // unmounting a volume. To avoid observer ordering issues, schedule
  // asynchronous task to remove unmounted items from the model.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&HoldingSpaceFileSystemDelegate::RemoveItemsParentedByPath,
                     weak_factory_.GetWeakPtr(), volume.mount_path()));
}

void HoldingSpaceFileSystemDelegate::OnFileCreatedFromShowSaveFilePicker(
    const GURL& file_picker_binding_context,
    const storage::FileSystemURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  holding_space_metrics::RecordFileCreatedFromShowSaveFilePicker(
      file_picker_binding_context, url.path());

  if (file_picker_binding_context.DomainIs("photoshop.adobe.com")) {
    service()->AddItemOfType(HoldingSpaceItem::Type::kPhotoshopWeb, url.path());
  }
}

void HoldingSpaceFileSystemDelegate::OnFileModified(
    const storage::FileSystemURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  OnFilePathModified(url.path());
}

void HoldingSpaceFileSystemDelegate::OnFileMoved(
    const storage::FileSystemURL& src,
    const storage::FileSystemURL& dst) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  OnFilePathMoved(src.path(), dst.path());
}

void HoldingSpaceFileSystemDelegate::OnFilePathChanged(
    const base::FilePath& file_path,
    bool error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!error);

  // The `file_path` that changed is a directory containing backing files for
  // one or more holding space items. Changes to this directory may indicate
  // that some, all, or none of these backing files have been removed. Verify
  // the existence of these backing files to trigger removal of any holding
  // space items that no longer exist.
  for (const auto& item : model()->items()) {
    if (file_path.IsParent(item->file().file_path)) {
      ScheduleFilePathValidityCheck(
          {item->file().file_path, /*requirements=*/{}});
    }
  }
}

void HoldingSpaceFileSystemDelegate::OnFilePathModified(
    const base::FilePath& file_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  model()->InvalidateItemImageIf(base::BindRepeating(
      [](const base::FilePath& file_path, const HoldingSpaceItem* item) {
        return item->file().file_path == file_path;
      },
      file_path));
}

void HoldingSpaceFileSystemDelegate::OnFilePathMoved(
    const base::FilePath& src,
    const base::FilePath& dst) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Collect items that should be moved to a new path. This includes:
  // *   Items whose path matches the source path.
  // *   Items parented by the source path.
  // Maps item ID to the item's new file path.
  std::vector<std::pair<std::string, base::FilePath>> items_to_move;
  for (auto& item : model()->items()) {
    if (src == item->file().file_path) {
      items_to_move.push_back(std::make_pair(item->id(), dst));
      continue;
    }

    if (src.IsParent(item->file().file_path)) {
      base::FilePath target_path(dst);
      if (!src.AppendRelativePath(item->file().file_path, &target_path)) {
        NOTREACHED_IN_MIGRATION();
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
    OnFilePathModified(dst);
    return;
  }

  // Get a list of the enabled Trash locations. Trash can be enabled and
  // disabled via policy, so ensure the latest list is retrieved.
  file_manager::trash::TrashPathsMap enabled_trash_locations =
      file_manager::trash::GenerateEnabledTrashLocationsForProfile(
          profile(), /*base_path=*/base::FilePath());

  // Mark items that were moved to an enabled Trash location for removal.
  std::set<std::string> item_ids_to_remove;
  for (const auto& it : enabled_trash_locations) {
    const base::FilePath& trash_location =
        it.first.Append(it.second.relative_folder_path);
    for (const auto& [id, file_path] : items_to_move) {
      if (trash_location.IsParent(file_path))
        item_ids_to_remove.insert(id);
    }
  }

  // Mark conflicts with existing items that arise from the move for removal.
  for (auto& item : model()->items()) {
    if (dst == item->file().file_path || dst.IsParent(item->file().file_path)) {
      item_ids_to_remove.insert(item->id());
    }
  }

  // Remove items which have been marked for removal.
  model()->RemoveItems(item_ids_to_remove);

  // Finally, update the files that have been moved.
  for (const auto& [id, file_path] : items_to_move) {
    if (item_ids_to_remove.count(id))
      continue;

    // File.
    const GURL file_system_url =
        holding_space_util::ResolveFileSystemUrl(profile(), file_path);
    const HoldingSpaceFile::FileSystemType file_system_type =
        holding_space_util::ResolveFileSystemType(profile(), file_system_url);

    // Update.
    model()->UpdateItem(id)->SetBackingFile(
        HoldingSpaceFile(file_path, file_system_type, file_system_url));
  }

  // If a backing file update occurred, it's possible that there are no longer
  // any holding space items associated with `src`. When that is the case, `src`
  // no longer needs to be watched.
  MaybeRemoveWatch(src);
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
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
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
        // In-progress items are not subject to validity checks.
        if (!item->progress().IsComplete())
          return false;

        // Avoid removing Android files if connection to ARC file system has
        // been lost (e.g. Android container might have crashed). Validity
        // checks will be re-run once the file system gets connected.
        if (arc_file_system_disconnected && ItemBackedByAndroidFile(item))
          return false;

        return base::Contains(*invalid_paths, item->file().file_path);
      },
      arc_file_system_disconnected, &invalid_paths));

  std::vector<const HoldingSpaceItem*> items_to_initialize;
  for (auto& item : model()->items()) {
    // Defer initialization of items backed by android files if the connection
    // to ARC file system service has been lost.
    if (arc_file_system_disconnected && ItemBackedByAndroidFile(item.get()))
      continue;

    if (!item->IsInitialized() &&
        base::Contains(valid_paths, item->file().file_path)) {
      items_to_initialize.push_back(item.get());
    }
  }

  for (auto* item : items_to_initialize) {
    const base::FilePath& file_path = item->file().file_path;
    const GURL file_system_url =
        holding_space_util::ResolveFileSystemUrl(profile(), file_path);
    const HoldingSpaceFile::FileSystemType file_system_type =
        holding_space_util::ResolveFileSystemType(profile(), file_system_url);

    model()->InitializeOrRemoveItem(
        item->id(),
        HoldingSpaceFile(file_path, file_system_type, file_system_url));
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
  const bool remove_watch =
      base::ranges::none_of(model()->items(), [&file_path](const auto& item) {
        return item->IsInitialized() &&
               item->file().file_path.DirName() == file_path;
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
        return parent_path.IsParent(item->file().file_path);
      },
      parent_path));
}

void HoldingSpaceFileSystemDelegate::ClearNonInitializedItems() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  model()->RemoveIf(base::BindRepeating(
      [](Profile* profile, const HoldingSpaceItem* item) {
        if (item->IsInitialized())
          return false;

        // Do not remove items whose path can be resolved to a file system URL.
        // In this case, the associated mount point has been mounted, but the
        // initialization may have been delayed - for example due to
        // issues/delays with initializing ARC.
        const GURL url = holding_space_util::ResolveFileSystemUrl(
            profile, item->file().file_path);
        return url.is_empty();
      },
      profile()));
}

}  // namespace ash
