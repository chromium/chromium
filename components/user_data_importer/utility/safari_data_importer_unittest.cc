// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_data_importer/utility/safari_data_importer.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/time/default_clock.h"
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/password_manager/core/browser/import/csv_password_sequence.h"
#include "components/password_manager/core/browser/import/import_results.h"
#include "components/password_manager/core/browser/import/password_importer.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "components/password_manager/core/common/password_manager_constants.h"
#include "components/password_manager/services/csv_password/fake_password_parser_service.h"
#include "components/reading_list/core/fake_reading_list_model_storage.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/reading_list/core/reading_list_model_impl.h"
#include "components/user_data_importer/utility/bookmark_parser.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;
using testing::IsEmpty;

namespace {
#if BUILDFLAG(IS_IOS)
// Asserts that `time` is within 2 minutes of the current time.
void ExpectTimeIsNowIsh(const base::Time& time) {
  base::Time now = base::Time::Now();
  EXPECT_LE(now - time, base::Minutes(2))
      << "Expected " << time
      << " to be no more than 2 minutes before now: " << now;

  EXPECT_LE(time - now, base::Minutes(2))
      << "Expected " << time
      << " to be no more than 2 minutes after now: " << now;
}
#endif  // BUILDFLAG(IS_IOS)
}  // namespace

namespace user_data_importer {

class SafariDataImporterTest : public testing::Test {
 public:
  SafariDataImporterTest() : receiver_{&service_} {}
  ~SafariDataImporterTest() override = default;

  SafariDataImporterTest(const SafariDataImporterTest&) = delete;
  SafariDataImporterTest& operator=(const SafariDataImporterTest&) = delete;

