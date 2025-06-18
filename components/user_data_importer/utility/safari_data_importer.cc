// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_data_importer/utility/safari_data_importer.h"

#include "base/containers/span_rust.h"
#include "base/files/file_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "components/user_data_importer/utility/safari_data_import_manager.h"
#include "components/user_data_importer/utility/zip_ffi_glue.rs.h"

namespace {

std::u16string RustStringToUTF16(const rust::String& rust_string) {
  return base::UTF8ToUTF16(
      std::string_view(rust_string.data(), rust_string.length()));
}

autofill::CreditCard ConvertToAutofillCreditCard(
    const user_data_importer::PaymentCardEntry& card,
    const std::string& app_locale) {
  autofill::CreditCard credit_card;

  credit_card.SetNumber(RustStringToUTF16(card.card_number));
  credit_card.SetNickname(RustStringToUTF16(card.card_name));
  credit_card.SetExpirationMonth(card.card_expiration_month);
  credit_card.SetExpirationYear(card.card_expiration_year);

  // Import all cards as local cards initially. Adding other card types
  // (server, etc) is too complex for an import flow.
  credit_card.set_record_type(autofill::CreditCard::RecordType::kLocalCard);

  credit_card.SetInfo(
      autofill::CREDIT_CARD_NAME_FULL,
      base::UTF8ToUTF16(std::string_view(card.cardholder_name.data(),
                                         card.cardholder_name.length())),
      app_locale);

  return credit_card;
}

}  // namespace

namespace user_data_importer {

SafariDataImporter::SafariDataImporter(
    password_manager::SavedPasswordsPresenter* presenter,
    std::unique_ptr<SafariDataImportManager> manager,
    std::string app_locale)
    : password_importer_(std::make_unique<password_manager::PasswordImporter>(
          presenter,
          /*user_confirmation_required=*/true)),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      manager_(std::move(manager)),
      app_locale_(std::move(app_locale)) {}

SafariDataImporter::~SafariDataImporter() = default;

void SafariDataImporter::StartImport(const base::FilePath& path,
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
      base::BindOnce(&SafariDataImporter::ImportInWorkerThread,
                     base::Unretained(this), std::move(zip_filename),
                     std::move(passwords_callback),
                     std::move(bookmarks_callback), std::move(history_callback),
                     std::move(payment_cards_callback)));
}

void SafariDataImporter::ContinueImport(
    const std::vector<int>& selected_password_ids,
    PasswordImportCallback passwords_callback,
    ImportCallback bookmarks_callback,
    ImportCallback history_callback,
    ImportCallback payment_cards_callback) {
  // TODO(crbug.com/407587751): Launch task on task_runner_.
  password_importer_->ContinueImport(selected_password_ids,
                                     std::move(passwords_callback));

  // TODO(crbug.com/407587751): Import other types here.
  PostCallback(std::move(bookmarks_callback), /*number_of_imports=*/0);
  PostCallback(std::move(payment_cards_callback), /*number_of_imports=*/0);

  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&SafariDataImporter::ImportHistory, base::Unretained(this),
                     std::move(history_callback))
          .Then(base::BindOnce(&SafariDataImporter::CloseZipFileArchive,
                               base::Unretained(this))));
}

// Called after calling "Import" in order to cancel the import process.
void SafariDataImporter::CancelImport() {
  // TODO(crbug.com/407587751): Notify password_importer_.

  CloseZipFileArchive();
}

void SafariDataImporter::CloseZipFileArchive() {
  zip_file_archive_.reset();
}

bool SafariDataImporter::CreateZipFileArchive(std::string zip_filename) {
  const std::vector<uint8_t> zip_filename_span(zip_filename.begin(),
                                               zip_filename.end());
  rust::Slice<const uint8_t> rs_zip_filename =
      base::SpanToRustSlice(zip_filename_span);
  rust::Box<ResultOfZipFileArchive> result_of_zip_file_archive =
      new_archive(rs_zip_filename);

  if (result_of_zip_file_archive->err()) {
    return false;
  }

  zip_file_archive_.emplace(result_of_zip_file_archive->unwrap());
  return true;
}

std::string SafariDataImporter::Unzip(FileType filetype) {
  std::string output_bytes;
  if (!zip_file_archive_ ||
      !(*zip_file_archive_)->unzip(filetype, output_bytes)) {
    return std::string();
  }
  return output_bytes;
}

