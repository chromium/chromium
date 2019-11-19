// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/export/password_manager_exporter.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/export/password_csv_writer.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/ui/credential_provider_interface.h"
#include "components/password_manager/core/browser/ui/export_progress_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::Return;
using ::testing::ReturnArg;
using ::testing::SaveArg;
using ::testing::StrEq;
using ::testing::StrictMock;

// A callback that matches the signature of base::WriteFile
using WriteCallback =
    base::RepeatingCallback<int(const base::FilePath&, const char*, int)>;
using DeleteCallback =
    password_manager::PasswordManagerExporter::DeleteCallback;
using SetPosixFilePermissionsCallback =
    password_manager::PasswordManagerExporter::SetPosixFilePermissionsCallback;

#if defined(OS_WIN)
const base::FilePath::CharType kNullFileName[] = FILE_PATH_LITERAL("/nul");
#else
const base::FilePath::CharType kNullFileName[] = FILE_PATH_LITERAL("/dev/null");
#endif

// Provides a predetermined set of credentials
class FakeCredentialProvider
    : public password_manager::CredentialProviderInterface {
 public:
  FakeCredentialProvider() {}

  void SetPasswordList(
      const std::vector<std::unique_ptr<autofill::PasswordForm>>&
          password_list) {
    password_list_.clear();
    for (const auto& form : password_list) {
      password_list_.push_back(std::make_unique<autofill::PasswordForm>(*form));
    }
  }

  // password_manager::CredentialProviderInterface:
  std::vector<std::unique_ptr<autofill::PasswordForm>> GetAllPasswords()
      override {
    std::vector<std::unique_ptr<autofill::PasswordForm>> ret_val;
    for (const auto& form : password_list_) {
      ret_val.push_back(std::make_unique<autofill::PasswordForm>(*form));
    }
    return ret_val;
  }

 private:
  std::vector<std::unique_ptr<autofill::PasswordForm>> password_list_;

  DISALLOW_COPY_AND_ASSIGN(FakeCredentialProvider);
};

// Creates a hardcoded set of credentials for tests.
std::vector<std::unique_ptr<autofill::PasswordForm>> CreatePasswordList() {
  auto password_form = std::make_unique<autofill::PasswordForm>();
  password_form->origin = GURL("http://accounts.google.com/a/LoginAuth");
  password_form->username_value = base::ASCIIToUTF16("test@gmail.com");
  password_form->password_value = base::ASCIIToUTF16("test1");

  std::vector<std::unique_ptr<autofill::PasswordForm>> password_forms;
  password_forms.push_back(std::move(password_form));
  return password_forms;
}

class PasswordManagerExporterTest : public testing::Test {
 public:
  PasswordManagerExporterTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI),
        exporter_(&fake_credential_provider_, mock_on_progress_.Get()),
        destination_path_(kNullFileName) {
    exporter_.SetWriteForTesting(mock_write_file_.Get());
    exporter_.SetDeleteForTesting(mock_delete_file_.Get());
    exporter_.SetSetPosixFilePermissionsForTesting(
        mock_set_posix_file_permissions_.Get());
    password_list_ = CreatePasswordList();
    fake_credential_provider_.SetPasswordList(password_list_);
  }

  ~PasswordManagerExporterTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  std::vector<std::unique_ptr<autofill::PasswordForm>> password_list_;
  FakeCredentialProvider fake_credential_provider_;
  base::MockCallback<base::RepeatingCallback<
      void(password_manager::ExportProgressStatus, const std::string&)>>
      mock_on_progress_;
  password_manager::PasswordManagerExporter exporter_;
  StrictMock<base::MockCallback<WriteCallback>> mock_write_file_;
  StrictMock<base::MockCallback<DeleteCallback>> mock_delete_file_;
  NiceMock<base::MockCallback<SetPosixFilePermissionsCallback>>
      mock_set_posix_file_permissions_;
  base::FilePath destination_path_;
  base::HistogramTester histogram_tester_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordManagerExporterTest);
};

TEST_F(PasswordManagerExporterTest, PasswordExportSetPasswordListFirst) {
  const std::string serialised(
      password_manager::PasswordCSVWriter::SerializePasswords(password_list_));

  EXPECT_CALL(mock_write_file_,
              Run(destination_path_, StrEq(serialised), serialised.size()))
      .WillOnce(ReturnArg<2>());
  EXPECT_CALL(
      mock_on_progress_,
      Run(password_manager::ExportProgressStatus::IN_PROGRESS, IsEmpty()));
  EXPECT_CALL(
      mock_on_progress_,
      Run(password_manager::ExportProgressStatus::SUCCEEDED, IsEmpty()));

  exporter_.PreparePasswordsForExport();
  exporter_.SetDestination(destination_path_);

  task_environment_.RunUntilIdle();
}

// When writing fails, we should notify the UI of the failure and try to cleanup
// a possibly partial passwords file.
TEST_F(PasswordManagerExporterTest, WriteFileFailed) {
  const std::string destination_folder_name(
      destination_path_.DirName().BaseName().AsUTF8Unsafe());

  EXPECT_CALL(mock_write_file_, Run(_, _, _)).WillOnce(Return(-1));
  EXPECT_CALL(mock_delete_file_, Run(destination_path_, false));
  EXPECT_CALL(
      mock_on_progress_,
      Run(password_manager::ExportProgressStatus::IN_PROGRESS, IsEmpty()));
  EXPECT_CALL(mock_on_progress_,
              Run(password_manager::ExportProgressStatus::FAILED_WRITE_FAILED,
                  StrEq(destination_folder_name)));

  exporter_.PreparePasswordsForExport();
  exporter_.SetDestination(destination_path_);

  task_environment_.RunUntilIdle();
}

