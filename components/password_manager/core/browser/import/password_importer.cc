// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/import/password_importer.h"

#include <optional>
#include <string>
#include <utility>

#include "base/barrier_closure.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "components/password_manager/core/browser/import/csv_password.h"
#include "components/password_manager/core/browser/import/csv_password_sequence.h"
#include "components/password_manager/core/browser/import/import_results.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/password_manager/core/browser/ui/credential_utils.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "components/password_manager/core/common/password_manager_constants.h"
#include "components/password_manager/services/csv_password/csv_password_parser_service.h"

using password_manager::ImportEntry;
namespace password_manager {

IncomingPasswords::IncomingPasswords() = default;
IncomingPasswords::~IncomingPasswords() = default;
IncomingPasswords::IncomingPasswords(IncomingPasswords&& other) = default;
IncomingPasswords& IncomingPasswords::operator=(IncomingPasswords&& other) =
    default;

struct ConflictsResolutionCache {
  ConflictsResolutionCache(
      IncomingPasswords incoming_passwords,
      std::vector<std::vector<password_manager::PasswordForm>> conflicts,
      ImportResults results,
      base::Time start_time)
      : incoming_passwords(std::move(incoming_passwords)),
        conflicts(std::move(conflicts)),
        results(std::move(results)),
        start_time(start_time) {}
  ~ConflictsResolutionCache() = default;

