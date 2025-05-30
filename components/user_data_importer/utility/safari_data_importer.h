// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_DATA_IMPORTER_UTILITY_SAFARI_DATA_IMPORTER_H_
#define COMPONENTS_USER_DATA_IMPORTER_UTILITY_SAFARI_DATA_IMPORTER_H_

#include "components/password_manager/core/browser/import/password_importer.h"

namespace user_data_importer {

class SafariDataImporter {
 public:
  // A callback used to obtain the number of successfully imported bookmarks,
  // urls (for history import) or payment cards.
  using ImportCallback = base::OnceCallback<void(int)>;

  SafariDataImporter(password_manager::SavedPasswordsPresenter* presenter);
  ~SafariDataImporter();

  // Attempts to import various data types (passwords, payment cards, bookmarks
  // and history) from the file provided in "path". Each data type is optional
  // may or may not be present in the file. "passwords_callback" is called at
  // the end of the password import process and will be provided a list of
  // successful imports as well as conflicts and errors.
  // "bookmarks_callback", "history_callback" and "payment_cards_callback" will
  // be called at the end of the import processes of each type of data to return
  // the number of successful imports.
  void Import(const base::FilePath& path,
              password_manager::PasswordImporter::ImportResultsCallback
                  passwords_callback,
              ImportCallback bookmarks_callback,
              ImportCallback history_callback,
              ImportCallback payment_cards_callback);

  // Can be called after calling "Import" in order to import passwords which
  // were originally not imported due to conflicts. "selected_ids" provides the
  // list of passwords to import.
  void ResolvePasswordConflicts(
      const std::vector<int>& selected_ids,
      password_manager::PasswordImporter::ImportResultsCallback
          passwords_callback);

 private:
  friend class SafariDataImporterTest;

  // Attempts to import bookmarks by parsing the provided HTML data.
  // Calls "bookmarks_callback" when done.
  void ImportBookmarks(std::string html_data,
                       ImportCallback bookmarks_callback);

  // Attempts to import history from the file provided by "zip_filename".
  // Calls "history_callback" when done.
  void ImportHistory(const std::string& zip_filename,
                     ImportCallback history_callback);

  // Attempts to import passwords by parsing the provided CSV data.
  // Calls "results_callback" when done.
  void ImportPasswords(std::string csv_data,
                       password_manager::PasswordImporter::ImportResultsCallback
                           results_callback);

  // Attempts to import payment cards by parsing the provided JSON data.
  // Calls "payment_cards_callback" when done.
  void ImportPaymentCards(std::string json_data,
                          ImportCallback payment_cards_callback);

  // The password importer used to import passwords and resolve conflicts.
  std::unique_ptr<password_manager::PasswordImporter> password_importer_;
};

}  // namespace user_data_importer

#endif  // COMPONENTS_USER_DATA_IMPORTER_UTILITY_SAFARI_DATA_IMPORTER_H_
