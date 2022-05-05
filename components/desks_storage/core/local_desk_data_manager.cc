// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/local_desk_data_manager.h"

#include "ash/public/cpp/desk_template.h"
#include "base/files/dir_reader_posix.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/json/json_string_value_serializer.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/values.h"
#include "components/account_id/account_id.h"
#include "components/app_restore/restore_data.h"
#include "components/desks_storage/core/desk_model.h"
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
const std::set<ash::DeskTemplateType> kDeskTypes = {
    ash::DeskTemplateType::kTemplate, ash::DeskTemplateType::kSaveAndRecall};

// Reads a file at `fully_qualified_path` into a
// std::unique_ptr<ash::DeskTemplate> This function returns a `nullptr` if the
// file does not exist or deserialization fails.
std::unique_ptr<ash::DeskTemplate> ReadFileToTemplate(
    const base::FilePath& fully_qualified_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  if (!base::PathExists(fully_qualified_path))
    return nullptr;

  std::string value_string;
  if (!base::ReadFileToString(fully_qualified_path, &value_string))
    return nullptr;

  std::string error_message;
  int error_code;
  JSONStringValueDeserializer deserializer(value_string);
  std::unique_ptr<base::Value> desk_template_value =
      deserializer.Deserialize(&error_code, &error_message);

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
  JSONStringValueSerializer serializer(&json_string);

  serializer.Serialize(json_value);

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  return base::WriteFile(path_to_template, json_string);
}

// Generates the fully qualified path to a desk template or save and recall desk
// file given the `file_path` to the desk template or save and recall desk
// directory and the entry's `uuid`.
base::FilePath GetFullyQualifiedPath(base::FilePath file_path,
                                     const std::string& uuid) {
  std::string filename(uuid);
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
  // Load the cache.
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&LocalDeskDataManager::EnsureCacheIsLoaded,
                                base::Unretained(this)));
  // Populate `saved_desks_list_` with all the desk types.
  for (const auto& desk_type : kDeskTypes) {
    saved_desks_list_[desk_type];
  }
}

LocalDeskDataManager::~LocalDeskDataManager() = default;

void LocalDeskDataManager::GetAllEntries(
    DeskModel::GetAllEntriesCallback callback) {
  auto status = std::make_unique<DeskModel::GetAllEntriesStatus>();
  auto entries = std::make_unique<std::vector<const ash::DeskTemplate*>>();

  // It's safe to pass base::Unretained(this) since the LocalDeskDataManager is
  // a long-lived object that should persist during user session.
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&LocalDeskDataManager::GetAllEntriesTask,
                     base::Unretained(this), status.get(), entries.get()),
      base::BindOnce(&LocalDeskDataManager::OnGetAllEntries,
                     weak_ptr_factory_.GetWeakPtr(), std::move(status),
                     std::move(entries), std::move(callback)));
}

void LocalDeskDataManager::GetEntryByUUID(
    const std::string& uuid,
    DeskModel::GetEntryByUuidCallback callback) {
  auto status = std::make_unique<DeskModel::GetEntryByUuidStatus>();
  auto entry_ptr = std::make_unique<ash::DeskTemplate*>();
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&LocalDeskDataManager::GetEntryByUuidTask,
                     base::Unretained(this), uuid, status.get(),
                     entry_ptr.get()),
      base::BindOnce(&LocalDeskDataManager::OnGetEntryByUuid,
                     weak_ptr_factory_.GetWeakPtr(), uuid, std::move(status),
                     std::move(entry_ptr), std::move(callback)));
}

