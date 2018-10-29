// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/drive/chromeos/resource_metadata.h"

#include <limits.h>
#include <stddef.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "components/drive/chromeos/file_cache.h"
#include "components/drive/drive.pb.h"
#include "components/drive/file_system_core_util.h"
#include "components/drive/resource_metadata_storage.h"

namespace drive {
namespace internal {
namespace {

// Returns true if enough disk space is available for DB operation.
// TODO(hashimoto): Merge this with FileCache's FreeDiskSpaceGetterInterface.
bool EnoughDiskSpaceIsAvailableForDBOperation(const base::FilePath& path) {
  const int64_t kRequiredDiskSpaceInMB =
      128;  // 128 MB seems to be large enough.
  return base::SysInfo::AmountOfFreeDiskSpace(path) >=
      kRequiredDiskSpaceInMB * (1 << 20);
}

// Returns a file name with a uniquifier appended. (e.g. "File (1).txt")
std::string GetUniquifiedName(const std::string& name, int uniquifier) {
  base::FilePath name_path = base::FilePath::FromUTF8Unsafe(name);
  name_path = name_path.InsertBeforeExtensionASCII(
      base::StringPrintf(" (%d)", uniquifier));
  return name_path.AsUTF8Unsafe();
}

// Returns true when there is no entry with the specified name under the parent
// other than the specified entry.
FileError EntryCanUseName(ResourceMetadataStorage* storage,
                          const std::string& parent_local_id,
                          const std::string& local_id,
                          const std::string& base_name,
                          bool* result) {
  std::string existing_entry_id;
  FileError error = storage->GetChild(parent_local_id, base_name,
                                      &existing_entry_id);
  if (error == FILE_ERROR_OK)
    *result = existing_entry_id == local_id;
  else if (error == FILE_ERROR_NOT_FOUND)
    *result = true;
  else
    return error;
  return FILE_ERROR_OK;
}

// Returns true when the ID is used by an immutable entry.
bool IsImmutableEntry(const std::string& id) {
  return id == util::kDriveGrandRootLocalId ||
      id == util::kDriveOtherDirLocalId ||
      id == util::kDriveTrashDirLocalId;
}

}  // namespace

ResourceMetadata::ResourceMetadata(
    ResourceMetadataStorage* storage,
    FileCache* cache,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
    : blocking_task_runner_(blocking_task_runner),
      storage_(storage),
      cache_(cache) {
}

FileError ResourceMetadata::Initialize() {
  DCHECK(blocking_task_runner_->RunsTasksInCurrentSequence());
  return SetUpDefaultEntries();
}

void ResourceMetadata::Destroy() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  blocking_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ResourceMetadata::DestroyOnBlockingPool,
                                base::Unretained(this)));
}

FileError ResourceMetadata::Reset() {
  DCHECK(blocking_task_runner_->RunsTasksInCurrentSequence());

  if (!EnoughDiskSpaceIsAvailableForDBOperation(storage_->directory_path()))
    return FILE_ERROR_NO_LOCAL_SPACE;

  FileError error = storage_->SetLargestChangestamp(0);
  if (error != FILE_ERROR_OK)
    return error;

  error = storage_->SetStartPageToken(std::string());
  if (error != FILE_ERROR_OK)
    return error;

  // Remove all root entries.
  std::unique_ptr<Iterator> it = GetIterator();
  for (; !it->IsAtEnd(); it->Advance()) {
    if (it->GetValue().parent_local_id().empty()) {
      error = RemoveEntryRecursively(it->GetID());
      if (error != FILE_ERROR_OK)
        return error;
    }
  }
  if (it->HasError())
    return FILE_ERROR_FAILED;

  return SetUpDefaultEntries();
}

ResourceMetadata::~ResourceMetadata() {
  DCHECK(blocking_task_runner_->RunsTasksInCurrentSequence());
}

