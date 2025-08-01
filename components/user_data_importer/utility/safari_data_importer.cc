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
#include "components/password_manager/core/browser/features/password_manager_features_util.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_data_importer/utility/bookmark_util.h"
#include "components/user_data_importer/utility/history_callback_from_rust.h"
#include "components/user_data_importer/utility/parsing_ffi/lib.rs.h"
#include "ui/base/l10n/l10n_util.h"

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
bool IsSkippedEntry(const user_data_importer::SafariHistoryEntry& entry) {
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
    const user_data_importer::SafariHistoryEntry& history_entry) {
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
    : public user_data_importer::HistoryCallbackFromRust<SafariHistoryEntry> {
 public:
  using ParseHistoryCallback = base::RepeatingCallback<void(
      std::vector<user_data_importer::SafariHistoryEntry>)>;

  explicit RustHistoryCallback(ParseHistoryCallback parse_history_callback,
                               base::OnceClosure done_closure)
      : parse_history_callback_(parse_history_callback),
        done_closure_(std::move(done_closure)) {}

  ~RustHistoryCallback() override = default;

  // Callback called while parsing the history file.
  void ImportHistoryEntries(
      std::unique_ptr<std::vector<SafariHistoryEntry>> history_entries,
      bool completed) override {
    parse_history_callback_.Run(std::move(*history_entries));

    if (completed) {
      std::move(done_closure_).Run();
    }
  }

  // Calls `done_callback_` with 0 to signal that parsing has failed.
  void Fail() { std::move(done_closure_).Run(); }

 private:
  ParseHistoryCallback parse_history_callback_;
  base::OnceClosure done_closure_;
};

SafariDataImporter::SafariDataImporter(
    SafariDataImportClient* client,
    password_manager::SavedPasswordsPresenter* presenter,
    autofill::PaymentsDataManager* payments_data_manager,
    history::HistoryService* history_service,
    bookmarks::BookmarkModel* bookmark_model,
    ReadingListModel* reading_list_model,
    syncer::SyncService* sync_service,
    std::unique_ptr<BookmarkParser> bookmark_parser,
    std::string app_locale)
    : blocking_queue_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE})),
      blocking_worker_(blocking_queue_, std::move(bookmark_parser)),
      password_importer_(std::make_unique<password_manager::PasswordImporter>(
          presenter,
          /*user_confirmation_required=*/true)),
      client_(CHECK_DEREF(client)),
      payments_data_manager_(CHECK_DEREF(payments_data_manager)),
      history_service_(CHECK_DEREF(history_service)),
      bookmark_model_(CHECK_DEREF(bookmark_model)),
      reading_list_model_(CHECK_DEREF(reading_list_model)),
      sync_service_(sync_service),
      app_locale_(std::move(app_locale)) {}

SafariDataImporter::~SafariDataImporter() = default;

void SafariDataImporter::PrepareImport(const base::FilePath& path) {
  std::string zip_filename = path.MaybeAsASCII();
  if (zip_filename.empty()) {
    // TODO(crbug.com/407587751): Log error.
    client_->OnTotalFailure();
    return;
  }

  blocking_worker_.AsyncCall(&BlockingWorker::CreateZipFileArchive)
      .WithArgs(std::move(zip_filename))
      .Then(base::BindOnce(&SafariDataImporter::OnZipArchiveReady,
                           weak_factory_.GetWeakPtr()));
}

void SafariDataImporter::CompleteImport(
    const std::vector<int>& selected_password_ids) {
  // The history import process is the only one requiring reading the zip file,
  // so launch it first.
  history_urls_imported_ = 0;
  RustHistoryCallback::ParseHistoryCallback parse_history_callback =
      base::BindPostTask(
          GetRunner(),
          base::BindRepeating(&SafariDataImporter::ImportHistoryEntries,
                              weak_factory_.GetWeakPtr()));

  base::OnceClosure done_history_closure = base::BindPostTask(
      GetRunner(), base::BindOnce(&SafariDataImporter::OnHistoryImportCompleted,
                                  weak_factory_.GetWeakPtr()));

  blocking_worker_.AsyncCall(&BlockingWorker::ImportHistory)
      .WithArgs(std::make_unique<RustHistoryCallback>(
                    std::move(parse_history_callback),
                    std::move(done_history_closure)),
                history_size_threshold_);

  if (password_importer_ &&
      password_importer_->IsState(
          password_manager::PasswordImporter::kUserInteractionRequired)) {
    // TODO(crbug.com/407587751): Move this to a task.
    password_importer_->ContinueImport(
        selected_password_ids,
        base::BindOnce(&SafariDataImportClient::OnPasswordsImported,
                       client_->AsWeakPtr()));
  }

  GetRunner()->PostTask(
      FROM_HERE, base::BindOnce(&SafariDataImporter::ContinueImportBookmarks,
                                weak_factory_.GetWeakPtr()));

  GetRunner()->PostTask(
      FROM_HERE, base::BindOnce(&SafariDataImporter::ContinueImportPaymentCards,
                                weak_factory_.GetWeakPtr()));
}

// Called after calling "Import" in order to cancel the import process.
void SafariDataImporter::CancelImport() {
  // TODO(crbug.com/407587751): Notify password_importer_.

  blocking_worker_.AsyncCall(&BlockingWorker::CloseZipFileArchive);
}

SafariDataImporter::BlockingWorker::BlockingWorker(
    std::unique_ptr<BookmarkParser> bookmark_parser)
    : bookmark_parser_(std::move(bookmark_parser)) {}

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

