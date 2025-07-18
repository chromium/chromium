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
#include "base/test/scoped_mock_clock_override.h"
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

using password_manager::ImportEntry;
using password_manager::ImportResults;

using testing::_;
using testing::AllOf;
using testing::Assign;
using testing::ElementsAre;
using testing::Field;
using testing::IsEmpty;
using testing::Property;

namespace user_data_importer {

class MockSafariDataImportClient : public SafariDataImportClient {
 public:
  MockSafariDataImportClient() = default;
  ~MockSafariDataImportClient() override = default;

  MOCK_METHOD(void, OnTotalFailure, (), (override));
  MOCK_METHOD(void, OnBookmarksReady, (size_t count), (override));
  MOCK_METHOD(void,
              OnHistoryReady,
              (size_t estimated_count, std::vector<std::u16string> profiles),
              (override));
  MOCK_METHOD(void,
              OnPasswordsReady,
              (const ImportResults& results),
              (override));
  MOCK_METHOD(void, OnPaymentCardsReady, (size_t count), (override));
  MOCK_METHOD(void, OnBookmarksImported, (size_t count), (override));
  MOCK_METHOD(void, OnHistoryImported, (size_t count), (override));
  MOCK_METHOD(void,
              OnPasswordsImported,
              (const ImportResults& results),
              (override));
  MOCK_METHOD(void, OnPaymentCardsImported, (size_t count), (override));

  base::WeakPtr<SafariDataImportClient> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockSafariDataImportClient> weak_ptr_factory_{this};
};

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
        &client_, &presenter_,
        &autofill_client_.GetPersonalDataManager().payments_data_manager(),
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
    Synchronize();
  }

  const std::vector<ImportedBookmarkEntry>& GetPendingBookmarks() const {
    return importer_->pending_bookmarks_;
  }

  const std::vector<ImportedBookmarkEntry>& GetPendingReadingList() const {
    return importer_->pending_reading_list_;
  }

  void PrepareBookmarks(std::string html_data) {
    bookmarks_idle_ = false;
    base::ScopedTempDir dir;
    ASSERT_TRUE(dir.CreateUniqueTempDir());
    base::FilePath path = dir.GetPath().AppendASCII("bookmarks.html");
    ASSERT_TRUE(base::WriteFile(path, html_data));

    importer_->PrepareBookmarks(std::move(path));
    Synchronize();
  }

  void PreparePasswords(std::string csv_data) {
    importer_->PreparePasswords(std::move(csv_data));
    Synchronize();
  }

  // Executes the import, using selected_ids to resolve password conflicts.
  void CompleteImport(const std::vector<int>& selected_ids) {
    importer_->CompleteImport(selected_ids);
    Synchronize();
  }

  void PreparePaymentCards(std::vector<PaymentCardEntry> payment_cards) {
    importer_->PreparePaymentCards(std::move(payment_cards));
    Synchronize();
  }

  void PrepareInvalidFile() {
    PrepareFile(base::FilePath(FILE_PATH_LITERAL("/invalid/path/to/zip/file")));
  }

  void PrepareImportFromFile() {
    base::FilePath zip_archive_path;
    ASSERT_TRUE(base::PathService::Get(base::DIR_ASSETS, &zip_archive_path));
    PrepareFile(zip_archive_path.Append(FILE_PATH_LITERAL("test_archive.zip")));
  }

  void CancelImport() { importer_->CancelImport(); }

  void SetHistorySizeThreshold(size_t history_size_threshold) {
    importer_->history_size_threshold_ = history_size_threshold;
  }

  // Sets an expectation of a call to `OnTotalFailure`, and adds the side
  // effect of setting the `bookmarks_idle_` bit.
  void ExpectTotalFailure() {
    EXPECT_CALL(client_, OnTotalFailure())
        .WillOnce(Assign(&bookmarks_idle_, true));
  }

  // Sets an expectation of a call to `OnBookmarksReady`, and adds the side
  // effect of setting the `bookmarks_idle_` bit.
  void ExpectBookmarksReady(auto result, int times = 1) {
    EXPECT_CALL(client_, OnBookmarksReady(result))
        .Times(times)
        .WillRepeatedly(Assign(&bookmarks_idle_, true));
  }

  testing::StrictMock<MockSafariDataImportClient> client_;

  base::ScopedMockClockOverride clock_;

 private:
  void WaitUntilPresenterIsReady() {
    ASSERT_TRUE(base::test::RunUntil([&]() { return presenter_ready_; }));
  }

  void OnPresenterReady() { presenter_ready_ = true; }

  void PrepareFile(const base::FilePath& file) {
    bookmarks_idle_ = false;
    importer_->PrepareImport(file);
    Synchronize();
  }

  void Synchronize() {
    task_environment_.RunUntilIdle();
#if BUILDFLAG(IS_IOS)
    // TODO(crbug.com/407587751): This hangs forever if not satisfied, probably
    // because of `clock_`. We should instead fail with a timeout, but this will
    // require refactoring how we mock time in this suite.
    ASSERT_TRUE(base::test::RunUntil([&]() { return bookmarks_idle_; }));
#endif  // BUILDFLAG(IS_IOS)
  }

  base::test::TaskEnvironment task_environment_;

  password_manager::FakePasswordParserService service_;
  mojo::Receiver<password_manager::mojom::CSVPasswordParser> receiver_;
  autofill::TestAutofillClient autofill_client_;
  base::ScopedTempDir history_dir_;
  std::unique_ptr<history::HistoryService> history_service_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<ReadingListModel> reading_list_model_;
  bool presenter_ready_ = false;

  // On iOS, bookmark processing happens in WebKit, so running until idle isn't
  // sufficient; we need to manually track when the parser becomes idle. This is
  // managed by the combination of `ExpectBookmarksReady` and `Synchronize`.
  bool bookmarks_idle_ = true;

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

