// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/export/password_manager_exporter.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "components/password_manager/core/browser/export/export_progress_status.h"
#include "components/password_manager/core/browser/export/password_csv_writer.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrEq;
using ::testing::StrictMock;

// A callback that matches the signature of the std::string_view variant of
// base::WriteFile().
using WriteCallback =
    base::RepeatingCallback<bool(const base::FilePath&, std::string_view)>;
using DeleteCallback = PasswordManagerExporter::DeleteCallback;
using SetPosixFilePermissionsCallback =
    PasswordManagerExporter::SetPosixFilePermissionsCallback;

#if BUILDFLAG(IS_WIN)
const base::FilePath::CharType kNullFileName[] = FILE_PATH_LITERAL("/nul");
#else
const base::FilePath::CharType kNullFileName[] = FILE_PATH_LITERAL("/dev/null");
#endif

// Creates a hardcoded set of credentials for tests.
PasswordForm CreateTestPassword() {
  PasswordForm password_form;
  password_form.url = GURL("http://accounts.google.com/a/LoginAuth");
  password_form.username_value = u"test@gmail.com";
  password_form.password_value = u"test1";
  password_form.in_store = PasswordForm::Store::kProfileStore;
  return password_form;
}

PasswordExportInfo CreateExportInProgressInfo() {
  return {.status = ExportProgressStatus::kInProgress};
}

PasswordExportInfo CreateSuccessfulExportInfo(const base::FilePath& path) {
  return {
      .status = ExportProgressStatus::kSucceeded,
#if !BUILDFLAG(IS_WIN)
      .file_path = path.value(),
#else
      .file_path = base::WideToUTF8(path.value()),
#endif
  };
}

PasswordExportInfo CreateFailedExportInfo(const base::FilePath& path) {
  return {.status = ExportProgressStatus::kFailedWrite,
          .folder_name = path.DirName().BaseName().AsUTF8Unsafe()};
}

PasswordExportInfo CreateCancelledExportInfo() {
  return {.status = ExportProgressStatus::kFailedCancelled};
}

class PasswordManagerExporterTest : public testing::Test {
 public:
  PasswordManagerExporterTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI),
        exporter_(&presenter_,
                  mock_on_progress_.Get(),
                  mock_completion_callback_.Get()),
        destination_path_(kNullFileName) {
    exporter_.SetWriteForTesting(mock_write_file_.Get());
    exporter_.SetDeleteForTesting(mock_delete_file_.Get());
    exporter_.SetSetPosixFilePermissionsForTesting(
        mock_set_posix_file_permissions_.Get());
    store_->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);
    presenter_.Init();
    task_environment_.RunUntilIdle();
  }

  PasswordManagerExporterTest(const PasswordManagerExporterTest&) = delete;
  PasswordManagerExporterTest& operator=(const PasswordManagerExporterTest&) =
      delete;

  ~PasswordManagerExporterTest() override {
    store_->ShutdownOnUIThread();
    task_environment_.RunUntilIdle();
  }

  void SetPasswordList(const std::vector<PasswordForm>& forms) {
    for (const auto& form : forms) {
      store_->AddLogin(form);
    }
    task_environment_.RunUntilIdle();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<TestPasswordStore> store_ =
      base::MakeRefCounted<TestPasswordStore>();
  affiliations::FakeAffiliationService affiliation_service_;
  SavedPasswordsPresenter presenter_{&affiliation_service_, store_,
                                     /*account_store=*/nullptr};
  base::MockCallback<base::RepeatingCallback<void(const PasswordExportInfo&)>>
      mock_on_progress_;
  base::MockOnceClosure mock_completion_callback_;
  PasswordManagerExporter exporter_;
  StrictMock<base::MockCallback<WriteCallback>> mock_write_file_;
  StrictMock<base::MockCallback<DeleteCallback>> mock_delete_file_;
  NiceMock<base::MockCallback<SetPosixFilePermissionsCallback>>
      mock_set_posix_file_permissions_;
  base::FilePath destination_path_;
  base::HistogramTester histogram_tester_;
};

TEST_F(PasswordManagerExporterTest, PasswordExportSetPasswordListFirst) {
  PasswordForm form = CreateTestPassword();
  SetPasswordList({form});
  const std::string serialised(
      PasswordCSVWriter::SerializePasswords({CredentialUIEntry(form)}));

  EXPECT_CALL(mock_write_file_, Run(destination_path_, StrEq(serialised)))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_on_progress_, Run(CreateExportInProgressInfo()));
  EXPECT_CALL(mock_on_progress_,
              Run(CreateSuccessfulExportInfo(destination_path_)));

  exporter_.PreparePasswordsForExport();
  exporter_.SetDestination(destination_path_);

  EXPECT_CALL(mock_completion_callback_, Run);
  task_environment_.RunUntilIdle();
}

