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
#include "base/task/bind_post_task.h"
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

std::string_view RustStringToStringView(const rust::String& rust_string) {
  return std::string_view(rust_string.data(), rust_string.length());
}

std::u16string RustStringToUTF16(const rust::String& rust_string) {
  return base::UTF8ToUTF16(RustStringToStringView(rust_string));
}

scoped_refptr<base::SequencedTaskRunner> GetRunner() {
  return base::SequencedTaskRunner::GetCurrentDefault();
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

  credit_card.SetInfo(autofill::CREDIT_CARD_NAME_FULL,
                      RustStringToUTF16(card.cardholder_name), app_locale);

  return credit_card;
}

bool IsRedirect(const GURL& source_url, const GURL& destination_url) {
  // If URLs are identical strings, it's not a redirect.
  if (source_url == destination_url) {
    return false;
  }

  // Check if URLs are valid.
  if (!source_url.is_valid() || !destination_url.is_valid()) {
    return false;  // Cannot reliably determine redirect if URLs are unparsable.
  }

  // Check for differences in scheme.
  if ((source_url.has_scheme() != destination_url.has_scheme()) ||
      (source_url.has_scheme() && destination_url.has_scheme() &&
       !source_url.SchemeIs(destination_url.scheme()))) {
    return true;
  }

  // Check for differences in host.
  if ((source_url.has_host() != destination_url.has_host()) ||
      (source_url.has_host() && destination_url.has_host() &&
       source_url.host() != destination_url.host())) {
    return true;
  }

  // Check for differences in path.
  if ((source_url.has_path() != destination_url.has_path()) ||
      (source_url.has_path() && destination_url.has_path() &&
       source_url.path() != destination_url.path())) {
    return true;
  }

  // Check for specific redirect pattern: source has no query, but destination
  // does.
  if (!source_url.has_query() && destination_url.has_query()) {
    return true;
  }

  // If none of the above conditions are met, it's not considered a redirect
  // by this logic (e.g., only fragment changes, or query changes where source
  // already had a query).
  return false;
}

// Returns whether to skip this history entry.
bool IsSkippedEntry(const user_data_importer::HistoryEntry& entry) {
  // If either source or destination URL is missing, we can't determine if this
  // entry should be skipped.
  if (entry.source_url.empty() || entry.destination_url.empty()) {
    return false;
  }

  // Parse URLs using GURL.
  GURL source_url(RustStringToStringView(entry.source_url));
  GURL destination_url(RustStringToStringView(entry.destination_url));

  // Only import history entries if the scheme is http or https.
  if ((source_url.has_scheme() && !source_url.SchemeIs(url::kHttpsScheme) &&
       !source_url.SchemeIs(url::kHttpScheme)) ||
      (destination_url.has_scheme() &&
       !destination_url.SchemeIs(url::kHttpsScheme) &&
       !destination_url.SchemeIs(url::kHttpScheme))) {
    return true;
  }

  // Redirects are skipped.
  return IsRedirect(source_url, destination_url);
}

std::optional<history::URLRow> ConvertToURLRow(
    const user_data_importer::HistoryEntry& history_entry) {
  GURL gurl(RustStringToStringView(history_entry.url));
  if (!gurl.is_valid() || IsSkippedEntry(history_entry)) {
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

}  // namespace

namespace user_data_importer {

// Object used to allow Rust History import pipeline to communicate results
// back to this importer.
class RustHistoryCallback final
    : public user_data_importer::HistoryCallbackFromRust {
 public:
  using ParseHistoryCallback = base::RepeatingCallback<void(
      std::vector<user_data_importer::HistoryEntry>,
      user_data_importer::SafariDataImporter::ImportCallback)>;

  explicit RustHistoryCallback(
      ParseHistoryCallback parse_history_callback,
      user_data_importer::SafariDataImporter::ImportCallback done_callback)
      : parse_history_callback_(parse_history_callback),
        done_callback_(std::move(done_callback)) {}

  ~RustHistoryCallback() override = default;

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

  // Calls `done_callback_` with 0 to signal that parsing has failed.
  void Fail() { std::move(done_callback_).Run(0); }

 private:
  ParseHistoryCallback parse_history_callback_;

  user_data_importer::SafariDataImporter::ImportCallback done_callback_;
};

SafariDataImporter::SafariDataImporter(
    password_manager::SavedPasswordsPresenter* presenter,
    autofill::PaymentsDataManager* payments_data_manager,
    history::HistoryService* history_service,
    bookmarks::BookmarkModel* bookmark_model,
    ReadingListModel* reading_list_model,
    std::unique_ptr<BookmarkParser> bookmark_parser,
    std::string app_locale)
    : blocking_queue_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE})),
      blocking_worker_(blocking_queue_),
      password_importer_(std::make_unique<password_manager::PasswordImporter>(
          presenter,
          /*user_confirmation_required=*/true)),
      payments_data_manager_(CHECK_DEREF(payments_data_manager)),
      history_service_(CHECK_DEREF(history_service)),
      bookmark_model_(CHECK_DEREF(bookmark_model)),
      reading_list_model_(CHECK_DEREF(reading_list_model)),
      bookmark_parser_(std::move(bookmark_parser)),
      app_locale_(std::move(app_locale)) {}

