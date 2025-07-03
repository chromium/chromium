// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_data_importer/utility/safari_data_importer.h"

#include "base/check_deref.h"
#include "base/containers/span_rust.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/types/expected_macros.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/history/core/browser/history_service.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/user_data_importer/common/imported_bookmark_entry.h"
#include "components/user_data_importer/utility/history_callback_from_rust.h"
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

std::optional<history::URLRow> ConvertToURLRow(
    const user_data_importer::HistoryEntry& history_entry) {
  GURL gurl(RustStringToUTF16(history_entry.url));
  if (!gurl.is_valid()) {
    return std::nullopt;
  }

  history::URLRow url_row(gurl);
  url_row.set_title(RustStringToUTF16(history_entry.title));
  url_row.set_visit_count(history_entry.visit_count);

  // "time_usec" is a UNIX timestamp in microseconds.
  url_row.set_last_visit(base::Time::UnixEpoch() +
                         base::Microseconds(history_entry.time_usec));

  return url_row;
}

class SafariDataImporterHistoryCallback final
    : public user_data_importer::HistoryCallbackFromRust {
 public:
  using ParseHistoryCallback = base::RepeatingCallback<void(
      std::vector<user_data_importer::HistoryEntry>,
      user_data_importer::SafariDataImporter::ImportCallback)>;

  explicit SafariDataImporterHistoryCallback(
      ParseHistoryCallback parse_history_callback,
      user_data_importer::SafariDataImporter::ImportCallback done_callback)
      : parse_history_callback_(parse_history_callback),
        done_callback_(std::move(done_callback)) {}

  ~SafariDataImporterHistoryCallback() override = default;

  // Callback called while parsing the history file.
  void ImportHistoryEntries(
      std::vector<user_data_importer::HistoryEntry>& history_entries,
      bool completed) override {
    parse_history_callback_.Run(
        std::move(history_entries),
        completed ? std::move(done_callback_) : base::DoNothing());

    // "history_entries" should be empty after std::move, but make sure it is in
    // a valid state by explicitly calling "clear" on it.
    history_entries.clear();
  }

 private:
  ParseHistoryCallback parse_history_callback_;

  user_data_importer::SafariDataImporter::ImportCallback done_callback_;
};

}  // namespace

namespace user_data_importer {

SafariDataImporter::SafariDataImporter(
    password_manager::SavedPasswordsPresenter* presenter,
    autofill::PaymentsDataManager* payments_data_manager,
    history::HistoryService* history_service,
    bookmarks::BookmarkModel* bookmark_model,
    ReadingListModel* reading_list_model,
    std::unique_ptr<BookmarkParser> bookmark_parser,
    std::string app_locale)
    : password_importer_(std::make_unique<password_manager::PasswordImporter>(
          presenter,
          /*user_confirmation_required=*/true)),
      payments_data_manager_(CHECK_DEREF(payments_data_manager)),
      history_service_(CHECK_DEREF(history_service)),
      bookmark_model_(CHECK_DEREF(bookmark_model)),
      reading_list_model_(CHECK_DEREF(reading_list_model)),
      bookmark_parser_(std::move(bookmark_parser)),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      app_locale_(std::move(app_locale)) {}

SafariDataImporter::~SafariDataImporter() {
  // If bookmarks_temp_dir_ is not empty, deleting the object will result in a
  // directory (and file) deletion operation on this thread, which is not
  // allowed, since any file IO operation must be done in a worker thread. To
  // make sure this isn't an issue, call LaunchDeleteBookmarksDir(), which will
  // transfer the ownership of the ScopedTempDir object to a worker thread to
  // perform the deletion there.
  if (bookmarks_temp_dir_) {
    LaunchDeleteBookmarksDir();
  }
}

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
  // The history import process is the only one requiring reading the zip file,
  // so launch it first.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&SafariDataImporter::ImportHistory, base::Unretained(this),
                     std::move(history_callback))
          .Then(base::BindOnce(&SafariDataImporter::CloseZipFileArchive,
                               base::Unretained(this))));

  // TODO(crbug.com/407587751): Launch task on task_runner_.
  password_importer_->ContinueImport(selected_password_ids,
                                     std::move(passwords_callback));

  // TODO(crbug.com/407587751): Import other types here.
  PostCallback(std::move(bookmarks_callback), /*number_of_imports=*/0);

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SafariDataImporter::ContinueImportPaymentCards,
                                base::Unretained(this),
                                std::move(payment_cards_callback)));
}

// Called after calling "Import" in order to cancel the import process.
void SafariDataImporter::CancelImport() {
  // TODO(crbug.com/407587751): Notify password_importer_.

  CloseZipFileArchive();
}

void SafariDataImporter::CloseZipFileArchive() {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&SafariDataImporter::CloseZipFileArchiveInWorkerThread,
                     base::Unretained(this)));
}

void SafariDataImporter::CloseZipFileArchiveInWorkerThread() {
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

size_t SafariDataImporter::UncompressedFileSizeInBytes(FileType filetype) {
  return zip_file_archive_ ? (*zip_file_archive_)->get_file_size_bytes(filetype)
                           : 0u;
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

std::optional<base::FilePath> SafariDataImporter::WriteBookmarksToTmpFile(
    const std::string& html_data) {
  if (html_data.empty()) {
    return std::nullopt;
  }

  bookmarks_temp_dir_ = std::make_unique<base::ScopedTempDir>();
  if (!bookmarks_temp_dir_->CreateUniqueTempDir()) {
    return std::nullopt;
  }

  base::FilePath path =
      bookmarks_temp_dir_->GetPath().AppendASCII("bookmarks.html");

  if (!base::WriteFile(path, html_data)) {
    bookmarks_temp_dir_ = nullptr;
    return std::nullopt;
  }
  return path;
}

void SafariDataImporter::LaunchDeleteBookmarksDir() {
  // Launch a low priority task to delete the bookmarks temp directory. Note
  // that bookmarks_temp_dir_ will be nullptr after std::move() is done, as the
  // ownership of the ScopedTempDir object will be transferred to temp_dir.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(
          [](std::unique_ptr<base::ScopedTempDir> temp_dir) {
            temp_dir = nullptr;
          },
          std::move(bookmarks_temp_dir_)));
}

