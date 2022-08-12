// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/import/password_importer.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "components/password_manager/core/browser/import/csv_password.h"
#include "components/password_manager/core/browser/import/csv_password_sequence.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
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

void AddCredentialsCallback(
    const base::Time& start_time,
    const std::vector<SavedPasswordsPresenter::AddResult>& results) {
  size_t success_count = base::ranges::count_if(
      results, [](SavedPasswordsPresenter::AddResult result) {
        return result == SavedPasswordsPresenter::AddResult::kSuccess;
      });
  UMA_HISTOGRAM_COUNTS_1M("PasswordManager.ImportedPasswordsPerUserInCSV",
                          success_count);

  base::UmaHistogramLongTimes("PasswordManager.ImportDuration",
                              base::Time::Now() - start_time);
}

}  // namespace

PasswordImporter::PasswordImporter(SavedPasswordsPresenter* presenter)
    : presenter_(presenter) {}

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
                              password_manager::PasswordForm::Store to_store,
                              ImportResultsCallback results_callback) {
  results_callback_ = std::move(results_callback);
  to_store_ = to_store;
  selected_file_name_ = path.BaseName().AsUTF8Unsafe();

  // Posting with USER_VISIBLE priority, because the result of the import is
  // visible to the user in the password settings page.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&ReadFileToString, path),
      base::BindOnce(&PasswordImporter::ParseCSVPasswordsInSandbox,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::BindOnce(&PasswordImporter::ConsumePasswords,
                                    weak_ptr_factory_.GetWeakPtr())));
}

void PasswordImporter::ConsumePasswords(
    password_manager::mojom::CSVPasswordSequencePtr seq) {
  password_manager::ImportResults results;
  results.file_name = std::move(selected_file_name_);

  if (!seq) {
    // TODO(crbug/1325290): Compute correct status.
    results.status = password_manager::ImportResults::Status::IO_ERROR;
    std::move(results_callback_).Run(results);
    return;
  }

  base::Time start_time = base::Time::Now();

  std::vector<password_manager::CredentialUIEntry> credentials;
  credentials.reserve(seq->csv_passwords.size());

  base::ranges::transform(
      seq->csv_passwords, std::back_inserter(credentials),
      [this](const password_manager::CSVPassword& csv_password) {
        return password_manager::CredentialUIEntry(csv_password, to_store_);
      });

  presenter_->AddCredentials(
      credentials, password_manager::PasswordForm::Type::kImported,
      base::BindOnce(&AddCredentialsCallback, start_time));

  // TODO(crbug/1325290):
  // - Check for conflicts.
  // - Compute and fill statuses for failed imports.
  // - Count actual number of successful imports.
  results.number_imported = seq->csv_passwords.size();
  results.status = password_manager::ImportResults::Status::SUCCESS;
  std::move(results_callback_).Run(results);
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