 protected:
  void SetUp() override {
    CHECK(history_dir_.CreateUniqueTempDir());
    history_service_ = history::CreateHistoryService(history_dir_.GetPath(),
                                                     /*create_db=*/false);

    auto bookmark_client = std::make_unique<bookmarks::TestBookmarkClient>();
    bookmark_model_ =
        std::make_unique<bookmarks::BookmarkModel>(std::move(bookmark_client));

    auto storage = std::make_unique<FakeReadingListModelStorage>();
    reading_list_model_ = std::make_unique<ReadingListModelImpl>(
        std::move(storage), syncer::StorageType::kUnspecified,
        syncer::WipeModelUponSyncDisabledBehavior::kNever,
        base::DefaultClock::GetInstance());

    importer_ = std::make_unique<SafariDataImporter>(
        &presenter_, &client_.GetPersonalDataManager().payments_data_manager(),
        history_service_.get(), bookmark_model_.get(),
        reading_list_model_.get(), MakeBookmarkParser(), "en-US");

    mojo::PendingRemote<password_manager::mojom::CSVPasswordParser>
        pending_remote{receiver_.BindNewPipeAndPassRemote()};
    importer_->password_importer_->SetServiceForTesting(
        std::move(pending_remote));
    importer_->password_importer_->SetDeleteFileForTesting(
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

  void TearDown() override {
    account_store_->ShutdownOnUIThread();
    profile_store_->ShutdownOnUIThread();
    task_environment_.RunUntilIdle();
  }

  password_manager::ImportResults GetImportResults() const {
    return import_results_;
  }

  int GetNumberOfBookmarksImported() const {
    return number_bookmarks_imported_;
  }

  const std::vector<ImportedBookmarkEntry>& GetPendingBookmarks() const {
    return importer_->pending_bookmarks_;
  }

  const std::vector<ImportedBookmarkEntry>& GetPendingReadingList() const {
    return importer_->pending_reading_list_;
  }

  int GetNumberOfPaymentCardsImported() const {
    return number_payment_cards_imported_;
  }

  int GetNumberOfURLsImported() const { return number_urls_imported_; }

  void ImportBookmarks(std::string html_data) {
    bookmarks_callback_called_ = false;
    base::ScopedTempDir dir;
    ASSERT_TRUE(dir.CreateUniqueTempDir());
    base::FilePath path = dir.GetPath().AppendASCII("bookmarks.html");
    ASSERT_TRUE(base::WriteFile(path, html_data));

    importer_->ImportBookmarks(
        path,
        // Use of Unretained below is safe because the RunUntil loop below
        // guarantees this outlives the tasks.
        base::BindOnce(&SafariDataImporterTest::OnBookmarksConsumed,
                       base::Unretained(this)));
    ASSERT_TRUE(
        base::test::RunUntil([&]() { return bookmarks_callback_called_; }));
  }

  void ImportHistory() {
    history_callback_called_ = false;
    importer_->ImportHistory(
        // Use of Unretained below is safe because the RunUntil loop below
        // guarantees this outlives the tasks.
        base::BindOnce(&SafariDataImporterTest::OnURLsConsumed,
                       base::Unretained(this)));
    ASSERT_TRUE(
        base::test::RunUntil([&]() { return history_callback_called_; }));
  }

  void ImportPasswords(std::string csv_data) {
    passwords_callback_called_ = false;
    importer_->ImportPasswords(
        std::move(csv_data),
        // Use of Unretained below is safe because the RunUntil loop below
        // guarantees this outlives the tasks.
        base::BindOnce(&SafariDataImporterTest::OnPasswordsConsumed,
                       base::Unretained(this)));
    ASSERT_TRUE(
        base::test::RunUntil([&]() { return passwords_callback_called_; }));
  }

  // Executes the import, using selected_ids to resolve password conflicts.
  void ExecuteImport(const std::vector<int>& selected_ids) {
    PrepareCallbacks();

    importer_->ContinueImport(
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

    WaitForCallbacks();
  }

  void ImportPaymentCards(std::vector<PaymentCardEntry> payment_cards) {
    payment_cards_callback_called_ = false;
    importer_->ImportPaymentCards(
        std::move(payment_cards),
        // Use of Unretained below is safe because the RunUntil loop below
        // guarantees this outlives the tasks.
        base::BindOnce(&SafariDataImporterTest::OnPaymentCardsConsumed,
                       base::Unretained(this)));
    ASSERT_TRUE(
        base::test::RunUntil([&]() { return payment_cards_callback_called_; }));
  }

  void ImportInvalidFile() {
    ImportFile(base::FilePath(FILE_PATH_LITERAL("/invalid/path/to/zip/file")));
  }

  void ImportFile() {
    base::FilePath zip_archive_path;
    ASSERT_TRUE(base::PathService::Get(base::DIR_ASSETS, &zip_archive_path));
    ImportFile(zip_archive_path.Append(FILE_PATH_LITERAL("test_archive.zip")));
  }

  void CancelImport() { importer_->CancelImport(); }

  void SetHistorySizeThreshold(size_t history_size_threshold) {
    importer_->history_size_threshold_ = history_size_threshold;
    importer_->history_size_threshold_ = history_size_threshold;
  }

  // Asserts that GetNumberOfBookmarksImported() is `num_bookmarks` on platforms
  // where bookmark import is implemented, or 0 (callback ran with error) on
  // other platforms.
  void ExpectBookmarksIfImplemented(int num_bookmarks) {
#if BUILDFLAG(IS_IOS)
    EXPECT_EQ(GetNumberOfBookmarksImported(), num_bookmarks);
#else
    EXPECT_EQ(GetNumberOfBookmarksImported(), 0);
#endif  // BUILDFLAG(IS_IOS)
  }

 private:
  void WaitUntilPresenterIsReady() {
    ASSERT_TRUE(base::test::RunUntil([&]() { return presenter_ready_; }));
  }

  void OnPresenterReady() { presenter_ready_ = true; }

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

  void PrepareCallbacks() {
    passwords_callback_called_ = false;
    bookmarks_callback_called_ = false;
    history_callback_called_ = false;
    payment_cards_callback_called_ = false;
  }

  void WaitForCallbacks() {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return passwords_callback_called_ && payment_cards_callback_called_ &&
             bookmarks_callback_called_ && history_callback_called_;
    })) << CallbackTimeoutMessage();
  }

