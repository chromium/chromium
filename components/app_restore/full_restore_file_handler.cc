// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/app_restore/full_restore_file_handler.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/app_restore/restore_data.h"

namespace full_restore {

namespace {

const base::FilePath::CharType kFullRestoreDataDirname[] =
    FILE_PATH_LITERAL("FullRestoreData");

}  // namespace

FullRestoreFileHandler::FullRestoreFileHandler(const base::FilePath& path)
    : RefCountedDeleteOnSequence(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      file_path_(path.Append(kFullRestoreDataDirname)) {}

void FullRestoreFileHandler::WriteToFile(
    std::unique_ptr<app_restore::RestoreData> restore_data) {
  if (!restore_data)
    return;

  std::string json_string;
  JSONStringValueSerializer serializer(&json_string);
  serializer.Serialize(restore_data->ConvertToValue());

  WriteDataBlocking(json_string);
}

std::unique_ptr<app_restore::RestoreData>
FullRestoreFileHandler::ReadFromFile() {
  if (!base::PathExists(file_path_))
    return std::make_unique<app_restore::RestoreData>();

  std::string full_restore_data;
  if (!ReadDataBlocking(full_restore_data) || full_restore_data.empty())
    return nullptr;

  // This JSON file is written by Chrome, so it is safe to deserialise it
  // in-process.
  base::JSONReader::Result full_restore_value =
      base::JSONReader::ReadAndReturnValueWithError(full_restore_data);
  if (!full_restore_value.has_value()) {
    DVLOG(0)
        << "Fail to deserialize json value from string with error message: "
        << full_restore_value.error().message << ", in line "
        << full_restore_value.error().line << ", column "
        << full_restore_value.error().column;
    return nullptr;
  }

  return std::make_unique<app_restore::RestoreData>(
      std::move(*full_restore_value));
}

FullRestoreFileHandler::~FullRestoreFileHandler() = default;

void FullRestoreFileHandler::WriteDataBlocking(
    const std::string& full_restore_data) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  if (!base::ImportantFileWriter::WriteFileAtomically(file_path_,
                                                      full_restore_data)) {
    LOG(ERROR) << "Fail to write full restore data to " << file_path_;
  }
}

bool FullRestoreFileHandler::ReadDataBlocking(std::string& full_restore_data) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  return base::ReadFileToString(file_path_, &full_restore_data);
}

}  // namespace full_restore