SafariDataImporter::~SafariDataImporter() = default;

void SafariDataImporter::PrepareImport(
    const base::FilePath& path,
    PasswordImportCallback passwords_callback,
    ImportCallback bookmarks_callback,
    ImportCallback history_callback,
    ImportCallback payment_cards_callback) {
  std::string zip_filename = path.MaybeAsASCII();
  if (zip_filename.empty()) {
    // Nothing to import, early exit.
    std::move(passwords_callback).Run({});
    std::move(bookmarks_callback).Run(/*number_of_imports=*/0);
    std::move(history_callback).Run(/*number_of_imports=*/0);
    std::move(payment_cards_callback).Run(/*number_of_imports=*/0);
    return;
  }

  blocking_worker_.AsyncCall(&BlockingWorker::CreateZipFileArchive)
      .WithArgs(std::move(zip_filename))
      .Then(base::BindOnce(
          &SafariDataImporter::OnZipArchiveReady, weak_factory_.GetWeakPtr(),
          std::move(passwords_callback), std::move(bookmarks_callback),
          std::move(history_callback), std::move(payment_cards_callback)));
}

void SafariDataImporter::CompleteImport(
    const std::vector<int>& selected_password_ids,
    PasswordImportCallback passwords_callback,
    ImportCallback bookmarks_callback,
    ImportCallback history_callback,
    ImportCallback payment_cards_callback) {
  // The history import process is the only one requiring reading the zip file,
  // so launch it first.
  history_urls_imported_ = 0;
  RustHistoryCallback::ParseHistoryCallback parse_callback = base::BindPostTask(
      GetRunner(),
      base::BindRepeating(&SafariDataImporter::ImportHistoryEntries,
                          weak_factory_.GetWeakPtr()));

  blocking_worker_.AsyncCall(&BlockingWorker::ImportHistory)
      .WithArgs(std::make_unique<RustHistoryCallback>(
                    std::move(parse_callback), std::move(history_callback)),
                history_size_threshold_);

  // TODO(crbug.com/407587751): Move this to a task.
  password_importer_->ContinueImport(selected_password_ids,
                                     std::move(passwords_callback));

  // TODO(crbug.com/407587751): Import other types here.
  std::move(bookmarks_callback).Run(/*number_of_imports=*/0);

  GetRunner()->PostTask(
      FROM_HERE, base::BindOnce(&SafariDataImporter::ContinueImportPaymentCards,
                                weak_factory_.GetWeakPtr(),
                                std::move(payment_cards_callback)));
}

// Called after calling "Import" in order to cancel the import process.
void SafariDataImporter::CancelImport() {
  // TODO(crbug.com/407587751): Notify password_importer_.

  blocking_worker_.AsyncCall(&BlockingWorker::CloseZipFileArchive);
}

SafariDataImporter::BlockingWorker::BlockingWorker() = default;
SafariDataImporter::BlockingWorker::~BlockingWorker() = default;

bool SafariDataImporter::BlockingWorker::CreateZipFileArchive(
    std::string zip_filename) {
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

void SafariDataImporter::BlockingWorker::CloseZipFileArchive() {
  zip_file_archive_.reset();
}

std::string SafariDataImporter::BlockingWorker::Unzip(FileType filetype) {
  std::string output_bytes;
  if (!zip_file_archive_ ||
      !(*zip_file_archive_)->unzip(filetype, output_bytes)) {
    return std::string();
  }
  return output_bytes;
}

size_t SafariDataImporter::BlockingWorker::GetUncompressedFileSizeInBytes(
    FileType filetype) {
  return zip_file_archive_ ? (*zip_file_archive_)->get_file_size_bytes(filetype)
                           : 0u;
}

std::optional<base::FilePath>
SafariDataImporter::BlockingWorker::WriteBookmarksToTmpFile() {
  std::string html_data = Unzip(FileType::Bookmarks);

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
    return std::nullopt;
  }
  return path;
}

std::vector<PaymentCardEntry>
SafariDataImporter::BlockingWorker::ParsePaymentCards() {
  std::vector<PaymentCardEntry> payment_cards;
  if (!zip_file_archive_ ||
      !(*zip_file_archive_)->parse_payment_cards(payment_cards)) {
    return {};
  }
  return payment_cards;
}

void SafariDataImporter::BlockingWorker::ImportHistory(
    std::unique_ptr<RustHistoryCallback> callback,
    size_t history_size_threshold) {
  if (!zip_file_archive_) {
    callback->Fail();
    return;
  }

  (*zip_file_archive_)
      ->parse_history(std::move(callback), history_size_threshold);

  CloseZipFileArchive();
}