TEST_F(SafariDataImporterTest, Bookmarks_Basic) {
  ExpectBookmarksReady(2u);

  PrepareBookmarks(R"(
      <!DOCTYPE NETSCAPE-Bookmark-file-1>
      <!--This is an automatically generated file.
      It will be read and overwritten.
      Do Not Edit! -->
      <DL>
      <DT><A HREF="https://www.google.com/" ADD_DATE="904914000">Google</A>
      <DT><A HREF="https://www.chromium.org/">Chromium</A>
      </DL>)");

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
  EXPECT_EQ(entry.creation_time, clock_.Now());
  EXPECT_EQ(entry.url, GURL("https://www.chromium.org/"));
  EXPECT_THAT(entry.path, IsEmpty());

  EXPECT_EQ(GetPendingReadingList().size(), 0u);
}

// Identical to the above test, but without the top-level <DL> tag enclosing it.
// It's documented as part of the format, but real-world Safari exports don't
// use it, so we have to support both with and without.
TEST_F(SafariDataImporterTest, Bookmarks_NoTopLevelDL) {
  ExpectBookmarksReady(2u);

  PrepareBookmarks(
      R"(<!DOCTYPE NETSCAPE-Bookmark-file-1>
      <!--This is an automatically generated file.
      It will be read and overwritten.
      Do Not Edit! -->
      <DT><A HREF="https://www.google.com/" ADD_DATE="904914000">Google</A>
      <DT><A HREF="https://www.chromium.org/">Chromium</A>)");

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
  EXPECT_EQ(entry.creation_time, clock_.Now());
  EXPECT_EQ(entry.url, GURL("https://www.chromium.org/"));
  EXPECT_THAT(entry.path, IsEmpty());

  EXPECT_EQ(GetPendingReadingList().size(), 0u);
}

