// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/local_desk_data_manager.h"

#include <utility>

#include "ash/public/cpp/desk_template.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/files/dir_reader_posix.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/json/json_string_value_serializer.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/values.h"
#include "components/account_id/account_id.h"
#include "components/app_restore/restore_data.h"
#include "components/desks_storage/core/desk_model.h"
#include "components/desks_storage/core/desk_storage_metrics_util.h"
#include "components/desks_storage/core/desk_template_conversion.h"
#include "components/desks_storage/core/desk_template_util.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "components/sync/protocol/workspace_desk_specifics.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/re2/src/re2/stringpiece.h"
#include "url/gurl.h"

namespace desks_storage {

namespace {

// Setting this to true allows us to add more than the maximum number of
// desk templates. Used only for testing.
bool g_disable_max_template_limit = false;

// Setting this to true allows us to exclude the max count of save and recall
// desk entries as part of `GetMaxEntryCount` since there are some tests
// treating save and recall desks behavior as regular desk templates (such as
// button enablement). Also, since save and recall desks and desk templates are
// currently being treated as desk templates, exclude save and recall desks
// limit until save and recall desks are enabled.
bool g_exclude_save_and_recall_desk_in_max_entry_count = true;

// File extension for saving template entries.
constexpr char kFileExtension[] = ".saveddesk";
constexpr char kSavedDeskDirectoryName[] = "saveddesk";
constexpr size_t kMaxDeskTemplateCount = 6u;
// Currently, the save for later button is dependent on the the max number of
// entries total.
constexpr size_t kMaxSaveAndRecallDeskCount = 6u;

// Set of valid desk types.
constexpr auto kValidDeskTypes = base::MakeFixedFlatSet<ash::DeskTemplateType>(
    {ash::DeskTemplateType::kTemplate, ash::DeskTemplateType::kSaveAndRecall});

// Reads a file at `fully_qualified_path` into a
// std::unique_ptr<ash::DeskTemplate> This function returns a `nullptr` if the
// file does not exist or deserialization fails.
std::unique_ptr<ash::DeskTemplate> ReadFileToTemplate(
    const base::FilePath& fully_qualified_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  std::string value_string;
  if (!base::ReadFileToString(fully_qualified_path, &value_string))
    return nullptr;

  std::string error_message;
  int error_code;
  std::unique_ptr<base::Value> desk_template_value =
      JSONStringValueDeserializer(value_string)
          .Deserialize(&error_code, &error_message);

  if (!desk_template_value) {
    DVLOG(1) << "Fail to deserialize json value from string with error code: "
             << error_code << " and error message: " << error_message;
    return nullptr;
  }

  return desk_template_conversion::ParseDeskTemplateFromSource(
      *desk_template_value, ash::DeskTemplateSource::kUser);
}

bool EndsWith(const char* input, const char* suffix) {
  size_t input_length = strlen(input);
  size_t suffix_length = strlen(suffix);
  if (suffix_length <= input_length) {
    return strcmp(input + input_length - suffix_length, suffix) == 0;
  }
  return false;
}

// TODO(crbug.com/1320836): Make template creation for
// local_desk_data_manager_unittests cleaner.
bool IsValidTemplateFileName(const char* name) {
  if (name == nullptr)
    return false;
  return EndsWith(name, kFileExtension);
}

// Writes a DeskTemplate or SaveAndRecallDesk base::Value `json_value` to a file
// at `path_to_template`. This function utilizes blocking calls and assumes that
// it is being called from a thread which can accept such calls, please don't
// call this function from the UI thread.
bool WriteTemplateFile(const base::FilePath& path_to_template,
                       base::Value json_value) {
  std::string json_string;
  JSONStringValueSerializer(&json_string).Serialize(json_value);

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  return base::WriteFile(path_to_template, json_string);
}

// Generates the fully qualified path to a desk template or save and recall desk
// file given the `file_path` to the desk template or save and recall desk
// directory and the entry's `uuid`.
base::FilePath GetFullyQualifiedPath(base::FilePath file_path,
                                     const base::GUID& uuid) {
  std::string filename = uuid.AsLowercaseString();
  filename.append(kFileExtension);

  return base::FilePath(file_path.Append(base::FilePath(filename)));
}

}  // namespace

LocalDeskDataManager::LocalDeskDataManager(
    const base::FilePath& user_data_dir_path,
    const AccountId& account_id)
    : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      user_data_dir_path_(user_data_dir_path),
      local_saved_desk_path_(
          user_data_dir_path.AppendASCII(kSavedDeskDirectoryName)),
      account_id_(account_id),
      cache_status_(CacheStatus::kNotInitialized) {
  // Populate `saved_desks_list_` with all the desk types.
  for (const auto& desk_type : kValidDeskTypes) {
    saved_desks_list_[desk_type];
  }
  // Load the cache.
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&LocalDeskDataManager::LoadCacheOnBackgroundSequence,
                     user_data_dir_path),
      base::BindOnce(&LocalDeskDataManager::MoveEntriesIntoCache,
                     weak_ptr_factory_.GetWeakPtr()));
}

