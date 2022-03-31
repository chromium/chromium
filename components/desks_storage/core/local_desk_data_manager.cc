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
#include "components/app_restore/restore_data.h"
#include "components/desks_storage/core/desk_model.h"
#include "components/desks_storage/core/desk_template_conversion.h"
#include "components/desks_storage/core/desk_template_util.h"
#include "components/sync/protocol/workspace_desk_specifics.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/re2/src/re2/stringpiece.h"
#include "url/gurl.h"

namespace desks_storage {

namespace {

// Setting this to true allows us to add more than the maximum number of
// templates. Used only for testing.
bool g_disable_max_template_limit = false;

// File extension for templates.
constexpr char kFileExtension[] = ".template";
// Key used in base::Value generation for the template name field.
constexpr char kDeskTemplateNameKey[] = "template_name";
// Key used in base::Value generation for the uuid field.
constexpr char kDeskTemplateUuidKey[] = "uuid";
// Key used in base::Value generation for the time created field.
constexpr char kDeskTemplateTimeCreatedKey[] = "time_created";
// Key used in base::Value generation for the restore data field.
constexpr char kDeskTemplateRestoreDataKey[] = "restore_data";
constexpr std::size_t kMaxTemplateCount = 6u;

// Converts ash::DeskTemplates to base::Value for serialization.
base::Value ConvertDeskTemplateToValue(ash::DeskTemplate* desk_template) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey(kDeskTemplateUuidKey,
              base::Value(desk_template->uuid().AsLowercaseString()));
  dict.SetKey(kDeskTemplateNameKey,
              base::Value(desk_template->template_name()));
  dict.SetKey(kDeskTemplateTimeCreatedKey,
              base::TimeToValue(desk_template->created_time()));
  DCHECK(desk_template->desk_restore_data() != nullptr);
  dict.SetKey(kDeskTemplateRestoreDataKey,
              desk_template->desk_restore_data()->ConvertToValue());
  return dict;
}

// Converts base::Values deserialized from template files to
// |ash::DeskTemplate|.
std::unique_ptr<ash::DeskTemplate> ConvertValueToDeskTemplate(
    base::Value& desk_template_value) {
  absl::optional<base::Time> created_time(base::ValueToTime(
      desk_template_value.FindKey(kDeskTemplateTimeCreatedKey)));
  if (!created_time)
    return nullptr;

  std::string* uuid = desk_template_value.FindStringKey(kDeskTemplateUuidKey);
  if (!uuid)
    return nullptr;

  std::string* name = desk_template_value.FindStringKey(kDeskTemplateNameKey);
  if (!name)
    return nullptr;

  std::unique_ptr<ash::DeskTemplate> desk_template =
      std::make_unique<ash::DeskTemplate>(*uuid, ash::DeskTemplateSource::kUser,
                                          *name, created_time.value());

  // Full Restore will only take in std::unique_ptr as it's constructor
  // parameter from base::Value.  We're not allowed to use the explicit
  // std::unique_ptr constructor so this is how we wrap the base::Value in a
  // std::unique_ptr
  std::unique_ptr<base::Value> restore_data_value_ptr =
      base::Value::ToUniquePtrValue(
          std::move(*desk_template_value.FindKey(kDeskTemplateRestoreDataKey)));
  DCHECK(restore_data_value_ptr);

  desk_template->set_desk_restore_data(
      std::make_unique<app_restore::RestoreData>(
          std::move(restore_data_value_ptr)));
  return desk_template;
}

// Reads a template file at fully_qualified_path| into a
// std::unique_ptr<ash::DeskTemplate> This function returns a |nullptr| if the
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

  return ConvertValueToDeskTemplate(*desk_template_value);
}

bool EndsWith(const char* input, const char* suffix) {
  size_t input_length = strlen(input);
  size_t suffix_length = strlen(suffix);
  if (suffix_length <= input_length) {
    return strcmp(input + input_length - suffix_length, suffix) == 0;
  }
  return false;
}

bool IsValidTemplateFileName(const char* name) {
  if (name == nullptr)
    return false;
  return EndsWith(name, kFileExtension);
}

// Writes a DeskTemplate |entry| to a file at |path_to_template|.
// This function utilizes blocking calls and assumes that it is being called
// from a thread which can accept such calls, please don't call this function
// from the UI thread.
bool WriteTemplateFile(const base::FilePath& path_to_template,
                       ash::DeskTemplate* entry) {
  std::string json_string;
  JSONStringValueSerializer serializer(&json_string);
  serializer.Serialize(ConvertDeskTemplateToValue(entry));

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  return base::WriteFile(path_to_template, json_string);
}

