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
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/bookmarks/test/test_matchers.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/password_manager/core/browser/features/password_manager_features_util.h"
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
#include "components/sync/service/sync_service.h"
#include "components/sync/test/test_sync_service.h"
#include "components/user_data_importer/utility/bookmark_parser.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#if !BUILDFLAG(IS_IOS)
#include "components/user_data_importer/content/content_bookmark_parser.h"
#include "components/user_data_importer/content/fake_bookmark_html_parser.h"
#include "components/user_data_importer/mojom/bookmark_html_parser.mojom.h"
#include "content/public/test/browser_task_environment.h"  // nogncheck
#endif  // !BUILDFLAG(IS_IOS)

using bookmarks::test::IsFolder;
using bookmarks::test::IsUrlBookmark;

using password_manager::ImportEntry;
using password_manager::ImportResults;

using testing::_;
using testing::AllOf;
using testing::Assign;
using testing::ElementsAre;
using testing::Field;
using testing::IsEmpty;
using testing::Property;
using testing::SizeIs;

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
#if BUILDFLAG(IS_IOS)
  SafariDataImporterTest() : receiver_{&service_} {}
#else
  SafariDataImporterTest()
      : html_parser_receiver_{&fake_utility_parser_}, receiver_{&service_} {}
#endif  // BUILDFLAG(IS_IOS)

  ~SafariDataImporterTest() override = default;

  SafariDataImporterTest(const SafariDataImporterTest&) = delete;
  SafariDataImporterTest& operator=(const SafariDataImporterTest&) = delete;

  syncer::TestSyncService sync_service_;

 protected:
