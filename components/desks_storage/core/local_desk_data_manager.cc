// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/local_desk_data_manager.h"

#include "ash/public/cpp/desk_template.h"
#include "base/files/dir_reader_posix.h"
#include "base/files/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/util/values/values_util.h"
#include "base/values.h"
#include "components/desks_storage/core/desk_model.h"
#include "components/desks_storage/core/desk_template.h"
#include "components/full_restore/restore_data.h"
#include "components/sync/protocol/workspace_desk_specifics.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace desks_storage {

namespace {

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

// Function for converting ash::DeskTemplates to base::Value for serialization.
base::Value ConvertDeskTemplateToValue(ash::DeskTemplate* desk_template) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey(kDeskTemplateUuidKey,
              base::Value(desk_template->uuid().AsLowercaseString()));
  dict.SetKey(kDeskTemplateNameKey,
              base::Value(desk_template->template_name()));
  dict.SetKey(kDeskTemplateTimeCreatedKey,
              util::TimeToValue(desk_template->created_time()));
  DCHECK(desk_template->desk_restore_data() != nullptr);
  dict.SetKey(kDeskTemplateRestoreDataKey,
              desk_template->desk_restore_data()->ConvertToValue());
  return dict;
}

// Function for converting base::Values deserialized from template files as
// ash::DeskTemplates.
std::unique_ptr<ash::DeskTemplate> ConvertDeskTemplateValueToDeskTemplate(
    base::Value& desk_template_value) {
  absl::optional<base::Time> created_time(util::ValueToTime(
      desk_template_value.FindKey(kDeskTemplateTimeCreatedKey)));
  if (!created_time)
    return nullptr;

  std::string* uuid = desk_template_value.FindStringKey(kDeskTemplateUuidKey);
  if (!uuid)
    return nullptr;

  std::string* name = desk_template_value.FindStringKey(kDeskTemplateNameKey);
  if (!name)
    return nullptr;

  std::unique_ptr<ash::DeskTemplate> converted_value =
      std::make_unique<ash::DeskTemplate>(*uuid, *name, created_time.value());

  // Full Restore will only take in std::unique_ptr as it's constructor
  // parameter from base::Value.  We're not allowed to use the explicit
  // std::unique_ptr constructor so this is how we wrap the base::Value in a
  // std::unique_ptr
  std::unique_ptr<base::Value> restore_data_value_ptr =
      base::Value::ToUniquePtrValue(
          std::move(*desk_template_value.FindKey(kDeskTemplateRestoreDataKey)));
  DCHECK(restore_data_value_ptr);

  converted_value->set_desk_restore_data(
      std::make_unique<full_restore::RestoreData>(
          std::move(restore_data_value_ptr)));
  return converted_value;
}

// WriteTemplateToFile is a method that takes a base::FilePath
// |path_to_template| and a DeskTemplate unique pointer |entry|
// and writes the entry out in its serialized form to the path
// represented by |path_to_template|.
//
// WARNING: This private helper function utilizes blocking calls
// and assumes that it is being called from a thread which can accept
// such calls, please don't call this function from the main thread.
bool WriteTemplateFile(const base::FilePath& path_to_template,
                       std::unique_ptr<ash::DeskTemplate> entry) {
  std::string json_string;
  JSONStringValueSerializer serializer(&json_string);
  serializer.Serialize(ConvertDeskTemplateToValue(entry.get()));

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  bool write_success = base::WriteFile(path_to_template, json_string);

  if (!write_success)
    return false;

  return true;
}

// AppendTemplateUUID appends a std::string |entry| to a std::vector of uuids.
// in order to be appended |entry| must contain .template within the string.
// This method populates the std::vector as a side effect and has a void return
// type hence |out_uuids|.
void AppendTemplateUUID(const std::string& entry,
                        std::vector<std::string>* out_uuids) {
  const size_t extension_at = entry.find(kFileExtension);

  if (extension_at == std::string::npos)
    return;

  out_uuids->push_back(entry.substr(0, extension_at));
}

// Returns the fully qualified path to a template file given the file path to
// the desk template directory.
base::FilePath GetFullyQualifiedPath(base::FilePath file_path,
                                     const std::string& uuid) {
  std::string filename(uuid);
  filename.append(kFileExtension);
  return base::FilePath(
      file_path.Append(base::FilePath::StringPieceType(filename.c_str())));
}

struct GetAllUuidsResult {
  DeskModel::GetAllUuidsStatus status;
  std::vector<std::string> uuids;
};

// This method gets all UUIDs available in the template directory.  This
// is a task that is posted to the local storage object's task runner.
GetAllUuidsResult GetAllUuidsTask(const base::FilePath local_template_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::DirReaderPosix dir_reader(local_template_path.AsUTF8Unsafe().c_str());
  if (!dir_reader.IsValid()) {
    return {DeskModel::GetAllUuidsStatus::kFailure, {}};
  }

  std::vector<std::string> uuids;
  while (dir_reader.Next()) {
    if (dir_reader.name() == nullptr)
      continue;

    AppendTemplateUUID(std::string(dir_reader.name()), &uuids);
  }

  return {DeskModel::GetAllUuidsStatus::kOk, std::move(uuids)};
}

// Handles GetAllUuidsTask and calls the callback with the result.
void HandleGetAllUuidsTask(DeskModel::GetAllUuidsCallback callback,
                           GetAllUuidsResult result) {
  std::move(callback).Run(result.status, std::move(result.uuids));
}

