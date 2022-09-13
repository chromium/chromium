// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_IMPORT_PASSWORD_IMPORTER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_IMPORT_PASSWORD_IMPORTER_H_

#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/types/expected.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/ui/import_results.h"
#include "components/password_manager/services/csv_password/csv_password_parser_service.h"
#include "components/password_manager/services/csv_password/public/mojom/csv_password_parser.mojom.h"

namespace password_manager {

class SavedPasswordsPresenter;

// Exposes an API for importing passwords from a file. Parsing of CSV will be
// performed using a utility SandBox process.
class PasswordImporter {
 public:
  static constexpr size_t MAX_PASSWORDS_PER_IMPORT = 3000;

  // CompletionCallback is the type of the processing function for parsed
  // passwords.
  using CompletionCallback =
      password_manager::mojom::CSVPasswordParser::ParseCSVCallback;

  using ImportResultsCallback =
      base::OnceCallback<void(const password_manager::ImportResults&)>;

  explicit PasswordImporter(SavedPasswordsPresenter* presenter);
  PasswordImporter(const PasswordImporter&) = delete;
  PasswordImporter& operator=(const PasswordImporter&) = delete;
  ~PasswordImporter();

  // Imports passwords from the file at |path| into the |to_store|.
  // |results_callback| is used to return import summary back to the user.
  // The only supported file format is CSV.
  void Import(const base::FilePath& path,
              password_manager::PasswordForm::Store to_store,
              ImportResultsCallback results_callback);

  // Returns the file extensions corresponding to supported formats.
  static std::vector<std::vector<base::FilePath::StringType>>
  GetSupportedFileExtensions();

  // Overrides the csv password parser service for testing.
  void SetServiceForTesting(
      mojo::PendingRemote<mojom::CSVPasswordParser> parser);

  bool IsRunning() const { return !results_callback_.is_null(); }

 private:
  // Parses passwords from |input| using a mojo sandbox process and
  // asynchronously calls |completion| with the results.
  void ParseCSVPasswordsInSandbox(
      CompletionCallback completion,
      base::expected<std::string, ImportResults::Status> result);

  void ConsumePasswords(std::string file_name,
                        password_manager::PasswordForm::Store store,
                        password_manager::mojom::CSVPasswordSequencePtr seq);

  const mojo::Remote<mojom::CSVPasswordParser>& GetParser();

  mojo::Remote<mojom::CSVPasswordParser> parser_;

  ImportResults::Status status_ = ImportResults::Status::NONE;

  ImportResultsCallback results_callback_;

  const raw_ptr<SavedPasswordsPresenter> presenter_;

  base::WeakPtrFactory<PasswordImporter> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_IMPORT_PASSWORD_IMPORTER_H_