#if BUILDFLAG(IS_IOS)
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
#else
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
#endif  // BUILDFLAG(IS_IOS)

  void SetUp() override {
    CHECK(history_dir_.CreateUniqueTempDir());
    history_service_ = history::CreateHistoryService(history_dir_.GetPath(),
                                                     /*create_db=*/false);

    auto bookmark_client = std::make_unique<bookmarks::TestBookmarkClient>();

    bookmark_model_ = bookmarks::TestBookmarkClient::CreateModelWithClient(
        std::move(bookmark_client));

    auto storage = std::make_unique<FakeReadingListModelStorage>();

    base::WeakPtr<FakeReadingListModelStorage> storage_ptr =
        storage->AsWeakPtr();

    reading_list_model_ = std::make_unique<ReadingListModelImpl>(
        std::move(storage), syncer::StorageType::kUnspecified,
        syncer::WipeModelUponSyncDisabledBehavior::kNever,
        base::DefaultClock::GetInstance());

    storage_ptr->TriggerLoadCompletion();

#if BUILDFLAG(IS_IOS)
    auto parser = MakeBookmarkParser();
#else
    auto parser = std::make_unique<ContentBookmarkParser>();
    parser->SetServiceForTesting(
        html_parser_receiver_.BindNewPipeAndPassRemote());
#endif  // BUILDFLAG(IS_IOS)

    importer_ = std::make_unique<SafariDataImporter>(
        &client_, &presenter_,
        &autofill_client_.GetPersonalDataManager().payments_data_manager(),
        history_service_.get(), bookmark_model_.get(),
        reading_list_model_.get(), &sync_service_, std::move(parser), "en-US");

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

  const bookmarks::BookmarkNode* GetOtherBookmarkNode() {
    return bookmark_model_->other_node();
  }

  // Helper function for the "sync enabled" test.
  void PasswordsImportToAccountStore() {
    sync_service_.SetSignedIn(signin::ConsentLevel::kSignin);
    ASSERT_TRUE(password_manager::features_util::IsAccountStorageEnabled(
        &sync_service_));

    constexpr char kTestCSVInput[] =
        "Url,Username,Password,Note\n"
        "https://account.example.com,user1,pass1,note1\n";

    EXPECT_CALL(client_, OnPasswordsReady(AllOf(
                             Field(&ImportResults::number_imported, 0u),
                             Field(&ImportResults::number_to_import, 1u))));
    PreparePasswords(kTestCSVInput);

    EXPECT_CALL(client_, OnPasswordsImported(AllOf(
                             Field(&ImportResults::number_imported, 1u),
                             Field(&ImportResults::number_to_import, 0u))));
    EXPECT_CALL(client_, OnBookmarksImported(0));
    EXPECT_CALL(client_, OnHistoryImported(0));
    EXPECT_CALL(client_, OnPaymentCardsImported(0));

    CompleteImport({});

    EXPECT_THAT(account_store()->stored_passwords(), SizeIs(1));
    EXPECT_THAT(profile_store()->stored_passwords(), IsEmpty());
  }

  // Helper function for the "sync disabled" test.
  void PasswordsImportToProfileStore() {
    sync_service_.SetSignedOut();
    ASSERT_FALSE(password_manager::features_util::IsAccountStorageEnabled(
        &sync_service_));

    constexpr char kTestCSVInput[] =
        "Url,Username,Password,Note\n"
        "https://profile.example.com,user2,pass2,note2\n";

    EXPECT_CALL(client_, OnPasswordsReady(AllOf(
                             Field(&ImportResults::number_imported, 0u),
                             Field(&ImportResults::number_to_import, 1u))));
    PreparePasswords(kTestCSVInput);

    EXPECT_CALL(client_, OnPasswordsImported(AllOf(
                             Field(&ImportResults::number_imported, 1u),
                             Field(&ImportResults::number_to_import, 0u))));
    EXPECT_CALL(client_, OnBookmarksImported(0));
    EXPECT_CALL(client_, OnHistoryImported(0));
    EXPECT_CALL(client_, OnPaymentCardsImported(0));

    CompleteImport({});

    EXPECT_THAT(profile_store()->stored_passwords(), SizeIs(1));
    EXPECT_THAT(account_store()->stored_passwords(), IsEmpty());
  }

  ReadingListModel* GetReadingListModel() { return reading_list_model_.get(); }

  password_manager::TestPasswordStore* profile_store() {
    return profile_store_.get();
  }
  password_manager::TestPasswordStore* account_store() {
    return account_store_.get();
  }

  testing::StrictMock<MockSafariDataImportClient> client_;

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

#if !BUILDFLAG(IS_IOS)
  FakeBookmarkHtmlParser fake_utility_parser_;
  mojo::Receiver<user_data_importer::mojom::BookmarkHtmlParser>
      html_parser_receiver_;
#endif  // !BUILDFLAG(IS_IOS)

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
  EXPECT_EQ(entry.creation_time, base::Time::Now());
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
  EXPECT_EQ(entry.creation_time, base::Time::Now());
  EXPECT_EQ(entry.url, GURL("https://www.chromium.org/"));
  EXPECT_THAT(entry.path, IsEmpty());

  EXPECT_EQ(GetPendingReadingList().size(), 0u);
}

