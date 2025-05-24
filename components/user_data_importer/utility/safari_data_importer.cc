// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_data_importer/utility/safari_data_importer.h"

namespace user_data_importer {

SafariDataImporter::SafariDataImporter(
    password_manager::SavedPasswordsPresenter* presenter)
    : password_importer_(
          std::make_unique<password_manager::PasswordImporter>(presenter)) {}

SafariDataImporter::~SafariDataImporter() = default;

void SafariDataImporter::Import(
    const base::FilePath& path,
    password_manager::PasswordImporter::ImportResultsCallback
        passwords_callback,
    ImportCallback bookmarks_callback,
    ImportCallback history_callback,
    ImportCallback payment_cards_callback) {
  std::string zip_filename = path.MaybeAsASCII();
  if (zip_filename.empty()) {
    password_manager::ImportResults results;
    std::move(passwords_callback).Run(std::move(results));
    std::move(bookmarks_callback).Run(0);
    std::move(history_callback).Run(0);
    std::move(payment_cards_callback).Run(0);
    return;
  }

  // Passwords import may require conflict resolution, so it is done first.
  ImportPasswords(zip_filename, std::move(passwords_callback));

  // Launch payment cards and bookmarks import processes.
  ImportPaymentCards(zip_filename, std::move(payment_cards_callback));
  ImportBookmarks(zip_filename, std::move(bookmarks_callback));

  // History import may require synchronously reading from the file, so it is
  // done last.
  ImportHistory(zip_filename, std::move(history_callback));
}

void SafariDataImporter::ImportPasswords(
    const std::string& zip_filename,
    password_manager::PasswordImporter::ImportResultsCallback
        passwords_callback) {
  // TODO(crbug.com/407587751): Import passwords.
  password_manager::ImportResults results;
  std::move(passwords_callback).Run(std::move(results));
}

void SafariDataImporter::ImportPaymentCards(
    const std::string& zip_filename,
    ImportCallback payment_cards_callback) {
  // TODO(crbug.com/407587751): Import payment cards.
  std::move(payment_cards_callback).Run(0);
}

void SafariDataImporter::ImportBookmarks(const std::string& zip_filename,
                                         ImportCallback bookmarks_callback) {
  // TODO(crbug.com/407587751): Import bookmarks.
  std::move(bookmarks_callback).Run(0);
}

void SafariDataImporter::ImportHistory(const std::string& zip_filename,
                                       ImportCallback history_callback) {
  // TODO(crbug.com/407587751): Import history.
  std::move(history_callback).Run(0);
}

void SafariDataImporter::ResolvePasswordConflicts(
    const std::vector<int>& selected_ids,
    password_manager::PasswordImporter::ImportResultsCallback
        passwords_callback) {
  // TODO(crbug.com/407587751): Resolve conflicts.
  password_manager::ImportResults results;
  std::move(passwords_callback).Run(std::move(results));
}

}  // namespace user_data_importer