void LocalDeskDataManager::AddOrUpdateEntry(
    std::unique_ptr<ash::DeskTemplate> new_entry,
    DeskModel::AddOrUpdateEntryCallback callback) {
  auto status = std::make_unique<DeskModel::AddOrUpdateEntryStatus>();
  if (cache_status_ != CacheStatus::kOk) {
    *status = DeskModel::AddOrUpdateEntryStatus::kFailure;
    std::move(callback).Run(*status);
    return;
  }

  const ash::DeskTemplateType desk_type = new_entry->type();
  size_t template_type_max_size = desk_type == ash::DeskTemplateType::kTemplate
                                      ? kMaxDeskTemplateCount
                                      : kMaxSaveAndRecallDeskCount;
  if (!g_disable_max_template_limit &&
      saved_desks_list_[desk_type].size() >= template_type_max_size) {
    *status = DeskModel::AddOrUpdateEntryStatus::kHitMaximumLimit;
    std::move(callback).Run(*status);
    return;
  }

  base::GUID uuid = new_entry->uuid();
  if (!uuid.is_valid()) {
    *status = DeskModel::AddOrUpdateEntryStatus::kInvalidArgument;
    std::move(callback).Run(*status);
    return;
  }

  // While we still find duplicate names iterate the duplicate number.  i.e.
  // if there are 4 duplicates of some template name then this iterates
  // until the current template will be named 5. Perform the conversion to
  // base::Value here so that AppRegistryCache doesn't run on the IO thread.
  while (HasEntryWithName(new_entry->template_name(), new_entry->type())) {
    new_entry->set_template_name(
        desk_template_util::AppendDuplicateNumberToDuplicateName(
            new_entry->template_name()));
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
  saved_desks_list_[desk_type][uuid] = std::move(deserialize_entry);

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&LocalDeskDataManager::AddOrUpdateEntryTask,
                     base::Unretained(this), uuid, status.get(),
                     std::move(template_base_value)),
      base::BindOnce(&LocalDeskDataManager::OnAddOrUpdateEntry,
                     weak_ptr_factory_.GetWeakPtr(), std::move(status),
                     std::move(callback)));
}

void LocalDeskDataManager::DeleteEntry(
    const std::string& uuid_str,
    DeskModel::DeleteEntryCallback callback) {
  auto status = std::make_unique<DeskModel::DeleteEntryStatus>();

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&LocalDeskDataManager::DeleteEntryTask,
                     base::Unretained(this), uuid_str, status.get()),
      base::BindOnce(&LocalDeskDataManager::OnDeleteEntry,
                     weak_ptr_factory_.GetWeakPtr(), std::move(status),
                     std::move(callback)));
}