TEST_F(SafariDataImporterTest, Bookmarks_Folders) {
  ExpectBookmarksReady(3u);

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
  EXPECT_EQ(entry.creation_time, base::Time::Now());
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
  EXPECT_EQ(entry.creation_time, base::Time::Now());
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
  EXPECT_EQ(entry.creation_time, base::Time::Now());
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
  EXPECT_EQ(entry.creation_time, base::Time::Now());
  EXPECT_TRUE(entry.url.is_empty());
  EXPECT_THAT(entry.path, IsEmpty());

  entry = GetPendingReadingList()[1];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"The Beach Boys");
  // No timestamp maps to current time.
  EXPECT_EQ(entry.creation_time, base::Time::Now());
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
  ExpectBookmarksReady(2u);

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
  EXPECT_EQ(entry.creation_time, base::Time::Now());
  EXPECT_TRUE(entry.url.is_empty());
  EXPECT_THAT(entry.path, IsEmpty());

  // The folder contains a mix of invalid and valid entries. Ensure the valid
  // ones are preserved.
  entry = GetPendingBookmarks()[1];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Chromium");
  // No timestamp maps to current time.
  EXPECT_EQ(entry.creation_time, base::Time::Now());
  EXPECT_EQ(entry.url, GURL("https://www.chromium.org/"));
  EXPECT_THAT(entry.path, ElementsAre(u"Folder 1"));

  entry = GetPendingBookmarks()[2];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Example");
  // Invalid timestamp maps to current time.
  EXPECT_EQ(entry.creation_time, base::Time::Now());
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
  EXPECT_EQ(entry.creation_time, base::Time::Now());
  EXPECT_EQ(entry.url, GURL("https://www.chromium.org/"));
  EXPECT_THAT(entry.path, ElementsAre(u"Folder 1"));

  entry = GetPendingBookmarks()[1];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Example");
  // Invalid timestamp maps to current time.
  EXPECT_EQ(entry.creation_time, base::Time::Now());
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

#if BUILDFLAG(IS_IOS)
  ExpectBookmarksReady(6u);
#else
  ExpectBookmarksReady(5u);
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

  EXPECT_CALL(client_, OnBookmarksImported(5u));
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

#if BUILDFLAG(IS_IOS)
  ExpectBookmarksReady(6u, /*times=*/2);
#else
  ExpectBookmarksReady(5u, /*times=*/2);
#endif

  EXPECT_CALL(client_, OnPaymentCardsReady(3u)).Times(2);
  EXPECT_CALL(client_, OnHistoryReady(13u, _)).Times(2);

  PrepareImportFromFile();
  PrepareImportFromFile();
}

// Tests importing a single bookmark into the "Imported from Safari" folder.
TEST_F(SafariDataImporterTest, ImportSingleBookmark) {
  ExpectBookmarksReady(1u);
  PrepareBookmarks(
      R"(<!DOCTYPE NETSCAPE-Bookmark-file-1>
          <DT><A HREF="https://www.example.com/">Single Bookmark</A>)");

  EXPECT_CALL(client_, OnBookmarksImported(1u));
  EXPECT_CALL(client_, OnHistoryImported(0));
  EXPECT_CALL(client_, OnPaymentCardsImported(0));

  CompleteImport({});

  const bookmarks::BookmarkNode* other_node = GetOtherBookmarkNode();
  EXPECT_THAT(other_node->children(),
              ElementsAre(IsFolder(
                  u"Imported from Safari",
                  ElementsAre(IsUrlBookmark(
                      u"Single Bookmark", GURL("https://www.example.com/"))))));
}

// Tests importing multiple bookmarks into the "Imported from Safari" folder.
TEST_F(SafariDataImporterTest, ImportsMultipleBookmarks) {
  ExpectBookmarksReady(2u);
  PrepareBookmarks(
      R"(<!DOCTYPE NETSCAPE-Bookmark-file-1>
          <DL>
            <DT><A HREF="https://www.one.com/">First Bookmark</A>
            <DT><A HREF="https://www.two.com/">Second Bookmark</A>
          </DL>)");

  EXPECT_CALL(client_, OnBookmarksImported(2u));
  EXPECT_CALL(client_, OnHistoryImported(0));
  EXPECT_CALL(client_, OnPaymentCardsImported(0));
  CompleteImport({});

  const bookmarks::BookmarkNode* other_node = GetOtherBookmarkNode();
  EXPECT_THAT(other_node->children(),
              ElementsAre(IsFolder(
                  u"Imported from Safari",
                  ElementsAre(IsUrlBookmark(u"First Bookmark",
                                            GURL("https://www.one.com/")),
                              IsUrlBookmark(u"Second Bookmark",
                                            GURL("https://www.two.com/"))))));
}

