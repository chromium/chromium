// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_data_importer/utility/safari_data_importer.h"

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "components/password_manager/core/browser/import/csv_password_sequence.h"
#include "components/password_manager/core/browser/import/import_results.h"
#include "components/password_manager/core/browser/import/password_importer.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "components/password_manager/core/common/password_manager_constants.h"
#include "components/password_manager/services/csv_password/fake_password_parser_service.h"
#include "components/user_data_importer/utility/safari_data_import_manager.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace user_data_importer {

class TestSafariDataImportManager : public SafariDataImportManager {
 public:
  TestSafariDataImportManager() = default;
  ~TestSafariDataImportManager() override = default;

  void ParseBookmarks(
      const base::FilePath& bookmarks_html,
      base::OnceCallback<void(BookmarkParsingResult)> callback) override {}
};

class SafariDataImporterTest : public testing::Test {
 public:
  SafariDataImporterTest()
      : receiver_{&service_},
        importer_(&presenter_,
                  std::make_unique<TestSafariDataImportManager>(),
                  "en-US") {
    mojo::PendingRemote<password_manager::mojom::CSVPasswordParser>
        pending_remote{receiver_.BindNewPipeAndPassRemote()};
    importer_.password_importer_->SetServiceForTesting(
        std::move(pending_remote));
    importer_.password_importer_->SetDeleteFileForTesting(
        mock_delete_file_.Get());

    profile_store_->Init(/*prefs=*/nullptr,
                         /*affiliated_match_helper=*/nullptr);
    account_store_->Init(/*prefs=*/nullptr,
                         /*affiliated_match_helper=*/nullptr);
    // Use of Unretained below is safe because the RunUntil loop below
    // guarantees this outlives the tasks.
    presenter_.Init(base::BindOnce(&SafariDataImporterTest::OnPresenterReady,
                                   base::Unretained(this)));
    WaitUntilPresenterIsReady();
  }

  SafariDataImporterTest(const SafariDataImporterTest&) = delete;
  SafariDataImporterTest& operator=(const SafariDataImporterTest&) = delete;

  ~SafariDataImporterTest() override {
    account_store_->ShutdownOnUIThread();
    profile_store_->ShutdownOnUIThread();
    task_environment_.RunUntilIdle();
  }

  void WaitUntilPresenterIsReady() {
    ASSERT_TRUE(base::test::RunUntil([&]() { return presenter_ready_; }));
  }

  void OnPresenterReady() { presenter_ready_ = true; }

  password_manager::ImportResults GetImportResults() const {
    return import_results_;
  }

  int GetNumberOfBookmarksImported() const {
    return number_bookmarks_imported_;
  }

  int GetNumberOfPaymentCardsImported() const {
    return number_payment_cards_imported_;
  }

  int GetNumberOfURLsImported() const { return number_urls_imported_; }

  void OnBookmarksConsumed(int number_imported) {
    bookmarks_callback_called_ = true;
    number_bookmarks_imported_ = number_imported;
  }

  void OnPasswordsConsumed(const password_manager::ImportResults& results) {
    passwords_callback_called_ = true;
    import_results_ = results;
  }

  void OnPaymentCardsConsumed(int number_imported) {
    payment_cards_callback_called_ = true;
    number_payment_cards_imported_ = number_imported;
  }

  void OnURLsConsumed(int number_imported) {
    history_callback_called_ = true;
    number_urls_imported_ = number_imported;
  }

  void ImportBookmarks(std::string html_data) {
    bookmarks_callback_called_ = false;
    importer_.ImportBookmarks(
        std::move(html_data),
        // Use of Unretained below is safe because the RunUntil loop below
        // guarantees this outlives the tasks.
        base::BindOnce(&SafariDataImporterTest::OnBookmarksConsumed,
                       base::Unretained(this)));
    ASSERT_TRUE(
        base::test::RunUntil([&]() { return bookmarks_callback_called_; }));
  }

  void ImportHistory() {
    history_callback_called_ = false;
    importer_.ImportHistory(
        // Use of Unretained below is safe because the RunUntil loop below
        // guarantees this outlives the tasks.
        base::BindOnce(&SafariDataImporterTest::OnURLsConsumed,
                       base::Unretained(this)));
    ASSERT_TRUE(
        base::test::RunUntil([&]() { return history_callback_called_; }));
  }