void SafariDataImporter::BlockingWorker::ParseBookmarks(
    std::optional<base::FilePath> bookmarks_html,
    user_data_importer::BookmarkParser::BookmarkParsingCallback
        bookmarks_callback) {
  bookmark_parser_->Parse(*bookmarks_html, std::move(bookmarks_callback));
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
      ->parse_safari_history(std::move(callback), history_size_threshold);

  CloseZipFileArchive();
}

void SafariDataImporter::OnZipArchiveReady(bool success) {
  if (!success) {
    // Nothing to import, early exit.
    client_->OnTotalFailure();
    return;
  }

  // Passwords import may require conflict resolution, so it is done first.
  blocking_worker_.AsyncCall(&BlockingWorker::Unzip)
      .WithArgs(FileType::Passwords)
      .Then(base::BindOnce(&SafariDataImporter::PreparePasswords,
                           weak_factory_.GetWeakPtr()));

  blocking_worker_.AsyncCall(&BlockingWorker::ParsePaymentCards)
      .Then(base::BindOnce(&SafariDataImporter::PreparePaymentCards,
                           weak_factory_.GetWeakPtr()));

  blocking_worker_.AsyncCall(&BlockingWorker::WriteBookmarksToTmpFile)
      .Then(base::BindOnce(&SafariDataImporter::PrepareBookmarks,
                           weak_factory_.GetWeakPtr()));

  blocking_worker_.AsyncCall(&BlockingWorker::GetUncompressedFileSizeInBytes)
      .WithArgs(FileType::SafariHistory)
      .Then(base::BindOnce(&SafariDataImporter::PrepareHistory,
                           weak_factory_.GetWeakPtr()));
}

void SafariDataImporter::PreparePasswords(std::string csv_data) {
  password_manager::PasswordForm::Store to_store = {
      password_manager::features_util::IsAccountStorageEnabled(sync_service_)
          ? password_manager::PasswordForm::Store::kAccountStore
          : password_manager::PasswordForm::Store::kProfileStore};

  password_importer_->Import(
      std::move(csv_data), to_store,
      base::BindOnce(&SafariDataImportClient::OnPasswordsReady,
                     client_->AsWeakPtr()));
}

void SafariDataImporter::PreparePaymentCards(
    std::vector<PaymentCardEntry> payment_cards) {
  if (payment_cards.empty()) {
    client_->OnPaymentCardsReady(/* count= */ 0);
    return;
  }

  cards_to_import_.clear();
  cards_to_import_.reserve(payment_cards.size());
  std::ranges::transform(payment_cards, std::back_inserter(cards_to_import_),
                         [this](const auto& card) {
                           return ConvertToAutofillCreditCard(card,
                                                              app_locale_);
                         });

  client_->OnPaymentCardsReady(cards_to_import_.size());
}

void SafariDataImporter::PrepareBookmarks(
    std::optional<base::FilePath> bookmarks_html) {
  if (!bookmarks_html || bookmarks_html->empty()) {
    client_->OnBookmarksReady(/* count= */ 0);
    return;
  }

  auto import_bookmarks_callback = base::BindPostTask(
      GetRunner(), base::BindOnce(&SafariDataImporter::OnBookmarksParsed,
                                  weak_factory_.GetWeakPtr()));

  blocking_worker_.AsyncCall(&BlockingWorker::ParseBookmarks)
      .WithArgs(bookmarks_html, std::move(import_bookmarks_callback));
}

void SafariDataImporter::OnBookmarksParsed(
    BookmarkParser::BookmarkParsingResult result) {
  ASSIGN_OR_RETURN(BookmarkParser::ParsedBookmarks value, std::move(result),
                   [this](auto) {
                     // TODO(crbug.com/407587751): Log error to UMA.
                     client_->OnBookmarksReady(/* count= */ 0);
                   });

  pending_bookmarks_ = std::move(value.bookmarks);
  pending_reading_list_ = std::move(value.reading_list);

  size_t importable_bookmarks_count = 0;
  for (const auto& bookmark : pending_bookmarks_) {
    if (!bookmark.is_folder) {
      ++importable_bookmarks_count;
    }
  }

  client_->OnBookmarksReady(importable_bookmarks_count +
                            pending_reading_list_.size());
}

void SafariDataImporter::PrepareHistory(size_t file_size_bytes) {
  // This is an approximation of the number of bytes per URL entry in the
  // history file.
  static const size_t kBytesPerURL = 250;
  size_t approximate_number_of_urls =
      (file_size_bytes > 0) ? (file_size_bytes / kBytesPerURL) + 1 : 0;

  // TODO(crbug.com/407587751): Pass list of profiles.
  client_->OnHistoryReady(approximate_number_of_urls, {});
}

void SafariDataImporter::ImportHistoryEntries(
    std::vector<SafariHistoryEntry> history_entries) {
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
}

// Invoked once parsing of history is completed. Forwards the results to
// `client_`.
void SafariDataImporter::OnHistoryImportCompleted() {
  client_->OnHistoryImported(history_urls_imported_);
}

void SafariDataImporter::ContinueImportPaymentCards() {
  if (cards_to_import_.empty()) {
    client_->OnPaymentCardsImported(/* count= */ 0);
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

    ++imported_credit_cards;
  }

  client_->OnPaymentCardsImported(imported_credit_cards);
}

void SafariDataImporter::ContinueImportBookmarks() {
  size_t imported_bookmarks_count = user_data_importer::ImportBookmarks(
      &*bookmark_model_, std::move(pending_bookmarks_),
      l10n_util::GetStringUTF16(IDS_IMPORTED_FROM_SAFARI_FOLDER));
  size_t imported_reading_list_count = user_data_importer::ImportReadingList(
      &*reading_list_model_, std::move(pending_reading_list_));

  client_->OnBookmarksImported(imported_bookmarks_count +
                               imported_reading_list_count);
}

}  // namespace user_data_importer