  void ImportFile(const base::FilePath& file) {
    PrepareCallbacks();

    importer_->StartImport(
        file,
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

    WaitForCallbacks();
  }

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
  password_manager::FakePasswordParserService service_;
  mojo::Receiver<password_manager::mojom::CSVPasswordParser> receiver_;
  autofill::TestAutofillClient client_;
  base::ScopedTempDir history_dir_;
  std::unique_ptr<history::HistoryService> history_service_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<ReadingListModel> reading_list_model_;
  bool presenter_ready_ = false;
  password_manager::ImportResults import_results_;
  bool passwords_callback_called_ = false;
  bool bookmarks_callback_called_ = false;
  bool history_callback_called_ = false;
  bool payment_cards_callback_called_ = false;
  int number_bookmarks_imported_ = -1;
  int number_urls_imported_ = -1;
  int number_payment_cards_imported_ = -1;
  scoped_refptr<password_manager::TestPasswordStore> profile_store_ =
      base::MakeRefCounted<password_manager::TestPasswordStore>(
          password_manager::IsAccountStore(false));
  scoped_refptr<password_manager::TestPasswordStore> account_store_ =
      base::MakeRefCounted<password_manager::TestPasswordStore>(
          password_manager::IsAccountStore(true));
  affiliations::FakeAffiliationService affiliation_service_;
  password_manager::SavedPasswordsPresenter presenter_{
      &affiliation_service_, profile_store_, account_store_};
  std::unique_ptr<SafariDataImporter> importer_;
  testing::StrictMock<base::MockCallback<
      password_manager::PasswordImporter::DeleteFileCallback>>
      mock_delete_file_;
};

// TODO(crbug.com/407587751): Enable Bookmark tests on non-IOS once stub method
// in content_bookmark_parser is functional.
#if BUILDFLAG(IS_IOS)
TEST_F(SafariDataImporterTest, Bookmarks_Basic) {
  ImportBookmarks(
      "<!DOCTYPE NETSCAPE-Bookmark-file-1>\
      <!--This is an automatically generated file.\
      It will be read and overwritten.\
      Do Not Edit! -->\
      <DL>\
      <DT><A HREF=\"https://www.google.com/\" ADD_DATE=\"904914000\">Google</A>\
      <DT><A HREF=\"https://www.chromium.org/\">Chromium</A>\
      </DL>");
  EXPECT_EQ(GetNumberOfBookmarksImported(), 2);

  ASSERT_EQ(GetPendingBookmarks().size(), 2u);
  ImportedBookmarkEntry entry = GetPendingBookmarks()[0];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Google");
  EXPECT_EQ(entry.creation_time,
            base::Time::FromSecondsSinceUnixEpoch(904914000));
  EXPECT_EQ(entry.url, GURL("https://www.google.com/"));
  EXPECT_THAT(entry.path, IsEmpty());

  entry = GetPendingBookmarks()[1];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Chromium");
  // No timestamp maps to current time.
  ExpectTimeIsNowIsh(entry.creation_time);
  EXPECT_EQ(entry.url, GURL("https://www.chromium.org/"));
  EXPECT_THAT(entry.path, IsEmpty());

  EXPECT_EQ(GetPendingReadingList().size(), 0u);
}

// Identical to the above test, but without the top-level <DL> tag enclosing it.
// It's documented as part of the format, but real-world Safari exports don't
// use it, so we have to support both with and without.
TEST_F(SafariDataImporterTest, Bookmarks_NoTopLevelDL) {
  ImportBookmarks(
      "<!DOCTYPE NETSCAPE-Bookmark-file-1>\
      <!--This is an automatically generated file.\
      It will be read and overwritten.\
      Do Not Edit! -->\
      <DT><A HREF=\"https://www.google.com/\" ADD_DATE=\"904914000\">Google</A>\
      <DT><A HREF=\"https://www.chromium.org/\">Chromium</A>\"");
  EXPECT_EQ(GetNumberOfBookmarksImported(), 2);

  ASSERT_EQ(GetPendingBookmarks().size(), 2u);
  ImportedBookmarkEntry entry = GetPendingBookmarks()[0];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Google");
  EXPECT_EQ(entry.creation_time,
            base::Time::FromSecondsSinceUnixEpoch(904914000));
  EXPECT_EQ(entry.url, GURL("https://www.google.com/"));
  EXPECT_THAT(entry.path, IsEmpty());

  entry = GetPendingBookmarks()[1];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Chromium");
  // No timestamp maps to current time.
  ExpectTimeIsNowIsh(entry.creation_time);
  EXPECT_EQ(entry.url, GURL("https://www.chromium.org/"));
  EXPECT_THAT(entry.path, IsEmpty());

  EXPECT_EQ(GetPendingReadingList().size(), 0u);
}

TEST_F(SafariDataImporterTest, Bookmarks_Folders) {
  ImportBookmarks(
      "<!DOCTYPE NETSCAPE-Bookmark-file-1>\
      <!--This is an automatically generated file.\
      It will be read and overwritten.\
      Do Not Edit! -->\
      <DL>\
      <DT><A HREF=\"https://www.google.com/\" ADD_DATE=\"904914000\">Google</A>\
      <DT><H3>Folder 1</H3>\
      <DL><p>\
        <DT><A HREF=\"https://www.example.com/\" ADD_DATE=\"915181200\">Example</A>\
        <DT><H3 ADD_DATE=\"1145523600\">Folder 1.1</H3>\
        <DL><p>\
          <DT><A HREF=\"https://en.wikipedia.org/wiki/Kitsune\" ADD_DATE=\"1674205200\">Kitsune</A>\
        </DL><p>\
      </DL><p>\
      <DT><H3>Empty Folder</H3>\
      <DL><p>\
      </DL>\
      </DL>");
  EXPECT_EQ(GetNumberOfBookmarksImported(), 6);

  ASSERT_EQ(GetPendingBookmarks().size(), 6u);

  ImportedBookmarkEntry entry = GetPendingBookmarks()[0];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Google");
  EXPECT_EQ(entry.creation_time,
            base::Time::FromSecondsSinceUnixEpoch(904914000));
  EXPECT_EQ(entry.url, GURL("https://www.google.com/"));
  EXPECT_THAT(entry.path, IsEmpty());

  entry = GetPendingBookmarks()[1];
  EXPECT_TRUE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Folder 1");
  // No timestamp maps to current time.
  ExpectTimeIsNowIsh(entry.creation_time);
  EXPECT_TRUE(entry.url.is_empty());
  EXPECT_THAT(entry.path, IsEmpty());

  entry = GetPendingBookmarks()[2];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Example");
  EXPECT_EQ(entry.creation_time,
            base::Time::FromSecondsSinceUnixEpoch(915181200));
  EXPECT_EQ(entry.url, GURL("https://www.example.com/"));
  EXPECT_THAT(entry.path, ElementsAre(u"Folder 1"));

  entry = GetPendingBookmarks()[3];
  EXPECT_TRUE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Folder 1.1");
  EXPECT_EQ(entry.creation_time,
            base::Time::FromSecondsSinceUnixEpoch(1145523600));
  EXPECT_TRUE(entry.url.is_empty());
  EXPECT_THAT(entry.path, ElementsAre(u"Folder 1"));

  entry = GetPendingBookmarks()[4];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Kitsune");
  EXPECT_EQ(entry.creation_time,
            base::Time::FromSecondsSinceUnixEpoch(1674205200));
  EXPECT_EQ(entry.url, GURL("https://en.wikipedia.org/wiki/Kitsune"));
  EXPECT_THAT(entry.path, ElementsAre(u"Folder 1", u"Folder 1.1"));

  entry = GetPendingBookmarks()[5];
  EXPECT_TRUE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Empty Folder");
  // No timestamp maps to current time.
  ExpectTimeIsNowIsh(entry.creation_time);
  EXPECT_TRUE(entry.url.is_empty());
  EXPECT_THAT(entry.path, IsEmpty());

  EXPECT_EQ(GetPendingReadingList().size(), 0u);
}

TEST_F(SafariDataImporterTest, Bookmarks_ReadingList) {
  ImportBookmarks(
      "<!DOCTYPE NETSCAPE-Bookmark-file-1>\
      <!--This is an automatically generated file.\
      It will be read and overwritten.\
      Do Not Edit! -->\
      <DL>\
      <DT><A HREF=\"https://www.google.com/\" ADD_DATE=\"904914000\">Google</A>\
      <DT><H3 id=\"com.apple.ReadingList\">Reading List</H3>\
      <DL><p>\
      <DT><A HREF=\"https://en.wikipedia.org/wiki/The_Beach_Boys\">The Beach Boys</A>\
      <DT><A HREF=\"https://en.wikipedia.org/wiki/Brian_Wilson\" ADD_DATE=\"-868878000\">Brian Wilson</A>\
      </DL><p>\
      </DL>");
  EXPECT_EQ(GetNumberOfBookmarksImported(), 4);

  EXPECT_EQ(GetPendingBookmarks().size(), 1u);

  ASSERT_EQ(GetPendingReadingList().size(), 3u);

  ImportedBookmarkEntry entry = GetPendingReadingList()[0];
  EXPECT_TRUE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Reading List");
  ExpectTimeIsNowIsh(entry.creation_time);
  EXPECT_TRUE(entry.url.is_empty());
  EXPECT_THAT(entry.path, IsEmpty());

  entry = GetPendingReadingList()[1];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"The Beach Boys");
  // No timestamp maps to current time.
  ExpectTimeIsNowIsh(entry.creation_time);
  EXPECT_EQ(entry.url, GURL("https://en.wikipedia.org/wiki/The_Beach_Boys"));
  EXPECT_THAT(entry.path, ElementsAre(u"Reading List"));