  void ImportPasswords(std::string csv_data) {
    passwords_callback_called_ = false;
    importer_.ImportPasswords(
        std::move(csv_data),
        // Use of Unretained below is safe because the RunUntil loop below
        // guarantees this outlives the tasks.
        base::BindOnce(&SafariDataImporterTest::OnPasswordsConsumed,
                       base::Unretained(this)));
    ASSERT_TRUE(
        base::test::RunUntil([&]() { return passwords_callback_called_; }));
  }

  void ExecuteImport() {
    passwords_callback_called_ = false;
    importer_.ContinueImport(
        std::vector<int>(),
        // Use of Unretained below is safe because the RunUntil loop below
        // guarantees this outlives the tasks.
        base::BindOnce(&SafariDataImporterTest::OnPasswordsConsumed,
                       base::Unretained(this)),
        base::BindOnce(&SafariDataImporterTest::OnBookmarksConsumed,
                       base::Unretained(this)),
        base::BindOnce(&SafariDataImporterTest::OnURLsConsumed,
                       base::Unretained(this)),
        base::BindOnce(&SafariDataImporterTest::OnPaymentCardsConsumed,
                       base::Unretained(this)));
    ASSERT_TRUE(
        base::test::RunUntil([&]() { return passwords_callback_called_; }));
  }

  void ResolvePasswordConflicts(const std::vector<int>& selected_ids) {
    passwords_callback_called_ = false;
    importer_.ContinueImport(
        selected_ids,
        // Use of Unretained below is safe because the RunUntil loop below
        // guarantees this outlives the tasks.
        base::BindOnce(&SafariDataImporterTest::OnPasswordsConsumed,
                       base::Unretained(this)),
        base::BindOnce(&SafariDataImporterTest::OnBookmarksConsumed,
                       base::Unretained(this)),
        base::BindOnce(&SafariDataImporterTest::OnURLsConsumed,
                       base::Unretained(this)),
        base::BindOnce(&SafariDataImporterTest::OnPaymentCardsConsumed,
                       base::Unretained(this)));
    ASSERT_TRUE(
        base::test::RunUntil([&]() { return passwords_callback_called_; }));
  }

  void ImportPaymentCards(std::vector<PaymentCardEntry> payment_cards) {
    payment_cards_callback_called_ = false;
    importer_.ImportPaymentCards(
        std::move(payment_cards),
        // Use of Unretained below is safe because the RunUntil loop below
        // guarantees this outlives the tasks.
        base::BindOnce(&SafariDataImporterTest::OnPaymentCardsConsumed,
                       base::Unretained(this)));
    ASSERT_TRUE(
        base::test::RunUntil([&]() { return payment_cards_callback_called_; }));
  }

  void ImportInvalidFile() {
    passwords_callback_called_ = false;
    bookmarks_callback_called_ = false;
    history_callback_called_ = false;
    payment_cards_callback_called_ = false;

    importer_.StartImport(
        base::FilePath(FILE_PATH_LITERAL("/invalid/path/to/zip/file")),
        // Use of Unretained below is safe because the RunUntil loop below
        // guarantees this outlives the tasks.
        base::BindOnce(&SafariDataImporterTest::OnPasswordsConsumed,
                       base::Unretained(this)),
        base::BindOnce(&SafariDataImporterTest::OnBookmarksConsumed,
                       base::Unretained(this)),
        base::BindOnce(&SafariDataImporterTest::OnURLsConsumed,
                       base::Unretained(this)),
        base::BindOnce(&SafariDataImporterTest::OnPaymentCardsConsumed,
                       base::Unretained(this)));

    ASSERT_TRUE(base::test::RunUntil([&]() {
      return passwords_callback_called_ && payment_cards_callback_called_ &&
             bookmarks_callback_called_ && history_callback_called_;
    })) << CallbackTimeoutMessage();
  }

