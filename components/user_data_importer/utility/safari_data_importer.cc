// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_data_importer/utility/safari_data_importer.h"

#include "base/containers/span_rust.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "components/user_data_importer/utility/zip_ffi_glue.rs.h"

namespace user_data_importer {

// Returns the contents of the file of the desired type contained in the
// provided zip file. Returns an empty string on failure.
std::string unzipFile(const std::string& zip_filename, FileType filetype) {
  // TODO(crbug.com/407587751): Add a base::ScopedBlockingCall object here, once
  // this code has been moved to the IO thread.
  const std::vector<uint8_t> zip_filename_span(zip_filename.begin(),
                                               zip_filename.end());
  rust::Slice<const uint8_t> rs_zip_filename =
      base::SpanToRustSlice(zip_filename_span);
  std::string output_bytes;
  if (!unzip_using_rust(rs_zip_filename, filetype, output_bytes)) {
    return std::string();
  }
  return output_bytes;
}

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

  // TODO(crbug.com/407587751): Run the following code in the IO thread.

  // Passwords import may require conflict resolution, so it is done first.
  ImportPasswords(unzipFile(zip_filename, FileType::Passwords),
                  std::move(passwords_callback));

  // Launch payment cards and bookmarks import processes.
  ImportPaymentCards(unzipFile(zip_filename, FileType::PaymentCards),
                     std::move(payment_cards_callback));
  ImportBookmarks(unzipFile(zip_filename, FileType::Bookmarks),
                  std::move(bookmarks_callback));

  // History import may require synchronously reading from the file, so it is
  // done last.
  ImportHistory(zip_filename, std::move(history_callback));
}

void SafariDataImporter::ImportPasswords(
    std::string csv_data,
    password_manager::PasswordImporter::ImportResultsCallback
        passwords_callback) {
  if (csv_data.empty()) {
    password_manager::ImportResults results;
    std::move(passwords_callback).Run(std::move(results));
    return;
  }

  // TODO(crbug.com/407587751): Pick a store based on whether the user is
  // signed in to their account.
  password_manager::PasswordForm::Store to_store =
      password_manager::PasswordForm::Store::kAccountStore;

  password_importer_->Import(std::move(csv_data), to_store,
                             std::move(passwords_callback));
}

void SafariDataImporter::ImportPaymentCards(
    std::string json_data,
    ImportCallback payment_cards_callback) {
  if (json_data.empty()) {
    std::move(payment_cards_callback).Run(0);
    return;
  }

  // TODO(crbug.com/407587751): Import payment cards.
  std::move(payment_cards_callback).Run(0);
}

void SafariDataImporter::ImportBookmarks(std::string html_data,
                                         ImportCallback bookmarks_callback) {
  if (html_data.empty()) {
    std::move(bookmarks_callback).Run(0);
    return;
  }

  // TODO(crbug.com/407587751): Import bookmarks.
  std::move(bookmarks_callback).Run(0);
}

void SafariDataImporter::ImportHistory(const std::string& zip_filename,
                                       ImportCallback history_callback) {
  // TODO(crbug.com/407587751): Import history.
  // Note: Because the history file can be very large, the parsing will happen
  // entirely in Rust, so that we can stream the unzipper's output to the JSON
  // parser's input.
  std::move(history_callback).Run(0);
}

void SafariDataImporter::ResolvePasswordConflicts(
    const std::vector<int>& selected_ids,
    password_manager::PasswordImporter::ImportResultsCallback
        passwords_callback) {
  password_importer_->ContinueImport(selected_ids,
                                     std::move(passwords_callback));
}

}  // namespace user_data_importer