// When writing fails, we should notify the UI of the failure and try to cleanup
// a possibly partial passwords file.
TEST_F(PasswordManagerExporterTest, WriteFileFailed) {
  SetPasswordList({CreateTestPassword()});

  EXPECT_CALL(mock_write_file_, Run).WillOnce(Return(false));
  EXPECT_CALL(mock_delete_file_, Run(destination_path_));
  EXPECT_CALL(mock_on_progress_, Run(CreateExportInProgressInfo()));
  EXPECT_CALL(mock_on_progress_,
              Run(CreateFailedExportInfo(destination_path_)));

  exporter_.PreparePasswordsForExport();
  exporter_.SetDestination(destination_path_);

  EXPECT_CALL(mock_completion_callback_, Run);
  task_environment_.RunUntilIdle();
}

// Test that GetProgressStatus() returns the last ExportProgressStatus sent
// to the callback.
TEST_F(PasswordManagerExporterTest, GetProgressReturnsLastCallbackStatus) {
  PasswordForm form = CreateTestPassword();
  SetPasswordList({form});

  // The last status seen in the callback.
  PasswordExportInfo export_info({.status = ExportProgressStatus::kNotStarted});

  EXPECT_CALL(mock_write_file_, Run).WillOnce(Return(true));
  EXPECT_CALL(mock_on_progress_, Run).WillRepeatedly(SaveArg<0>(&export_info));

  ASSERT_EQ(exporter_.GetProgressStatus(), export_info.status);
  exporter_.PreparePasswordsForExport();
  exporter_.SetDestination(destination_path_);
  ASSERT_EQ(exporter_.GetProgressStatus(), export_info.status);

  EXPECT_CALL(mock_completion_callback_, Run);
  task_environment_.RunUntilIdle();
  ASSERT_EQ(exporter_.GetProgressStatus(), export_info.status);
}

TEST_F(PasswordManagerExporterTest, DontExportWithOnlyDestination) {
  SetPasswordList({CreateTestPassword()});

  EXPECT_CALL(mock_write_file_, Run).Times(0);
  EXPECT_CALL(mock_on_progress_, Run(CreateExportInProgressInfo()));

  exporter_.SetDestination(destination_path_);

  EXPECT_CALL(mock_completion_callback_, Run).Times(0);
  task_environment_.RunUntilIdle();
}

TEST_F(PasswordManagerExporterTest, CancelAfterPasswords) {
  SetPasswordList({CreateTestPassword()});

  EXPECT_CALL(mock_write_file_, Run).Times(0);
  EXPECT_CALL(mock_on_progress_, Run(CreateCancelledExportInfo()));

  exporter_.PreparePasswordsForExport();
  EXPECT_CALL(mock_completion_callback_, Run);
  exporter_.Cancel();

  task_environment_.RunUntilIdle();
}

TEST_F(PasswordManagerExporterTest, CancelWhileExporting) {
  SetPasswordList({CreateTestPassword()});

  EXPECT_CALL(mock_write_file_, Run).Times(0);
  EXPECT_CALL(mock_delete_file_, Run(destination_path_));
  EXPECT_CALL(mock_on_progress_, Run(CreateExportInProgressInfo()));
  EXPECT_CALL(mock_on_progress_, Run(CreateCancelledExportInfo()));

  exporter_.PreparePasswordsForExport();
  exporter_.SetDestination(destination_path_);
  EXPECT_CALL(mock_completion_callback_, Run);
  exporter_.Cancel();

  task_environment_.RunUntilIdle();
}

#if BUILDFLAG(IS_POSIX)
// Chrome creates files using the broadest permissions allowed. Passwords are
// sensitive and should be explicitly limited to the owner.
TEST_F(PasswordManagerExporterTest, OutputHasRestrictedPermissions) {
  SetPasswordList({CreateTestPassword()});

  EXPECT_CALL(mock_write_file_, Run).WillOnce(Return(true));
  EXPECT_CALL(mock_set_posix_file_permissions_, Run(destination_path_, 0600))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_on_progress_, Run).Times(AnyNumber());

  exporter_.PreparePasswordsForExport();
  EXPECT_CALL(mock_completion_callback_, Run);
  exporter_.SetDestination(destination_path_);

  task_environment_.RunUntilIdle();
}
#endif

TEST_F(PasswordManagerExporterTest, DeduplicatesAcrossPasswordStores) {
  PasswordForm password;
  password.in_store = PasswordForm::Store::kProfileStore;
  password.url = GURL("http://g.com/auth");
  password.username_value = u"user";
  password.password_value = u"password";

  PasswordForm password_duplicate = password;
  password_duplicate.in_store = PasswordForm::Store::kAccountStore;

  const std::string single_password_serialised(
      PasswordCSVWriter::SerializePasswords({CredentialUIEntry(password)}));
  SetPasswordList({password, password_duplicate});

  // The content written to the file should be the same as what would be
  // computed before the duplicated password was added.
  EXPECT_CALL(mock_write_file_,
              Run(destination_path_, StrEq(single_password_serialised)))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_on_progress_, Run(CreateExportInProgressInfo()));
  EXPECT_CALL(mock_on_progress_,
              Run(CreateSuccessfulExportInfo(destination_path_)));
  exporter_.PreparePasswordsForExport();
  EXPECT_CALL(mock_completion_callback_, Run);
  exporter_.SetDestination(destination_path_);

  task_environment_.RunUntilIdle();
}

}  // namespace

}  // namespace password_manager