FileError ResourceMetadata::SetUpDefaultEntries() {
  DCHECK(blocking_task_runner_->RunsTasksInCurrentSequence());

  // Initialize "/drive".
  ResourceEntry entry;
  FileError error = storage_->GetEntry(util::kDriveGrandRootLocalId, &entry);
  if (error == FILE_ERROR_NOT_FOUND) {
    ResourceEntry root;
    root.mutable_file_info()->set_is_directory(true);
    root.set_local_id(util::kDriveGrandRootLocalId);
    root.set_title(util::kDriveGrandRootDirName);
    root.set_base_name(util::kDriveGrandRootDirName);
    error = storage_->PutEntry(root);
    if (error != FILE_ERROR_OK)
      return error;
  } else if (error == FILE_ERROR_OK) {
    if (!entry.resource_id().empty()) {
      // Old implementations used kDriveGrandRootLocalId as a resource ID.
      entry.clear_resource_id();
      error = storage_->PutEntry(entry);
      if (error != FILE_ERROR_OK)
        return error;
    }
  } else {
    return error;
  }

  // Initialize "/drive/other".
  error = storage_->GetEntry(util::kDriveOtherDirLocalId, &entry);
  if (error == FILE_ERROR_NOT_FOUND) {
    ResourceEntry other_dir;
    other_dir.mutable_file_info()->set_is_directory(true);
    other_dir.set_local_id(util::kDriveOtherDirLocalId);
    other_dir.set_parent_local_id(util::kDriveGrandRootLocalId);
    other_dir.set_title(util::kDriveOtherDirName);
    error = PutEntryUnderDirectory(other_dir);
    if (error != FILE_ERROR_OK)
      return error;
  } else if (error == FILE_ERROR_OK) {
    if (!entry.resource_id().empty()) {
      // Old implementations used kDriveOtherDirLocalId as a resource ID.
      entry.clear_resource_id();
      error = storage_->PutEntry(entry);
      if (error != FILE_ERROR_OK)
        return error;
    }
  } else {
    return error;
  }

  // Initialize "drive/trash".
  error = storage_->GetEntry(util::kDriveTrashDirLocalId, &entry);
  if (error == FILE_ERROR_NOT_FOUND) {
    ResourceEntry trash_dir;
    trash_dir.mutable_file_info()->set_is_directory(true);
    trash_dir.set_local_id(util::kDriveTrashDirLocalId);
    trash_dir.set_parent_local_id(util::kDriveGrandRootLocalId);
    trash_dir.set_title(util::kDriveTrashDirName);
    error = PutEntryUnderDirectory(trash_dir);
    if (error != FILE_ERROR_OK)
      return error;
  } else if (error != FILE_ERROR_OK) {
    return error;
  }

  // Initialize "drive/root".
  std::string child_id;
  error = storage_->GetChild(
      util::kDriveGrandRootLocalId, util::kDriveMyDriveRootDirName, &child_id);
  if (error == FILE_ERROR_NOT_FOUND) {
    ResourceEntry mydrive;
    mydrive.mutable_file_info()->set_is_directory(true);
    mydrive.set_parent_local_id(util::kDriveGrandRootLocalId);
    mydrive.set_title(util::kDriveMyDriveRootDirName);

    std::string local_id;
    error = AddEntry(mydrive, &local_id);
    if (error != FILE_ERROR_OK)
      return error;
  } else if (error != FILE_ERROR_OK) {
    return error;
  }

  // Initialize "/drive/team_drives".
  error = storage_->GetEntry(util::kDriveTeamDrivesDirLocalId, &entry);
  if (error == FILE_ERROR_NOT_FOUND) {
    ResourceEntry team_drives_dir;
    team_drives_dir.mutable_file_info()->set_is_directory(true);
    team_drives_dir.set_local_id(util::kDriveTeamDrivesDirLocalId);
    team_drives_dir.set_parent_local_id(util::kDriveGrandRootLocalId);
    team_drives_dir.set_title(util::kDriveTeamDrivesDirName);
    error = PutEntryUnderDirectory(team_drives_dir);
    if (error != FILE_ERROR_OK)
      return error;
  } else if (error != FILE_ERROR_OK) {
    return error;
  }

  // Initialize "/drive/Computers".
  error = storage_->GetEntry(util::kDriveComputersDirLocalId, &entry);
  if (error == FILE_ERROR_NOT_FOUND) {
    ResourceEntry computers_dir;
    computers_dir.mutable_file_info()->set_is_directory(true);
    computers_dir.set_local_id(util::kDriveComputersDirLocalId);
    computers_dir.set_parent_local_id(util::kDriveGrandRootLocalId);
    computers_dir.set_title(util::kDriveComputersDirName);
    error = PutEntryUnderDirectory(computers_dir);
    if (error != FILE_ERROR_OK)
      return error;
  } else if (error != FILE_ERROR_OK) {
    return error;
  }

  return FILE_ERROR_OK;
}

void ResourceMetadata::DestroyOnBlockingPool() {
  DCHECK(blocking_task_runner_->RunsTasksInCurrentSequence());
  delete this;
}

FileError ResourceMetadata::GetStartPageToken(std::string* out_value) {
  DCHECK(blocking_task_runner_->RunsTasksInCurrentSequence());
  return storage_->GetStartPageToken(out_value);
}