  entry = GetPendingReadingList()[2];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Brian Wilson");
  EXPECT_EQ(entry.creation_time,
            base::Time::FromSecondsSinceUnixEpoch(-868878000));
  EXPECT_EQ(entry.url, GURL("https://en.wikipedia.org/wiki/Brian_Wilson"));
  EXPECT_THAT(entry.path, ElementsAre(u"Reading List"));
}

TEST_F(SafariDataImporterTest, Bookmarks_MiscJunk) {
  ImportBookmarks(
      "<!DOCTYPE NETSCAPE-Bookmark-file-1>\
      <!--This is an automatically generated file.\
      It will be read and overwritten.\
      Do Not Edit! -->\
      <DL>\
      <DT><A>Google</A>\
      <DT><H3>Folder 1</H3>\
      <DL><p>\
        <DT><A HREF=\"https://www.chromium.org/\">Chromium</A>\
        ICON_URI=\"https://www.chromium.org/favicon.ico\"\
        <DT><A HREF=\"https://www.example.org/\" ADD_DATE=\"Last Tuesday\">Example</A>\
        <DT><A>Google Reader</A>\
      </DL><p>\
      <!-- Various unsupported types below -->\
      FEED=\"true\"\
      FEEDURL=\"https://www.example.com\"\
      WEBSLICE=\"true\"\
      ISLIVEPREVIEW=\"true\"\
      PREVIEWSIZE=\"100 x 100\"\
      </DL>");
  EXPECT_EQ(GetNumberOfBookmarksImported(), 3);

  ASSERT_EQ(GetPendingBookmarks().size(), 3u);

  // <A>Google</A> was skipped for lack of URL

  ImportedBookmarkEntry entry = GetPendingBookmarks()[0];
  EXPECT_TRUE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Folder 1");
  // No timestamp maps to current time.
  ExpectTimeIsNowIsh(entry.creation_time);
  EXPECT_TRUE(entry.url.is_empty());
  EXPECT_THAT(entry.path, IsEmpty());

  // The folder contains a mix of invalid and valid entries. Ensure the valid
  // ones are preserved.
  entry = GetPendingBookmarks()[1];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Chromium");
  // No timestamp maps to current time.
  ExpectTimeIsNowIsh(entry.creation_time);
  EXPECT_EQ(entry.url, GURL("https://www.chromium.org/"));
  EXPECT_THAT(entry.path, ElementsAre(u"Folder 1"));

  entry = GetPendingBookmarks()[2];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Example");
  // Invalid timestamp maps to current time.
  ExpectTimeIsNowIsh(entry.creation_time);
  EXPECT_EQ(entry.url, GURL("https://www.example.org/"));
  EXPECT_THAT(entry.path, ElementsAre(u"Folder 1"));

  // <A>Google Reader</A> was skipped for lack of URL
}

#endif  // BUILDFLAG(IS_IOS)

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
  ExecuteImport({});
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
  ExecuteImport({});
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
  ExecuteImport(selected_ids);
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
  ExpectBookmarksIfImplemented(7);
  ASSERT_EQ(GetNumberOfPaymentCardsImported(), 3);
  ASSERT_EQ(GetNumberOfURLsImported(), 13);  // Note: Approximation.

