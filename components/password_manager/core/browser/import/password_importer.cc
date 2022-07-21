// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/import/password_importer.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "components/password_manager/core/browser/import/csv_password.h"
#include "components/password_manager/core/browser/import/csv_password_sequence.h"
#include "components/password_manager/services/csv_password/csv_password_parser_service.h"
#include "components/sync/base/bind_to_task_runner.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace password_manager {

namespace {

// Preferred filename extension for the imported files.
const base::FilePath::CharType kFileExtension[] = FILE_PATH_LITERAL("csv");

// Limiting the file size to 150 KB: a limit is introduced to limit the
// number of passwords and limit the amount of data that can be displayed in
// memory to preview the content of the import in a single run.
const int32_t kMaxFileSizeBytes = 150 * 1024;

// Reads and returns a status and the contents of the file at |path| as a
// optional string. The string will be present if the status is SUCCESS.
base::expected<std::string, PasswordImporter::Status> ReadFileToString(
    const base::FilePath& path) {
  int64_t file_size;
  if (GetFileSize(path, &file_size) && file_size > kMaxFileSizeBytes)
    return base::unexpected(PasswordImporter::Status::LARGE_FILE);

  std::string contents;
  if (!base::ReadFileToString(path, &contents))
    return base::unexpected(PasswordImporter::Status::IO_ERROR);

  return std::move(contents);
}

}  // namespace

PasswordImporter::PasswordImporter() = default;

PasswordImporter::~PasswordImporter() = default;

const mojo::Remote<mojom::CSVPasswordParser>& PasswordImporter::GetParser() {
  if (!parser_) {
    parser_ = LaunchCSVPasswordParser();
    parser_.reset_on_disconnect();
  }
  return parser_;
}

PasswordImporter::Status PasswordImporter::GetStatus() const {
  return status_;
}

void PasswordImporter::ParseCSVPasswordsInSandbox(
    PasswordImporter::CompletionCallback completion,
    base::expected<std::string, PasswordImporter::Status> result) {
  // Currently, CSV is the only supported format.
  if (!result.has_value()) {
    this->status_ = result.error();
    std::move(completion).Run(nullptr);
  } else {
    this->status_ = PasswordImporter::Status::SUCCESS;
    GetParser()->ParseCSV(std::move(result.value()), std::move(completion));
  }
}

void PasswordImporter::Import(const base::FilePath& path,
                              CompletionCallback completion) {
  // Posting with USER_VISIBLE priority, because the result of the import is
  // visible to the user in the password settings page.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&ReadFileToString, path),
      base::BindOnce(&PasswordImporter::ParseCSVPasswordsInSandbox,
                     weak_ptr_factory_.GetWeakPtr(), std::move(completion)));
}

void PasswordImporter::SetServiceForTesting(
    mojo::PendingRemote<mojom::CSVPasswordParser> parser) {
  parser_.Bind(std::move(parser));
}

// static
std::vector<std::vector<base::FilePath::StringType>>
PasswordImporter::GetSupportedFileExtensions() {
  return std::vector<std::vector<base::FilePath::StringType>>(
      1, std::vector<base::FilePath::StringType>(1, kFileExtension));
}

}  // namespace password_manager
