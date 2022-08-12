// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/import/password_importer.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/import/csv_password_sequence.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/password_manager/core/browser/ui/import_results.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::IsEmpty;

namespace password_manager {

namespace {
const char kTestOriginURL[] = "http://accounts.google.com/a/LoginAuth";
const char kTestSignonRealm[] = "http://accounts.google.com/";
const char16_t kTestUsername[] = u"test@gmail.com";
const char16_t kTestPassword[] = u"test1";
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
      if (result)
        for (const auto& pwd : seq)
          result->csv_passwords.push_back(pwd);
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
    store_->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);
  }

  PasswordImporterTest(const PasswordImporterTest&) = delete;
  PasswordImporterTest& operator=(const PasswordImporterTest&) = delete;

  ~PasswordImporterTest() override {
    store_->ShutdownOnUIThread();
    task_environment_.RunUntilIdle();
  }

 protected:
  void StartImportAndWaitForCompletion(const base::FilePath& input_file) {
    importer_.Import(input_file,
                     password_manager::PasswordForm::Store::kProfileStore,
                     base::BindOnce(&PasswordImporterTest::OnPasswordsConsumed,
                                    base::Unretained(this)));
    task_environment_.RunUntilIdle();
    ASSERT_TRUE(results_callback_called_);
  }

  std::vector<CredentialUIEntry> imported_passwords() {
    return presenter_.GetSavedCredentials();
  }

  PasswordImporter::Status GetImportStatus() const {
    return importer_.GetStatus();
  }

  // Directory for creating files by this test.
  base::ScopedTempDir temp_directory_;

 private:
  void OnPasswordsConsumed(const password_manager::ImportResults& results) {
    results_callback_called_ = true;
    import_results_ = std::move(results);
  }

  base::test::TaskEnvironment task_environment_;
  password_manager::ImportResults import_results_;
  bool results_callback_called_ = false;
  FakePasswordParserService service_;
  mojo::Receiver<mojom::CSVPasswordParser> receiver_;
  scoped_refptr<TestPasswordStore> store_ =
      base::MakeRefCounted<TestPasswordStore>();
  password_manager::SavedPasswordsPresenter presenter_{store_};
  password_manager::PasswordImporter importer_;
};

TEST_F(PasswordImporterTest, CSVImport) {
  constexpr char kTestCSVInput[] =
      "Url,Username,Password\n"
      "http://accounts.google.com/a/LoginAuth,test@gmail.com,test1\n";

  base::HistogramTester histogram_tester;

  base::FilePath input_path =
      temp_directory_.GetPath().AppendASCII(kTestFileName);
  ASSERT_EQ(
      static_cast<int>(std::size(kTestCSVInput)),
      base::WriteFile(input_path, kTestCSVInput, std::size(kTestCSVInput)));
  ASSERT_NO_FATAL_FAILURE(StartImportAndWaitForCompletion(input_path));

  histogram_tester.ExpectTotalCount("PasswordManager.ImportDuration", 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ImportedPasswordsPerUserInCSV", 1, 1);

  ASSERT_EQ(1u, imported_passwords().size());
  EXPECT_EQ(GURL(kTestOriginURL), imported_passwords()[0].url);
  EXPECT_EQ(kTestSignonRealm, imported_passwords()[0].signon_realm);
  EXPECT_EQ(kTestUsername, imported_passwords()[0].username);
  EXPECT_EQ(kTestPassword, imported_passwords()[0].password);
}

TEST_F(PasswordImporterTest, PartialImportSucceeds) {
  // This tests that when some rows aren't valid (2nd row in this case is
  // missing a site), only valid rows are imported.
  constexpr char kTestCSVInput[] =
      "Url,Username,Password\n"
      "http://accounts.google.com/a/LoginAuth,test@gmail.com,test1\n"
      ",test@gmail.com,test1\n";

  base::HistogramTester histogram_tester;

  base::FilePath input_path =
      temp_directory_.GetPath().AppendASCII(kTestFileName);
  ASSERT_EQ(
      static_cast<int>(std::size(kTestCSVInput)),
      base::WriteFile(input_path, kTestCSVInput, std::size(kTestCSVInput)));
  ASSERT_NO_FATAL_FAILURE(StartImportAndWaitForCompletion(input_path));

  histogram_tester.ExpectTotalCount("PasswordManager.ImportDuration", 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ImportedPasswordsPerUserInCSV", 1, 1);

  ASSERT_EQ(1u, imported_passwords().size());
  EXPECT_EQ(GURL(kTestOriginURL), imported_passwords()[0].url);
  EXPECT_EQ(kTestSignonRealm, imported_passwords()[0].signon_realm);
  EXPECT_EQ(kTestUsername, imported_passwords()[0].username);
  EXPECT_EQ(kTestPassword, imported_passwords()[0].password);
}

TEST_F(PasswordImporterTest, CSVImportLargeFileShouldFail) {
  base::HistogramTester histogram_tester;
  // content has more than kMaxFileSizeBytes (150KB) of bytes.
  std::string content(150 * 1024 + 100, '*');

  base::FilePath temp_file_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&temp_file_path));
  ASSERT_TRUE(base::WriteFile(temp_file_path, std::move(content)));

  ASSERT_NO_FATAL_FAILURE(StartImportAndWaitForCompletion(temp_file_path));

  EXPECT_THAT(imported_passwords(), IsEmpty());
  histogram_tester.ExpectTotalCount("PasswordManager.ImportDuration", 0);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.ImportedPasswordsPerUserInCSV", 0);

  EXPECT_EQ(PasswordImporter::Status::LARGE_FILE, GetImportStatus());
  base::DeleteFile(temp_file_path);
}

TEST_F(PasswordImporterTest, CSVImportNonExistingFile) {
  base::HistogramTester histogram_tester;
  base::FilePath src_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &src_dir));
  static const base::FilePath kTestsDirectory(FILE_PATH_LITERAL(
      "components/password_manager/core/browser/import/test"));
  base::FilePath input_path =
      src_dir.Append(kTestsDirectory).AppendASCII("non_existing_path");

  ASSERT_NO_FATAL_FAILURE(StartImportAndWaitForCompletion(input_path));

  histogram_tester.ExpectTotalCount("PasswordManager.ImportDuration", 0);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.ImportedPasswordsPerUserInCSV", 0);
  EXPECT_THAT(imported_passwords(), IsEmpty());
  EXPECT_EQ(PasswordImporter::Status::IO_ERROR, GetImportStatus());
}

TEST_F(PasswordImporterTest, ImportIOErrorDueToUnreadableFile) {
  base::HistogramTester histogram_tester;
  base::FilePath non_existent_input_file(FILE_PATH_LITERAL("nonexistent.csv"));
  ASSERT_NO_FATAL_FAILURE(
      StartImportAndWaitForCompletion(non_existent_input_file));

  histogram_tester.ExpectTotalCount("PasswordManager.ImportDuration", 0);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.ImportedPasswordsPerUserInCSV", 0);
  ASSERT_EQ(0u, imported_passwords().size());
}

}  // namespace password_manager
