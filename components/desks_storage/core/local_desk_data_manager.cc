// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/local_desk_data_manager.h"

#include "base/files/dir_reader_posix.h"
#include "base/files/file_util.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/desks_storage/core/desk_model.h"
#include "components/desks_storage/core/desk_template.h"
#include "components/sync/protocol/workspace_desk_specifics.pb.h"

namespace desks_storage {

namespace {

constexpr char kFileExtension[] = ".template";

// WriteTemplateToFile is a method that takes a base::FilePath
// |path_to_template| and a DeskTemplate unique pointer |entry|
// and writes the entry out in its serialized form to the path
// represented by |path_to_template|.
//
// WARNING: This private helper function utilizes blocking calls
// and assumes that it is being called from a thread which can accept
// such calls, please don't call this function from the main thread.
bool WriteTemplateFile(const base::FilePath& path_to_template,
                       std::unique_ptr<DeskTemplate> entry) {
  std::string proto_string;
  bool string_conversion_success =
      entry->AsSyncProto().SerializeToString(&proto_string);

  if (!string_conversion_success)
    return false;

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  bool write_success = base::WriteFile(path_to_template, proto_string);

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

// returns the fully qualified path to a template file given the file path to
// the desk template directory.
base::FilePath GetFullyQualifiedPath(base::FilePath file_path,
                                     std::string uuid) {
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
    std::unique_ptr<DeskTemplate> new_entry) {
  const base::FilePath fully_qualified_path =
      GetFullyQualifiedPath(local_template_path, new_entry->uuid());

  if (WriteTemplateFile(fully_qualified_path, std::move(new_entry)))
    return DeskModel::AddOrUpdateEntryStatus::kOk;
  else
    return DeskModel::AddOrUpdateEntryStatus::kFailure;
}

struct GetEntryByUuidResult {
  DeskModel::GetEntryByUuidStatus status;
  std::unique_ptr<DeskTemplate> desk_template;
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

  std::string proto_string;
  bool read_success =
      base::ReadFileToString(fully_qualified_path, &proto_string);

  if (!read_success)
    return {DeskModel::GetEntryByUuidStatus::kFailure, nullptr};

  sync_pb::WorkspaceDeskSpecifics desk_proto;
  bool parse_success = desk_proto.ParseFromString(proto_string);

  if (!parse_success)
    return {DeskModel::GetEntryByUuidStatus::kFailure, nullptr};

  return {DeskModel::GetEntryByUuidStatus::kOk,
          DeskTemplate::FromProto(desk_proto)};
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
    std::unique_ptr<DeskTemplate> new_entry,
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