TEST_F(SafariDataImporterTest, Bookmarks_Folders) {
// TODO(crbug.com/407587751): Align iOS and Blink implementation on if non-empty
// folders should be added explicitly.
#if BUILDFLAG(IS_IOS)
  ExpectBookmarksReady(6u);
#else
  ExpectBookmarksReady(4u);
#endif

  PrepareBookmarks(
      R"(<!DOCTYPE NETSCAPE-Bookmark-file-1>
      <!--This is an automatically generated file.
      It will be read and overwritten.
      Do Not Edit! -->
      <DL>
      <DT><A HREF="https://www.google.com/" ADD_DATE="904914000">Google</A>
      <DT><H3>Folder 1</H3>
      <DL><p>
        <DT><A HREF="https://www.example.com/" ADD_DATE="915181200">Example</A>
        <DT><H3 ADD_DATE="1145523600">Folder 1.1</H3>
        <DL><p>
          <DT><A HREF="https://en.wikipedia.org/wiki/Kitsune" ADD_DATE="1674205200">Kitsune</A>
        </DL><p>
      </DL><p>
      <DT><H3>Empty Folder</H3>
      <DL><p>
      </DL>
      </DL>)");

#if BUILDFLAG(IS_IOS)
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
  EXPECT_EQ(entry.creation_time, clock_.Now());
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
  EXPECT_EQ(entry.creation_time, clock_.Now());
  EXPECT_TRUE(entry.url.is_empty());
  EXPECT_THAT(entry.path, IsEmpty());

  EXPECT_EQ(GetPendingReadingList().size(), 0u);
#else
  ASSERT_EQ(GetPendingBookmarks().size(), 4u);

  ImportedBookmarkEntry entry = GetPendingBookmarks()[0];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Google");
  EXPECT_EQ(entry.creation_time,
            base::Time::FromSecondsSinceUnixEpoch(904914000));
  EXPECT_EQ(entry.url, GURL("https://www.google.com/"));
  EXPECT_THAT(entry.path, IsEmpty());

  entry = GetPendingBookmarks()[1];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Example");
  EXPECT_EQ(entry.creation_time,
            base::Time::FromSecondsSinceUnixEpoch(915181200));
  EXPECT_EQ(entry.url, GURL("https://www.example.com/"));
  EXPECT_THAT(entry.path, ElementsAre(u"Folder 1"));

  entry = GetPendingBookmarks()[2];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Kitsune");
  EXPECT_EQ(entry.creation_time,
            base::Time::FromSecondsSinceUnixEpoch(1674205200));
  EXPECT_EQ(entry.url, GURL("https://en.wikipedia.org/wiki/Kitsune"));
  EXPECT_THAT(entry.path, ElementsAre(u"Folder 1", u"Folder 1.1"));

  entry = GetPendingBookmarks()[3];
  EXPECT_TRUE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Empty Folder");
  // No timestamp maps to current time.
  EXPECT_EQ(entry.creation_time, clock_.Now());
  EXPECT_TRUE(entry.url.is_empty());
  EXPECT_THAT(entry.path, IsEmpty());

  EXPECT_EQ(GetPendingReadingList().size(), 0u);
#endif  // BUILDFLAG(IS_IOS)
}

#if BUILDFLAG(IS_IOS)
TEST_F(SafariDataImporterTest, Bookmarks_ReadingList) {
  ExpectBookmarksReady(4u);

  PrepareBookmarks(
      R"(<!DOCTYPE NETSCAPE-Bookmark-file-1>
      <!--This is an automatically generated file.
      It will be read and overwritten.
      Do Not Edit! -->
      <DL>
      <DT><A HREF="https://www.google.com/" ADD_DATE="904914000">Google</A>
      <DT><H3 id="com.apple.ReadingList">Reading List</H3>
      <DL><p>
      <DT><A HREF="https://en.wikipedia.org/wiki/The_Beach_Boys">The Beach Boys</A>
      <DT><A HREF="https://en.wikipedia.org/wiki/Brian_Wilson" ADD_DATE="-868878000">Brian Wilson</A>
      </DL><p>
      </DL>)");

  EXPECT_EQ(GetPendingBookmarks().size(), 1u);

  ASSERT_EQ(GetPendingReadingList().size(), 3u);

  ImportedBookmarkEntry entry = GetPendingReadingList()[0];
  EXPECT_TRUE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Reading List");
  EXPECT_EQ(entry.creation_time, clock_.Now());
  EXPECT_TRUE(entry.url.is_empty());
  EXPECT_THAT(entry.path, IsEmpty());

  entry = GetPendingReadingList()[1];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"The Beach Boys");
  // No timestamp maps to current time.
  EXPECT_EQ(entry.creation_time, clock_.Now());
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
#endif  // BUILDFLAG(IS_IOS)