// Tests that the folder hierarchy is preserved when importing a nested
// bookmark.
TEST_F(SafariDataImporterTest, ImportsNestedBookmark) {
  ExpectBookmarksReady(1u);
  PrepareBookmarks(
      R"(<!DOCTYPE NETSCAPE-Bookmark-file-1>
          <DL>
            <DT><H3>Top Folder</H3>
            <DL>
              <DT><H3>Second Folder</H3>
                <DL>
                  <DT><A HREF="https://www.nested.com/">Nested Bookmark</A>
                </DL>
            </DL>
          </DL>)");

  EXPECT_CALL(client_, OnBookmarksImported(1u));
  EXPECT_CALL(client_, OnHistoryImported(0));
  EXPECT_CALL(client_, OnPaymentCardsImported(0));
  CompleteImport({});

  const bookmarks::BookmarkNode* other_node = GetOtherBookmarkNode();
  EXPECT_THAT(
      other_node->children(),
      ElementsAre(IsFolder(
          u"Imported from Safari",
          ElementsAre(IsFolder(
              u"Top Folder",
              ElementsAre(IsFolder(u"Second Folder",
                                   ElementsAre(IsUrlBookmark(
                                       u"Nested Bookmark",
                                       GURL("https://www.nested.com/"))))))))));
}

// Tests that an empty bookmark folder is imported correctly.
TEST_F(SafariDataImporterTest, ImportsEmptyFolder) {
  ExpectBookmarksReady(0u);
  PrepareBookmarks(
      R"(<!DOCTYPE NETSCAPE-Bookmark-file-1>
          <DL>
            <DT><H3>Empty Folder</H3>
            <DL></DL>
          </DL>)");

  EXPECT_CALL(client_, OnBookmarksImported(0u));
  EXPECT_CALL(client_, OnHistoryImported(0));
  EXPECT_CALL(client_, OnPaymentCardsImported(0));
  CompleteImport({});

  const bookmarks::BookmarkNode* other_node = GetOtherBookmarkNode();
  EXPECT_THAT(
      other_node->children(),
      ElementsAre(IsFolder(u"Imported from Safari",
                           ElementsAre(IsFolder(u"Empty Folder", IsEmpty())))));
}

// Tests that the reading lists are imported into the Reading List model on iOS.
#if BUILDFLAG(IS_IOS)
TEST_F(SafariDataImporterTest, ImportsMultipleReadingListItems) {
  ExpectBookmarksReady(5u);
  PrepareBookmarks(
      R"(<!DOCTYPE NETSCAPE-Bookmark-file-1>
                          <DL>
                            <DT><H3 id="com.apple.ReadingList">Reading List</H3>
                            <DL>
                              <DT><A HREF="https://www.item1.com/">First Item</A>
                              <DT><A HREF="https://www.item2.com/">Second Item</A>
                              <DT>Third Item No URL</DT>
                              <DT><A HREF="invalid_url">Invalid URL</A>
                              <DT><A HREF="https://www.item3.com/">Third Item</A>
                            </DL>
                          </DL>)");

  EXPECT_CALL(client_, OnBookmarksImported(3u));
  EXPECT_CALL(client_, OnHistoryImported(0));
  EXPECT_CALL(client_, OnPaymentCardsImported(0));
  CompleteImport({});

  const ReadingListModel* model = GetReadingListModel();

  const auto& reading_list_entries = model->GetKeys();
  ASSERT_EQ(reading_list_entries.size(), 3u);

  const ReadingListEntry* entry1 =
      model->GetEntryByURL(GURL("https://www.item1.com/")).get();
  ASSERT_TRUE(entry1);
  EXPECT_EQ(entry1->Title(), "First Item");

  const ReadingListEntry* entry2 =
      model->GetEntryByURL(GURL("https://www.item2.com/")).get();
  ASSERT_TRUE(entry2);
  EXPECT_EQ(entry2->Title(), "Second Item");

  const ReadingListEntry* entry3 =
      model->GetEntryByURL(GURL("https://www.item3.com/")).get();
  ASSERT_TRUE(entry3);
  EXPECT_EQ(entry3->Title(), "Third Item");
}
#endif  // BUILDFLAG(IS_IOS)