// Generates the fully qualified path to a template file given the |file_path|
// to the desk template directory and the template's |uuid|.
base::FilePath GetFullyQualifiedPath(base::FilePath file_path,
                                     const std::string& uuid) {
  std::string filename(uuid);
  filename.append(kFileExtension);
  return base::FilePath(file_path.Append(base::FilePath(filename)));
}

}  // namespace

LocalDeskDataManager::LocalDeskDataManager(const base::FilePath& path)
    : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      local_path_(path),
      cache_status_(CacheStatus::kNotInitialized) {}

LocalDeskDataManager::~LocalDeskDataManager() = default;

void LocalDeskDataManager::GetAllEntries(
    DeskModel::GetAllEntriesCallback callback) {
  auto status = std::make_unique<DeskModel::GetAllEntriesStatus>();
  auto entries = std::make_unique<std::vector<ash::DeskTemplate*>>();

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

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&LocalDeskDataManager::AddOrUpdateEntryTask,
                     base::Unretained(this), std::move(new_entry),
                     status.get()),
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

std::size_t LocalDeskDataManager::GetEntryCount() const {
  return templates_.size() + policy_entries_.size();
}

std::size_t LocalDeskDataManager::GetMaxEntryCount() const {
  return kMaxTemplateCount + policy_entries_.size();
}