void LocalDeskDataManager::DeleteAllEntries(
    DeskModel::DeleteEntryCallback callback) {
  auto status = std::make_unique<DeskModel::DeleteEntryStatus>();

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&LocalDeskDataManager::DeleteAllEntriesTask,
                     base::Unretained(this), status.get()),
      base::BindOnce(&LocalDeskDataManager::OnDeleteEntry,
                     weak_ptr_factory_.GetWeakPtr(), std::move(status),
                     std::move(callback)));
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
  for (const auto& save_desks : saved_desks_list_) {
    for (const auto& [uuid, template_entry] : save_desks.second) {
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

// static
void LocalDeskDataManager::SetDisableMaxTemplateLimitForTesting(bool disabled) {
  g_disable_max_template_limit = disabled;
}

// static
void LocalDeskDataManager::SetExcludeSaveAndRecallDeskInMaxEntryCountForTesting(
    bool exclude) {
  g_exclude_save_and_recall_desk_in_max_entry_count = exclude;
}

void LocalDeskDataManager::EnsureCacheIsLoaded() {
  // Cache is already loaded. Do nothing.
  if (cache_status_ == CacheStatus::kOk)
    return;
  base::DirReaderPosix user_data_dir_reader(
      user_data_dir_path_.AsUTF8Unsafe().c_str());
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  if (!user_data_dir_reader.IsValid()) {
    // User data directory path is invalid. This local storage cannot load any
    // templates from disk.
    cache_status_ = CacheStatus::kInvalidPath;
    return;
  }

  ReadFilesIntoCache();
}

void LocalDeskDataManager::GetAllEntriesTask(
    DeskModel::GetAllEntriesStatus* out_status_ptr,
    std::vector<const ash::DeskTemplate*>* out_entries_ptr) {
  EnsureCacheIsLoaded();
  if (cache_status_ == CacheStatus::kInvalidPath) {
    *out_status_ptr = DeskModel::GetAllEntriesStatus::kFailure;
    return;
  }
  for (const auto& it : policy_entries_)
    out_entries_ptr->push_back(it.get());

  for (auto& saved_desk : saved_desks_list_) {
    for (auto& [uuid, template_entry] : saved_desk.second) {
      DCHECK_EQ(uuid, template_entry->uuid());
      out_entries_ptr->push_back(template_entry.get());
    }
  }
  if (cache_status_ == CacheStatus::kOk) {
    *out_status_ptr = DeskModel::GetAllEntriesStatus::kOk;
  } else {
    *out_status_ptr = DeskModel::GetAllEntriesStatus::kPartialFailure;
  }
}

void LocalDeskDataManager::OnGetAllEntries(
    std::unique_ptr<DeskModel::GetAllEntriesStatus> status_ptr,
    std::unique_ptr<std::vector<const ash::DeskTemplate*>> entries_ptr,
    DeskModel::GetAllEntriesCallback callback) {
  std::move(callback).Run(*status_ptr, *entries_ptr);
}

void LocalDeskDataManager::GetEntryByUuidTask(
    const std::string& uuid_str,
    DeskModel::GetEntryByUuidStatus* out_status_ptr,
    ash::DeskTemplate** out_entry_ptr_ptr) {
  EnsureCacheIsLoaded();

  if (cache_status_ == LocalDeskDataManager::CacheStatus::kInvalidPath) {
    *out_status_ptr = DeskModel::GetEntryByUuidStatus::kFailure;
    return;
  }

  const base::GUID uuid = base::GUID::ParseCaseInsensitive(uuid_str);
  if (!uuid.is_valid()) {
    *out_status_ptr = DeskModel::GetEntryByUuidStatus::kInvalidUuid;
    return;
  }

  const ash::DeskTemplateType desk_type = GetDeskTypeOfUuid(uuid);

  const auto cache_entry = saved_desks_list_[desk_type].find(uuid);

  if (cache_entry != saved_desks_list_[desk_type].end()) {
    *out_status_ptr = DeskModel::GetEntryByUuidStatus::kOk;
    *out_entry_ptr_ptr = cache_entry->second.get();
  } else {
    *out_status_ptr = DeskModel::GetEntryByUuidStatus::kNotFound;
  }
}

void LocalDeskDataManager::OnGetEntryByUuid(
    const std::string& uuid_str,
    std::unique_ptr<DeskModel::GetEntryByUuidStatus> status_ptr,
    std::unique_ptr<ash::DeskTemplate*> entry_ptr_ptr,
    DeskModel::GetEntryByUuidCallback callback) {
  if (*entry_ptr_ptr == nullptr) {
    std::unique_ptr<ash::DeskTemplate> policy_entry =
        GetAdminDeskTemplateByUUID(uuid_str);

    if (policy_entry) {
      std::move(callback).Run(DeskModel::GetEntryByUuidStatus::kOk,
                              std::move(policy_entry));
    } else {
      std::move(callback).Run(*status_ptr, nullptr);
    }
  } else {
    std::move(callback).Run(*status_ptr, (*entry_ptr_ptr)->Clone());
  }
}

void LocalDeskDataManager::AddOrUpdateEntryTask(
    const base::GUID uuid,
    DeskModel::AddOrUpdateEntryStatus* out_status_ptr,
    base::Value entry_base_value) {
  EnsureCacheIsLoaded();
  const base::FilePath fully_qualified_path =
      GetFullyQualifiedPath(local_saved_desk_path_, uuid.AsLowercaseString());
  if (WriteTemplateFile(fully_qualified_path, std::move(entry_base_value))) {
    *out_status_ptr = DeskModel::AddOrUpdateEntryStatus::kOk;
  } else {
    *out_status_ptr = DeskModel::AddOrUpdateEntryStatus::kFailure;
  }
}

void LocalDeskDataManager::OnAddOrUpdateEntry(
    std::unique_ptr<DeskModel::AddOrUpdateEntryStatus> status_ptr,
    DeskModel::AddOrUpdateEntryCallback callback) {
  std::move(callback).Run(*status_ptr);
}

void LocalDeskDataManager::DeleteEntryTask(
    const std::string& uuid_str,
    DeskModel::DeleteEntryStatus* out_status_ptr) {
  EnsureCacheIsLoaded();
  if (cache_status_ == CacheStatus::kInvalidPath) {
    *out_status_ptr = DeskModel::DeleteEntryStatus::kFailure;
    return;
  }
  const base::GUID uuid = base::GUID::ParseCaseInsensitive(uuid_str);
  if (!uuid.is_valid()) {
    // There does not exist an entry with invalid UUID.
    // Therefore the deletion request is vicariously successful.
    *out_status_ptr = DeskModel::DeleteEntryStatus::kOk;
    return;
  }
  const ash::DeskTemplateType desk_type = GetDeskTypeOfUuid(uuid);
  saved_desks_list_[desk_type].erase(uuid);

  const base::FilePath fully_qualified_path =
      GetFullyQualifiedPath(local_saved_desk_path_, uuid_str);
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  if (base::DeleteFile(fully_qualified_path)) {
    *out_status_ptr = DeskModel::DeleteEntryStatus::kOk;
  } else {
    *out_status_ptr = DeskModel::DeleteEntryStatus::kFailure;
  }
}

void LocalDeskDataManager::DeleteAllEntriesTask(
    DeskModel::DeleteEntryStatus* out_status_ptr) {
  EnsureCacheIsLoaded();
  if (cache_status_ == CacheStatus::kInvalidPath) {
    *out_status_ptr = DeskModel::DeleteEntryStatus::kFailure;
    return;
  }
  // Deletes all desk templates and save and recall desks.
  for (auto& saved_desk : saved_desks_list_) {
    saved_desk.second.clear();
  }
  base::DirReaderPosix dir_reader(
      local_saved_desk_path_.AsUTF8Unsafe().c_str());

  if (!dir_reader.IsValid()) {
    *out_status_ptr = DeskModel::DeleteEntryStatus::kFailure;
    return;
  }

  DeskModel::DeleteEntryStatus overall_delete_successes =
      DeskModel::DeleteEntryStatus::kOk;

  while (dir_reader.Next()) {
    if (!IsValidTemplateFileName(dir_reader.name()))
      continue;

    base::FilePath fully_qualified_path(
        local_saved_desk_path_.Append(dir_reader.name()));
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    bool delete_success = base::DeleteFile(fully_qualified_path);
    if (!delete_success)
      overall_delete_successes = DeskModel::DeleteEntryStatus::kFailure;
  }
  *out_status_ptr = overall_delete_successes;
}

void LocalDeskDataManager::OnDeleteEntry(
    std::unique_ptr<DeskModel::DeleteEntryStatus> status_ptr,
    DeskModel::DeleteEntryCallback callback) {
  std::move(callback).Run(*status_ptr);
}

bool LocalDeskDataManager::HasEntryWithName(const std::u16string& name,
                                            ash::DeskTemplateType type) const {
  return std::find_if(
             saved_desks_list_.at(type).begin(),
             saved_desks_list_.at(type).end(),
             [&name](
                 const std::pair<const base::GUID,
                                 std::unique_ptr<ash::DeskTemplate>>& entry) {
               return entry.second->template_name() == name;
             }) != saved_desks_list_.at(type).end();
}

ash::DeskTemplateType LocalDeskDataManager::GetDeskTypeOfUuid(
    const base::GUID uuid) const {
  for (const auto& [desk_type, saved_desk] : saved_desks_list_) {
    bool found_uuid =
        std::find_if(
            saved_desk.begin(), saved_desk.end(),
            [&uuid](
                const std::pair<const base::GUID,
                                std::unique_ptr<ash::DeskTemplate>>& entry) {
              return entry.first == uuid;
            }) != saved_desk.end();
    if (found_uuid)
      return desk_type;
  }
  return ash::DeskTemplateType::kTemplate;
}

void LocalDeskDataManager::ReadFilesIntoCache() {
  // Set dir_reader to read from the `local_saved_desk_path_` directory.
  // check to make sure there is a `local_saved_desk_path_` directory. If not
  // create it.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  bool dir_create_success = base::CreateDirectory(local_saved_desk_path_);
  base::DirReaderPosix dir_reader(
      local_saved_desk_path_.AsUTF8Unsafe().c_str());

  if (!dir_create_success || !dir_reader.IsValid()) {
    // Failed to find or create the `local_saved_desk_path_` directory path.
    // This local storage cannot load any entry of `type` from disk.
    cache_status_ = CacheStatus::kInvalidPath;
    return;
  }

  while (dir_reader.Next()) {
    if (!IsValidTemplateFileName(dir_reader.name())) {
      continue;
    }

    std::unique_ptr<ash::DeskTemplate> entry =
        ReadFileToTemplate(local_saved_desk_path_.Append(dir_reader.name()));
    if (entry) {
      const base::GUID uuid = entry->uuid();
      saved_desks_list_[entry->type()][uuid] = std::move(entry);
    }
  }

  cache_status_ = CacheStatus::kOk;
}

}  // namespace desks_storage
