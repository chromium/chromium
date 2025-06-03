// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_data_importer/utility/safari_data_importer.h"

#include "base/containers/span_rust.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/thread_pool.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "components/user_data_importer/utility/zip_ffi_glue.rs.h"

namespace user_data_importer {

// Returns the contents of the file of the desired type contained in the
// provided zip file. Returns an empty string on failure.
std::string unzipFile(const std::string& zip_filename, FileType filetype) {
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
    : password_importer_(std::make_unique<password_manager::PasswordImporter>(
          presenter,
          /*user_confirmation_required=*/true)),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

SafariDataImporter::~SafariDataImporter() = default;

base::WeakPtr<SafariDataImporter> SafariDataImporter::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void SafariDataImporter::Import(const base::FilePath& path,
                                PasswordImportCallback passwords_callback,
                                ImportCallback bookmarks_callback,
                                ImportCallback history_callback,
                                ImportCallback payment_cards_callback) {
  std::string zip_filename = path.MaybeAsASCII();
  if (zip_filename.empty()) {
    // Nothing to import, early exit.
    PasswordImportResults results;
    PostCallback(std::move(passwords_callback), std::move(results));
    PostCallback(std::move(bookmarks_callback), /*number_of_imports=*/0);
    PostCallback(std::move(history_callback), /*number_of_imports=*/0);
    PostCallback(std::move(payment_cards_callback), /*number_of_imports=*/0);
    return;
  }

  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&SafariDataImporter::ImportInWorkerThread, AsWeakPtr(),
                     std::move(zip_filename), std::move(passwords_callback),
                     std::move(bookmarks_callback), std::move(history_callback),
                     std::move(payment_cards_callback)));
}

void SafariDataImporter::ImportInWorkerThread(
    std::string zip_filename,
    PasswordImportCallback passwords_callback,
    ImportCallback bookmarks_callback,
    ImportCallback history_callback,
    ImportCallback payment_cards_callback) {
  // Passwords import may require conflict resolution, so it is done first.
  LaunchImportPasswordsTask(zip_filename, std::move(passwords_callback));

  // Launch payment cards and bookmarks import processes.
  LaunchImportPaymentCardsTask(zip_filename, std::move(payment_cards_callback));
  LaunchImportBookmarksTask(zip_filename, std::move(bookmarks_callback));

  // History import may require synchronously reading from the file, so it is
  // done last in this thread.
  ImportHistory(zip_filename, std::move(history_callback));
}

void SafariDataImporter::LaunchImportBookmarksTask(
    const std::string& zip_filename,
    ImportCallback bookmarks_callback) {
  std::string html_data = unzipFile(zip_filename, FileType::Bookmarks);
  if (html_data.empty()) {
    PostCallback(std::move(bookmarks_callback), /*number_of_imports=*/0);
  } else {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&SafariDataImporter::ImportBookmarks, AsWeakPtr(),
                       std::move(html_data), std::move(bookmarks_callback)));
  }
}

void SafariDataImporter::LaunchImportPasswordsTask(
    const std::string& zip_filename,
    PasswordImportCallback passwords_callback) {
  std::string csv_data = unzipFile(zip_filename, FileType::Passwords);
  if (csv_data.empty()) {
    PasswordImportResults results;
    PostCallback(std::move(passwords_callback), std::move(results));
  } else {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&SafariDataImporter::ImportPasswords, AsWeakPtr(),
                       std::move(csv_data), std::move(passwords_callback)));
  }
}

void SafariDataImporter::LaunchImportPaymentCardsTask(
    const std::string& zip_filename,
    ImportCallback payment_cards_callback) {
  std::string json_data = unzipFile(zip_filename, FileType::PaymentCards);
  if (json_data.empty()) {
    PostCallback(std::move(payment_cards_callback), /*number_of_imports=*/0);
  } else {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&SafariDataImporter::ImportPaymentCards,
                                  AsWeakPtr(), std::move(json_data),
                                  std::move(payment_cards_callback)));
  }
}

void SafariDataImporter::PostCallback(auto callback, auto results) {
  // Post the callback back to the original task runner.
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(
                             [](auto callback, auto results) {
                               std::move(callback).Run(std::move(results));
                             },
                             std::move(callback), std::move(results)));
}

void SafariDataImporter::ImportPasswords(
    std::string csv_data,
    PasswordImportCallback passwords_callback) {
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
    PostCallback(std::move(payment_cards_callback), /*number_of_imports=*/0);
    return;
  }

  // TODO(crbug.com/407587751): Import payment cards.
  PostCallback(std::move(payment_cards_callback), /*number_of_imports=*/0);
}

void SafariDataImporter::ImportBookmarks(std::string html_data,
                                         ImportCallback bookmarks_callback) {
  if (html_data.empty()) {
    PostCallback(std::move(bookmarks_callback), /*number_of_imports=*/0);
    return;
  }

  // TODO(crbug.com/407587751): Import bookmarks.
  PostCallback(std::move(bookmarks_callback), /*number_of_imports=*/0);
}

void SafariDataImporter::ImportHistory(const std::string& zip_filename,
                                       ImportCallback history_callback) {
  // TODO(crbug.com/407587751): Import history.
  // Note: Because the history file can be very large, the parsing will happen
  // entirely in Rust, so that we can stream the unzipper's output to the JSON
  // parser's input.
  PostCallback(std::move(history_callback), /*number_of_imports=*/0);
}

void SafariDataImporter::ContinuePasswordImport(
    const std::vector<int>& selected_ids,
    PasswordImportCallback passwords_callback) {
  password_importer_->ContinueImport(selected_ids,
                                     std::move(passwords_callback));
}

}  // namespace user_data_importer