  void ImportFile() {
    base::FilePath zip_archive_path;
    ASSERT_TRUE(base::PathService::Get(base::DIR_ASSETS, &zip_archive_path));
    zip_archive_path =
        zip_archive_path.Append(FILE_PATH_LITERAL("test_archive.zip"));

    passwords_callback_called_ = false;
    bookmarks_callback_called_ = false;
    history_callback_called_ = false;
    payment_cards_callback_called_ = false;

    importer_.StartImport(
        zip_archive_path,
        // Use of Unretained below is safe because the RunUntil loop below
        // guarantees this outlives the tasks.
        base::BindOnce(&SafariDataImporterTest::OnPasswordsConsumed,
                       base::Unretained(this)),
        base::BindOnce(&SafariDataImporterTest::OnBookmarksConsumed,
                       base::Unretained(this)),
        base::BindOnce(&SafariDataImporterTest::OnURLsConsumed,
                       base::Unretained(this)),
        base::BindOnce(&SafariDataImporterTest::OnPaymentCardsConsumed,
                       base::Unretained(this)));

    ASSERT_TRUE(base::test::RunUntil([&]() {
      return passwords_callback_called_ && payment_cards_callback_called_ &&
             bookmarks_callback_called_ && history_callback_called_;
    })) << CallbackTimeoutMessage();
  }

  void CancelImport() { importer_.CancelImport(); }

 private:
  // Formats an error message when timing out while waiting for callbacks.
  std::string CallbackTimeoutMessage() {
    std::string message = "Timed out waiting for: ";
    bool found_uncalled_callback = false;
    if (!passwords_callback_called_) {
      message += "passwords";
      found_uncalled_callback = true;
    }

    if (!payment_cards_callback_called_) {
      if (found_uncalled_callback) {
        message += ", ";
      }
      message += "payment cards";
      found_uncalled_callback = true;
    }

    if (!bookmarks_callback_called_) {
      if (found_uncalled_callback) {
        message += ", ";
      }
      message += "bookmarks";
      found_uncalled_callback = true;
    }

    if (!history_callback_called_) {
      if (found_uncalled_callback) {
        message += ", ";
      }
      message += "history";
      found_uncalled_callback = true;
    }

    if (!found_uncalled_callback) {
      message += "unknown reason";
    }

    return message;
  }

  base::test::TaskEnvironment task_environment_;
  bool presenter_ready_ = false;
  password_manager::ImportResults import_results_;
  bool passwords_callback_called_ = false;
  bool bookmarks_callback_called_ = false;
  bool history_callback_called_ = false;
  bool payment_cards_callback_called_ = false;
  int number_bookmarks_imported_ = -1;
  int number_urls_imported_ = -1;
  int number_payment_cards_imported_ = -1;
  password_manager::FakePasswordParserService service_;
  mojo::Receiver<password_manager::mojom::CSVPasswordParser> receiver_;
  scoped_refptr<password_manager::TestPasswordStore> profile_store_ =
      base::MakeRefCounted<password_manager::TestPasswordStore>(
          password_manager::IsAccountStore(false));
  scoped_refptr<password_manager::TestPasswordStore> account_store_ =
      base::MakeRefCounted<password_manager::TestPasswordStore>(
          password_manager::IsAccountStore(true));
  affiliations::FakeAffiliationService affiliation_service_;
  password_manager::SavedPasswordsPresenter presenter_{
      &affiliation_service_, profile_store_, account_store_};
  SafariDataImporter importer_;
  testing::StrictMock<base::MockCallback<
      password_manager::PasswordImporter::DeleteFileCallback>>
      mock_delete_file_;
};

TEST_F(SafariDataImporterTest, NoBookmark) {
  ImportBookmarks("");

  ASSERT_EQ(GetNumberOfBookmarksImported(), 0);
}

TEST_F(SafariDataImporterTest, NoHistory) {
  ImportHistory();

  ASSERT_EQ(GetNumberOfURLsImported(), 0);
}

TEST_F(SafariDataImporterTest, NoPassword) {
  ImportPasswords("");

  password_manager::ImportResults import_results = GetImportResults();
  ASSERT_EQ(import_results.number_imported, 0u);
}

TEST_F(SafariDataImporterTest, NoPaymentCard) {
  ImportPaymentCards(std::vector<PaymentCardEntry>());

  ASSERT_EQ(GetNumberOfPaymentCardsImported(), 0);
}

