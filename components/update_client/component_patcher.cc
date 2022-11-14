// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/component_patcher.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "components/update_client/component_patcher_operation.h"
#include "components/update_client/patcher.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"

namespace update_client {

namespace {

// Deserialize the commands file (present in delta update packages). The top
// level must be a list.
absl::optional<base::Value::List> ReadCommands(
    const base::FilePath& unpack_path) {
  const base::FilePath commands =
      unpack_path.Append(FILE_PATH_LITERAL("commands.json"));
  if (!base::PathExists(commands))
    return absl::nullopt;

  JSONFileValueDeserializer deserializer(commands);
  std::unique_ptr<base::Value> root =
      deserializer.Deserialize(nullptr, nullptr);

  return (root.get() && root->is_list())
             ? absl::make_optional(std::move(*root).TakeList())
             : absl::nullopt;
}

}  // namespace

ComponentPatcher::ComponentPatcher(const base::FilePath& input_dir,
                                   const base::FilePath& unpack_dir,
                                   scoped_refptr<CrxInstaller> installer,
                                   scoped_refptr<Patcher> patcher)
    : input_dir_(input_dir),
      unpack_dir_(unpack_dir),
      installer_(installer),
      patcher_(patcher) {}

ComponentPatcher::~ComponentPatcher() = default;

void ComponentPatcher::Start(Callback callback) {
  callback_ = std::move(callback);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ComponentPatcher::StartPatching,
                                scoped_refptr<ComponentPatcher>(this)));
}

void ComponentPatcher::StartPatching() {
  commands_ = ReadCommands(input_dir_);
  if (!commands_) {
    DonePatching(UnpackerError::kDeltaBadCommands, 0);
  } else {
    next_command_ = commands_->begin();
    PatchNextFile();
  }
}

void ComponentPatcher::PatchNextFile() {
  if (next_command_ == commands_->end()) {
    DonePatching(UnpackerError::kNone, 0);
    return;
  }
  if (!next_command_->is_dict()) {
    DonePatching(UnpackerError::kDeltaBadCommands, 0);
    return;
  }
  const base::Value::Dict& command_args = next_command_->GetDict();

  if (const std::string* operation = command_args.FindString(kOp)) {
    current_operation_ = CreateDeltaUpdateOp(*operation, patcher_);
  }

  if (!current_operation_) {
    DonePatching(UnpackerError::kDeltaUnsupportedCommand, 0);
    return;
  }
  current_operation_->Run(
      command_args, input_dir_, unpack_dir_, installer_,
      base::BindOnce(&ComponentPatcher::DonePatchingFile,
                     scoped_refptr<ComponentPatcher>(this)));
}

void ComponentPatcher::DonePatchingFile(UnpackerError error,
                                        int extended_error) {
  if (error != UnpackerError::kNone) {
    DonePatching(error, extended_error);
  } else {
    ++next_command_;
    PatchNextFile();
  }
}

void ComponentPatcher::DonePatching(UnpackerError error, int extended_error) {
  current_operation_ = nullptr;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_), error, extended_error));
}

}  // namespace update_client
