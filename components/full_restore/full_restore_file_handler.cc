// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/full_restore/full_restore_file_handler.h"

#include "base/files/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/full_restore/restore_data.h"

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
    std::unique_ptr<RestoreData> restore_data) {
  if (!restore_data)
    return;

  std::string json_string;
  JSONStringValueSerializer serializer(&json_string);
  serializer.Serialize(restore_data->ConvertToValue());

  WriteDataBlocking(json_string);
}

FullRestoreFileHandler::~FullRestoreFileHandler() = default;

void FullRestoreFileHandler::WriteDataBlocking(
    const std::string& full_restore_data) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  bool write_success = base::WriteFile(file_path_, full_restore_data.c_str(),
                                       full_restore_data.size()) != -1;
  if (!write_success)
    DVLOG(0) << "Fail to write full restore data to " << file_path_;
}

}  // namespace full_restore