LocalDeskDataManager::~LocalDeskDataManager() = default;

LocalDeskDataManager::LoadCacheResult::LoadCacheResult(
    CacheStatus status,
    std::vector<std::unique_ptr<ash::DeskTemplate>> out_entries)
    : status(status), entries(std::move(out_entries)) {}

LocalDeskDataManager::LoadCacheResult::LoadCacheResult(
    LoadCacheResult&& other) = default;

LocalDeskDataManager::LoadCacheResult::~LoadCacheResult() = default;

LocalDeskDataManager::DeleteTaskResult::DeleteTaskResult(
    DeleteEntryStatus status,
    std::vector<std::unique_ptr<ash::DeskTemplate>> out_entries)
    : status(status), entries(std::move(out_entries)) {}

LocalDeskDataManager::DeleteTaskResult::DeleteTaskResult(
    DeleteTaskResult&& other) = default;

LocalDeskDataManager::DeleteTaskResult::~DeleteTaskResult() = default;

DeskModel::GetAllEntriesResult LocalDeskDataManager::GetAllEntries() {
  std::vector<const ash::DeskTemplate*> entries;
  if (cache_status_ != CacheStatus::kOk) {
    return GetAllEntriesResult(GetAllEntriesStatus::kFailure,
                               std::move(entries));
  }

  for (const auto& it : policy_entries_)
    entries.push_back(it.get());

  for (auto& saved_desk : saved_desks_list_) {
    for (auto& [uuid, template_entry] : saved_desk.second) {
      DCHECK_EQ(uuid, template_entry->uuid());
      entries.push_back(template_entry.get());
    }
  }
  return GetAllEntriesResult(GetAllEntriesStatus::kOk, std::move(entries));
}

DeskModel::GetEntryByUuidResult LocalDeskDataManager::GetEntryByUUID(
    const base::GUID& uuid) {
  if (cache_status_ != LocalDeskDataManager::CacheStatus::kOk) {
    return DeskModel::GetEntryByUuidResult(
        DeskModel::GetEntryByUuidStatus::kFailure, nullptr);
  }

  if (!uuid.is_valid()) {
    return DeskModel::GetEntryByUuidResult(
        DeskModel::GetEntryByUuidStatus::kInvalidUuid, nullptr);
  }

  const ash::DeskTemplateType desk_type = GetDeskTypeOfUuid(uuid);

  const auto cache_entry = saved_desks_list_[desk_type].find(uuid);

  if (cache_entry == saved_desks_list_[desk_type].end()) {
    std::unique_ptr<ash::DeskTemplate> policy_entry =
        GetAdminDeskTemplateByUUID(uuid);

    if (policy_entry) {
      return DeskModel::GetEntryByUuidResult(
          DeskModel::GetEntryByUuidStatus::kOk, std::move(policy_entry));
    } else {
      return DeskModel::GetEntryByUuidResult(
          DeskModel::GetEntryByUuidStatus::kNotFound, nullptr);
    }
  } else {
    return DeskModel::GetEntryByUuidResult(DeskModel::GetEntryByUuidStatus::kOk,
                                           cache_entry->second.get()->Clone());
  }
}