void SafariDataImporter::LaunchImportBookmarksTask(
    ImportCallback bookmarks_callback) {
  // Unzip the file and write the contents to a tmp file.
  std::string html_data = Unzip(FileType::Bookmarks);

  ASSIGN_OR_RETURN(base::FilePath path, WriteBookmarksToTmpFile(html_data),
                   [this, &bookmarks_callback](auto) {
                     // TODO(crbug.com/407587751): Log error.
                     PostCallback(std::move(bookmarks_callback),
                                  /*number_of_imports=*/0);
                   });

  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&SafariDataImporter::ImportBookmarks,
                                        base::Unretained(this), path,
                                        std::move(bookmarks_callback)));
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

void SafariDataImporter::ContinueImportPaymentCards(
    ImportCallback payment_cards_callback) {
  if (cards_to_import_.empty()) {
    PostCallback(std::move(payment_cards_callback), /*number_of_imports=*/0);
    return;
  }

  size_t imported_credit_cards = 0u;

  for (const auto& credit_card : cards_to_import_) {
    if (!credit_card.IsValid()) {
      continue;
    }

    const autofill::CreditCard* existing_card =
        payments_data_manager_->GetCreditCardByNumber(
            base::UTF16ToUTF8(credit_card.number()));

    // If a local card with the same number already exists, update it.
    if (existing_card && existing_card->record_type() ==
                             autofill::CreditCard::RecordType::kLocalCard) {
      payments_data_manager_->UpdateCreditCard(credit_card);
    } else {
      payments_data_manager_->AddCreditCard(credit_card);
    }

    imported_credit_cards++;
  }

  PostCallback(std::move(payment_cards_callback), imported_credit_cards);
}

void SafariDataImporter::ImportBookmarks(base::FilePath bookmarks_html,
                                         ImportCallback bookmarks_callback) {
  CHECK(!bookmarks_html.empty());

  bookmark_parser_->Parse(
      bookmarks_html,
      base::BindOnce(&SafariDataImporter::OnBookmarksParsed,
                     // Safe to bind to WeakPtr because this runs on the
                     // same sequence where the factory was created.
                     weak_factory_.GetWeakPtr(),
                     std::move(bookmarks_callback)));
}

void SafariDataImporter::OnBookmarksParsed(
    ImportCallback callback,
    BookmarkParser::BookmarkParsingResult result) {
  LaunchDeleteBookmarksDir();

  ASSIGN_OR_RETURN(BookmarkParser::ParsedBookmarks value, std::move(result),
                   [this, &callback](auto) {
                     // TODO(crbug.com/407587751): Log error to UMA.
                     PostCallback(std::move(callback), 0);
                   });

  pending_bookmarks_ = std::move(value.bookmarks);
  pending_reading_list_ = std::move(value.reading_list);

  PostCallback(std::move(callback),
               pending_bookmarks_.size() + pending_reading_list_.size());
}

void SafariDataImporter::StartImportHistory(ImportCallback history_callback) {
  // This is an approximation of the number of bytes per URL entry in the
  // history file.
  static const size_t kBytesPerURL = 250;
  size_t file_size_bytes = UncompressedFileSizeInBytes(FileType::History);
  size_t approximate_number_of_urls =
      (file_size_bytes > 0) ? (file_size_bytes / kBytesPerURL) + 1 : 0;
  PostCallback(std::move(history_callback),
               /*number_of_imports=*/approximate_number_of_urls);
}

void SafariDataImporter::ParseHistoryCallback(
    std::vector<HistoryEntry> history_entries,
    ImportCallback history_callback) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SafariDataImporter::ImportHistoryEntries,
                     base::Unretained(this), std::move(history_entries),
                     std::move(history_callback)));
}

void SafariDataImporter::ImportHistoryEntries(
    std::vector<HistoryEntry> history_entries,
    ImportCallback history_callback) {
  history::URLRows url_rows;
  url_rows.reserve(history_entries.size());
  for (auto history_entry : history_entries) {
    auto opt_row = ConvertToURLRow(history_entry);
    if (opt_row) {
      url_rows.push_back(opt_row.value());
    }
  }

  if (!url_rows.empty()) {
    history_service_->AddPagesWithDetails(url_rows,
                                          history::SOURCE_SAFARI_IMPORTED);

    history_urls_imported_ += url_rows.size();
  }
  PostCallback(std::move(history_callback), history_urls_imported_);
}

void SafariDataImporter::ImportHistory(ImportCallback history_callback) {
  history_urls_imported_ = 0;
  if (!zip_file_archive_) {
    PostCallback(std::move(history_callback), /*number_of_imports=*/0);
    return;
  }

  (*zip_file_archive_)
      ->parse_history(
          std::make_unique<SafariDataImporterHistoryCallback>(
              base::BindRepeating(&SafariDataImporter::ParseHistoryCallback,
                                  base::Unretained(this)),
              std::move(history_callback)),
          history_size_threshold_);
}

}  // namespace user_data_importer
