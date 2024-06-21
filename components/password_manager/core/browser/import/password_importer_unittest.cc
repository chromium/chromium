// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/import/password_importer.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "components/password_manager/core/browser/import/csv_password_sequence.h"
#include "components/password_manager/core/browser/import/import_results.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "components/password_manager/core/common/password_manager_constants.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::IsEmpty;
namespace password_manager {

using DeleteFileCallback = PasswordImporter::DeleteFileCallback;

namespace {
const char kTestOriginURL[] = "http://accounts.google.com/a/LoginAuth";
const char kTestSignonRealm[] = "http://accounts.google.com/";
const char16_t kTestUsername[] = u"test@gmail.com";
const char16_t kTestPassword[] = u"test1";
const char16_t kTestNote[] = u"secret-note";
const char kTestFileName[] = "test_only.csv";
}  // namespace

// A wrapper on CSVPasswordSequence that mimics the sandbox behaviour.
class FakePasswordParserService : public mojom::CSVPasswordParser {
 public:
  void ParseCSV(const std::string& raw_json,
                ParseCSVCallback callback) override {
    mojom::CSVPasswordSequencePtr result = nullptr;
    CSVPasswordSequence seq(raw_json);
    if (seq.result() == CSVPassword::Status::kOK) {
      result = mojom::CSVPasswordSequence::New();
      if (result) {
        for (const auto& pwd : seq) {
          result->csv_passwords.push_back(pwd);
        }
      }
    }
    std::move(callback).Run(std::move(result));
  }
};

class PasswordImporterTest : public testing::Test {
 public:
  PasswordImporterTest() : receiver_{&service_}, importer_(&presenter_) {
    CHECK(temp_directory_.CreateUniqueTempDir());
    mojo::PendingRemote<mojom::CSVPasswordParser> pending_remote{
        receiver_.BindNewPipeAndPassRemote()};
    importer_.SetServiceForTesting(std::move(pending_remote));
    importer_.SetDeleteFileForTesting(mock_delete_file_.Get());

    profile_store_->Init(/*prefs=*/nullptr,
                         /*affiliated_match_helper=*/nullptr);
    account_store_->Init(/*prefs=*/nullptr,
                         /*affiliated_match_helper=*/nullptr);
    presenter_.Init();
    task_environment_.RunUntilIdle();
  }

  PasswordImporterTest(const PasswordImporterTest&) = delete;
  PasswordImporterTest& operator=(const PasswordImporterTest&) = delete;

  ~PasswordImporterTest() override {
    account_store_->ShutdownOnUIThread();
    profile_store_->ShutdownOnUIThread();
    task_environment_.RunUntilIdle();
  }

 protected:
  void StartImportAndWaitForCompletion(
      const base::FilePath& input_file,
      PasswordForm::Store to_store =
          password_manager::PasswordForm::Store::kProfileStore) {
    file_path_ = input_file;
    importer_.Import(input_file, to_store,
                     base::BindOnce(&PasswordImporterTest::OnPasswordsConsumed,
                                    base::Unretained(this)));
    task_environment_.RunUntilIdle();
    ASSERT_TRUE(results_callback_called_);
  }

  void AssertNotStartedState() {
    ASSERT_TRUE(importer_.IsState(PasswordImporter::kNotStarted));
  }

  void AssertFinishedState() {
    ASSERT_TRUE(importer_.IsState(PasswordImporter::kFinished));
  }

  void AssertConflictsState() {
    ASSERT_TRUE(importer_.IsState(PasswordImporter::kConflicts));
  }

  void ContinueImportAndWaitForCompletion(
      const std::vector<int>& selected_ids) {
    ASSERT_TRUE(results_callback_called_);
    results_callback_called_ = false;
    importer_.ContinueImport(
        selected_ids, base::BindOnce(&PasswordImporterTest::OnPasswordsConsumed,
                                     base::Unretained(this)));
    task_environment_.RunUntilIdle();
    ASSERT_TRUE(results_callback_called_);
  }

  void TriggerDeleteFile() {
    EXPECT_CALL(mock_delete_file_, Run(file_path_)).Times(1);
    importer_.DeleteFile();
    // Deletion is happening on the Thread pool asynchronously.
    task_environment_.RunUntilIdle();
  }

  std::vector<CredentialUIEntry> stored_passwords() {
    return presenter_.GetSavedCredentials();
  }

  // Adding via the store interface directly, since adding to both stores using
  // the presenter is not possible (a check for collision prevents that).
  void AddToProfileAndAccountStores(PasswordForm form) {
    form.in_store = password_manager::PasswordForm::Store::kProfileStore;
    profile_store_->AddLogin(form);
    task_environment_.RunUntilIdle();
    form.in_store = password_manager::PasswordForm::Store::kAccountStore;
    account_store_->AddLogin(form);
    task_environment_.RunUntilIdle();
  }

  bool AddPasswordForm(const PasswordForm& form) {
    bool result = presenter_.AddCredential(CredentialUIEntry(form));

    task_environment_.RunUntilIdle();
    return result;
  }

  ImportResults::Status GetResultsStatus() const {
    return import_results_.status;
  }

  password_manager::ImportResults GetImportResults() const {
    return import_results_;
  }

  base::FilePath temp_file_path() const {
    return temp_directory_.GetPath().AppendASCII(kTestFileName);
  }

 private:
  void OnPasswordsConsumed(const password_manager::ImportResults& results) {
    results_callback_called_ = true;
    import_results_ = results;
  }

  base::test::TaskEnvironment task_environment_;
  password_manager::ImportResults import_results_;
  bool results_callback_called_ = false;
  FakePasswordParserService service_;
  mojo::Receiver<mojom::CSVPasswordParser> receiver_;
  scoped_refptr<TestPasswordStore> profile_store_ =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(false));
  scoped_refptr<TestPasswordStore> account_store_ =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(true));
  affiliations::FakeAffiliationService affiliation_service_;
  SavedPasswordsPresenter presenter_{&affiliation_service_, profile_store_,
                                     account_store_};
  password_manager::PasswordImporter importer_;
  testing::StrictMock<base::MockCallback<DeleteFileCallback>> mock_delete_file_;
  // Directory for creating files by this test.
  base::ScopedTempDir temp_directory_;
  base::FilePath file_path_;
};

