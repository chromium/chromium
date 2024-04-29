// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/common/connections_manager/nearby_file_handler.h"

#include "base/files/file_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_types.mojom.h"

namespace {

scoped_refptr<base::SequencedTaskRunner> CreateFileTaskRunner() {
  // The tasks posted to this sequenced task runner do synchronous File I/O for
  // the purposes of opening and closing files for Nearby Share file payloads.
  return base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
}

// Called on the FileTaskRunner to actually open the files passed.
std::vector<NearbyFileHandler::FileInfo> DoOpenFiles(
    std::vector<base::FilePath> file_paths) {
  std::vector<NearbyFileHandler::FileInfo> files;
  for (const auto& path : file_paths) {
    base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    if (!file.IsValid())
      return {};

    int64_t size = file.GetLength();
    if (size < 0)
      return {};

    files.push_back({size, std::move(file)});
  }
  return files;
}

NearbyFileHandler::CreateFileResult DoCreateFile(base::FilePath file_path) {
  NearbyFileHandler::CreateFileResult result;
  result.output_file.Initialize(
      file_path,
      base::File::Flags::FLAG_CREATE_ALWAYS | base::File::Flags::FLAG_WRITE);
  result.input_file.Initialize(
      file_path, base::File::Flags::FLAG_OPEN | base::File::Flags::FLAG_READ);
  return result;
}

}  // namespace

NearbyFileHandler::NearbyFileHandler() : task_runner_(CreateFileTaskRunner()) {}

NearbyFileHandler::~NearbyFileHandler() = default;

void NearbyFileHandler::OpenFiles(std::vector<base::FilePath> file_paths,
                                  OpenFilesCallback callback) {
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&DoOpenFiles, std::move(file_paths)),
      std::move(callback));
}

void NearbyFileHandler::GetUniquePath(const base::FilePath& file_path,
                                      GetUniquePathCallback callback) {
  // TODO(crbug.com/40132032) - Confirm if this should be run on
  // DownloadManager's task runner.
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&base::GetUniquePath, file_path),
      std::move(callback));
}

void NearbyFileHandler::ReleaseFilePayloads(std::vector<PayloadPtr> payloads) {
  auto files = std::make_unique<std::vector<base::File>>();
  for (auto& payload : payloads) {
    if (payload->content->is_file())
      files->push_back(std::move(payload->content->get_file()->file));
  }
  if (!files->empty())
    task_runner_->DeleteSoon(FROM_HERE, std::move(files));
}

void NearbyFileHandler::CreateFile(const base::FilePath& file_path,
                                   CreateFileCallback callback) {
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&DoCreateFile, file_path), std::move(callback));
}

void NearbyFileHandler::DeleteFilesFromDisk(std::vector<base::FilePath> paths) {
  task_runner_->PostTask(FROM_HERE, base::BindOnce(
                                        [](std::vector<base::FilePath> paths) {
                                          for (const auto& path : paths)
                                            base::DeleteFile(path);
                                        },
                                        std::move(paths)));
}