void LocalDeskDataManager::AddOrUpdateEntry(
    std::unique_ptr<ash::DeskTemplate> new_entry,
    AddOrUpdateEntryCallback callback) {
  if (cache_status_ != CacheStatus::kOk) {
    std::move(callback).Run(AddOrUpdateEntryStatus::kFailure,
                            std::move(new_entry));
    return;
  }

  const ash::DeskTemplateType desk_type = new_entry->type();
  const base::GUID uuid = new_entry->uuid();
  if (!uuid.is_valid() || desk_type == ash::DeskTemplateType::kUnknown) {
    std::move(callback).Run(AddOrUpdateEntryStatus::kInvalidArgument,
                            std::move(new_entry));
    return;
  }
  size_t template_type_max_size = GetMaxEntryCountByDeskType(desk_type);
  if (!g_disable_max_template_limit &&
      saved_desks_list_[desk_type].size() >= template_type_max_size) {
    std::move(callback).Run(AddOrUpdateEntryStatus::kHitMaximumLimit,
                            std::move(new_entry));
    return;
  }

  apps::AppRegistryCache* cache =
      apps::AppRegistryCacheWrapper::Get().GetAppRegistryCache(account_id_);
  DCHECK(cache);
  base::Value template_base_value =
      desk_template_conversion::SerializeDeskTemplateAsPolicy(new_entry.get(),
                                                              cache);
  // Deserialize the `template_base_value` to a desk template to make sure that
  // we can properly get the correct information now instead of during a future
  // user operation.
  std::unique_ptr<ash::DeskTemplate> deserialize_entry =
      desk_template_conversion::ParseDeskTemplateFromSource(
          template_base_value, new_entry->source());
  auto& saved_desks = saved_desks_list_[desk_type];
  auto existing_it = saved_desks.find(uuid);
  std::unique_ptr<ash::DeskTemplate> old_entry = nullptr;
  bool is_update = existing_it != saved_desks.end();
  if (is_update) {
    old_entry = std::move(existing_it->second);
    existing_it->second = std::move(deserialize_entry);
  } else {
    saved_desks[uuid] = std::move(deserialize_entry);
  }

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&LocalDeskDataManager::AddOrUpdateEntryTask,
                     local_saved_desk_path_, uuid,
                     std::move(template_base_value), desk_type),
      base::BindOnce(&LocalDeskDataManager::OnAddOrUpdateEntry,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     is_update, desk_type, uuid, std::move(old_entry),
                     std::move(new_entry)));
}