TEST_F(SafariDataImporterTest, Bookmarks_MiscJunk) {
  // TODO(crbug.com/407587751): Align iOS and Blink implementation on if
  // non-empty folders should be added explicitly.
#if BUILDFLAG(IS_IOS)
  ExpectBookmarksReady(3u);
#else
  ExpectBookmarksReady(2u);
#endif

  PrepareBookmarks(R"(
      <!DOCTYPE NETSCAPE-Bookmark-file-1>
      <!--This is an automatically generated file.
      It will be read and overwritten.
      Do Not Edit! -->
      <DL>
      <DT><A>Google</A>
      <DT><H3>Folder 1</H3>
      <DL><p>
        <DT><A HREF="https://www.chromium.org/">Chromium</A>
        ICON_URI="https://www.chromium.org/favicon.ico"
        <DT><A HREF="https://www.example.org/" ADD_DATE="Last Tuesday">Example</A>
        <DT><A>Google Reader</A>
      </DL><p>
      <!-- Various unsupported types below -->
      FEED="true"
      FEEDURL="https://www.example.com"
      WEBSLICE="true"
      ISLIVEPREVIEW="true"
      PREVIEWSIZE="100 x 100"
      </DL>)");

#if BUILDFLAG(IS_IOS)
  ASSERT_EQ(GetPendingBookmarks().size(), 3u);

  // <A>Google</A> was skipped for lack of URL.

  ImportedBookmarkEntry entry = GetPendingBookmarks()[0];
  EXPECT_TRUE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Folder 1");
  // No timestamp maps to current time.
  EXPECT_EQ(entry.creation_time, clock_.Now());
  EXPECT_TRUE(entry.url.is_empty());
  EXPECT_THAT(entry.path, IsEmpty());

  // The folder contains a mix of invalid and valid entries. Ensure the valid
  // ones are preserved.
  entry = GetPendingBookmarks()[1];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Chromium");
  // No timestamp maps to current time.
  EXPECT_EQ(entry.creation_time, clock_.Now());
  EXPECT_EQ(entry.url, GURL("https://www.chromium.org/"));
  EXPECT_THAT(entry.path, ElementsAre(u"Folder 1"));

  entry = GetPendingBookmarks()[2];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Example");
  // Invalid timestamp maps to current time.
  EXPECT_EQ(entry.creation_time, clock_.Now());
  EXPECT_EQ(entry.url, GURL("https://www.example.org/"));
  EXPECT_THAT(entry.path, ElementsAre(u"Folder 1"));

  // <A>Google Reader</A> was skipped for lack of URL.
#else
  ASSERT_EQ(GetPendingBookmarks().size(), 2u);

  // <A>Google</A> was skipped for lack of URL.

  // The folder contains a mix of invalid and valid entries. Ensure the valid
  // ones are preserved.
  ImportedBookmarkEntry entry = GetPendingBookmarks()[0];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Chromium");
  // No timestamp maps to current time.
  EXPECT_EQ(entry.creation_time, clock_.Now());
  EXPECT_EQ(entry.url, GURL("https://www.chromium.org/"));
  EXPECT_THAT(entry.path, ElementsAre(u"Folder 1"));

  entry = GetPendingBookmarks()[1];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Example");
  // Invalid timestamp maps to current time.
  EXPECT_EQ(entry.creation_time, clock_.Now());
  EXPECT_EQ(entry.url, GURL("https://www.example.org/"));
  EXPECT_THAT(entry.path, ElementsAre(u"Folder 1"));

  // <A>Google Reader</A> was skipped for lack of URL.
#endif  // BUILDFLAG(IS_IOS)
}

TEST_F(SafariDataImporterTest, NoPassword) {
  EXPECT_CALL(client_,
              OnPasswordsReady(Field(&ImportResults::number_imported, 0u)));

  PreparePasswords("");
}

TEST_F(SafariDataImporterTest, NoPaymentCard) {
  EXPECT_CALL(client_, OnPaymentCardsReady(0));

  PreparePaymentCards(std::vector<PaymentCardEntry>());
}