TEST_F(SafariDataImporterTest, PasswordImport) {
  constexpr char kTestCSVInput[] =
      "Url,Username,Password,Note\n"
      "http://example1.com,username1,password1,note1\n"
      "http://example1.com,username2,password2,note2\n"
      "http://example2.com,username1,password3,note3\n";

  ImportPasswords(kTestCSVInput);
  password_manager::ImportResults import_results = GetImportResults();
  ASSERT_EQ(import_results.number_imported, 0u);
  ASSERT_EQ(import_results.number_to_import, 3u);

  // Confirm password import.
  ExecuteImport();
  import_results = GetImportResults();
  ASSERT_EQ(import_results.number_imported, 3u);
  ASSERT_EQ(import_results.number_to_import, 0u);
}

TEST_F(SafariDataImporterTest, PasswordImportConflicts) {
  constexpr char kTestCSVInput[] =
      "Url,Username,Password,Note\n"
      "http://example1.com,username1,password1,note1\n"
      "http://example1.com,username2,password2,note2\n"
      "http://example2.com,username1,password3,note3\n";

  constexpr char kTestCSVConflicts[] =
      "Url,Username,Password,Note\n"
      "http://example1.com,username2,password4,note2\n"
      "http://example2.com,username1,password5,note3\n";

  // Import 3 passwords.
  ImportPasswords(kTestCSVInput);
  password_manager::ImportResults import_results = GetImportResults();
  ASSERT_EQ(import_results.number_imported, 0u);
  ASSERT_EQ(import_results.number_to_import, 3u);

  // Confirm password import.
  ExecuteImport();
  import_results = GetImportResults();
  ASSERT_EQ(import_results.number_imported, 3u);
  ASSERT_EQ(import_results.number_to_import, 0u);

  // Attempt to import 2 conflicting passwords, which should fail.
  ImportPasswords(kTestCSVConflicts);
  import_results = GetImportResults();
  ASSERT_EQ(import_results.number_imported, 0u);
  ASSERT_EQ(import_results.number_to_import, 0u);
  // 2 conflicting entries need to be displayed to the user.
  ASSERT_EQ(import_results.displayed_entries.size(), 2u);

  // Resolve the 2 conflicts.
  std::vector<int> selected_ids;
  selected_ids.push_back(0);
  selected_ids.push_back(1);
  ResolvePasswordConflicts(selected_ids);
  import_results = GetImportResults();
  ASSERT_EQ(import_results.number_imported, 2u);
  ASSERT_EQ(import_results.number_to_import, 0u);
}

TEST_F(SafariDataImporterTest, CallbacksAreCalled) {
  ImportInvalidFile();
}

TEST_F(SafariDataImporterTest, CancelImport) {
  ImportFile();

  password_manager::ImportResults import_results = GetImportResults();
  ASSERT_EQ(import_results.number_to_import, 3u);
  // TODO(crbug.com/407587751): Update test when bookmarks parsing is
  // implemented.
  ASSERT_EQ(GetNumberOfBookmarksImported(), 0);
  ASSERT_EQ(GetNumberOfPaymentCardsImported(), 3);
  ASSERT_EQ(GetNumberOfURLsImported(), 5);  // Note: Approximation.

  CancelImport();
}

TEST_F(SafariDataImporterTest, ExecuteImport) {
  ImportFile();

  password_manager::ImportResults import_results = GetImportResults();
  ASSERT_EQ(import_results.number_to_import, 3u);
  ASSERT_EQ(import_results.number_imported, 0u);
  // TODO(crbug.com/407587751): Update test when bookmarks parsing is
  // implemented.
  ASSERT_EQ(GetNumberOfBookmarksImported(), 0);
  ASSERT_EQ(GetNumberOfPaymentCardsImported(), 3);
  ASSERT_EQ(GetNumberOfURLsImported(), 5);  // Note: Approximation.

  ExecuteImport();
  import_results = GetImportResults();
  ASSERT_EQ(import_results.number_imported, 3u);
  ASSERT_EQ(import_results.number_to_import, 0u);
  // TODO(crbug.com/407587751): Update test when bookmarks parsing is
  // implemented.
  ASSERT_EQ(GetNumberOfBookmarksImported(), 0);
  // TODO(crbug.com/407587751): Update test when payment cards import is
  // implemented.
  ASSERT_EQ(GetNumberOfPaymentCardsImported(), 0);
  ASSERT_EQ(GetNumberOfURLsImported(), 5);
}

}  // namespace user_data_importer
