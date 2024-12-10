// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/export/login_db_deprecation_password_exporter.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/password_manager/core/browser/password_store/fake_password_store_backend.h"
#include "components/password_manager/core/browser/password_store/password_store_backend.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {
const std::string kLineEnding = "\n";
}

class LoginDbDeprecationPasswordExporterTest : public testing::Test {
 public:
  LoginDbDeprecationPasswordExporterTest() = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    password_store_ = base::MakeRefCounted<TestPasswordStore>();
    password_store_->Init(&pref_service_, /*affiliated_match_helper_*/ nullptr);
    exporter_ = std::make_unique<LoginDbDeprecationPasswordExporter>(
        temp_dir_.GetPath());
  }

  void TearDown() override {
    password_store_->ShutdownOnUIThread();
    ASSERT_TRUE(temp_dir_.Delete());
  }

  const base::FilePath& export_dir() { return temp_dir_.GetPath(); }

  LoginDbDeprecationPasswordExporter* exporter() { return exporter_.get(); }
  PasswordStoreInterface* password_store() { return password_store_.get(); }

 protected:
  base::test::TaskEnvironment task_env_;

 private:
  std::unique_ptr<LoginDbDeprecationPasswordExporter> exporter_;
  TestingPrefServiceSimple pref_service_;
  scoped_refptr<TestPasswordStore> password_store_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(LoginDbDeprecationPasswordExporterTest, DoesntExportIfNoPasswords) {
  // No passwords in the backend.
  exporter()->Start(password_store());

  task_env_.RunUntilIdle();

  base::FilePath expected_file_path =
      export_dir().Append(FILE_PATH_LITERAL("ChromePasswords.csv"));
  EXPECT_FALSE(base::PathExists(expected_file_path));
}

TEST_F(LoginDbDeprecationPasswordExporterTest, ExportsBackendPasswords) {
  const std::u16string kNoteValue =
      base::UTF8ToUTF16("Note Line 1" + kLineEnding + "Note Line 2");
  PasswordForm form;
  form.signon_realm = "https://example.com/";
  form.url = GURL("https://example.com");
  form.username_value = u"Someone";
  form.password_value = u"Secret";
  form.notes = {PasswordNote(kNoteValue, base::Time::Now())};

  base::test::TestFuture<PasswordChangesOrError> future;
  password_store()->AddLogin(form, task_env_.QuitClosure());
  task_env_.RunUntilQuit();

  exporter()->Start(password_store());
  base::FilePath expected_file_path =
      export_dir().Append(FILE_PATH_LITERAL("ChromePasswords.csv"));
  ASSERT_TRUE(base::test::RunUntil([&expected_file_path]() {
    return base::PathExists(expected_file_path);
  }));

  std::string contents;
  const std::string kExpectedContents =
      "name,url,username,password,note" + kLineEnding +
      "example.com,https://example.com/,Someone,Secret,\"Note Line "
      "1" +
      kLineEnding + "Note Line 2\"" + kLineEnding;
  ASSERT_TRUE(base::ReadFileToString(expected_file_path, &contents));
  EXPECT_TRUE(!contents.empty());
  EXPECT_EQ(kExpectedContents, contents);
}

}  // namespace password_manager
