// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/import/password_importer.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "components/password_manager/core/browser/import/csv_password.h"
#include "components/password_manager/core/browser/import/csv_password_sequence.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/password_manager/core/browser/ui/import_results.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "components/password_manager/services/csv_password/csv_password_parser_service.h"
#include "components/sync/base/bind_to_task_runner.h"
#include "components/sync/base/features.h"
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
    if (file_size > kMaxFileSizeBytes) {
      return base::unexpected(ImportResults::Status::MAX_FILE_SIZE);
    }
  }

  std::string contents;
  if (!base::ReadFileToString(path, &contents)) {
    return base::unexpected(ImportResults::Status::IO_ERROR);
  }

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

ImportEntry CreateFailedImportEntry(const CredentialUIEntry& credential,
                                    const ImportEntry::Status status) {
  ImportEntry result;
  result.url = credential.GetURL().possibly_invalid_spec();
  result.username = base::UTF16ToUTF8(credential.username);
  result.status = status;
  return result;
}

ImportEntry CreateFailedImportEntry(
    SavedPasswordsPresenter::AddResult add_result,
    const CredentialUIEntry& credential) {
  DCHECK(!IsSuccessOrExactMatch(add_result));
  return CreateFailedImportEntry(credential, ToImportEntryStatus(add_result));
}

bool IsDuplicate(const SavedPasswordsPresenter::AddResult& status) {
  return status == SavedPasswordsPresenter::AddResult::kExactMatch;
}

bool IsConflict(const SavedPasswordsPresenter::AddResult& status) {
  return status == SavedPasswordsPresenter::AddResult::
                       kConflictInProfileAndAccountStore ||
         status ==
             SavedPasswordsPresenter::AddResult::kConflictInProfileStore ||
         status == SavedPasswordsPresenter::AddResult::kConflictInAccountStore;
}

bool IsPasswordMissing(const ImportEntry& entry) {
  return entry.status == ImportEntry::MISSING_PASSWORD;
}

bool IsUsernameMissing(const ImportEntry& entry) {
  return entry.username.empty();
}

bool IsURLMissing(const ImportEntry& entry) {
  return entry.url.empty();
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

  if (csv_password.GetParseStatus() != CSVPassword::Status::kOK) {
    return MakeError(ImportEntry::Status::UNKNOWN_ERROR);
  }

  if (csv_password.GetPassword().empty()) {
    return MakeError(ImportEntry::Status::MISSING_PASSWORD);
  }

  if (!url.has_value() && url.error().empty()) {
    return MakeError(ImportEntry::Status::MISSING_URL);
  }

  if (url.has_value() && url.value().spec().length() > 2048) {
    return MakeError(ImportEntry::Status::LONG_URL);
  }

  if (!url.has_value() && !base::IsStringASCII(url.error())) {
    return MakeError(ImportEntry::Status::NON_ASCII_URL);
  }

  if (!url.has_value() ||
      !password_manager_util::IsValidPasswordURL(url.value())) {
    return MakeError(ImportEntry::Status::INVALID_URL);
  }

  if (csv_password.GetPassword().length() > 1000) {
    return MakeError(ImportEntry::Status::LONG_PASSWORD);
  }

  if (csv_password.GetUsername().length() > 1000) {
    return MakeError(ImportEntry::Status::LONG_USERNAME);
  }

  if (base::FeatureList::IsEnabled(syncer::kPasswordNotesWithBackup) &&
      csv_password.GetNote().length() > 1000) {
    return MakeError(ImportEntry::Status::LONG_NOTE);
  }

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
  size_t duplicates_count = 0;  // Number of duplicates per imported file.
  size_t conflicts_count = 0;   // Number of conflict per imported file.
  // Number of rows with missing password, but username and URL are non-empty.
  size_t missing_only_password_rows = 0;
  // Number of rows with missing password and username, but URL is non-empty.
  size_t missing_password_and_username_rows = 0;
  // Number of rows with all login fields (URL, username, password) empty.
  size_t empty_all_login_fields = 0;

  for (size_t i = 0; i < add_results.size(); i++) {
    duplicates_count += IsDuplicate(add_results[i]);
    conflicts_count += IsConflict(add_results[i]);
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
    missing_only_password_rows += IsPasswordMissing(entry) &&
                                  !IsUsernameMissing(entry) &&
                                  !IsURLMissing(entry);
    missing_password_and_username_rows += IsPasswordMissing(entry) &&
                                          IsUsernameMissing(entry) &&
                                          !IsURLMissing(entry);
    empty_all_login_fields += IsPasswordMissing(entry) &&
                              IsUsernameMissing(entry) && IsURLMissing(entry);

    base::UmaHistogramEnumeration("PasswordManager.ImportEntryStatus",
                                  entry.status);
  }

  base::UmaHistogramLongTimes("PasswordManager.ImportDuration",
                              base::Time::Now() - start_time);

  const size_t all_errors_count = import_results.failed_imports.size();

  base::UmaHistogramCounts1M("PasswordManager.Import.PerFile.AnyErrors",
                             all_errors_count);
  base::UmaHistogramCounts1M("PasswordManager.Import.PerFile.Duplicates",
                             duplicates_count);
  base::UmaHistogramCounts1M("PasswordManager.Import.PerFile.Conflicts",
                             conflicts_count);
  base::UmaHistogramCounts1M(
      "PasswordManager.Import.PerFile.OnlyPasswordMissing",
      missing_only_password_rows);
  base::UmaHistogramCounts1M(
      "PasswordManager.Import.PerFile.PasswordAndUsernameMissing",
      missing_password_and_username_rows);
  base::UmaHistogramCounts1M(
      "PasswordManager.Import.PerFile.AllLoginFieldsEmtpy",
      empty_all_login_fields);

  if (all_errors_count > 0) {
    base::UmaHistogramBoolean("PasswordManager.Import.OnlyConflicts",
                              all_errors_count == conflicts_count);
    base::UmaHistogramBoolean("PasswordManager.Import.OnlyMissingPasswords",
                              all_errors_count == missing_only_password_rows);
  }

  import_results.status = password_manager::ImportResults::Status::SUCCESS;

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
    std::move(completion).Run(nullptr);
  } else {
    GetParser()->ParseCSV(std::move(result.value()), std::move(completion));
  }
}