void SafariDataImporter::OnZipArchiveReady(
    PasswordImportCallback passwords_callback,
    ImportCallback bookmarks_callback,
    ImportCallback history_callback,
    ImportCallback payment_cards_callback,
    bool success) {
  if (!success) {
    // Nothing to import, early exit.
    PasswordImportResults results;
    std::move(passwords_callback).Run(std::move(results));
    std::move(bookmarks_callback).Run(/*number_of_imports=*/0);
    std::move(history_callback).Run(/*number_of_imports=*/0);
    std::move(payment_cards_callback).Run(/*number_of_imports=*/0);
    return;
  }

  // Passwords import may require conflict resolution, so it is done first.
  blocking_worker_.AsyncCall(&BlockingWorker::Unzip)
      .WithArgs(FileType::Passwords)
      .Then(base::BindOnce(&SafariDataImporter::PreparePasswords,
                           weak_factory_.GetWeakPtr(),
                           std::move(passwords_callback)));

  blocking_worker_.AsyncCall(&BlockingWorker::ParsePaymentCards)
      .Then(base::BindOnce(&SafariDataImporter::PreparePaymentCards,
                           weak_factory_.GetWeakPtr(),
                           std::move(payment_cards_callback)));

  blocking_worker_.AsyncCall(&BlockingWorker::WriteBookmarksToTmpFile)
      .Then(base::BindOnce(&SafariDataImporter::PrepareBookmarks,
                           weak_factory_.GetWeakPtr(),
                           std::move(bookmarks_callback)));

  blocking_worker_.AsyncCall(&BlockingWorker::GetUncompressedFileSizeInBytes)
      .WithArgs(FileType::History)
      .Then(base::BindOnce(&SafariDataImporter::PrepareHistory,
                           weak_factory_.GetWeakPtr(),
                           std::move(history_callback)));
}

void SafariDataImporter::PreparePasswords(
    PasswordImportCallback passwords_callback,
    std::string csv_data) {
  // TODO(crbug.com/407587751): Pick a store based on whether the user is
  // signed in to their account.
  password_manager::PasswordForm::Store to_store =
      password_manager::PasswordForm::Store::kAccountStore;

  password_importer_->Import(std::move(csv_data), to_store,
                             std::move(passwords_callback));
}

void SafariDataImporter::PreparePaymentCards(
    ImportCallback payment_cards_callback,
    std::vector<PaymentCardEntry> payment_cards) {
  if (payment_cards.empty()) {
    std::move(payment_cards_callback).Run(/*number_of_imports=*/0);
    return;
  }

  cards_to_import_.clear();
  cards_to_import_.reserve(payment_cards.size());
  std::ranges::transform(payment_cards, std::back_inserter(cards_to_import_),
                         [this](const auto& card) {
                           return ConvertToAutofillCreditCard(card,
                                                              app_locale_);
                         });

  std::move(payment_cards_callback).Run(cards_to_import_.size());
}

void SafariDataImporter::PrepareBookmarks(
    ImportCallback bookmarks_callback,
    std::optional<base::FilePath> bookmarks_html) {
  if (!bookmarks_html || bookmarks_html->empty()) {
    // TODO(crbug.com/407587751): Log error.
    std::move(bookmarks_callback).Run(/*number_of_imports=*/0);
    return;
  }

  bookmark_parser_->Parse(
      *bookmarks_html,
      base::BindPostTask(GetRunner(),
                         base::BindOnce(&SafariDataImporter::OnBookmarksParsed,
                                        weak_factory_.GetWeakPtr(),
                                        std::move(bookmarks_callback))));
}

void SafariDataImporter::OnBookmarksParsed(
    ImportCallback callback,
    BookmarkParser::BookmarkParsingResult result) {
  ASSIGN_OR_RETURN(BookmarkParser::ParsedBookmarks value, std::move(result),
                   [&callback](auto) {
                     // TODO(crbug.com/407587751): Log error to UMA.
                     std::move(callback).Run(0);
                   });

  pending_bookmarks_ = std::move(value.bookmarks);
  pending_reading_list_ = std::move(value.reading_list);

  std::move(callback).Run(pending_bookmarks_.size() +
                          pending_reading_list_.size());
}

void SafariDataImporter::PrepareHistory(ImportCallback history_callback,
                                        size_t file_size_bytes) {
  // This is an approximation of the number of bytes per URL entry in the
  // history file.
  static const size_t kBytesPerURL = 250;
  size_t approximate_number_of_urls =
      (file_size_bytes > 0) ? (file_size_bytes / kBytesPerURL) + 1 : 0;
  std::move(history_callback)
      .Run(/*number_of_imports=*/approximate_number_of_urls);
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
  std::move(history_callback).Run(history_urls_imported_);
}

void SafariDataImporter::ContinueImportPaymentCards(
    ImportCallback payment_cards_callback) {
  if (cards_to_import_.empty()) {
    std::move(payment_cards_callback).Run(/*number_of_imports=*/0);
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

  std::move(payment_cards_callback).Run(imported_credit_cards);
}

}  // namespace user_data_importer