TEST_F(PasswordImporterTest, CSVImportBaseFields) {
  constexpr char kTestCSVInput[] =
      "Url,Username,Password\n"
      "http://accounts.google.com/a/LoginAuth,test@gmail.com,test1\n";

  base::HistogramTester histogram_tester;

  base::FilePath input_path = temp_file_path();
  ASSERT_TRUE(base::WriteFile(input_path, kTestCSVInput));
  ASSERT_NO_FATAL_FAILURE(StartImportAndWaitForCompletion(input_path));
  AssertFinishedState();

  histogram_tester.ExpectTotalCount("PasswordManager.ImportDuration", 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ImportedPasswordsPerUserInCSV", 1, 1);

  password_manager::ImportResults results = GetImportResults();

  EXPECT_EQ(ImportResults::Status::SUCCESS, results.status);
  EXPECT_EQ(1u, results.number_imported);
  ASSERT_EQ(1u, stored_passwords().size());
  EXPECT_EQ(GURL(kTestOriginURL), stored_passwords()[0].GetURL());
  EXPECT_EQ(kTestSignonRealm, stored_passwords()[0].GetFirstSignonRealm());
  EXPECT_EQ(kTestUsername, stored_passwords()[0].username);
  EXPECT_EQ(kTestPassword, stored_passwords()[0].password);
}

TEST_F(PasswordImporterTest, CanDeleteFileAfterImportWithNoErrors) {
  constexpr char kTestCSVInput[] =
      "Url,Username,Password\n"
      "http://accounts.google.com/a/LoginAuth,test@gmail.com,test1\n";

  base::FilePath input_path = temp_file_path();
  ASSERT_TRUE(base::WriteFile(input_path, kTestCSVInput));
  ASSERT_NO_FATAL_FAILURE(StartImportAndWaitForCompletion(input_path));
  AssertFinishedState();

  ASSERT_NO_FATAL_FAILURE(TriggerDeleteFile());
}

TEST_F(PasswordImporterTest, CSVImportWithNote) {
  constexpr char kTestCSVInput[] =
      "Url,Username,Password,Note\n"
      "http://accounts.google.com/a/"
      "LoginAuth,test@gmail.com,test1,secret-note\n";

  base::HistogramTester histogram_tester;

  base::FilePath input_path = temp_file_path();
  ASSERT_TRUE(base::WriteFile(input_path, kTestCSVInput));
  ASSERT_NO_FATAL_FAILURE(StartImportAndWaitForCompletion(input_path));
  AssertFinishedState();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.Import.PerFile.Notes.TotalCount", 1, 1);

  password_manager::ImportResults results = GetImportResults();

  EXPECT_EQ(1u, results.number_imported);
  ASSERT_EQ(1u, stored_passwords().size());
  EXPECT_EQ(kTestNote, stored_passwords()[0].note);
}

TEST_F(PasswordImporterTest, CSVImportLongNote) {
  std::string long_note = std::string(1001, '*');
  std::string kTestCSVInput = std::string("Url,Username,Password,Notes\n") +
                              "https://test.com,test@gmail.com,pwd," +
                              long_note + "\n";

  base::HistogramTester histogram_tester;

  base::FilePath input_path = temp_file_path();
  ASSERT_TRUE(base::WriteFile(input_path, kTestCSVInput));
  ASSERT_NO_FATAL_FAILURE(StartImportAndWaitForCompletion(input_path));
  AssertNotStartedState();

  histogram_tester.ExpectUniqueSample("PasswordManager.ImportEntryStatus",
                                      ImportEntry::Status::LONG_NOTE, 1);

  ASSERT_EQ(0u, stored_passwords().size());

  const password_manager::ImportResults results = GetImportResults();
  EXPECT_EQ(password_manager::ImportResults::Status::SUCCESS, results.status);
  ASSERT_EQ(1u, results.displayed_entries.size());
  EXPECT_EQ(password_manager::ImportEntry::Status::LONG_NOTE,
            results.displayed_entries[0].status);
}

TEST_F(PasswordImporterTest, CSVImportAndroidCredential) {
  constexpr char kTestAndroidSignonRealm[] =
      "android://"
      "Jzj5T2E45Hb33D-lk-"
      "EHZVCrb7a064dEicTwrTYQYGXO99JqE2YERhbMP1qLogwJiy87OsBzC09Gk094Z-U_hg==@"
      "com.netflix.mediaclient";
  constexpr char kTestCSVInput[] =
      "Url,Username,Password\n"
      "android://"
      "Jzj5T2E45Hb33D-lk-"
      "EHZVCrb7a064dEicTwrTYQYGXO99JqE2YERhbMP1qLogwJiy87OsBzC09Gk094Z-U_hg==@"
      "com.netflix.mediaclient,test@gmail.com,test1\n";

  base::HistogramTester histogram_tester;

  base::FilePath input_path = temp_file_path();
  ASSERT_TRUE(base::WriteFile(input_path, kTestCSVInput));
  ASSERT_NO_FATAL_FAILURE(StartImportAndWaitForCompletion(input_path));
  AssertFinishedState();

  histogram_tester.ExpectTotalCount("PasswordManager.ImportDuration", 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ImportedPasswordsPerUserInCSV", 1, 1);

  password_manager::ImportResults results = GetImportResults();

  EXPECT_EQ(ImportResults::Status::SUCCESS, results.status);
  EXPECT_EQ(1u, results.number_imported);
  ASSERT_EQ(1u, stored_passwords().size());
  EXPECT_EQ(GURL(kTestAndroidSignonRealm), stored_passwords()[0].GetURL());
  EXPECT_EQ(kTestAndroidSignonRealm,
            stored_passwords()[0].GetFirstSignonRealm());
  EXPECT_EQ(kTestUsername, stored_passwords()[0].username);
  EXPECT_EQ(kTestPassword, stored_passwords()[0].password);
}

TEST_F(PasswordImporterTest, CSVImportBadHeaderReturnsBadFormat) {
  constexpr char kTestCSVInput[] =
      "Non Canonical Field,Bar - another one,FooBar - another one\n"
      "http://accounts.google.com/a/LoginAuth,test@gmail.com,test1\n";

  base::HistogramTester histogram_tester;

  base::FilePath input_path = temp_file_path();
  ASSERT_TRUE(base::WriteFile(input_path, kTestCSVInput));
  ASSERT_NO_FATAL_FAILURE(StartImportAndWaitForCompletion(input_path));
  AssertNotStartedState();

  histogram_tester.ExpectTotalCount("PasswordManager.ImportDuration", 0);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.ImportedPasswordsPerUserInCSV", 0);
  histogram_tester.ExpectUniqueSample("PasswordManager.ImportFileSize", 120, 1);

  password_manager::ImportResults results = GetImportResults();

  EXPECT_EQ(ImportResults::Status::BAD_FORMAT, results.status);
  EXPECT_EQ(0u, results.number_imported);
  EXPECT_THAT(results.displayed_entries, IsEmpty());
  EXPECT_THAT(stored_passwords(), IsEmpty());
}

TEST_F(PasswordImporterTest,
       ExactMatchWithConflictingNotesTooLongConcatenation) {
  std::u16string local_note = std::u16string(501, 'b');
  std::string imported_note = std::string(501, 'a');
  std::string kTestCSVInput =
      "Url,Username,Password,Comments\n"
      "https://"
      "test.com,username_exists_in_profile_store,password_already_stored," +
      imported_note;

  PasswordForm form_profile_store;
  form_profile_store.url = GURL("https://test.com");
  form_profile_store.signon_realm = form_profile_store.url.spec();
  form_profile_store.username_value = u"username_exists_in_profile_store";
  form_profile_store.password_value = u"password_already_stored";
  form_profile_store.SetNoteWithEmptyUniqueDisplayName(local_note);
  form_profile_store.in_store =
      password_manager::PasswordForm::Store::kProfileStore;

  ASSERT_TRUE(AddPasswordForm(form_profile_store));

  base::HistogramTester histogram_tester;

  base::FilePath input_path = temp_file_path();
  ASSERT_TRUE(base::WriteFile(input_path, kTestCSVInput));
  ASSERT_NO_FATAL_FAILURE(StartImportAndWaitForCompletion(input_path));
  AssertNotStartedState();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ImportEntryStatus",
      ImportEntry::Status::LONG_CONCATENATED_NOTE, 1);

  const password_manager::ImportResults& results = GetImportResults();

  ASSERT_EQ(1u, results.displayed_entries.size());
  EXPECT_EQ(password_manager::ImportEntry::Status::LONG_CONCATENATED_NOTE,
            results.displayed_entries[0].status);

  EXPECT_EQ(0u, results.number_imported);
  ASSERT_EQ(1u, stored_passwords().size());
  EXPECT_EQ(local_note, stored_passwords()[0].note);
}

TEST_F(PasswordImporterTest, ExactMatchWithConflictingNotesValidConcatenation) {
  std::u16string local_note = u"local note";
  std::string imported_note = "imported note";
  std::string kTestCSVInput =
      "Url,Username,Password,Note\n"
      "https://"
      "test.com,username_exists_in_profile_store,password_already_stored," +
      imported_note;

  PasswordForm form_profile_store;
  form_profile_store.url = GURL("https://test.com");
  form_profile_store.signon_realm = form_profile_store.url.spec();
  form_profile_store.username_value = u"username_exists_in_profile_store";
  form_profile_store.password_value = u"password_already_stored";
  form_profile_store.SetNoteWithEmptyUniqueDisplayName(local_note);
  form_profile_store.in_store =
      password_manager::PasswordForm::Store::kProfileStore;

  ASSERT_TRUE(AddPasswordForm(form_profile_store));

  base::HistogramTester histogram_tester;

  base::FilePath input_path = temp_file_path();
  ASSERT_TRUE(base::WriteFile(input_path, kTestCSVInput));
  ASSERT_NO_FATAL_FAILURE(StartImportAndWaitForCompletion(input_path));
  AssertFinishedState();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.Import.PerFile.Notes.Concatenations", 1, 1);

  const password_manager::ImportResults& results = GetImportResults();

  ASSERT_EQ(0u, results.displayed_entries.size());
  EXPECT_EQ(1u, results.number_imported);
  ASSERT_EQ(1u, stored_passwords().size());
  EXPECT_EQ(u"local note\nimported note", stored_passwords()[0].note);
}

TEST_F(PasswordImporterTest, ExactMatchImportedNoteIsSubstingOfLocalNote) {
  std::u16string local_note = u"local note\nidentical part\n";
  std::string imported_note = "identical part\n";
  std::string kTestCSVInput =
      "Url,Username,Password,Note\n"
      "https://"
      "test.com,username_exists_in_profile_store,password_already_stored," +
      imported_note;

  PasswordForm form_profile_store;
  form_profile_store.url = GURL("https://test.com");
  form_profile_store.signon_realm = form_profile_store.url.spec();
  form_profile_store.username_value = u"username_exists_in_profile_store";
  form_profile_store.password_value = u"password_already_stored";
  form_profile_store.SetNoteWithEmptyUniqueDisplayName(local_note);
  form_profile_store.in_store =
      password_manager::PasswordForm::Store::kProfileStore;

  ASSERT_TRUE(AddPasswordForm(form_profile_store));

  base::HistogramTester histogram_tester;

  base::FilePath input_path = temp_file_path();
  ASSERT_TRUE(base::WriteFile(input_path, kTestCSVInput));
  ASSERT_NO_FATAL_FAILURE(StartImportAndWaitForCompletion(input_path));
  AssertFinishedState();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.Import.PerFile.Notes.Substrings", 1, 1);

  const password_manager::ImportResults& results = GetImportResults();

  ASSERT_EQ(0u, results.displayed_entries.size());
  EXPECT_EQ(1u, results.number_imported);
  ASSERT_EQ(1u, stored_passwords().size());
  EXPECT_EQ(local_note, stored_passwords()[0].note);
}

TEST_F(PasswordImporterTest, CSVImportExactMatchProfileStore) {
  constexpr char kTestCSVInput[] =
      "Url,Username,Password,Comment\n"
      "https://"
      "test.com,username_exists_in_profile_store,password_already_stored,"
      "secret-note\n";

  PasswordForm form_profile_store;
  form_profile_store.url = GURL("https://test.com");
  form_profile_store.signon_realm = form_profile_store.url.spec();
  form_profile_store.username_value = u"username_exists_in_profile_store";
  form_profile_store.password_value = u"password_already_stored";
  form_profile_store.SetNoteWithEmptyUniqueDisplayName(kTestNote);
  form_profile_store.in_store =
      password_manager::PasswordForm::Store::kProfileStore;

  ASSERT_TRUE(AddPasswordForm(form_profile_store));

  base::HistogramTester histogram_tester;

  base::FilePath input_path = temp_file_path();
  ASSERT_TRUE(base::WriteFile(input_path, kTestCSVInput));
  ASSERT_NO_FATAL_FAILURE(StartImportAndWaitForCompletion(input_path));
  AssertFinishedState();

  histogram_tester.ExpectTotalCount("PasswordManager.ImportDuration", 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ImportedPasswordsPerUserInCSV", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.Import.PerFile.Duplicates", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.Import.PerFile.Notes.Duplicates", 1, 1);

  const password_manager::ImportResults& results = GetImportResults();

  ASSERT_EQ(0u, results.displayed_entries.size());

  EXPECT_EQ(ImportResults::Status::SUCCESS, results.status);
  EXPECT_EQ(1u, results.number_imported);
  ASSERT_EQ(1u, stored_passwords().size());
  EXPECT_EQ(GURL("https://test.com"), stored_passwords()[0].GetURL());
  EXPECT_EQ(u"username_exists_in_profile_store",
            stored_passwords()[0].username);
  EXPECT_EQ(u"password_already_stored", stored_passwords()[0].password);
  EXPECT_EQ(kTestNote, stored_passwords()[0].note);
}

TEST_F(PasswordImporterTest, CSVImportExactMatchAccountStore) {
  constexpr char kTestCSVInput[] =
      "Url,Username,Password\n"
      "https://"
      "test.com,username_exists_in_account_store,password_already_stored\n";

  PasswordForm form_account_store;
  form_account_store.url = GURL("https://test.com");
  form_account_store.signon_realm = form_account_store.url.spec();
  form_account_store.username_value = u"username_exists_in_account_store";
  form_account_store.password_value = u"password_already_stored";
  form_account_store.in_store =
      password_manager::PasswordForm::Store::kAccountStore;

  ASSERT_TRUE(AddPasswordForm(form_account_store));

  base::HistogramTester histogram_tester;

  base::FilePath input_path = temp_file_path();
  ASSERT_TRUE(base::WriteFile(input_path, kTestCSVInput));
  ASSERT_NO_FATAL_FAILURE(StartImportAndWaitForCompletion(
      input_path, PasswordForm::Store::kAccountStore));
  AssertFinishedState();

  histogram_tester.ExpectTotalCount("PasswordManager.ImportDuration", 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ImportedPasswordsPerUserInCSV", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.Import.PerFile.Duplicates", 1, 1);

  const password_manager::ImportResults& results = GetImportResults();

  ASSERT_EQ(0u, results.displayed_entries.size());

  EXPECT_EQ(ImportResults::Status::SUCCESS, results.status);
  EXPECT_EQ(1u, results.number_imported);
  ASSERT_EQ(1u, stored_passwords().size());
  EXPECT_EQ(GURL("https://test.com"), stored_passwords()[0].GetURL());
  EXPECT_EQ(u"username_exists_in_account_store",
            stored_passwords()[0].username);
  EXPECT_EQ(u"password_already_stored", stored_passwords()[0].password);
}

TEST_F(PasswordImporterTest, CSVImportExactMatchProfileAndAccountStore) {
  constexpr char kTestCSVInput[] =
      "Url,Username,Password\n"
      "https://"
      "test.com,username_exists_in_profile_and_account_store,password_already_"
      "stored\n"
      "https://test2.com,username2,password2\n";

  PasswordForm form_account_profile_store;
  form_account_profile_store.url = GURL("https://test.com");
  form_account_profile_store.signon_realm =
      form_account_profile_store.url.spec();
  form_account_profile_store.username_value =
      u"username_exists_in_profile_and_account_store";
  form_account_profile_store.password_value = u"password_already_stored";

  AddToProfileAndAccountStores(std::move(form_account_profile_store));

  base::HistogramTester histogram_tester;

  base::FilePath input_path = temp_file_path();
  ASSERT_TRUE(base::WriteFile(input_path, kTestCSVInput));
  ASSERT_NO_FATAL_FAILURE(StartImportAndWaitForCompletion(
      input_path, PasswordForm::Store::kAccountStore));
  AssertFinishedState();

  histogram_tester.ExpectTotalCount("PasswordManager.ImportDuration", 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ImportedPasswordsPerUserInCSV", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.Import.PerFile.Duplicates", 1, 1);

  const password_manager::ImportResults& results = GetImportResults();

  ASSERT_EQ(0u, results.displayed_entries.size());

  EXPECT_EQ(ImportResults::Status::SUCCESS, results.status);
  EXPECT_EQ(2u, results.number_imported);
  ASSERT_EQ(2u, stored_passwords().size());
  EXPECT_EQ(GURL("https://test.com"), stored_passwords()[0].GetURL());
  EXPECT_EQ(u"username_exists_in_profile_and_account_store",
            stored_passwords()[0].username);
  EXPECT_EQ(u"password_already_stored", stored_passwords()[0].password);
  EXPECT_EQ(GURL("https://test2.com"), stored_passwords()[1].GetURL());
  EXPECT_EQ(u"username2", stored_passwords()[1].username);
  EXPECT_EQ(u"password2", stored_passwords()[1].password);
}

TEST_F(PasswordImporterTest, ImportReportsConflicts) {
  constexpr char kTestCSVInput[] =
      "Url,Username,Password\n"
      "https://test.com,username_exists_in_profile_store,new_password\n"
      "https://test2.com,username2,password2\n";

  PasswordForm form_profile_store;
  form_profile_store.url = GURL("https://test.com");
  form_profile_store.signon_realm = form_profile_store.url.spec();
  form_profile_store.username_value = u"username_exists_in_profile_store";
  form_profile_store.password_value = u"password_does_not_match";
  form_profile_store.in_store =
      password_manager::PasswordForm::Store::kProfileStore;

  ASSERT_TRUE(AddPasswordForm(form_profile_store));

  base::HistogramTester histogram_tester;

  base::FilePath input_path = temp_file_path();
  ASSERT_TRUE(base::WriteFile(input_path, kTestCSVInput));
  ASSERT_NO_FATAL_FAILURE(StartImportAndWaitForCompletion(
      input_path, password_manager::PasswordForm::Store::kProfileStore));
  AssertConflictsState();

  const password_manager::ImportResults& conflicts_results = GetImportResults();

  EXPECT_EQ(password_manager::ImportResults::Status::CONFLICTS,
            conflicts_results.status);
  ASSERT_EQ(1u, conflicts_results.displayed_entries.size());
  EXPECT_EQ("test.com", conflicts_results.displayed_entries[0].url);
  EXPECT_EQ("username_exists_in_profile_store",
            conflicts_results.displayed_entries[0].username);
  EXPECT_EQ("new_password", conflicts_results.displayed_entries[0].password);
  EXPECT_EQ(0, conflicts_results.displayed_entries[0].id);
  EXPECT_EQ(password_manager::ImportEntry::Status::VALID,
            conflicts_results.displayed_entries[0].status);

  // Non-conflicting password has not been imported yet.
  ASSERT_EQ(1u, stored_passwords().size());
}

TEST_F(PasswordImporterTest, ContinueImportCanReplaceConflictingPassword) {
  constexpr char kTestCSVInput[] =
      "Url,Username,Password\n"
      "https://test.com,username_exists_in_profile_store,new_password\n"
      "https://test2.com,username2,password2\n";

  PasswordForm form_profile_store;
  form_profile_store.url = GURL("https://test.com");
  form_profile_store.signon_realm = form_profile_store.url.spec();
  form_profile_store.username_value = u"username_exists_in_profile_store";
  form_profile_store.password_value = u"password_does_not_match";
  form_profile_store.in_store =
      password_manager::PasswordForm::Store::kProfileStore;

  ASSERT_TRUE(AddPasswordForm(form_profile_store));

  base::HistogramTester histogram_tester;

  base::FilePath input_path = temp_file_path();
  ASSERT_TRUE(base::WriteFile(input_path, kTestCSVInput));
  ASSERT_NO_FATAL_FAILURE(StartImportAndWaitForCompletion(
      input_path, password_manager::PasswordForm::Store::kProfileStore));
  AssertConflictsState();

  ASSERT_NO_FATAL_FAILURE(
      ContinueImportAndWaitForCompletion(/*selected_ids=*/{0}));
  ASSERT_NO_FATAL_FAILURE(TriggerDeleteFile());

  histogram_tester.ExpectTotalCount("PasswordManager.ImportEntryStatus", 0);
  histogram_tester.ExpectTotalCount("PasswordManager.ImportDuration", 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ImportedPasswordsPerUserInCSV", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.Import.PerFile.Conflicts", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.Import.PerFile.ConflictsResolved", 1, 1);

  const password_manager::ImportResults& results = GetImportResults();

  EXPECT_EQ(password_manager::ImportResults::Status::SUCCESS, results.status);
  ASSERT_EQ(0u, results.displayed_entries.size());

  EXPECT_EQ(2u, results.number_imported);
  ASSERT_EQ(2u, stored_passwords().size());
  EXPECT_EQ(GURL("https://test.com"), stored_passwords()[0].GetURL());
  EXPECT_EQ(u"username_exists_in_profile_store",
            stored_passwords()[0].username);
  EXPECT_EQ(u"new_password", stored_passwords()[0].password);
}

// A PasswordForm already exists in the ProfileStore, while conflicting
// credentials are imported to the AccountStore. No conflicts are reported to
// the  user.
TEST_F(PasswordImporterTest,
       CSVImportConflictSkippedInStoreDifferentFromTarget) {
  constexpr char kTestCSVInput[] =
      "Url,Username,Password\n"
      "https://"
      "test.com,username_exists_in_profile_store,password1\n"
      "https://test2.com,username2,password2\n";

  PasswordForm form_profile_store;
  form_profile_store.url = GURL("https://test.com");
  form_profile_store.signon_realm = form_profile_store.url.spec();
  form_profile_store.username_value = u"username_exists_in_profile_store";
  form_profile_store.password_value = u"password_does_not_match";
  form_profile_store.in_store =
      password_manager::PasswordForm::Store::kProfileStore;

  ASSERT_TRUE(AddPasswordForm(form_profile_store));

  base::HistogramTester histogram_tester;

  base::FilePath input_path = temp_file_path();
  ASSERT_TRUE(base::WriteFile(input_path, kTestCSVInput));
  ASSERT_NO_FATAL_FAILURE(StartImportAndWaitForCompletion(
      input_path, password_manager::PasswordForm::Store::kAccountStore));

  histogram_tester.ExpectTotalCount("PasswordManager.ImportEntryStatus", 0);
  histogram_tester.ExpectTotalCount("PasswordManager.ImportDuration", 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ImportedPasswordsPerUserInCSV", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.Import.PerFile.Conflicts", 0, 1);

  const password_manager::ImportResults& results = GetImportResults();

  EXPECT_EQ(0u, results.displayed_entries.size());

  EXPECT_EQ(password_manager::ImportResults::Status::SUCCESS, results.status);
  EXPECT_EQ(2u, results.number_imported);
  ASSERT_EQ(3u, stored_passwords().size());
}

TEST_F(PasswordImporterTest, CSVImportEmptyPasswordReported) {
  constexpr char kTestCSVInput[] =
      "Url,Username,Password\n"
      "http://accounts.google.com/a/LoginAuth,test@gmail.com,\n"
      "http://accounts.google.com/a/LoginAuth,,\n"
      ",,\n";

  base::HistogramTester histogram_tester;

  base::FilePath input_path = temp_file_path();
  ASSERT_TRUE(base::WriteFile(input_path, kTestCSVInput));
  ASSERT_NO_FATAL_FAILURE(StartImportAndWaitForCompletion(input_path));
  AssertNotStartedState();

  histogram_tester.ExpectUniqueSample("PasswordManager.ImportEntryStatus",
                                      ImportEntry::Status::MISSING_PASSWORD, 3);
  histogram_tester.ExpectTotalCount("PasswordManager.ImportDuration", 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ImportedPasswordsPerUserInCSV", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.Import.PerFile.PasswordAndUsernameMissing", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.Import.PerFile.AllLoginFieldsEmtpy", 1, 1);

  const password_manager::ImportResults& results = GetImportResults();

  EXPECT_EQ(password_manager::ImportResults::Status::SUCCESS, results.status);
  EXPECT_EQ(0u, results.number_imported);
  EXPECT_EQ(0u, stored_passwords().size());
  ASSERT_EQ(3u, results.displayed_entries.size());
  EXPECT_EQ(password_manager::ImportEntry::Status::MISSING_PASSWORD,
            results.displayed_entries[0].status);
  EXPECT_EQ(kTestOriginURL, results.displayed_entries[0].url);
  ASSERT_EQ("test@gmail.com", results.displayed_entries[0].username);
}

TEST_F(PasswordImporterTest, CSVImportEmptyURLReported) {
  constexpr char kTestCSVInput[] =
      "Url,Username,Password\n"
      ",test@gmail.com,test1   \n";

  base::HistogramTester histogram_tester;

  base::FilePath input_path = temp_file_path();
  ASSERT_TRUE(base::WriteFile(input_path, kTestCSVInput));
  ASSERT_NO_FATAL_FAILURE(StartImportAndWaitForCompletion(input_path));
  AssertNotStartedState();

  histogram_tester.ExpectUniqueSample("PasswordManager.ImportEntryStatus",
                                      ImportEntry::Status::MISSING_URL, 1);
  histogram_tester.ExpectTotalCount("PasswordManager.ImportDuration", 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.Import.PerFile.AnyErrors", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ImportedPasswordsPerUserInCSV", 0, 1);

  ASSERT_EQ(0u, stored_passwords().size());

  const password_manager::ImportResults results = GetImportResults();
  EXPECT_EQ(password_manager::ImportResults::Status::SUCCESS, results.status);
  ASSERT_EQ(1u, results.displayed_entries.size());
  EXPECT_EQ(password_manager::ImportEntry::Status::MISSING_URL,
            results.displayed_entries[0].status);
  EXPECT_EQ("test@gmail.com", results.displayed_entries[0].username);
}

TEST_F(PasswordImporterTest, CSVImportLongURLReported) {
  std::string long_url = "https://" + std::string(2048, 'a') + ".com";
  std::string kTestCSVInput =
      "Url,Username,Password\n" + long_url + ",test@gmail.com,test1   \n";

  base::HistogramTester histogram_tester;

  base::FilePath input_path = temp_file_path();
  ASSERT_TRUE(base::WriteFile(input_path, kTestCSVInput));
  ASSERT_NO_FATAL_FAILURE(StartImportAndWaitForCompletion(input_path));
  AssertNotStartedState();

  histogram_tester.ExpectUniqueSample("PasswordManager.ImportEntryStatus",
                                      ImportEntry::Status::LONG_URL, 1);
  histogram_tester.ExpectTotalCount("PasswordManager.ImportDuration", 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ImportedPasswordsPerUserInCSV", 0, 1);

  ASSERT_EQ(0u, stored_passwords().size());

  const password_manager::ImportResults results = GetImportResults();
  EXPECT_EQ(password_manager::ImportResults::Status::SUCCESS, results.status);
  ASSERT_EQ(1u, results.displayed_entries.size());
  EXPECT_EQ(password_manager::ImportEntry::Status::LONG_URL,
            results.displayed_entries[0].status);
  EXPECT_EQ("test@gmail.com", results.displayed_entries[0].username);
  std::string expected_url = long_url + "/";
  EXPECT_EQ(expected_url, results.displayed_entries[0].url);
}

TEST_F(PasswordImporterTest, CSVImportLongPassword) {
  std::string long_password = "https://" + std::string(1001, '*') + ".com";
  std::string kTestCSVInput = std::string("Url,Username,Password\n") +
                              "https://test.com,test@gmail.com," +
                              long_password + "\n";

  base::HistogramTester histogram_tester;

  base::FilePath input_path = temp_file_path();
  ASSERT_TRUE(base::WriteFile(input_path, kTestCSVInput));
  ASSERT_NO_FATAL_FAILURE(StartImportAndWaitForCompletion(input_path));
  AssertNotStartedState();

  histogram_tester.ExpectUniqueSample("PasswordManager.ImportEntryStatus",
                                      ImportEntry::Status::LONG_PASSWORD, 1);
  histogram_tester.ExpectTotalCount("PasswordManager.ImportDuration", 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ImportedPasswordsPerUserInCSV", 0, 1);

  ASSERT_EQ(0u, stored_passwords().size());

  const password_manager::ImportResults results = GetImportResults();
  EXPECT_EQ(password_manager::ImportResults::Status::SUCCESS, results.status);
  ASSERT_EQ(1u, results.displayed_entries.size());
  EXPECT_EQ(password_manager::ImportEntry::Status::LONG_PASSWORD,
            results.displayed_entries[0].status);
  EXPECT_EQ("test@gmail.com", results.displayed_entries[0].username);
  EXPECT_EQ("https://test.com/", results.displayed_entries[0].url);
}

TEST_F(PasswordImporterTest, CSVImportLongUsername) {
  std::string long_username = "https://" + std::string(1001, '*') + ".com";
  std::string kTestCSVInput = std::string("Url,Username,Password\n") +
                              "https://test.com," + long_username +
                              ",password\n";

  base::HistogramTester histogram_tester;

  base::FilePath input_path = temp_file_path();
  ASSERT_TRUE(base::WriteFile(input_path, kTestCSVInput));
  ASSERT_NO_FATAL_FAILURE(StartImportAndWaitForCompletion(input_path));
  AssertNotStartedState();

  histogram_tester.ExpectUniqueSample("PasswordManager.ImportEntryStatus",
                                      ImportEntry::Status::LONG_USERNAME, 1);
  histogram_tester.ExpectTotalCount("PasswordManager.ImportDuration", 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ImportedPasswordsPerUserInCSV", 0, 1);

  ASSERT_EQ(0u, stored_passwords().size());

  const password_manager::ImportResults results = GetImportResults();
  EXPECT_EQ(password_manager::ImportResults::Status::SUCCESS, results.status);
  ASSERT_EQ(1u, results.displayed_entries.size());
  EXPECT_EQ(password_manager::ImportEntry::Status::LONG_USERNAME,
            results.displayed_entries[0].status);
  EXPECT_EQ(long_username, results.displayed_entries[0].username);
  EXPECT_EQ("https://test.com/", results.displayed_entries[0].url);
}

TEST_F(PasswordImporterTest, CSVImportInvalidURLReported) {
  constexpr char kTestCSVInput[] =
      "Url,Username,Password\n"
      "ww1.google.com,test@gmail.com,test1   \n";

  base::HistogramTester histogram_tester;

  base::FilePath input_path = temp_file_path();
  ASSERT_TRUE(base::WriteFile(input_path, kTestCSVInput));
  ASSERT_NO_FATAL_FAILURE(StartImportAndWaitForCompletion(input_path));
  AssertNotStartedState();

  histogram_tester.ExpectUniqueSample("PasswordManager.ImportEntryStatus",
                                      ImportEntry::Status::INVALID_URL, 1);
  histogram_tester.ExpectTotalCount("PasswordManager.ImportDuration", 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ImportedPasswordsPerUserInCSV", 0, 1);

  ASSERT_EQ(0u, stored_passwords().size());

  const password_manager::ImportResults results = GetImportResults();
  EXPECT_EQ(password_manager::ImportResults::Status::SUCCESS, results.status);
  ASSERT_EQ(1u, results.displayed_entries.size());
  EXPECT_EQ(password_manager::ImportEntry::Status::INVALID_URL,
            results.displayed_entries[0].status);
  EXPECT_EQ("test@gmail.com", results.displayed_entries[0].username);

  EXPECT_EQ("ww1.google.com", results.displayed_entries[0].url);
}

TEST_F(PasswordImporterTest, CSVImportNonASCIIURL) {
  constexpr char kTestCSVInput[] =
      "Url,Username,Password\n"
      "https://.إلياس.com,test@gmail.com,test1   \n";

  base::HistogramTester histogram_tester;

  base::FilePath input_path = temp_file_path();
  ASSERT_TRUE(base::WriteFile(input_path, kTestCSVInput));
  ASSERT_NO_FATAL_FAILURE(StartImportAndWaitForCompletion(input_path));
  AssertFinishedState();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ImportedPasswordsPerUserInCSV", 1, 1);

  ASSERT_EQ(1u, stored_passwords().size());

  const password_manager::ImportResults results = GetImportResults();
  EXPECT_EQ(password_manager::ImportResults::Status::SUCCESS, results.status);
  ASSERT_EQ(0u, results.displayed_entries.size());
  EXPECT_EQ(GURL("https://.إلياس.com"), stored_passwords()[0].GetURL());
}

TEST_F(PasswordImporterTest, SingleFailedSingleSucceeds) {
  // This tests that when some rows aren't valid (2nd row in this case is
  // missing a site), only valid rows are imported.
  constexpr char kTestCSVInput[] =
      "Url,Password,Username\n"
      ",password1,test1   \n"
      "https://test2.com,password2,test2   \n";

  base::HistogramTester histogram_tester;

  base::FilePath input_path = temp_file_path();
  ASSERT_TRUE(base::WriteFile(input_path, kTestCSVInput));
  ASSERT_NO_FATAL_FAILURE(StartImportAndWaitForCompletion(input_path));
  AssertNotStartedState();

  histogram_tester.ExpectUniqueSample("PasswordManager.ImportEntryStatus",
                                      ImportEntry::Status::MISSING_URL, 1);
  histogram_tester.ExpectTotalCount("PasswordManager.ImportDuration", 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ImportedPasswordsPerUserInCSV", 1, 1);

  ASSERT_EQ(1u, stored_passwords().size());

  const password_manager::ImportResults results = GetImportResults();
  EXPECT_EQ(password_manager::ImportResults::Status::SUCCESS, results.status);
  EXPECT_EQ(1u, results.number_imported);
  ASSERT_EQ(1u, results.displayed_entries.size());
  EXPECT_EQ(password_manager::ImportEntry::Status::MISSING_URL,
            results.displayed_entries[0].status);
  EXPECT_EQ("test1   ", results.displayed_entries[0].username);
}

TEST_F(PasswordImporterTest, PartialImportSucceeds) {
  // This tests that when some rows aren't valid (2nd row in this case is
  // missing a site), only valid rows are imported.
  constexpr char kTestCSVInput[] =
      "Url,Username,Password\n"
      "http://accounts.google.com/a/LoginAuth,test@gmail.com,test1\n"
      ",test@gmail.com,test1\n";

  base::HistogramTester histogram_tester;

  base::FilePath input_path = temp_file_path();
  ASSERT_TRUE(base::WriteFile(input_path, kTestCSVInput));
  ASSERT_NO_FATAL_FAILURE(StartImportAndWaitForCompletion(input_path));
  AssertNotStartedState();

  histogram_tester.ExpectUniqueSample("PasswordManager.ImportEntryStatus",
                                      ImportEntry::Status::MISSING_URL, 1);
  histogram_tester.ExpectUniqueSample("PasswordManager.ImportFileSize",
                                      /*sample=*/105,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount("PasswordManager.ImportDuration", 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ImportedPasswordsPerUserInCSV", 1, 1);

  ASSERT_EQ(1u, stored_passwords().size());
  EXPECT_EQ(GURL(kTestOriginURL), stored_passwords()[0].GetURL());
  EXPECT_EQ(kTestSignonRealm, stored_passwords()[0].GetFirstSignonRealm());
  EXPECT_EQ(kTestUsername, stored_passwords()[0].username);
  EXPECT_EQ(kTestPassword, stored_passwords()[0].password);

  const password_manager::ImportResults& results = GetImportResults();

  EXPECT_EQ(password_manager::ImportResults::Status::SUCCESS, results.status);
  ASSERT_EQ(1u, results.displayed_entries.size());
  EXPECT_EQ(password_manager::ImportEntry::Status::MISSING_URL,
            results.displayed_entries[0].status);
  EXPECT_EQ("test@gmail.com", results.displayed_entries[0].username);
}

TEST_F(PasswordImporterTest, CSVImportLargeFileShouldFail) {
  base::HistogramTester histogram_tester;
  // content has more than kMaxFileSizeBytes (150KB) of bytes.
  std::string content(150 * 1024 + 100, '*');

  base::FilePath temp_file_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&temp_file_path));
  ASSERT_TRUE(base::WriteFile(temp_file_path, std::move(content)));

  ASSERT_NO_FATAL_FAILURE(StartImportAndWaitForCompletion(temp_file_path));
  AssertNotStartedState();

  EXPECT_THAT(stored_passwords(), IsEmpty());

  histogram_tester.ExpectUniqueSample("PasswordManager.ImportFileSize",
                                      /*sample=*/153700,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount("PasswordManager.ImportDuration", 0);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.ImportedPasswordsPerUserInCSV", 0);

  const password_manager::ImportResults& results = GetImportResults();
  EXPECT_EQ(ImportResults::Status::MAX_FILE_SIZE, results.status);

  base::DeleteFile(temp_file_path);
}

TEST_F(PasswordImporterTest, CSVImportHitMaxPasswordsLimit) {
  base::HistogramTester histogram_tester;
  std::string content = "url,login,password\n";
  std::string row = "http://a.b,c,d\n";
  const size_t EXCEEDS_LIMIT = constants::kMaxPasswordsPerCSVFile + 1;
  content.reserve(row.size() * EXCEEDS_LIMIT);
  for (size_t i = 0; i < EXCEEDS_LIMIT; i++) {
    content.append(row);
  }

  base::FilePath temp_file_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&temp_file_path));
  ASSERT_TRUE(base::WriteFile(temp_file_path, std::move(content)));

  ASSERT_NO_FATAL_FAILURE(StartImportAndWaitForCompletion(temp_file_path));
  AssertNotStartedState();

  EXPECT_THAT(stored_passwords(), IsEmpty());
  EXPECT_EQ(ImportResults::Status::NUM_PASSWORDS_EXCEEDED, GetResultsStatus());
  base::DeleteFile(temp_file_path);
}

TEST_F(PasswordImporterTest, CSVImportNonExistingFile) {
  base::HistogramTester histogram_tester;
  base::FilePath src_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_dir));
  static const base::FilePath kTestsDirectory(FILE_PATH_LITERAL(
      "components/password_manager/core/browser/import/test"));
  base::FilePath input_path =
      src_dir.Append(kTestsDirectory).AppendASCII("non_existing_path");

  ASSERT_NO_FATAL_FAILURE(StartImportAndWaitForCompletion(input_path));
  AssertNotStartedState();

  histogram_tester.ExpectTotalCount("PasswordManager.ImportFileSize", 0);
  histogram_tester.ExpectTotalCount("PasswordManager.ImportDuration", 0);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.ImportedPasswordsPerUserInCSV", 0);
  EXPECT_THAT(GetImportResults().displayed_entries, IsEmpty());
  EXPECT_THAT(stored_passwords(), IsEmpty());
  EXPECT_EQ(ImportResults::Status::IO_ERROR, GetResultsStatus());
}

TEST_F(PasswordImporterTest, ImportIOErrorDueToUnreadableFile) {
  base::HistogramTester histogram_tester;
  base::FilePath non_existent_input_file(FILE_PATH_LITERAL("nonexistent.csv"));
  ASSERT_NO_FATAL_FAILURE(
      StartImportAndWaitForCompletion(non_existent_input_file));
  AssertNotStartedState();

  histogram_tester.ExpectTotalCount("PasswordManager.ImportFileSize", 0);
  histogram_tester.ExpectTotalCount("PasswordManager.ImportDuration", 0);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.ImportedPasswordsPerUserInCSV", 0);
  EXPECT_THAT(GetImportResults().displayed_entries, IsEmpty());
  EXPECT_THAT(stored_passwords(), IsEmpty());
  EXPECT_EQ(ImportResults::Status::IO_ERROR, GetResultsStatus());
}

}  // namespace password_manager