size_t SafariDataImporter::UncompressedFileSize(FileType filetype) {
  return zip_file_archive_ ? (*zip_file_archive_)->get_file_size(filetype) : 0u;
}

void SafariDataImporter::ImportInWorkerThread(
    std::string zip_filename,
    PasswordImportCallback passwords_callback,
    ImportCallback bookmarks_callback,
    ImportCallback history_callback,
    ImportCallback payment_cards_callback) {
  if (!CreateZipFileArchive(std::move(zip_filename))) {
    // Nothing to import, early exit.
    PasswordImportResults results;
    PostCallback(std::move(passwords_callback), std::move(results));
    PostCallback(std::move(bookmarks_callback), /*number_of_imports=*/0);
    PostCallback(std::move(history_callback), /*number_of_imports=*/0);
    PostCallback(std::move(payment_cards_callback), /*number_of_imports=*/0);
    return;
  }

  // Passwords import may require conflict resolution, so it is done first.
  LaunchImportPasswordsTask(std::move(passwords_callback));

  // Launch payment cards and bookmarks import processes.
  LaunchImportPaymentCardsTask(std::move(payment_cards_callback));
  LaunchImportBookmarksTask(std::move(bookmarks_callback));

  // History import may require synchronously reading from the file, so it is
  // done last in this thread.
  StartImportHistory(std::move(history_callback));
}

void SafariDataImporter::LaunchImportBookmarksTask(
    ImportCallback bookmarks_callback) {
  std::string html_data = Unzip(FileType::Bookmarks);
  if (html_data.empty()) {
    PostCallback(std::move(bookmarks_callback), /*number_of_imports=*/0);
  } else {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&SafariDataImporter::ImportBookmarks,
                                  base::Unretained(this), std::move(html_data),
                                  std::move(bookmarks_callback)));
  }
}

void SafariDataImporter::LaunchImportPasswordsTask(
    PasswordImportCallback passwords_callback) {
  std::string csv_data = Unzip(FileType::Passwords);
  if (csv_data.empty()) {
    PasswordImportResults results;
    PostCallback(std::move(passwords_callback), std::move(results));
  } else {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&SafariDataImporter::ImportPasswords,
                                  base::Unretained(this), std::move(csv_data),
                                  std::move(passwords_callback)));
  }
}

void SafariDataImporter::LaunchImportPaymentCardsTask(
    ImportCallback payment_cards_callback) {
  std::vector<PaymentCardEntry> payment_cards;
  if (!zip_file_archive_ ||
      !(*zip_file_archive_)->parse_payment_cards(payment_cards)) {
    PostCallback(std::move(payment_cards_callback), /*number_of_imports=*/0);
  } else {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&SafariDataImporter::ImportPaymentCards,
                       base::Unretained(this), std::move(payment_cards),
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
    std::vector<PaymentCardEntry> payment_cards,
    ImportCallback payment_cards_callback) {
  if (payment_cards.empty()) {
    PostCallback(std::move(payment_cards_callback), /*number_of_imports=*/0);
    return;
  }

  cards_to_import_.clear();

  cards_to_import_.reserve(payment_cards.size());

  std::ranges::transform(payment_cards, std::back_inserter(cards_to_import_),
                         [this](const auto& card) {
                           return ConvertToAutofillCreditCard(card,
                                                              app_locale_);
                         });

  PostCallback(std::move(payment_cards_callback), cards_to_import_.size());
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

void SafariDataImporter::StartImportHistory(ImportCallback history_callback) {
  // This is an approximation of the number of bytes per URL entry in the
  // history file.
  static const size_t kBytesPerURL = 250;
  size_t file_size = UncompressedFileSize(FileType::History);
  size_t approximate_number_of_urls =
      (file_size > 0) ? (file_size / kBytesPerURL) + 1 : 0;
  PostCallback(std::move(history_callback),
               /*number_of_imports=*/approximate_number_of_urls);
}

void SafariDataImporter::ImportHistory(ImportCallback history_callback) {
  // Note: Because the history file can be very large, the parsing will happen
  // entirely in Rust, so that we can stream the unzipper's output to the JSON
  // parser's input.
  std::vector<HistoryEntry> history_entries;
  if (!zip_file_archive_ ||
      !(*zip_file_archive_)->parse_history(history_entries)) {
    PostCallback(std::move(history_callback), /*number_of_imports=*/0);
    return;
  }

  // TODO(crbug.com/407587751): Save imported history.

  PostCallback(std::move(history_callback), history_entries.size());
}

}  // namespace user_data_importer