TEST_F(SafariDataImporterTest, PasswordImport) {
  constexpr char kTestCSVInput[] =
      "Url,Username,Password,Note\n"
      "http://example1.com,username1,password1,note1\n"
      "http://example1.com,username2,password2,note2\n"
      "http://example2.com,username1,password3,note3\n";

  EXPECT_CALL(client_, OnPasswordsReady(
                           AllOf(Field(&ImportResults::number_imported, 0u),
                                 Field(&ImportResults::number_to_import, 3u))));
  PreparePasswords(kTestCSVInput);

  EXPECT_CALL(client_, OnPasswordsImported(
                           AllOf(Field(&ImportResults::number_imported, 3u),
                                 Field(&ImportResults::number_to_import, 0u))));

  EXPECT_CALL(client_, OnBookmarksImported(_));
  EXPECT_CALL(client_, OnHistoryImported(_));
  EXPECT_CALL(client_, OnPaymentCardsImported(_));

  CompleteImport({});
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
  EXPECT_CALL(client_, OnPasswordsReady(
                           AllOf(Field(&ImportResults::number_imported, 0u),
                                 Field(&ImportResults::number_to_import, 3u))));
  PreparePasswords(kTestCSVInput);

  // Confirm password import.
  EXPECT_CALL(client_, OnPasswordsImported(
                           AllOf(Field(&ImportResults::number_imported, 3u),
                                 Field(&ImportResults::number_to_import, 0u))));

  EXPECT_CALL(client_, OnBookmarksImported(_));
  EXPECT_CALL(client_, OnHistoryImported(_));
  EXPECT_CALL(client_, OnPaymentCardsImported(_));

  CompleteImport({});

  // Attempt to import 2 conflicting passwords, which should return conflicts.
  EXPECT_CALL(client_,
              OnPasswordsReady(
                  AllOf(Field(&ImportResults::number_imported, 0u),
                        Field(&ImportResults::number_to_import, 0u),
                        Field(&ImportResults::displayed_entries,
                              Property(&std::vector<ImportEntry>::size, 2u)))));

  PreparePasswords(kTestCSVConflicts);

  // Resolve the 2 conflicts.
  EXPECT_CALL(client_, OnPasswordsImported(
                           AllOf(Field(&ImportResults::number_imported, 2u),
                                 Field(&ImportResults::number_to_import, 0u))));

  EXPECT_CALL(client_, OnBookmarksImported(_));
  EXPECT_CALL(client_, OnHistoryImported(_));
  EXPECT_CALL(client_, OnPaymentCardsImported(_));

  CompleteImport({0, 1});
}

TEST_F(SafariDataImporterTest, TotalFailure) {
  ExpectTotalFailure();
  PrepareInvalidFile();
}

TEST_F(SafariDataImporterTest, CancelImport) {
  ExpectBookmarksReady(_);
  EXPECT_CALL(client_, OnHistoryReady(_, _));
  EXPECT_CALL(client_, OnPasswordsReady(_));
  EXPECT_CALL(client_, OnPaymentCardsReady(_));

  PrepareImportFromFile();

  // No additional calls to the client are made after a cancellation, since
  // nothing is ultimately imported.
  CancelImport();
}

TEST_F(SafariDataImporterTest, ImportFileEndToEnd) {
  EXPECT_CALL(client_, OnPasswordsReady(
                           AllOf(Field(&ImportResults::number_imported, 0u),
                                 Field(&ImportResults::number_to_import, 3u))));
  // TODO(crbug.com/407587751): Align iOS and Blink implementation on if
  // non-empty folders should be added explicitly.
#if BUILDFLAG(IS_IOS)
  ExpectBookmarksReady(7u);
#else
  ExpectBookmarksReady(6u);
#endif
  EXPECT_CALL(client_, OnPaymentCardsReady(3u));
  EXPECT_CALL(client_, OnHistoryReady(13u, _));  // Approximation.

  PrepareImportFromFile();

  // Use a small history size threshold so that ParseHistoryCallback gets called
  // multiple times internally.
  SetHistorySizeThreshold(3u);

  EXPECT_CALL(client_, OnPasswordsImported(
                           AllOf(Field(&ImportResults::number_imported, 3u),
                                 Field(&ImportResults::number_to_import, 0u))));
  EXPECT_CALL(client_, OnBookmarksImported(0u));
  EXPECT_CALL(client_, OnPaymentCardsImported(3u));
  EXPECT_CALL(client_, OnHistoryImported(7u));  // Actual.

  CompleteImport({});
}

// Smoke test to make sure that PrepareImport is idempotent(ish).
TEST_F(SafariDataImporterTest, PrepareImportFileTwice) {
  // Despite running twice, the results should be identical both times.
  EXPECT_CALL(client_, OnPasswordsReady(
                           AllOf(Field(&ImportResults::number_imported, 0u),
                                 Field(&ImportResults::number_to_import, 3u))))
      .Times(2);

  // TODO(crbug.com/407587751): Align iOS and Blink implementation on if
  // non-empty folders should be added explicitly.
#if BUILDFLAG(IS_IOS)
  ExpectBookmarksReady(7u, /*times=*/2);
#else
  ExpectBookmarksReady(6u, /*times=*/2);
#endif

  EXPECT_CALL(client_, OnPaymentCardsReady(3u)).Times(2);
  EXPECT_CALL(client_, OnHistoryReady(13u, _)).Times(2);

  PrepareImportFromFile();
  PrepareImportFromFile();
}

}  // namespace user_data_importer
