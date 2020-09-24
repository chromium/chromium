// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/import/password_importer.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/import/csv_password_sequence.h"
#include "components/password_manager/core/browser/password_form.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {
const char kTestOriginURL[] = "http://accounts.google.com/a/LoginAuth";
const char kTestSignonRealm[] = "http://accounts.google.com/";
const char kTestUsername[] = "test@gmail.com";
const char kTestPassword[] = "test1";
const char kTestFileName[] = "test_only.csv";
}  // namespace

class PasswordImporterTest : public testing::Test {
 public:
  PasswordImporterTest() { CHECK(temp_directory_.CreateUniqueTempDir()); }

 protected:
  void StartImportAndWaitForCompletion(const base::FilePath& input_file) {
    PasswordImporter::Import(
        input_file, base::BindOnce(&PasswordImporterTest::OnImportFinished,
                                   base::Unretained(this)));

    task_environment_.RunUntilIdle();

    ASSERT_TRUE(callback_called_);
  }

  void OnImportFinished(PasswordImporter::Result result,
                        CSVPasswordSequence seq) {
    callback_called_ = true;
    result_ = result;
    imported_passwords_.clear();
    if (result != password_manager::PasswordImporter::SUCCESS)
      return;
    for (const auto& pwd : seq) {
      imported_passwords_.push_back(pwd.ParseValid());
    }
  }

  const PasswordImporter::Result& result() { return result_; }
  const std::vector<PasswordForm>& imported_passwords() {
    return imported_passwords_;
  }

  // Directory for creating files by this test.
  base::ScopedTempDir temp_directory_;

 private:
  base::test::TaskEnvironment task_environment_;

  bool callback_called_ = false;
  PasswordImporter::Result result_ = PasswordImporter::NUM_IMPORT_RESULTS;
  std::vector<PasswordForm> imported_passwords_;

  DISALLOW_COPY_AND_ASSIGN(PasswordImporterTest);
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

  EXPECT_EQ(PasswordImporter::SUCCESS, result());
  ASSERT_EQ(1u, imported_passwords().size());
  EXPECT_EQ(GURL(kTestOriginURL), imported_passwords()[0].url);
  EXPECT_EQ(kTestSignonRealm, imported_passwords()[0].signon_realm);
  EXPECT_EQ(base::ASCIIToUTF16(kTestUsername),
            imported_passwords()[0].username_value);
  EXPECT_EQ(base::ASCIIToUTF16(kTestPassword),
            imported_passwords()[0].password_value);
}

TEST_F(PasswordImporterTest, ImportIOErrorDueToUnreadableFile) {
  base::FilePath non_existent_input_file(FILE_PATH_LITERAL("nonexistent.csv"));
  ASSERT_NO_FATAL_FAILURE(
      StartImportAndWaitForCompletion(non_existent_input_file));

  EXPECT_EQ(PasswordImporter::IO_ERROR, result());
  ASSERT_EQ(0u, imported_passwords().size());
}

}  // namespace password_manager