// A partial write should be considered a failure and be cleaned up.
TEST_F(PasswordManagerExporterTest, WriteFileFailedHalfway) {
  const std::string serialised(
      password_manager::PasswordCSVWriter::SerializePasswords(password_list_));
  const std::string destination_folder_name(
      destination_path_.DirName().BaseName().AsUTF8Unsafe());

  EXPECT_CALL(mock_write_file_, Run(_, _, _))
      .WillOnce(Return(serialised.size() / 2));
  EXPECT_CALL(mock_delete_file_, Run(destination_path_, false));
  EXPECT_CALL(
      mock_on_progress_,
      Run(password_manager::ExportProgressStatus::IN_PROGRESS, IsEmpty()));
  EXPECT_CALL(mock_on_progress_,
              Run(password_manager::ExportProgressStatus::FAILED_WRITE_FAILED,
                  StrEq(destination_folder_name)));

  exporter_.PreparePasswordsForExport();
  exporter_.SetDestination(destination_path_);

  task_environment_.RunUntilIdle();
}

// Test that GetProgressStatus() returns the last ExportProgressStatus sent
// to the callback.
TEST_F(PasswordManagerExporterTest, GetProgressReturnsLastCallbackStatus) {
  const std::string serialised(
      password_manager::PasswordCSVWriter::SerializePasswords(password_list_));
  const std::string destination_folder_name(
      destination_path_.DirName().BaseName().AsUTF8Unsafe());

  // The last status seen in the callback.
  password_manager::ExportProgressStatus status =
      password_manager::ExportProgressStatus::NOT_STARTED;

  EXPECT_CALL(mock_write_file_, Run(_, _, _)).WillOnce(ReturnArg<2>());
  EXPECT_CALL(mock_on_progress_, Run(_, _)).WillRepeatedly(SaveArg<0>(&status));

  ASSERT_EQ(exporter_.GetProgressStatus(), status);
  exporter_.PreparePasswordsForExport();
  exporter_.SetDestination(destination_path_);
  ASSERT_EQ(exporter_.GetProgressStatus(), status);

  task_environment_.RunUntilIdle();
  ASSERT_EQ(exporter_.GetProgressStatus(), status);
}

TEST_F(PasswordManagerExporterTest, DontExportWithOnlyDestination) {
  EXPECT_CALL(mock_write_file_, Run(_, _, _)).Times(0);
  EXPECT_CALL(
      mock_on_progress_,
      Run(password_manager::ExportProgressStatus::IN_PROGRESS, IsEmpty()));

  exporter_.SetDestination(destination_path_);

  task_environment_.RunUntilIdle();
}

TEST_F(PasswordManagerExporterTest, CancelAfterPasswords) {
  EXPECT_CALL(mock_write_file_, Run(_, _, _)).Times(0);
  EXPECT_CALL(
      mock_on_progress_,
      Run(password_manager::ExportProgressStatus::FAILED_CANCELLED, IsEmpty()));

  exporter_.PreparePasswordsForExport();
  exporter_.Cancel();

  task_environment_.RunUntilIdle();
}

TEST_F(PasswordManagerExporterTest, CancelWhileExporting) {
  EXPECT_CALL(mock_write_file_, Run(_, _, _)).Times(0);
  EXPECT_CALL(mock_delete_file_, Run(destination_path_, false));
  EXPECT_CALL(
      mock_on_progress_,
      Run(password_manager::ExportProgressStatus::IN_PROGRESS, IsEmpty()));
  EXPECT_CALL(
      mock_on_progress_,
      Run(password_manager::ExportProgressStatus::FAILED_CANCELLED, IsEmpty()));

  exporter_.PreparePasswordsForExport();
  exporter_.SetDestination(destination_path_);
  exporter_.Cancel();

  task_environment_.RunUntilIdle();
}

// The "Cancel" button may still be visible on the UI after we've completed
// exporting. If they choose to cancel, we should clear the file.
TEST_F(PasswordManagerExporterTest, CancelAfterExporting) {
  EXPECT_CALL(mock_write_file_, Run(_, _, _)).WillOnce(ReturnArg<2>());
  EXPECT_CALL(mock_delete_file_, Run(destination_path_, false));
  EXPECT_CALL(
      mock_on_progress_,
      Run(password_manager::ExportProgressStatus::IN_PROGRESS, IsEmpty()));
  EXPECT_CALL(
      mock_on_progress_,
      Run(password_manager::ExportProgressStatus::SUCCEEDED, IsEmpty()));
  EXPECT_CALL(
      mock_on_progress_,
      Run(password_manager::ExportProgressStatus::FAILED_CANCELLED, IsEmpty()));

  exporter_.PreparePasswordsForExport();
  exporter_.SetDestination(destination_path_);

  task_environment_.RunUntilIdle();
  exporter_.Cancel();

  task_environment_.RunUntilIdle();
}

#if defined(OS_POSIX)
// Chrome creates files using the broadest permissions allowed. Passwords are
// sensitive and should be explicitly limited to the owner.
TEST_F(PasswordManagerExporterTest, OutputHasRestrictedPermissions) {
  EXPECT_CALL(mock_write_file_, Run(_, _, _)).WillOnce(ReturnArg<2>());
  EXPECT_CALL(mock_set_posix_file_permissions_, Run(destination_path_, 0600))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_on_progress_, Run(_, _)).Times(AnyNumber());

  exporter_.PreparePasswordsForExport();
  exporter_.SetDestination(destination_path_);

  task_environment_.RunUntilIdle();
}
#endif

}  // namespace