TEST_F(SafariDataImporterTest, DuplicateBookmarkFolders) {
// TODO(crbug.com/407587751): Align behaviour of ContentBookmarkParser and
// IOSBookmarkParser.
#if BUILDFLAG(IS_IOS)
  ExpectBookmarksReady(3u);
#else
  ExpectBookmarksReady(2u);
#endif

  PrepareBookmarks(
      R"(<!DOCTYPE NETSCAPE-Bookmark-file-1>
          <DL>
            <DT><H3>Folder A</H3>
            <DL>
              <DT><A HREF="https://www.example1.com/">Bookmark 1</A>
            </DL>
            <DT><H3>Folder A</H3> <DL>
              <DT><H3>Folder B</H3>
              <DL>
                <DT><A HREF="https://www.example2.com/">Bookmark 2</A>
              </DL>
            </DL>
            <DT><H3>Folder A</H3> <DL>
              <DT><A HREF="https://www.example3.com/">Bookmark 3</A>
            </DL>
          </DL>)");

// TODO(crbug.com/407587751): Align behaviour of ContentBookmarkParser and
// IOSBookmarkParser.
#if BUILDFLAG(IS_IOS)
  EXPECT_CALL(client_, OnBookmarksImported(3u));
#else
  EXPECT_CALL(client_, OnBookmarksImported(2u));
#endif

  EXPECT_CALL(client_, OnHistoryImported(0));
  EXPECT_CALL(client_, OnPaymentCardsImported(0));
  CompleteImport({});

  const bookmarks::BookmarkNode* import_folder =
      GetOtherBookmarkNode()->children().at(0).get();

#if BUILDFLAG(IS_IOS)
  EXPECT_THAT(
      import_folder->children(),
      ElementsAre(
          IsFolder(u"Folder A",
                   ElementsAre(IsUrlBookmark(
                       u"Bookmark 1", GURL("https://www.example1.com/")))),
          IsFolder(u"Folder A",
                   ElementsAre(IsFolder(
                       u"Folder B", ElementsAre(IsUrlBookmark(
                                        u"Bookmark 2",
                                        GURL("https://www.example2.com/")))))),
          IsFolder(u"Folder A",
                   ElementsAre(IsUrlBookmark(
                       u"Bookmark 3", GURL("https://www.example3.com/"))))));
#else
  EXPECT_THAT(
      import_folder->children(),
      ElementsAre(
          IsFolder(u"Folder A",
                   ElementsAre(IsUrlBookmark(
                       u"Bookmark 1", GURL("https://www.example1.com/")))),
          IsFolder(u"Folder B",
                   ElementsAre(IsUrlBookmark(
                       u"Bookmark 2", GURL("https://www.example2.com/"))))));
#endif
}

// Tests that passwords are imported to the account store when sync is on.
TEST_F(SafariDataImporterTest,
       PasswordsImportedToAccountStoreWhenSyncIsEnabled) {
  PasswordsImportToAccountStore();
}

// Tests that passwords are imported to the profile store when sync is off.
TEST_F(SafariDataImporterTest,
       PasswordsImportedToProfileStoreWhenSyncIsDisabled) {
  PasswordsImportToProfileStore();
}

// Tests both password import scenarios (account and profile) sequentially.
TEST_F(SafariDataImporterTest, ImportToBothStoresSequentially) {
  PasswordsImportToAccountStore();

  // Clear the account store before the next import since
  // `PasswordsImportToProfileStore` expects account store to be empty.
  account_store()->Clear();

  PasswordsImportToProfileStore();
}

}  // namespace user_data_importer