void PasswordImporter::Import(const base::FilePath& path,
                              password_manager::PasswordForm::Store to_store,
                              ImportResultsCallback results_callback,
                              base::OnceClosure cleanup_callback) {
  results_callback_ =
      std::move(results_callback).Then(std::move(cleanup_callback));

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

  DCHECK_EQ(results.number_imported, 0u);

  if (!seq) {
    // A nullptr returned by the parser means a bad format.
    if (results.status == ImportResults::Status::NONE) {
      results.status = password_manager::ImportResults::Status::BAD_FORMAT;
    }

    std::move(results_callback_).Run(std::move(results));
    return;
  }
  if (seq->csv_passwords.size() > MAX_PASSWORDS_PER_IMPORT) {
    results.status =
        password_manager::ImportResults::Status::NUM_PASSWORDS_EXCEEDED;
    std::move(results_callback_).Run(results);
    return;
  }

  size_t notes_per_file_count = 0;
  size_t notes_duplicates_per_file_count = 0;
  size_t notes_substrings_per_file_count = 0;
  size_t notes_concatenations_per_file_count = 0;

  // TODO(crbug/1325290): Either move to earlier point or update histogram.
  base::Time start_time = base::Time::Now();
  std::vector<password_manager::CredentialUIEntry> credentials;
  credentials.reserve(seq->csv_passwords.size());

  auto ResolveConflictingNotes = [&](const CredentialUIEntry& credential) {
    // Returns `true` if the credential has been resolved (i.e.: conflicting
    // local credential(s) will be edited to include imported note or error will
    // be reported to the user) and doesn't need to be further processed.
    // Returns `false` if note doesn't require special treatment (i.e.: the note
    // data will not get lost due to conflicts).
    DCHECK(base::FeatureList::IsEnabled(syncer::kPasswordNotesWithBackup));

    const auto& imported_note = credential.note;
    DCHECK(imported_note.size() <= MAX_NOTE_LENGTH);

    if (imported_note.empty()) {
      // Nothing to resolve.
      return false;
    }

    notes_per_file_count++;

    auto forms = presenter_->GetCorrespondingPasswordForms(credential);
    if (forms.empty()) {
      // No matching local credentials.
      return false;
    }

    auto local_credential = CredentialUIEntry(forms);
    if (local_credential.note == imported_note) {
      notes_duplicates_per_file_count++;
      return false;
    }

    if (local_credential.note.find(imported_note) != std::u16string::npos) {
      notes_substrings_per_file_count++;
      return false;
    }

    if (imported_note.size() + 1u + local_credential.note.size() >=
        MAX_NOTE_LENGTH) {
      // Notes concatenation size should not exceed 1000 characters.
      results.failed_imports.push_back(CreateFailedImportEntry(
          credential, ImportEntry::Status::LONG_CONCATENATED_NOTE));
      return true;
    }

    std::u16string concatenation =
        local_credential.note.empty()
            ? imported_note
            : local_credential.note + u"\n" + imported_note;
    DCHECK(concatenation.size() <= MAX_NOTE_LENGTH);
    CredentialUIEntry updated_credential = local_credential;
    updated_credential.note = concatenation;
    // TODO(crbug.com/1407114): This is supposed to be a very rare operation.
    // Otherwise, accumulate credentials that need to be edited and ideally do
    // updates as a bulk operation.
    presenter_->EditSavedCredentials(local_credential, updated_credential);
    notes_concatenations_per_file_count++;
    results.number_imported++;

    // Matching local credentials were updated with notes concatenation.
    // Imported credential doesn't require further processing.
    return true;
  };

  // Go over all canonically parsed passwords:
  // 1) aggregate all valid ones in `credentials` to be passed over to the
  // presenter. 2) aggregate all parsing errors in the results.
  for (const password_manager::CSVPassword& csv_password : seq->csv_passwords) {
    base::expected<password_manager::CredentialUIEntry, ImportEntry>
        credential = CSVPasswordToCredentialUIEntry(csv_password, store);

    if (!credential.has_value()) {
      results.failed_imports.emplace_back(std::move(credential.error()));
      continue;
    }

    const CredentialUIEntry& current_credential = credential.value();
    if (!base::FeatureList::IsEnabled(syncer::kPasswordNotesWithBackup)) {
      credentials.emplace_back(std::move(current_credential));
      credentials.back().note.clear();
      continue;
    }
    if (!ResolveConflictingNotes(current_credential)) {
      credentials.emplace_back(std::move(current_credential));
    }
  }

  base::UmaHistogramCounts1000(
      "PasswordManager.Import.PerFile.Notes.TotalCount", notes_per_file_count);
  base::UmaHistogramCounts1000(
      "PasswordManager.Import.PerFile.Notes.Concatenations",
      notes_concatenations_per_file_count);
  base::UmaHistogramCounts1000(
      "PasswordManager.Import.PerFile.Notes.Duplicates",
      notes_duplicates_per_file_count);
  base::UmaHistogramCounts1000(
      "PasswordManager.Import.PerFile.Notes.Substrings",
      notes_substrings_per_file_count);

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