std::vector<base::GUID> LocalDeskDataManager::GetAllEntryUuids() const {
  std::vector<base::GUID> keys;
  for (const auto& it : templates_) {
    DCHECK_EQ(it.first, it.second->uuid());
    keys.emplace_back(it.first);
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

void LocalDeskDataManager::EnsureCacheIsLoaded() {
  if (cache_status_ == CacheStatus::kOk) {
    // Cache is already loaded.
    return;
  }

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::DirReaderPosix dir_reader(local_path_.AsUTF8Unsafe().c_str());
  if (!dir_reader.IsValid()) {
    // Template directory path is invalid. This local storage cannot load any
    // templates from disk.
    cache_status_ = CacheStatus::kInvalidPath;
    return;
  }

  while (dir_reader.Next()) {
    if (!IsValidTemplateFileName(dir_reader.name())) {
      continue;
    }

    std::unique_ptr<ash::DeskTemplate> desk_template =
        ReadFileToTemplate(local_path_.Append(dir_reader.name()));
    if (desk_template) {
      const base::GUID uuid = desk_template->uuid();
      templates_[uuid] = std::move(desk_template);
    }
  }

  cache_status_ = CacheStatus::kOk;
}

void LocalDeskDataManager::GetAllEntriesTask(
    DeskModel::GetAllEntriesStatus* status_ptr,
    std::vector<ash::DeskTemplate*>* entries_ptr) {
  EnsureCacheIsLoaded();
  if (cache_status_ == CacheStatus::kInvalidPath) {
    *status_ptr = DeskModel::GetAllEntriesStatus::kFailure;
    return;
  }

  for (const auto& it : policy_entries_)
    entries_ptr->push_back(it.get());

  for (auto& it : templates_) {
    DCHECK_EQ(it.first, it.second->uuid());
    entries_ptr->push_back(it.second.get());
  }

  if (cache_status_ == CacheStatus::kOk) {
    *status_ptr = DeskModel::GetAllEntriesStatus::kOk;
  } else {
    *status_ptr = DeskModel::GetAllEntriesStatus::kPartialFailure;
  }
}

void LocalDeskDataManager::OnGetAllEntries(
    std::unique_ptr<DeskModel::GetAllEntriesStatus> status_ptr,
    std::unique_ptr<std::vector<ash::DeskTemplate*>> entries_ptr,
    DeskModel::GetAllEntriesCallback callback) {
  std::move(callback).Run(*status_ptr, *entries_ptr);
}

void LocalDeskDataManager::GetEntryByUuidTask(
    const std::string& uuid_str,
    DeskModel::GetEntryByUuidStatus* status_ptr,
    ash::DeskTemplate** entry_ptr_ptr) {
  EnsureCacheIsLoaded();

  if (cache_status_ == CacheStatus::kInvalidPath) {
    *status_ptr = DeskModel::GetEntryByUuidStatus::kFailure;
    return;
  }

  const base::GUID uuid = base::GUID::ParseCaseInsensitive(uuid_str);
  if (!uuid.is_valid()) {
    *status_ptr = DeskModel::GetEntryByUuidStatus::kInvalidUuid;
    return;
  }

  const auto cache_entry = templates_.find(uuid);

  if (cache_entry != templates_.end()) {
    *status_ptr = DeskModel::GetEntryByUuidStatus::kOk;
    *entry_ptr_ptr = cache_entry->second.get();
  } else {
    *status_ptr = DeskModel::GetEntryByUuidStatus::kNotFound;
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
    std::unique_ptr<ash::DeskTemplate> new_entry,
    DeskModel::AddOrUpdateEntryStatus* status_ptr) {
  EnsureCacheIsLoaded();
  if (cache_status_ == CacheStatus::kInvalidPath) {
    *status_ptr = DeskModel::AddOrUpdateEntryStatus::kFailure;
    return;
  }

  if (!g_disable_max_template_limit && templates_.size() >= kMaxTemplateCount) {
    *status_ptr = DeskModel::AddOrUpdateEntryStatus::kHitMaximumLimit;
    return;
  }

  if (!new_entry->uuid().is_valid()) {
    *status_ptr = DeskModel::AddOrUpdateEntryStatus::kInvalidArgument;
    return;
  }

  base::GUID uuid = new_entry->uuid();

  // While we still find duplicate names iterate the duplicate number.  i.e.
  // if there are 4 duplicates of some template name then this iterates until
  // the current template will be named 5.
  while (HasTemplateWithName(new_entry->template_name())) {
    new_entry->set_template_name(
        desk_template_util::AppendDuplicateNumberToDuplicateName(
            new_entry->template_name()));
  }

  templates_[uuid] = std::move(new_entry);

  const base::FilePath fully_qualified_path =
      GetFullyQualifiedPath(local_path_, uuid.AsLowercaseString());

  if (WriteTemplateFile(fully_qualified_path, templates_[uuid].get())) {
    *status_ptr = DeskModel::AddOrUpdateEntryStatus::kOk;
  } else {
    *status_ptr = DeskModel::AddOrUpdateEntryStatus::kFailure;
  }
}

void LocalDeskDataManager::OnAddOrUpdateEntry(
    std::unique_ptr<DeskModel::AddOrUpdateEntryStatus> status_ptr,
    DeskModel::AddOrUpdateEntryCallback callback) {
  std::move(callback).Run(*status_ptr);
}

void LocalDeskDataManager::DeleteEntryTask(
    const std::string& uuid_str,
    DeskModel::DeleteEntryStatus* status_ptr) {
  EnsureCacheIsLoaded();
  if (cache_status_ == LocalDeskDataManager::CacheStatus::kInvalidPath) {
    *status_ptr = DeskModel::DeleteEntryStatus::kFailure;
    return;
  }

  const base::GUID uuid = base::GUID::ParseCaseInsensitive(uuid_str);
  if (!uuid.is_valid()) {
    // There does not exist a desk template with invalid UUID.
    // Therefore the deletion request is vicariously successful.
    *status_ptr = DeskModel::DeleteEntryStatus::kOk;
    return;
  }

  templates_.erase(uuid);

  const base::FilePath fully_qualified_path =
      GetFullyQualifiedPath(local_path_, uuid_str);
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  if (base::DeleteFile(fully_qualified_path)) {
    *status_ptr = DeskModel::DeleteEntryStatus::kOk;
  } else {
    *status_ptr = DeskModel::DeleteEntryStatus::kFailure;
  }
}

void LocalDeskDataManager::DeleteAllEntriesTask(
    DeskModel::DeleteEntryStatus* status_ptr) {
  EnsureCacheIsLoaded();
  if (cache_status_ == CacheStatus::kInvalidPath) {
    *status_ptr = DeskModel::DeleteEntryStatus::kFailure;
    return;
  }

  templates_.clear();

  base::DirReaderPosix dir_reader(local_path_.AsUTF8Unsafe().c_str());

  if (!dir_reader.IsValid()) {
    *status_ptr = DeskModel::DeleteEntryStatus::kFailure;
    return;
  }

  DeskModel::DeleteEntryStatus overall_delete_successes =
      DeskModel::DeleteEntryStatus::kOk;
  while (dir_reader.Next()) {
    if (!IsValidTemplateFileName(dir_reader.name())) {
      continue;
    }

    base::FilePath fully_qualified_path(local_path_.Append(dir_reader.name()));
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    bool delete_success = base::DeleteFile(fully_qualified_path);
    if ((overall_delete_successes == DeskModel::DeleteEntryStatus::kOk) &&
        !delete_success)
      overall_delete_successes = DeskModel::DeleteEntryStatus::kFailure;
  }

  *status_ptr = overall_delete_successes;
}

void LocalDeskDataManager::OnDeleteEntry(
    std::unique_ptr<DeskModel::DeleteEntryStatus> status_ptr,
    DeskModel::DeleteEntryCallback callback) {
  std::move(callback).Run(*status_ptr);
}

bool LocalDeskDataManager::HasTemplateWithName(const std::u16string& name) {
  return std::find_if(
             templates_.begin(), templates_.end(),
             [&name](std::pair<const base::GUID,
                               std::unique_ptr<ash::DeskTemplate>>& entry) {
               return entry.second->template_name() == name;
             }) != templates_.end();
}

}  // namespace desks_storage