void LocalDeskDataManager::DeleteEntry(const base::GUID& uuid,
                                       DeleteEntryCallback callback) {
  if (cache_status_ != CacheStatus::kOk) {
    std::move(callback).Run(DeleteEntryStatus::kFailure);
    return;
  }

  if (!uuid.is_valid()) {
    // There does not exist an entry with invalid UUID.
    // Therefore the deletion request is vicariously successful.
    std::move(callback).Run(DeleteEntryStatus::kOk);
    return;
  }
  const ash::DeskTemplateType desk_type = GetDeskTypeOfUuid(uuid);
  // `entry` is used to keep track of the deleted entry in case we need to
  // rollback the deletion if the file operation fails to delete it.
  std::vector<std::unique_ptr<ash::DeskTemplate>> entry;
  auto& saved_desks = saved_desks_list_[desk_type];
  auto existing_it = saved_desks.find(uuid);

  // The deletion is successful if the entry does not exist.
  if (existing_it == saved_desks.end()) {
    std::move(callback).Run(DeleteEntryStatus::kOk);
    return;
  }

  entry.push_back(std::move(existing_it->second));
  saved_desks_list_[desk_type].erase(existing_it);

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&LocalDeskDataManager::DeleteEntryTask,
                     local_saved_desk_path_, uuid, std::move(entry)),
      base::BindOnce(&LocalDeskDataManager::OnDeleteEntry,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void LocalDeskDataManager::DeleteAllEntries(DeleteEntryCallback callback) {
  if (cache_status_ != CacheStatus::kOk) {
    std::move(callback).Run(DeleteEntryStatus::kFailure);
    return;
  }

  // `entries` is used to keep track of any desk template entry that failed to
  // be deleted by the file system. This is used to rollback the deletion of
  // those fail to delete files.
  std::vector<std::unique_ptr<ash::DeskTemplate>> entries;

  // Deletes all desk templates and save and recall desks.
  for (auto& type_and_saved_desk : saved_desks_list_) {
    for (auto& [uuid, template_entry] : type_and_saved_desk.second) {
      entries.push_back(std::move(template_entry));
    }
    type_and_saved_desk.second.clear();
  }

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&LocalDeskDataManager::DeleteAllEntriesTask,
                     local_saved_desk_path_, std::move(entries)),
      base::BindOnce(&LocalDeskDataManager::OnDeleteEntry,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

// TODO(crbug.com/1320805): Remove this function once both desk models support
// desk type counts.
size_t LocalDeskDataManager::GetEntryCount() const {
  return GetSaveAndRecallDeskEntryCount() + GetDeskTemplateEntryCount();
}

size_t LocalDeskDataManager::GetSaveAndRecallDeskEntryCount() const {
  return saved_desks_list_.at(ash::DeskTemplateType::kSaveAndRecall).size();
}

size_t LocalDeskDataManager::GetDeskTemplateEntryCount() const {
  return saved_desks_list_.at(ash::DeskTemplateType::kTemplate).size() +
         policy_entries_.size();
}

size_t LocalDeskDataManager::GetMaxEntryCount() const {
  return kMaxDeskTemplateCount +
         (!g_exclude_save_and_recall_desk_in_max_entry_count
              ? kMaxSaveAndRecallDeskCount
              : 0u) +
         policy_entries_.size();
}

size_t LocalDeskDataManager::GetMaxSaveAndRecallDeskEntryCount() const {
  return kMaxSaveAndRecallDeskCount;
}

size_t LocalDeskDataManager::GetMaxDeskTemplateEntryCount() const {
  return kMaxDeskTemplateCount + policy_entries_.size();
}

std::vector<base::GUID> LocalDeskDataManager::GetAllEntryUuids() const {
  std::vector<base::GUID> keys;
  for (const auto& type_and_saved_desks : saved_desks_list_) {
    for (const auto& [uuid, template_entry] : type_and_saved_desks.second) {
      DCHECK_EQ(uuid, template_entry->uuid());
      keys.emplace_back(uuid);
    }
  }
  return keys;
}

bool LocalDeskDataManager::IsReady() const {
  return cache_status_ == CacheStatus::kOk;
}

bool LocalDeskDataManager::IsSyncing() const {
  // Local storage backend never syncs to server.
  return false;
}

ash::DeskTemplate* LocalDeskDataManager::FindOtherEntryWithName(
    const std::u16string& name,
    ash::DeskTemplateType type,
    const base::GUID& uuid) const {
  return desk_template_util::FindOtherEntryWithName(name, uuid,
                                                    saved_desks_list_.at(type));
}

// static
void LocalDeskDataManager::SetDisableMaxTemplateLimitForTesting(bool disabled) {
  g_disable_max_template_limit = disabled;
}

// static
void LocalDeskDataManager::SetExcludeSaveAndRecallDeskInMaxEntryCountForTesting(
    bool exclude) {
  g_exclude_save_and_recall_desk_in_max_entry_count = exclude;
}

// static
LocalDeskDataManager::LoadCacheResult
LocalDeskDataManager::LoadCacheOnBackgroundSequence(
    const base::FilePath& user_data_dir_path) {
  std::vector<std::unique_ptr<ash::DeskTemplate>> entries;
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  if (!base::DirectoryExists(user_data_dir_path)) {
    // User data directory path is invalid. This local storage cannot load any
    // templates from disk.
    return {CacheStatus::kInvalidPath, std::move(entries)};
  }

  // Set dir_reader to read from the `local_saved_desk_path_` directory.
  // check to make sure there is a `local_saved_desk_path_` directory. If not
  // create it.
  base::FilePath local_saved_desk_path =
      user_data_dir_path.AppendASCII(kSavedDeskDirectoryName);
  base::CreateDirectory(local_saved_desk_path);
  base::DirReaderPosix dir_reader(local_saved_desk_path.AsUTF8Unsafe().c_str());

  if (!dir_reader.IsValid()) {
    // Failed to find or create the `local_saved_desk_path_` directory path.
    // This local storage cannot load any entry of `type` from disk.
    return {CacheStatus::kInvalidPath, std::move(entries)};
  }

  while (dir_reader.Next()) {
    if (!IsValidTemplateFileName(dir_reader.name())) {
      continue;
    }

    base::FilePath fully_qualified_path =
        base::FilePath(local_saved_desk_path.Append(dir_reader.name()));
    std::unique_ptr<ash::DeskTemplate> entry =
        ReadFileToTemplate(fully_qualified_path);

    // TODO(b/248645596): Record metrics about files that failed to parse.
    if (entry == nullptr)
      continue;

    // Rename file for saved desk if uuid in file and file name are different.
    std::string entry_uuid_string = entry->uuid().AsLowercaseString();
    entry_uuid_string.append(kFileExtension);

    if (dir_reader.name() != entry_uuid_string) {
      const base::FilePath renamed_fully_qualified_path =
          GetFullyQualifiedPath(local_saved_desk_path, entry->uuid());
      if (!base::Move(fully_qualified_path, renamed_fully_qualified_path)) {
        DVLOG(1) << "Fail to rename saved desk template to proper UUID";
      }
    }

    entries.push_back(std::move(entry));
  }
  return {CacheStatus::kOk, std::move(entries)};
}

DeskModel::AddOrUpdateEntryStatus LocalDeskDataManager::AddOrUpdateEntryTask(
    const base::FilePath& local_saved_desk_path,
    const base::GUID uuid,
    base::Value entry_base_value,
    ash::DeskTemplateType desk_type) {
  const base::FilePath fully_qualified_path =
      GetFullyQualifiedPath(local_saved_desk_path, uuid);
  if (WriteTemplateFile(fully_qualified_path, std::move(entry_base_value))) {
    int64_t file_size;
    GetFileSize(fully_qualified_path, &file_size);
    RecordSavedDeskTemplateSizeHistogram(desk_type, file_size);
    return AddOrUpdateEntryStatus::kOk;
  }
  return AddOrUpdateEntryStatus::kFailure;
}

void LocalDeskDataManager::OnAddOrUpdateEntry(
    AddOrUpdateEntryCallback callback,
    bool is_update,
    ash::DeskTemplateType desk_type,
    const base::GUID uuid,
    std::unique_ptr<ash::DeskTemplate> old_entry,
    std::unique_ptr<ash::DeskTemplate> new_entry,
    AddOrUpdateEntryStatus status) {
  // Rollback the template addition to the cache if there's a failure.
  if (status == AddOrUpdateEntryStatus::kFailure) {
    if (is_update) {
      saved_desks_list_[desk_type][uuid] = std::move(old_entry);
    } else {
      saved_desks_list_[desk_type].erase(uuid);
    }
  }
  std::move(callback).Run(status, std::move(new_entry));
}

// static
LocalDeskDataManager::DeleteTaskResult LocalDeskDataManager::DeleteEntryTask(
    const base::FilePath& local_saved_desk_path,
    const base::GUID& uuid,
    std::vector<std::unique_ptr<ash::DeskTemplate>> roll_back_entry) {
  const base::FilePath fully_qualified_path =
      GetFullyQualifiedPath(local_saved_desk_path, uuid);
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  if (base::DeleteFile(fully_qualified_path))
    return {DeleteEntryStatus::kOk, std::move(roll_back_entry)};
  return {DeleteEntryStatus::kFailure, std::move(roll_back_entry)};
}

// static
LocalDeskDataManager::DeleteTaskResult
LocalDeskDataManager::DeleteAllEntriesTask(
    const base::FilePath& local_saved_desk_path,
    std::vector<std::unique_ptr<ash::DeskTemplate>> entries) {
  if (!base::DirReaderPosix(local_saved_desk_path.AsUTF8Unsafe().c_str())
           .IsValid()) {
    return {DeleteEntryStatus::kFailure, std::move(entries)};
  }
  DeleteEntryStatus overall_delete_successes = DeleteEntryStatus::kOk;
  for (auto it = entries.begin(); it != entries.end();) {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    bool delete_success = base::DeleteFile(
        GetFullyQualifiedPath(local_saved_desk_path, (*it)->uuid()));
    // On a successful file delete, we perform a move and delete on the vector
    // by moving the last entry in the vector to the beginning and delete the
    // last entry. On a failed file delete, we increment the iterator up by one
    // to keep the entry for a rollback.
    if (delete_success) {
      *it = std::move(entries.back());
      entries.pop_back();
    } else {
      overall_delete_successes = DeleteEntryStatus::kFailure;
      ++it;
    }
  }
  return {overall_delete_successes, std::move(entries)};
}

void LocalDeskDataManager::OnDeleteEntry(
    DeskModel::DeleteEntryCallback callback,
    DeleteTaskResult delete_return) {
  // Rollback deletes from the cache for the failed file deletes.
  if (delete_return.status == DeskModel::DeleteEntryStatus::kFailure) {
    MoveEntriesIntoCache({CacheStatus::kOk, std::move(delete_return.entries)});
  }
  std::move(callback).Run(delete_return.status);
}

ash::DeskTemplateType LocalDeskDataManager::GetDeskTypeOfUuid(
    const base::GUID uuid) const {
  for (const auto& [desk_type, saved_desk] : saved_desks_list_) {
    if (base::Contains(saved_desk, uuid))
      return desk_type;
  }
  return ash::DeskTemplateType::kUnknown;
}

size_t LocalDeskDataManager::GetMaxEntryCountByDeskType(
    ash::DeskTemplateType desk_type) const {
  switch (desk_type) {
    case ash::DeskTemplateType::kTemplate:
      return kMaxDeskTemplateCount;
    case ash::DeskTemplateType::kSaveAndRecall:
      return kMaxSaveAndRecallDeskCount;
    case ash::DeskTemplateType::kFloatingWorkspace:
    case ash::DeskTemplateType::kUnknown:
      return 0;
  }
}

void LocalDeskDataManager::MoveEntriesIntoCache(LoadCacheResult cache_result) {
  cache_status_ = cache_result.status;
  // Do nothing if the cache isn't ready.
  if (cache_status_ != CacheStatus::kOk)
    return;
  for (auto& template_entry : cache_result.entries) {
    DCHECK(template_entry);
    saved_desks_list_[template_entry->type()][template_entry->uuid()] =
        std::move(template_entry);
  }
}

}  // namespace desks_storage