FileError ResourceMetadata::SetStartPageToken(const std::string& value) {
  DCHECK(blocking_task_runner_->RunsTasksInCurrentSequence());

  if (!EnoughDiskSpaceIsAvailableForDBOperation(storage_->directory_path()))
    return FILE_ERROR_NO_LOCAL_SPACE;

  return storage_->SetStartPageToken(value);
}

FileError ResourceMetadata::AddEntry(const ResourceEntry& entry,
                                     std::string* out_id) {
  DCHECK(blocking_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(entry.local_id().empty());

  if (!EnoughDiskSpaceIsAvailableForDBOperation(storage_->directory_path()))
    return FILE_ERROR_NO_LOCAL_SPACE;

  ResourceEntry parent;
  FileError error = storage_->GetEntry(entry.parent_local_id(), &parent);
  if (error != FILE_ERROR_OK)
    return error;
  if (!parent.file_info().is_directory())
    return FILE_ERROR_NOT_A_DIRECTORY;

  // Multiple entries with the same resource ID should not be present.
  std::string local_id;
  ResourceEntry existing_entry;
  if (!entry.resource_id().empty()) {
    error = storage_->GetIdByResourceId(entry.resource_id(), &local_id);
    if (error == FILE_ERROR_OK)
      error = storage_->GetEntry(local_id, &existing_entry);

    if (error == FILE_ERROR_OK)
      return FILE_ERROR_EXISTS;
    if (error != FILE_ERROR_NOT_FOUND)
      return error;
  }

  // Generate unique local ID when needed.
  // We don't check for ID collisions as its probability is extremely low.
  if (local_id.empty())
    local_id = base::GenerateGUID();

  ResourceEntry new_entry(entry);
  new_entry.set_local_id(local_id);

  error = PutEntryUnderDirectory(new_entry);
  if (error != FILE_ERROR_OK)
    return error;

  *out_id = local_id;
  return FILE_ERROR_OK;
}

FileError ResourceMetadata::RemoveEntry(const std::string& id) {
  DCHECK(blocking_task_runner_->RunsTasksInCurrentSequence());

  if (!EnoughDiskSpaceIsAvailableForDBOperation(storage_->directory_path()))
    return FILE_ERROR_NO_LOCAL_SPACE;

  // Disallow deletion of default entries.
  if (IsImmutableEntry(id))
    return FILE_ERROR_ACCESS_DENIED;

  ResourceEntry entry;
  FileError error = storage_->GetEntry(id, &entry);
  if (error != FILE_ERROR_OK)
    return error;

  return RemoveEntryRecursively(id);
}

FileError ResourceMetadata::GetResourceEntryById(const std::string& id,
                                                 ResourceEntry* out_entry) {
  DCHECK(blocking_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!id.empty());
  DCHECK(out_entry);

  return storage_->GetEntry(id, out_entry);
}

FileError ResourceMetadata::GetResourceEntryByPath(const base::FilePath& path,
                                                   ResourceEntry* out_entry) {
  DCHECK(blocking_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(out_entry);

  std::string id;
  FileError error = GetIdByPath(path, &id);
  if (error != FILE_ERROR_OK)
    return error;

  return GetResourceEntryById(id, out_entry);
}

FileError ResourceMetadata::ReadDirectoryByPath(
    const base::FilePath& path,
    ResourceEntryVector* out_entries) {
  DCHECK(blocking_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(out_entries);

  std::string id;
  FileError error = GetIdByPath(path, &id);
  if (error != FILE_ERROR_OK)
    return error;
  return ReadDirectoryById(id, out_entries);
}

FileError ResourceMetadata::ReadDirectoryById(
    const std::string& id,
    ResourceEntryVector* out_entries) {
  DCHECK(blocking_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(out_entries);

  ResourceEntry entry;
  FileError error = GetResourceEntryById(id, &entry);
  if (error != FILE_ERROR_OK)
    return error;

  if (!entry.file_info().is_directory())
    return FILE_ERROR_NOT_A_DIRECTORY;

  std::vector<std::string> children;
  error = storage_->GetChildren(id, &children);
  if (error != FILE_ERROR_OK)
    return error;

  ResourceEntryVector entries(children.size());
  for (size_t i = 0; i < children.size(); ++i) {
    error = storage_->GetEntry(children[i], &entries[i]);
    if (error != FILE_ERROR_OK)
      return error;
  }
  out_entries->swap(entries);
  return FILE_ERROR_OK;
}

FileError ResourceMetadata::RefreshEntry(const ResourceEntry& entry) {
  DCHECK(blocking_task_runner_->RunsTasksInCurrentSequence());

  if (!EnoughDiskSpaceIsAvailableForDBOperation(storage_->directory_path()))
    return FILE_ERROR_NO_LOCAL_SPACE;

  ResourceEntry old_entry;
  FileError error = storage_->GetEntry(entry.local_id(), &old_entry);
  if (error != FILE_ERROR_OK)
    return error;

  if (IsImmutableEntry(entry.local_id()) ||
      old_entry.file_info().is_directory() !=  // Reject incompatible input.
      entry.file_info().is_directory())
    return FILE_ERROR_INVALID_OPERATION;

  if (!entry.resource_id().empty()) {
    // Multiple entries cannot share the same resource ID.
    std::string local_id;
    FileError error = GetIdByResourceId(entry.resource_id(), &local_id);
    switch (error) {
      case FILE_ERROR_OK:
        if (local_id != entry.local_id())
          return FILE_ERROR_INVALID_OPERATION;
        break;

      case FILE_ERROR_NOT_FOUND:
        break;

      default:
        return error;
    }
  }

  // Make sure that the new parent exists and it is a directory.
  ResourceEntry new_parent;
  error = storage_->GetEntry(entry.parent_local_id(), &new_parent);
  if (error != FILE_ERROR_OK)
    return error;

  if (!new_parent.file_info().is_directory())
    return FILE_ERROR_NOT_A_DIRECTORY;

  // Do not overwrite cache states.
  // Cache state should be changed via FileCache.
  ResourceEntry updated_entry(entry);
  if (old_entry.file_specific_info().has_cache_state()) {
    *updated_entry.mutable_file_specific_info()->mutable_cache_state() =
        old_entry.file_specific_info().cache_state();
  } else if (updated_entry.file_specific_info().has_cache_state()) {
    updated_entry.mutable_file_specific_info()->clear_cache_state();
  }
  // Remove from the old parent and add it to the new parent with the new data.
  return PutEntryUnderDirectory(updated_entry);
}

FileError ResourceMetadata::GetSubDirectoriesRecursively(
    const std::string& id,
    std::set<base::FilePath>* sub_directories) {
  DCHECK(blocking_task_runner_->RunsTasksInCurrentSequence());

  std::vector<std::string> children;
  FileError error = storage_->GetChildren(id, &children);
  if (error != FILE_ERROR_OK)
    return error;
  for (size_t i = 0; i < children.size(); ++i) {
    ResourceEntry entry;
    error = storage_->GetEntry(children[i], &entry);
    if (error != FILE_ERROR_OK)
      return error;
    if (entry.file_info().is_directory()) {
      base::FilePath path;
      error = GetFilePath(children[i], &path);
      if (error != FILE_ERROR_OK)
        return error;
      sub_directories->insert(path);
      GetSubDirectoriesRecursively(children[i], sub_directories);
    }
  }
  return FILE_ERROR_OK;
}

FileError ResourceMetadata::GetChildId(const std::string& parent_local_id,
                                       const std::string& base_name,
                                       std::string* out_child_id) {
  DCHECK(blocking_task_runner_->RunsTasksInCurrentSequence());
  return storage_->GetChild(parent_local_id, base_name, out_child_id);
}

std::unique_ptr<ResourceMetadata::Iterator> ResourceMetadata::GetIterator() {
  DCHECK(blocking_task_runner_->RunsTasksInCurrentSequence());

  return storage_->GetIterator();
}

FileError ResourceMetadata::GetFilePath(const std::string& id,
                                        base::FilePath* out_file_path) {
  DCHECK(blocking_task_runner_->RunsTasksInCurrentSequence());

  ResourceEntry entry;
  FileError error = storage_->GetEntry(id, &entry);
  if (error != FILE_ERROR_OK)
    return error;

  base::FilePath path;
  if (!entry.parent_local_id().empty()) {
    error = GetFilePath(entry.parent_local_id(), &path);
    if (error != FILE_ERROR_OK)
      return error;
  } else if (entry.local_id() != util::kDriveGrandRootLocalId) {
    DVLOG(1) << "Entries not under the grand root don't have paths.";
    return FILE_ERROR_NOT_FOUND;
  }
  path = path.Append(base::FilePath::FromUTF8Unsafe(entry.base_name()));
  *out_file_path = path;
  return FILE_ERROR_OK;
}

FileError ResourceMetadata::GetIdByPath(const base::FilePath& file_path,
                                        std::string* out_id) {
  DCHECK(blocking_task_runner_->RunsTasksInCurrentSequence());

  // Start from the root.
  std::vector<base::FilePath::StringType> components;
  file_path.GetComponents(&components);
  if (components.empty() ||
      components[0] != util::GetDriveGrandRootPath().value())
    return FILE_ERROR_NOT_FOUND;

  // Iterate over the remaining components.
  std::string id = util::kDriveGrandRootLocalId;
  for (size_t i = 1; i < components.size(); ++i) {
    const std::string component = base::FilePath(components[i]).AsUTF8Unsafe();
    std::string child_id;
    FileError error = storage_->GetChild(id, component, &child_id);
    if (error != FILE_ERROR_OK)
      return error;
    id = child_id;
  }
  *out_id = id;
  return FILE_ERROR_OK;
}

FileError ResourceMetadata::GetIdByResourceId(const std::string& resource_id,
                                              std::string* out_local_id) {
  DCHECK(blocking_task_runner_->RunsTasksInCurrentSequence());
  return storage_->GetIdByResourceId(resource_id, out_local_id);
}

FileError ResourceMetadata::PutEntryUnderDirectory(const ResourceEntry& entry) {
  DCHECK(blocking_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!entry.local_id().empty());
  DCHECK(!entry.parent_local_id().empty());

  std::string base_name;
  FileError error = GetDeduplicatedBaseName(entry, &base_name);
  if (error != FILE_ERROR_OK)
    return error;
  ResourceEntry updated_entry(entry);
  updated_entry.set_base_name(base_name);
  return storage_->PutEntry(updated_entry);
}

FileError ResourceMetadata::GetDeduplicatedBaseName(
    const ResourceEntry& entry,
    std::string* base_name) {
  DCHECK(blocking_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!entry.parent_local_id().empty());
  DCHECK(!entry.title().empty());

  // The entry name may have been changed due to prior name de-duplication.
  // We need to first restore the file name based on the title before going
  // through name de-duplication again when it is added to another directory.
  *base_name = entry.title();
  if (entry.has_file_specific_info() &&
      entry.file_specific_info().is_hosted_document()) {
    *base_name += entry.file_specific_info().document_extension();
  }
  *base_name = util::NormalizeFileName(*base_name);

  // If |base_name| is not used, just return it.
  bool can_use_name = false;
  FileError error = EntryCanUseName(storage_, entry.parent_local_id(),
                                    entry.local_id(), *base_name,
                                    &can_use_name);
  if (error != FILE_ERROR_OK || can_use_name)
    return error;

  // Find an unused number with binary search.
  int smallest_known_unused_modifier = 1;
  while (true) {
    error = EntryCanUseName(storage_, entry.parent_local_id(), entry.local_id(),
                            GetUniquifiedName(*base_name,
                                              smallest_known_unused_modifier),
                            &can_use_name);
    if (error != FILE_ERROR_OK)
      return error;
    if (can_use_name)
      break;

    const int delta = base::RandInt(1, smallest_known_unused_modifier);
    if (smallest_known_unused_modifier <= INT_MAX - delta) {
      smallest_known_unused_modifier += delta;
    } else {  // No luck finding an unused number. Try again.
      smallest_known_unused_modifier = 1;
    }
  }

  int largest_known_used_modifier = 1;
  while (smallest_known_unused_modifier - largest_known_used_modifier > 1) {
    const int modifier = largest_known_used_modifier +
        (smallest_known_unused_modifier - largest_known_used_modifier) / 2;

    error = EntryCanUseName(storage_, entry.parent_local_id(), entry.local_id(),
                            GetUniquifiedName(*base_name, modifier),
                            &can_use_name);
    if (error != FILE_ERROR_OK)
      return error;
    if (can_use_name) {
      smallest_known_unused_modifier = modifier;
    } else {
      largest_known_used_modifier = modifier;
    }
  }
  *base_name = GetUniquifiedName(*base_name, smallest_known_unused_modifier);
  return FILE_ERROR_OK;
}

FileError ResourceMetadata::RemoveEntryRecursively(const std::string& id) {
  DCHECK(blocking_task_runner_->RunsTasksInCurrentSequence());

  ResourceEntry entry;
  FileError error = storage_->GetEntry(id, &entry);
  if (error != FILE_ERROR_OK)
    return error;

  if (entry.file_info().is_directory()) {
    std::vector<std::string> children;
    error = storage_->GetChildren(id, &children);
    if (error != FILE_ERROR_OK)
      return error;
    for (size_t i = 0; i < children.size(); ++i) {
      error = RemoveEntryRecursively(children[i]);
      if (error != FILE_ERROR_OK)
        return error;
    }
  }

  error = cache_->Remove(id);
  if (error != FILE_ERROR_OK)
    return error;

  return storage_->RemoveEntry(id);
}

}  // namespace internal
}  // namespace drive
