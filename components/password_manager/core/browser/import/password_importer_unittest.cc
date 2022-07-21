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
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/import/csv_password_sequence.h"
#include "components/password_manager/core/browser/password_form.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "base/test/task_environment.h"

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
  PasswordImporterTest() : receiver_{&service_} {
    CHECK(temp_directory_.CreateUniqueTempDir());
    mojo::PendingRemote<mojom::CSVPasswordParser> pending_remote{
        receiver_.BindNewPipeAndPassRemote()};
    importer_.SetServiceForTesting(std::move(pending_remote));
  }

  PasswordImporterTest(const PasswordImporterTest&) = delete;
  PasswordImporterTest& operator=(const PasswordImporterTest&) = delete;

 protected:
  void StartImportAndWaitForCompletion(const base::FilePath& input_file) {
    importer_.Import(input_file,
                     base::BindOnce(&PasswordImporterTest::OnImportFinished,
                                    base::Unretained(this)));

    task_environment_.RunUntilIdle();

    ASSERT_TRUE(callback_called_);
  }

  void OnImportFinished(mojom::CSVPasswordSequencePtr seq) {
    callback_called_ = true;
    imported_passwords_.clear();
    if (!seq)
      return;
    for (const auto& pwd : seq->csv_passwords) {
      imported_passwords_.push_back(pwd.ToPasswordForm());
    }
  }

  const std::vector<PasswordForm>& imported_passwords() {
    return imported_passwords_;
  }

  PasswordImporter::Status GetImportStatus() const {
    return importer_.GetStatus();
  }

  // Directory for creating files by this test.
  base::ScopedTempDir temp_directory_;

 private:
  base::test::TaskEnvironment task_environment_;
  bool callback_called_ = false;
  std::vector<PasswordForm> imported_passwords_;
  FakePasswordParserService service_;
  mojo::Receiver<mojom::CSVPasswordParser> receiver_;
  password_manager::PasswordImporter importer_;
};

TEST_F(PasswordImporterTest, CSVImport) {
  const char kTestCSVInput[] =
      "Url,Username,Password\n"
      "http://accounts.google.com/a/LoginAuth,test@gmail.com,test1\n";

  base::FilePath input_path =
      temp_directory_.GetPath().AppendASCII(kTestFileName);
  ASSERT_EQ(static_cast<int>(strlen(kTestCSVInput)),
            base::WriteFile(input_path, kTestCSVInput, strlen(kTestCSVInput)));
  ASSERT_NO_FATAL_FAILURE(StartImportAndWaitForCompletion(input_path));

  ASSERT_EQ(1u, imported_passwords().size());
  EXPECT_EQ(GURL(kTestOriginURL), imported_passwords()[0].url);
  EXPECT_EQ(kTestSignonRealm, imported_passwords()[0].signon_realm);
  EXPECT_EQ(kTestUsername, imported_passwords()[0].username_value);
  EXPECT_EQ(kTestPassword, imported_passwords()[0].password_value);
}

TEST_F(PasswordImporterTest, CSVImportLargeFile) {
  // content has more than kMaxFileSizeBytes (150KB) of bytes.
  std::string content(150 * 1024 + 100, '*');

  base::FilePath temp_file_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&temp_file_path));
  ASSERT_TRUE(base::WriteFile(temp_file_path, std::move(content)));

  ASSERT_NO_FATAL_FAILURE(StartImportAndWaitForCompletion(temp_file_path));

  EXPECT_THAT(imported_passwords(), IsEmpty());
  EXPECT_EQ(PasswordImporter::Status::LARGE_FILE, GetImportStatus());
  base::DeleteFile(temp_file_path);
}

TEST_F(PasswordImporterTest, CSVImportNonExistingFile) {
  base::FilePath src_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &src_dir));
  static const base::FilePath kTestsDirectory(FILE_PATH_LITERAL(
      "components/password_manager/core/browser/import/test"));
  base::FilePath input_path =
      src_dir.Append(kTestsDirectory).AppendASCII("non_existing_path");

  ASSERT_NO_FATAL_FAILURE(StartImportAndWaitForCompletion(input_path));

  EXPECT_THAT(imported_passwords(), IsEmpty());
  EXPECT_EQ(PasswordImporter::Status::IO_ERROR, GetImportStatus());
}

TEST_F(PasswordImporterTest, ImportIOErrorDueToUnreadableFile) {
  base::FilePath non_existent_input_file(FILE_PATH_LITERAL("nonexistent.csv"));
  ASSERT_NO_FATAL_FAILURE(
      StartImportAndWaitForCompletion(non_existent_input_file));

  ASSERT_EQ(0u, imported_passwords().size());
}

}  // namespace password_manager
