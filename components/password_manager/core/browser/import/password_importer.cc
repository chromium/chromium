// Copyright 2016 The Chromium Authors
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
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "components/password_manager/services/csv_password/csv_password_parser_service.h"
#include "components/sync/base/bind_to_task_runner.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using password_manager::ImportEntry;
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
base::expected<std::string, ImportResults::Status> ReadFileToString(
    const base::FilePath& path) {
  int64_t file_size;

  if (GetFileSize(path, &file_size)) {
    base::UmaHistogramCounts1M("PasswordManager.ImportFileSize", file_size);
    if (file_size > kMaxFileSizeBytes)
      return base::unexpected(ImportResults::Status::MAX_FILE_SIZE);
  }

  std::string contents;
  if (!base::ReadFileToString(path, &contents))
    return base::unexpected(ImportResults::Status::IO_ERROR);

  return std::move(contents);
}

ImportEntry::Status ToImportEntryStatus(
    SavedPasswordsPresenter::AddResult add_result) {
  switch (add_result) {
    case SavedPasswordsPresenter::AddResult::kConflictInProfileStore:
      return ImportEntry::Status::CONFLICT_PROFILE;

    case SavedPasswordsPresenter::AddResult::kConflictInAccountStore:
    // We report a double collision for now as a collision in account store.
    case SavedPasswordsPresenter::AddResult::kConflictInProfileAndAccountStore:
      return ImportEntry::Status::CONFLICT_ACCOUNT;

    case SavedPasswordsPresenter::AddResult::kInvalid:
      return ImportEntry::Status::UNKNOWN_ERROR;

    default:
      NOTREACHED();
  }

  return ImportEntry::Status::UNKNOWN_ERROR;
}

bool IsSuccessOrExactMatch(const SavedPasswordsPresenter::AddResult& status) {
  return status == SavedPasswordsPresenter::AddResult::kSuccess ||
         status == SavedPasswordsPresenter::AddResult::kExactMatch;
}

ImportEntry CreateFailedImportEntry(
    SavedPasswordsPresenter::AddResult add_result,
    const CredentialUIEntry& credential) {
  DCHECK(!IsSuccessOrExactMatch(add_result));
  ImportEntry result;
  result.url = credential.GetURL().possibly_invalid_spec();
  result.username = base::UTF16ToUTF8(credential.username);
  result.status = ToImportEntryStatus(add_result);
  return result;
}

base::expected<password_manager::CredentialUIEntry, ImportEntry>
CSVPasswordToCredentialUIEntry(const CSVPassword& csv_password,
                               password_manager::PasswordForm::Store store) {
  auto MakeError = [&](ImportEntry::Status status) {
    ImportEntry entry;
    entry.status = status;
    if (csv_password.GetURL().has_value()) {
      entry.url = csv_password.GetURL().value().spec();
    } else {
      entry.url = csv_password.GetURL().error();
    }

    entry.username = csv_password.GetUsername();
    return base::unexpected(entry);
  };

  base::expected<GURL, std::string> url = csv_password.GetURL();

  if (csv_password.GetParseStatus() != CSVPassword::Status::kOK)
    return MakeError(ImportEntry::Status::UNKNOWN_ERROR);

  if (csv_password.GetPassword().empty())
    return MakeError(ImportEntry::Status::MISSING_PASSWORD);

  if (!url.has_value() && url.error().empty())
    return MakeError(ImportEntry::Status::MISSING_URL);

  if (url.has_value() && url.value().spec().length() > 2048)
    return MakeError(ImportEntry::Status::LONG_URL);

  if (!url.has_value() && !base::IsStringASCII(url.error()))
    return MakeError(ImportEntry::Status::NON_ASCII_URL);

  if (!url.has_value() ||
      !password_manager_util::IsValidPasswordURL(url.value())) {
    return MakeError(ImportEntry::Status::INVALID_URL);
  }

  if (csv_password.GetPassword().length() > 1000)
    return MakeError(ImportEntry::Status::LONG_PASSWORD);

  if (csv_password.GetUsername().length() > 1000)
    return MakeError(ImportEntry::Status::LONG_USERNAME);

  DCHECK(url.has_value());
  return password_manager::CredentialUIEntry(csv_password, store);
}