  CancelImport();
}

TEST_F(SafariDataImporterTest, ExecuteImport) {
  ImportFile();

  password_manager::ImportResults import_results = GetImportResults();
  ASSERT_EQ(import_results.number_to_import, 3u);
  ASSERT_EQ(import_results.number_imported, 0u);
  // TODO(crbug.com/407587751): Update test when bookmarks parsing is
  // implemented.
  ExpectBookmarksIfImplemented(7);
  ASSERT_EQ(GetNumberOfPaymentCardsImported(), 3);
  ASSERT_EQ(GetNumberOfURLsImported(), 13);  // Note: Approximation.

  // Use a small history size threshold so that ParseHistoryCallback gets called
  // multiple times internally.
  SetHistorySizeThreshold(3u);

  ExecuteImport({});
  import_results = GetImportResults();
  ASSERT_EQ(import_results.number_imported, 3u);
  ASSERT_EQ(import_results.number_to_import, 0u);
  // TODO(crbug.com/407587751): Update test when bookmarks parsing is
  // implemented.
  ASSERT_EQ(GetNumberOfBookmarksImported(), 0);
  ASSERT_EQ(GetNumberOfPaymentCardsImported(), 3);
  ASSERT_EQ(GetNumberOfURLsImported(), 7);
}

}  // namespace user_data_importer
