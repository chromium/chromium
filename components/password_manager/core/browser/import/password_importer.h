// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_IMPORT_PASSWORD_IMPORTER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_IMPORT_PASSWORD_IMPORTER_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "components/password_manager/core/browser/import/import_results.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/password_manager/services/csv_password/csv_password_parser_service.h"
#include "components/password_manager/services/csv_password/public/mojom/csv_password_parser.mojom.h"

namespace password_manager {

struct NotesImportMetrics {
  // Number of valid non-empty notes (note's length is not greater than 1000
  // characters).
  size_t notes_per_file_count = 0;
  // Number of imported notes that are duplicates of local notes of the same
  // credential.
  size_t notes_duplicates_per_file_count = 0;
  // Number of imported notes that are substrings of local notes of the same
  // credential.
  size_t notes_substrings_per_file_count = 0;
  // Number of imported notes that were concatenated with local notes of the
  // same credential.
  size_t notes_concatenations_per_file_count = 0;
};

struct IncomingPasswords {
  IncomingPasswords();
  IncomingPasswords(IncomingPasswords&& other);
  ~IncomingPasswords();

  IncomingPasswords& operator=(IncomingPasswords&& other);

  // Passwords that should be added to the store.
  std::vector<password_manager::CredentialUIEntry> add_credentials;
  // Passwords that should be updated in the store.
  std::vector<password_manager::PasswordForm> edit_forms;
};

struct ConflictsResolutionCache;

class SavedPasswordsPresenter;

// Exposes an API for importing passwords from a file. Parsing of CSV will be
// performed using a utility SandBox process.
class PasswordImporter {
 public:
  enum State {
    // The object has just been created, but the import process has not been
    // launched yet. Or the import has finished with some errors and the object
    // has switched to the initial state.
    kNotStarted = 0,
    // PasswordImporter is busy, while the user is waiting for a response from
    // it. In case of new import requests, the user should receive an
    // IMPORT_ALREADY_ACTIVE error.
    kInProgress = 1,
    // Conflicts were found in the selected file. PasswordImporter is waiting
    // for the user to select which passwords to replace or to cancel the
    // import. In case of new import requests the current state should be
    // returned.
    kConflicts = 2,
    // Import has successufly finished with no errors. PasswordImporter is
    // waiting for the user to decide if they want to delete the file.
    kFinished = 3,
  };

  // ConsumePasswordsCallback is the type of the processing function for parsed
  // passwords.
  using ConsumePasswordsCallback =
      password_manager::mojom::CSVPasswordParser::ParseCSVCallback;

  using ImportResultsCallback =
      base::OnceCallback<void(const password_manager::ImportResults&)>;

  using DeleteFileCallback =
      base::RepeatingCallback<bool(const base::FilePath&)>;

  explicit PasswordImporter(SavedPasswordsPresenter* presenter);
  PasswordImporter(const PasswordImporter&) = delete;
  PasswordImporter& operator=(const PasswordImporter&) = delete;
  ~PasswordImporter();

  // Imports passwords from the file at |path| into the |to_store|.
  // |results_callback| is used to return import summary back to the user.
  // |cleanup_callback] is called when current object can be destroyed.
  // The only supported file format is CSV.
  void Import(const base::FilePath& path,
              password_manager::PasswordForm::Store to_store,
              ImportResultsCallback results_callback);

  // Resumes the import process when user has selected which passwords to
  // replace. The caller earlier received an array with conflicting
  // ImportEntry's that are displayed to the user, the ids of the selected items
  // correspond to indices of credentials in `conflicts_cache_->conflicts`.
  // |selected_ids|: The indices of passwords that need to be replaced.
  // |results_callback| is used to return import summary back to the user.
  void ContinueImport(const std::vector<int>& selected_ids,
                      ImportResultsCallback results_callback);

  // Triggers the deletion of the imported file at `file_path_` when the
  // importer is in the kFinished state.
  void DeleteFile();

  bool IsState(PasswordImporter::State state) const { return state_ == state; }

  // Returns the file extensions corresponding to supported formats.
  static std::vector<std::vector<base::FilePath::StringType>>
  GetSupportedFileExtensions();

  // Overrides the csv password parser service for testing.
  void SetServiceForTesting(
      mojo::PendingRemote<mojom::CSVPasswordParser> parser);

  void SetDeleteFileForTesting(DeleteFileCallback delete_callback) {
    delete_function_ = std::move(delete_callback);
  }

 private:
  // Parses passwords from |input| using a mojo sandbox process and
  // asynchronously calls |completion| with the results.
  void ParseCSVPasswordsInSandbox(
      PasswordForm::Store to_store,
      ImportResultsCallback results_callback,
      base::expected<std::string, ImportResults::Status> result);

  // Processes passwords when they've been parsed by ParseCSVPasswordsInSandbox.
  void ConsumePasswords(PasswordForm::Store to_store,
                        ImportResultsCallback results_callback,
                        password_manager::mojom::CSVPasswordSequencePtr seq);

  // Triggers the processes for adding and updating `incoming_passwords`.
  void ExecuteImport(ImportResultsCallback results_callback,
                     ImportResults results,
                     IncomingPasswords incoming_passwords,
                     base::Time start_time,
                     size_t conflicts_count);

  // Runs `results_callback` with aggregate results `results_` after all
  // imported passwords were added and updated.
  // Also, reports import results metrics.
  void ImportFinished(ImportResultsCallback results_callback,
                      ImportResults results,
                      base::Time start_time,
                      size_t conflicts_count);

  const mojo::Remote<mojom::CSVPasswordParser>& GetParser();

  mojo::Remote<mojom::CSVPasswordParser> parser_;

  PasswordImporter::State state_ = kNotStarted;

  // Path of the imported file.
  base::FilePath file_path_;

  // Used to cache intermediate results of the import during kConflicts state.
  std::unique_ptr<ConflictsResolutionCache> conflicts_cache_;

  // The function which does the actual deleting of a file. It should wrap
  // base::DeleteFile, unless it's changed for testing purposes.
  DeleteFileCallback delete_function_;

  const raw_ptr<SavedPasswordsPresenter> presenter_;

  base::WeakPtrFactory<PasswordImporter> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_IMPORT_PASSWORD_IMPORTER_H_