// `credentials` is a copy of what was passed to the AddCredentials() method.
// It is hence a 1 to 1 correspondence between `credentials` and `add_results`.
void AddCredentialsCallback(
    const base::Time& start_time,
    password_manager::ImportResults import_results,
    const std::vector<CredentialUIEntry>& credentials,
    password_manager::PasswordImporter::ImportResultsCallback
        import_results_callback,
    const std::vector<SavedPasswordsPresenter::AddResult>& add_results) {
  import_results.number_imported = 0;
  for (size_t i = 0; i < add_results.size(); i++) {
    if (IsSuccessOrExactMatch(add_results[i])) {
      import_results.number_imported++;
    } else {
      import_results.failed_imports.emplace_back(
          CreateFailedImportEntry(add_results[i], credentials[i]));
    }
  }

  UMA_HISTOGRAM_COUNTS_1M("PasswordManager.ImportedPasswordsPerUserInCSV",
                          import_results.number_imported);
  for (const ImportEntry& entry : import_results.failed_imports) {
    base::UmaHistogramEnumeration("PasswordManager.ImportEntryStatus",
                                  entry.status);
  }

  base::UmaHistogramLongTimes("PasswordManager.ImportDuration",
                              base::Time::Now() - start_time);

  import_results.status = password_manager::ImportResults::Status::SUCCESS;
  base::UmaHistogramEnumeration("PasswordManager.ImportResultsStatus",
                                import_results.status);

  std::move(import_results_callback).Run(std::move(import_results));
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

void PasswordImporter::ParseCSVPasswordsInSandbox(
    PasswordImporter::CompletionCallback completion,
    base::expected<std::string, ImportResults::Status> result) {
  // Currently, CSV is the only supported format.
  if (!result.has_value()) {
    this->status_ = result.error();
    base::UmaHistogramEnumeration("PasswordManager.ImportResultsStatus",
                                  this->status_);
    std::move(completion).Run(nullptr);
  } else {
    GetParser()->ParseCSV(std::move(result.value()), std::move(completion));
  }
}

void PasswordImporter::Import(const base::FilePath& path,
                              password_manager::PasswordForm::Store to_store,
                              ImportResultsCallback results_callback) {
  results_callback_ = std::move(results_callback);

  // Posting with USER_VISIBLE priority, because the result of the import is
  // visible to the user in the password settings page.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&ReadFileToString, path),
      base::BindOnce(&PasswordImporter::ParseCSVPasswordsInSandbox,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::BindOnce(&PasswordImporter::ConsumePasswords,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    path.BaseName().AsUTF8Unsafe(), to_store)));
}

void PasswordImporter::ConsumePasswords(
    std::string file_name,
    password_manager::PasswordForm::Store store,
    password_manager::mojom::CSVPasswordSequencePtr seq) {
  ImportResults results;
  results.file_name = std::move(file_name);
  results.status = status_;

  if (!seq) {
    // A nullptr returned by the parser means a bad format.
    if (results.status == ImportResults::Status::NONE) {
      results.status = password_manager::ImportResults::Status::BAD_FORMAT;
      base::UmaHistogramEnumeration("PasswordManager.ImportResultsStatus",
                                    results.status);
    }

    std::move(results_callback_).Run(std::move(results));
    return;
  }
  if (seq->csv_passwords.size() > MAX_PASSWORDS_PER_IMPORT) {
    results.status =
        password_manager::ImportResults::Status::NUM_PASSWORDS_EXCEEDED;
    base::UmaHistogramEnumeration("PasswordManager.ImportResultsStatus",
                                  results.status);
    std::move(results_callback_).Run(results);
    return;
  }

  // TODO(crbug/1325290): Either move to earlier point or update histogram.
  base::Time start_time = base::Time::Now();
  std::vector<password_manager::CredentialUIEntry> credentials;
  credentials.reserve(seq->csv_passwords.size());

  // Go over all canonically parsed passwords:
  // 1) aggregate all valid ones in `credentials` to be passed over to the
  // presenter. 2) aggregate all parsing errors in the results.
  for (const password_manager::CSVPassword& csv_password : seq->csv_passwords) {
    base::expected<password_manager::CredentialUIEntry, ImportEntry>
        credential = CSVPasswordToCredentialUIEntry(csv_password, store);

    if (credential.has_value())
      credentials.emplace_back(std::move(credential.value()));
    else
      results.failed_imports.emplace_back(std::move(credential.error()));
  }

  // Pass `credentials` along with import results `results` to the callback
  // too since they are necessary to report which imports did actually fail.
  // (e.g. which url, username ...etc). Pass the import results (`results`)
  // to the callback to aggregate other errors.
  presenter_->AddCredentials(
      credentials, password_manager::PasswordForm::Type::kImported,
      base::BindOnce(&AddCredentialsCallback, start_time, std::move(results),
                     credentials, std::move(results_callback_)));
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