  // Aggregated passwords that need to be added or updated.
  IncomingPasswords incoming_passwords;
  // Conflicting credential that could be updated. Each nested vector
  // represents one credential, i.e. all PasswordForm's in such a vector have
  // the same signon_ream, username, password.
  std::vector<std::vector<password_manager::PasswordForm>> conflicts;
  // Aggregated results of the current import.
  ImportResults results;
  // Used to track the time needed to process the already parsed credentials,
  // checking for conflicts, generating status and storing them.
  base::Time start_time;
};

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

ImportEntry::Status GetConflictType(
    password_manager::PasswordForm::Store target_store) {
  switch (target_store) {
    case PasswordForm::Store::kProfileStore:
      return ImportEntry::Status::CONFLICT_PROFILE;
    case PasswordForm::Store::kAccountStore:
      return ImportEntry::Status::CONFLICT_ACCOUNT;
    case PasswordForm::Store::kNotSet:
      return ImportEntry::Status::UNKNOWN_ERROR;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return ImportEntry::Status::UNKNOWN_ERROR;
}

ImportEntry CreateFailedImportEntry(const CredentialUIEntry& credential,
                                    const ImportEntry::Status status) {
  ImportEntry result;
  result.url = credential.GetAffiliatedDomains()[0].name;
  result.username = base::UTF16ToUTF8(credential.username);
  result.status = status;
  return result;
}

ImportEntry CreateValidImportEntry(const CredentialUIEntry& credential,
                                   int id) {
  ImportEntry result;
  result.id = id;
  result.url = credential.GetAffiliatedDomains()[0].name;
  result.username = base::UTF16ToUTF8(credential.username);
  result.password = base::UTF16ToUTF8(credential.password);
  result.status = ImportEntry::VALID;
  return result;
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
  auto with_status = [&](ImportEntry::Status status) {
    ImportEntry entry;
    entry.status = status;
    // The raw URL is shown in the errors list in the UI to make it easier to
    // match the listed entry with the one in the CSV file.
    auto url = csv_password.GetURL();
    entry.url = url.has_value() ? url.value().spec() : url.error();
    entry.username = csv_password.GetUsername();
    return entry;
  };

  if (csv_password.GetParseStatus() != CSVPassword::Status::kOK) {
    return base::unexpected(with_status(ImportEntry::Status::UNKNOWN_ERROR));
  }

  const std::string& password = csv_password.GetPassword();
  if (password.empty()) {
    return base::unexpected(with_status(ImportEntry::Status::MISSING_PASSWORD));
  }
  if (password.length() > 1000) {
    return base::unexpected(with_status(ImportEntry::Status::LONG_PASSWORD));
  }

  if (csv_password.GetUsername().length() > 1000) {
    return base::unexpected(with_status(ImportEntry::Status::LONG_USERNAME));
  }

  if (csv_password.GetNote().length() > 1000) {
    return base::unexpected(with_status(ImportEntry::Status::LONG_NOTE));
  }

  ASSIGN_OR_RETURN(
      GURL url, csv_password.GetURL(), [&](const std::string& error) {
        return with_status(error.empty() ? ImportEntry::Status::MISSING_URL
                                         : ImportEntry::Status::INVALID_URL);
      });
  if (url.spec().length() > 2048) {
    return base::unexpected(with_status(ImportEntry::Status::LONG_URL));
  }
  if (!IsValidPasswordURL(url)) {
    return base::unexpected(with_status(ImportEntry::Status::INVALID_URL));
  }

  return password_manager::CredentialUIEntry(csv_password, store);
}

std::optional<CredentialUIEntry> GetConflictingCredential(
    const std::map<std::u16string, std::vector<CredentialUIEntry>>&
        credentials_by_username,
    const CredentialUIEntry& imported_credential) {
  auto it = credentials_by_username.find(imported_credential.username);
  if (it != credentials_by_username.end()) {
    // Iterate over all local credentials with matching username.
    for (const CredentialUIEntry& local_credential : it->second) {
      // Check if `local_credential` has matching `signon_realm`, but different
      // `password`.
      if (local_credential.password != imported_credential.password &&
          base::ranges::any_of(
              local_credential.facets,
              [&imported_credential](const CredentialFacet& facet) {
                return facet.signon_realm ==
                       imported_credential.facets[0].signon_realm;
              })) {
        return local_credential;
      }
    }
  }
  return std::nullopt;
}

std::vector<PasswordForm> GetMatchingPasswordForms(
    SavedPasswordsPresenter* presenter,
    const CredentialUIEntry& credential,
    PasswordForm::Store store) {
  // Returns matching local forms for a given `credential`, excluding grouped
  // forms with different `signon_realm`.
  CHECK(presenter);
  std::vector<PasswordForm> results;
  base::ranges::copy_if(
      presenter->GetCorrespondingPasswordForms(credential),
      std::back_inserter(results), [&](const PasswordForm& form) {
        return form.signon_realm == credential.GetFirstSignonRealm() &&
               store == form.in_store;
      });
  return results;
}

std::u16string ComputeNotesConcatenation(const std::u16string& local_note,
                                         const std::u16string& imported_note,
                                         NotesImportMetrics& metrics) {
  CHECK_LE(imported_note.size(),
           static_cast<unsigned>(constants::kMaxPasswordNoteLength));

  if (imported_note.empty()) {
    return local_note;
  }

  if (local_note.empty()) {
    return imported_note;
  }

  if (local_note == imported_note) {
    metrics.notes_duplicates_per_file_count++;
    return local_note;
  }

  if (local_note.find(imported_note) != std::u16string::npos) {
    metrics.notes_substrings_per_file_count++;
    return local_note;
  }

  return base::JoinString(/*parts=*/{local_note, imported_note}, u"\n");
}

void MergeNotesOrReportError(const std::vector<PasswordForm>& local_forms,
                             const CredentialUIEntry& imported_credential,
                             ImportResults& results,
                             std::vector<PasswordForm>& edit_forms,
                             NotesImportMetrics& metrics) {
  // Attempts to concatenate the note of `imported_credential` with
  // `local_forms`.
  // If notes concatenation is possible and required, `local_forms` will be
  // updated and added to `edit_forms`.
  // If concatenation exceeds MAX_NOTE_LENGTH, the error entry will be added to
  // the `results`.
  // `metrics` is used to track different outcomes.
  const std::u16string local_note = CredentialUIEntry(local_forms).note;
  const std::u16string& imported_note = imported_credential.note;
  const std::u16string concatenation =
      ComputeNotesConcatenation(local_note, imported_note, metrics);

  if (concatenation.size() > constants::kMaxPasswordNoteLength) {
    // Notes concatenation size should not exceed 1000 characters.
    results.displayed_entries.push_back(CreateFailedImportEntry(
        imported_credential, ImportEntry::Status::LONG_CONCATENATED_NOTE));
    return;
  }

  if (concatenation != local_note) {
    // Local credential needs to be updated with concatenation.
    for (PasswordForm form : local_forms) {
      form.SetNoteWithEmptyUniqueDisplayName(concatenation);
      edit_forms.emplace_back(std::move(form));
    }
    metrics.notes_concatenations_per_file_count++;
  }

  results.number_imported++;
}

void ReportNotesMetrics(const NotesImportMetrics& metrics) {
  base::UmaHistogramCounts1000(
      "PasswordManager.Import.PerFile.Notes.TotalCount",
      metrics.notes_per_file_count);
  base::UmaHistogramCounts1000(
      "PasswordManager.Import.PerFile.Notes.Concatenations",
      metrics.notes_concatenations_per_file_count);
  base::UmaHistogramCounts1000(
      "PasswordManager.Import.PerFile.Notes.Duplicates",
      metrics.notes_duplicates_per_file_count);
  base::UmaHistogramCounts1000(
      "PasswordManager.Import.PerFile.Notes.Substrings",
      metrics.notes_substrings_per_file_count);
}

void ReportImportResultsMetrics(const ImportResults& results,
                                base::Time start_time,
                                size_t conflicts_count) {
  // Number of rows with missing password, but username and URL are non-empty.
  size_t missing_only_password_rows = 0;
  // Number of rows with missing password and username, but URL is non-empty.
  size_t missing_password_and_username_rows = 0;
  // Number of rows with all login fields (URL, username, password) empty.
  size_t empty_all_login_fields = 0;

  UMA_HISTOGRAM_COUNTS_1M("PasswordManager.ImportedPasswordsPerUserInCSV",
                          results.number_imported);
  for (const ImportEntry& entry : results.displayed_entries) {
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

  const size_t all_errors_count = results.displayed_entries.size();

  base::UmaHistogramCounts1M("PasswordManager.Import.PerFile.AnyErrors",
                             all_errors_count);
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
}

bool DefaultDeleteFunction(const base::FilePath& file) {
  return base::DeleteFile(file);
}

void ProcessParsedCredential(
    const CredentialUIEntry& imported_credential,
    SavedPasswordsPresenter* presenter,
    const std::map<std::u16string, std::vector<CredentialUIEntry>>&
        credentials_by_username,
    PasswordForm::Store to_store,
    IncomingPasswords& incoming_passwords,
    std::vector<std::vector<password_manager::PasswordForm>>& conflicts,
    ImportResults& results,
    NotesImportMetrics& notes_metrics,
    size_t& duplicates_count) {
  if (!imported_credential.note.empty()) {
    notes_metrics.notes_per_file_count++;
  }

  // Check if there are local credentials with the same signon_realm and
  // username, but different password. Such credentials are considered
  // conflicts.
  std::optional<CredentialUIEntry> conflicting_credential =
      GetConflictingCredential(credentials_by_username, imported_credential);
  if (conflicting_credential.has_value()) {
    std::vector<PasswordForm> forms = GetMatchingPasswordForms(
        presenter, conflicting_credential.value(), to_store);
    // Password notes are not taken into account when conflicting passwords
    // are overwritten. Only the local note is persisted.
    for (PasswordForm& form : forms) {
      form.password_value = imported_credential.password;
    }
    conflicts.push_back(std::move(forms));
    return;
  }

  // Check for duplicates.
  std::vector<PasswordForm> forms =
      GetMatchingPasswordForms(presenter, imported_credential, to_store);
  if (!forms.empty()) {
    duplicates_count++;

    if (imported_credential.note.empty()) {
      // Duplicates are reported as successfully imported credentials.
      results.number_imported++;
      return;
    }

    MergeNotesOrReportError(
        /*local_forms=*/forms, /*imported_credential=*/imported_credential,
        /*results=*/results, /*edit_forms=*/incoming_passwords.edit_forms,
        /*metrics=*/notes_metrics);
    return;
  }

  // Valid credential with no conflicts and no duplicates.
  incoming_passwords.add_credentials.push_back(imported_credential);
}

}  // namespace

PasswordImporter::PasswordImporter(SavedPasswordsPresenter* presenter)
    : delete_function_(base::BindRepeating(&DefaultDeleteFunction)),
      presenter_(presenter) {}

PasswordImporter::~PasswordImporter() = default;

const mojo::Remote<mojom::CSVPasswordParser>& PasswordImporter::GetParser() {
  if (!parser_) {
    parser_ = LaunchCSVPasswordParser();
    parser_.reset_on_disconnect();
  }
  return parser_;
}

void PasswordImporter::ParseCSVPasswordsInSandbox(
    PasswordForm::Store to_store,
    ImportResultsCallback results_callback,
    base::expected<std::string, ImportResults::Status> result) {
  // Currently, CSV is the only supported format.
  if (result.has_value()) {
    GetParser()->ParseCSV(
        std::move(result.value()),
        base::BindOnce(&PasswordImporter::ConsumePasswords,
                       weak_ptr_factory_.GetWeakPtr(), to_store,
                       std::move(results_callback)));
  } else {
    ImportResults results;
    results.status = result.error();
    // Importer is reset to the initial state, due to the error.
    state_ = kNotStarted;
    std::move(results_callback).Run(std::move(results));
  }
}

void PasswordImporter::Import(const base::FilePath& path,
                              password_manager::PasswordForm::Store to_store,
                              ImportResultsCallback results_callback) {
  // Blocks concurrent import requests.
  state_ = kInProgress;
  file_path_ = path;

  // Posting with USER_VISIBLE priority, because the result of the import is
  // visible to the user in the password settings page.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&ReadFileToString, path),
      base::BindOnce(&PasswordImporter::ParseCSVPasswordsInSandbox,
                     weak_ptr_factory_.GetWeakPtr(), to_store,
                     std::move(results_callback)));
}

void PasswordImporter::ContinueImport(const std::vector<int>& selected_ids,
                                      ImportResultsCallback results_callback) {
  CHECK(IsState(kConflicts));
  CHECK(conflicts_cache_);
  // Blocks concurrent import requests, when switching from `kConflicts` state.
  state_ = kInProgress;

  for (int id : selected_ids) {
    conflicts_cache_->results.number_imported++;
    CHECK_LT(static_cast<size_t>(id), conflicts_cache_->conflicts.size());
    for (const PasswordForm& form : conflicts_cache_->conflicts[id]) {
      conflicts_cache_->incoming_passwords.edit_forms.push_back(form);
    }
  }

  ExecuteImport(
      std::move(results_callback), std::move(conflicts_cache_->results),
      std::move(conflicts_cache_->incoming_passwords),
      conflicts_cache_->start_time, conflicts_cache_->conflicts.size());

  conflicts_cache_.reset();

  base::UmaHistogramCounts1M("PasswordManager.Import.PerFile.ConflictsResolved",
                             selected_ids.size());
}

void PasswordImporter::ConsumePasswords(
    PasswordForm::Store to_store,
    ImportResultsCallback results_callback,
    password_manager::mojom::CSVPasswordSequencePtr seq) {
  // Used to aggregate final results of the current import.
  ImportResults results;
  results.file_name = file_path_.BaseName().AsUTF8Unsafe();
  CHECK_EQ(results.number_imported, 0u);

  if (!seq) {
    // A nullptr returned by the parser means a bad format.
    results.status = password_manager::ImportResults::Status::BAD_FORMAT;
    // Importer is reset to the initial state, due to the error.
    state_ = kNotStarted;
    std::move(results_callback).Run(std::move(results));
    return;
  }
  if (seq->csv_passwords.size() > constants::kMaxPasswordsPerCSVFile) {
    results.status =
        password_manager::ImportResults::Status::NUM_PASSWORDS_EXCEEDED;

    // Importer is reset to the initial state, due to the error.
    state_ = kNotStarted;
    std::move(results_callback).Run(results);
    return;
  }

  // TODO(crbug.com/40225420): Either move to earlier point or update histogram.
  base::Time start_time = base::Time::Now();
  // Used to compute conflicts and duplicates.
  std::map<std::u16string, std::vector<CredentialUIEntry>>
      credentials_by_username;
  for (const CredentialUIEntry& credential : presenter_->GetSavedPasswords()) {
    // Don't consider credentials from a store other than the target store.
    if (credential.stored_in.contains(to_store)) {
      credentials_by_username[credential.username].push_back(credential);
    }
  }

  NotesImportMetrics notes_metrics;
  size_t duplicates_count = 0;  // Number of duplicates per imported file.

  // Aggregate all passwords that might need to be added or updated.
  IncomingPasswords incoming_passwords;

  // Conflicting credential that could be updated. Each nested vector
  // represents one credential, i.e. all PasswordForm's in such a vector have
  // the same signon_ream, username, password.
  std::vector<std::vector<password_manager::PasswordForm>> conflicts;

  // Go over all canonically parsed passwords:
  // 1) aggregate all valid ones in `incoming_passwords` to be passed over to
  // the presenter. 2) aggregate all parsing errors in the `results`.
  for (const password_manager::CSVPassword& csv_password : seq->csv_passwords) {
    base::expected<password_manager::CredentialUIEntry, ImportEntry>
        credential = CSVPasswordToCredentialUIEntry(csv_password, to_store);

    if (!credential.has_value()) {
      results.displayed_entries.emplace_back(std::move(credential.error()));
      continue;
    }

    ProcessParsedCredential(credential.value(), presenter_,
                            credentials_by_username, to_store,
                            incoming_passwords, conflicts, results,
                            notes_metrics, duplicates_count);
  }

  results.number_imported += incoming_passwords.add_credentials.size();

  ReportNotesMetrics(notes_metrics);
  base::UmaHistogramCounts1M("PasswordManager.Import.PerFile.Duplicates",
                             duplicates_count);

  if (conflicts.empty()) {
    for (const std::vector<PasswordForm>& forms : conflicts) {
      results.displayed_entries.push_back(CreateFailedImportEntry(
          CredentialUIEntry(forms), GetConflictType(to_store)));
    }

    ExecuteImport(std::move(results_callback), std::move(results),
                  std::move(incoming_passwords), start_time, conflicts.size());
    return;
  }

  state_ = kConflicts;
  ImportResults conflicts_results;
  conflicts_results.status = ImportResults::CONFLICTS;
  for (size_t idx = 0; idx < conflicts.size(); idx++) {
    conflicts_results.displayed_entries.push_back(
        CreateValidImportEntry(CredentialUIEntry(conflicts[idx]), idx));
  }

  conflicts_cache_ = std::make_unique<ConflictsResolutionCache>(
      std::move(incoming_passwords), std::move(conflicts), std::move(results),
      start_time);

  std::move(results_callback).Run(std::move(conflicts_results));
}

void PasswordImporter::ExecuteImport(ImportResultsCallback results_callback,
                                     ImportResults results,
                                     IncomingPasswords incoming_passwords,
                                     base::Time start_time,
                                     size_t conflicts_count) {
  // Run `results_callback` when both `AddCredentials` and
  // `UpdatePasswordForms` have finished running.
  auto barrier_done_callback = base::BarrierClosure(
      2, base::BindOnce(base::BindOnce(
             &PasswordImporter::ImportFinished, weak_ptr_factory_.GetWeakPtr(),
             std::move(results_callback), std::move(results), start_time,
             conflicts_count)));

  presenter_->AddCredentials(incoming_passwords.add_credentials,
                             password_manager::PasswordForm::Type::kImported,
                             barrier_done_callback);
  presenter_->UpdatePasswordForms(incoming_passwords.edit_forms,
                                  barrier_done_callback);
}

void PasswordImporter::ImportFinished(ImportResultsCallback results_callback,
                                      ImportResults results,
                                      base::Time start_time,
                                      size_t conflicts_count) {
  ReportImportResultsMetrics(results, start_time, conflicts_count);

  if (results.displayed_entries.empty()) {
    // After successful import with no errors, the user has an option to delete
    // the imported file.
    state_ = kFinished;
  } else {
    // After successful import with some errors, the importer is reset to the
    // initial state.
    state_ = kNotStarted;
  }

  results.status = password_manager::ImportResults::Status::SUCCESS;
  std::move(results_callback).Run(std::move(results));
}

void PasswordImporter::DeleteFile() {
  CHECK(IsState(kFinished));
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(base::IgnoreResult(delete_function_), file_path_));
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