// Adds or updates an entry. This is a task that is posted to base::ThreadPool
// in order to complete io operations.
DeskModel::AddOrUpdateEntryStatus AddOrUpdateEntryTask(
    const base::FilePath local_template_path,
    std::unique_ptr<ash::DeskTemplate> new_entry) {
  const base::FilePath fully_qualified_path = GetFullyQualifiedPath(
      local_template_path, new_entry->uuid().AsLowercaseString());

  if (WriteTemplateFile(fully_qualified_path, std::move(new_entry)))
    return DeskModel::AddOrUpdateEntryStatus::kOk;
  else
    return DeskModel::AddOrUpdateEntryStatus::kFailure;
}

struct GetEntryByUuidResult {
  DeskModel::GetEntryByUuidStatus status;
  std::unique_ptr<ash::DeskTemplate> desk_template;
};

// This method Handles getting the task of getting an entry by it's Uuid. Unlike
// the other statuses this function returns the DeskTemplate pointer instead of
// a status.  This is because this method has to instantiate the DeskTemplate
// itself in order to use the DeskTemplate::FromProto factory method.
GetEntryByUuidResult GetEntryByUuidTask(
    const base::FilePath local_template_path,
    const std::string& uuid) {
  const base::FilePath fully_qualified_path =
      GetFullyQualifiedPath(local_template_path, uuid);

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  if (!base::PathExists(fully_qualified_path))
    return {DeskModel::GetEntryByUuidStatus::kNotFound, nullptr};

  std::string value_string;
  bool read_success =
      base::ReadFileToString(fully_qualified_path, &value_string);
  if (!read_success)
    return {DeskModel::GetEntryByUuidStatus::kFailure, nullptr};

  std::string error_message;
  int error_code;
  JSONStringValueDeserializer deserializer(value_string);
  auto desk_template_value =
      deserializer.Deserialize(&error_code, &error_message);

  if (!desk_template_value) {
    DVLOG(0) << "Fail to deserialize json value from string with error code: "
             << error_code << " and error message: " << error_message;
    return {DeskModel::GetEntryByUuidStatus::kFailure, nullptr};
  }

  return {DeskModel::GetEntryByUuidStatus::kOk,
          ConvertDeskTemplateValueToDeskTemplate(*desk_template_value)};
}

// Handles replies from |GetEntryByUuidTask| and calls callback.
void HandleGetEntryByUuidTask(DeskModel::GetEntryByUuidCallback callback,
                              GetEntryByUuidResult result) {
  std::move(callback).Run(result.status, std::move(result.desk_template));
}

// This task deletes a single entry keyed by its |uuid|.
DeskModel::DeleteEntryStatus DeleteSingleEntryTask(
    const base::FilePath local_file_path,
    const std::string& uuid) {
  const base::FilePath fully_qualified_path =
      GetFullyQualifiedPath(local_file_path, uuid);

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  if (base::DeleteFile(fully_qualified_path))
    return DeskModel::DeleteEntryStatus::kOk;

  return DeskModel::DeleteEntryStatus::kFailure;
}

// Deletes all entries.
DeskModel::DeleteEntryStatus DeleteAllEntriesTask(
    const base::FilePath local_file_path) {
  base::DirReaderPosix dir_reader(local_file_path.AsUTF8Unsafe().c_str());

  if (!dir_reader.IsValid())
    return DeskModel::DeleteEntryStatus::kFailure;

  DeskModel::DeleteEntryStatus overall_delete_successes =
      DeskModel::DeleteEntryStatus::kOk;
  while (dir_reader.Next()) {
    if (dir_reader.name() == nullptr)
      continue;

    std::string filename(dir_reader.name());
    size_t extension_at = filename.find(kFileExtension);

    if (extension_at == std::string::npos)
      continue;

    base::FilePath fully_qualified_path(local_file_path.Append(filename));
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    bool delete_success = base::DeleteFile(fully_qualified_path);
    if ((overall_delete_successes == DeskModel::DeleteEntryStatus::kOk) &&
        !delete_success)
      overall_delete_successes = DeskModel::DeleteEntryStatus::kFailure;
  }

  return overall_delete_successes;
}

}  // namespace

LocalDeskDataManager::LocalDeskDataManager(const base::FilePath& path)
    : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      local_path_(path) {}

LocalDeskDataManager::~LocalDeskDataManager() = default;

void LocalDeskDataManager::AddOrUpdateEntry(
    std::unique_ptr<ash::DeskTemplate> new_entry,
    DeskModel::AddOrUpdateEntryCallback callback) {
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&AddOrUpdateEntryTask, base::FilePath(local_path_),
                     std::move(new_entry)),
      std::move(callback));
}

void LocalDeskDataManager::GetAllUuids(
    DeskModel::GetAllUuidsCallback callback) {
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&GetAllUuidsTask, base::FilePath(local_path_)),
      base::BindOnce(&HandleGetAllUuidsTask, std::move(callback)));
}

void LocalDeskDataManager::GetEntryByUUID(
    const std::string& uuid,
    DeskModel::GetEntryByUuidCallback callback) {
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&GetEntryByUuidTask, base::FilePath(local_path_), uuid),
      base::BindOnce(&HandleGetEntryByUuidTask, std::move(callback)));
}

void LocalDeskDataManager::DeleteEntry(
    const std::string& uuid,
    DeskModel::DeleteEntryCallback callback) {
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&DeleteSingleEntryTask, base::FilePath(local_path_), uuid),
      std::move(callback));
}

void LocalDeskDataManager::DeleteAllEntries(
    DeskModel::DeleteEntryCallback callback) {
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&DeleteAllEntriesTask, base::FilePath(local_path_)),
      std::move(callback));
}

}  // namespace desks_storage