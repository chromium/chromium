// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/export/login_db_deprecation_password_exporter.h"

#include <iterator>
#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/fake_password_store_backend.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_backend.h"
#include "components/password_manager/core/browser/password_store/password_store.h"
#include "components/password_manager/core/browser/password_store/password_store_backend.h"
#include "components/password_manager/core/browser/password_store/password_store_backend_error.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/password_store/password_store_results_observer.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::WithArg;
using ::testing::WithArgs;

const std::string kLineEnding = "\n";
const std::string kExportFilename = "ChromePasswords.csv";
const std::string kExportResultHistogram =
    "PasswordManager.UPM.LoginDbDeprecationExport.Result";
const std::string kExportLatencyHistogram =
    "PasswordManager.UPM.LoginDbDeprecationExport.Latency";

std::pair<PasswordForm, std::string> GetTestFormWithExpectedExportData() {
  const std::u16string kNoteValue =
      base::UTF8ToUTF16("Note Line 1" + kLineEnding + "Note Line 2");
  PasswordForm form;
  form.signon_realm = "https://example.com/";
  form.url = GURL("https://example.com");
  form.username_value = u"Someone";
  form.password_value = u"Secret";
  form.notes = {PasswordNote(kNoteValue, base::Time::Now())};

  std::string expected_export_data =
      "name,url,username,password,note" + kLineEnding +
      "example.com,https://example.com/,Someone,Secret,\"Note Line "
      "1" +
      kLineEnding + "Note Line 2\"" + kLineEnding;
  return {form, expected_export_data};
}

}  // namespace

class LoginDbDeprecationPasswordExporterTest : public testing::Test {
 public:
  LoginDbDeprecationPasswordExporterTest() = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kUpmUnmigratedPasswordsExported, false);

    password_store_ = base::MakeRefCounted<TestPasswordStore>();
    password_store_->Init(&pref_service_, /*affiliated_match_helper=*/nullptr);
    exporter_ = std::make_unique<LoginDbDeprecationPasswordExporter>(
        &pref_service_, temp_dir_.GetPath());
  }

  void TearDown() override {
    password_store_->ShutdownOnUIThread();
    ASSERT_TRUE(temp_dir_.Delete());
  }

  const base::FilePath& export_dir() { return temp_dir_.GetPath(); }

  LoginDbDeprecationPasswordExporter* exporter() { return exporter_.get(); }

  TestingPrefServiceSimple* pref_service() { return &pref_service_; }

  PasswordStoreInterface* password_store() { return password_store_.get(); }

 protected:
  base::test::TaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
  base::PassKey<class LoginDbDeprecationPasswordExporterTest> passkey =
      base::PassKey<class LoginDbDeprecationPasswordExporterTest>();

 private:
  std::unique_ptr<LoginDbDeprecationPasswordExporter> exporter_;
  TestingPrefServiceSimple pref_service_;
  scoped_refptr<TestPasswordStore> password_store_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(LoginDbDeprecationPasswordExporterTest, DoesntCreateFileIfNoPasswords) {
  base::HistogramTester histogram_tester;
  // No passwords in the backend.
  exporter()->Start(password_store(), task_env_.QuitClosure());
  task_env_.RunUntilQuit();

  base::FilePath expected_file_path =
      export_dir().Append(FILE_PATH_LITERAL("ChromePasswords.csv"));
  EXPECT_FALSE(base::PathExists(expected_file_path));
  histogram_tester.ExpectUniqueSample(
      kExportResultHistogram, LoginDbDeprecationExportResult::kNoPasswords, 1);
  histogram_tester.ExpectTotalCount(kExportLatencyHistogram, 0);
  EXPECT_TRUE(
      pref_service()->GetBoolean(prefs::kUpmUnmigratedPasswordsExported));
}

TEST_F(LoginDbDeprecationPasswordExporterTest, ExportFailedToFetchPasswords) {
  base::HistogramTester histogram_tester;
  // No passwords in the backend.
  std::unique_ptr<MockPasswordStoreBackend> mock_backend =
      std::make_unique<MockPasswordStoreBackend>();
  MockPasswordStoreBackend* weak_mock_backend = mock_backend.get();
  scoped_refptr<PasswordStore> password_store =
      base::MakeRefCounted<PasswordStore>(std::move(mock_backend));

  EXPECT_CALL(*weak_mock_backend, InitBackend)
      .WillOnce(WithArgs<3>([](base::OnceCallback<void(bool)> completion) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(completion), /*success=*/true));
      }));

  password_store->Init(pref_service(), /*affiliation_match_service=*/nullptr);

  EXPECT_CALL(*weak_mock_backend, GetAutofillableLoginsAsync)
      .WillOnce(WithArg<0>([password_store](LoginsOrErrorReply callback) {
        PasswordStoreBackendError error{
            PasswordStoreBackendErrorType::kUncategorized};
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), std::move(error)));
      }));

  exporter()->Start(password_store, task_env_.QuitClosure());
  task_env_.RunUntilQuit();

  base::FilePath expected_file_path =
      export_dir().Append(FILE_PATH_LITERAL("ChromePasswords.csv"));
  EXPECT_FALSE(base::PathExists(expected_file_path));
  histogram_tester.ExpectUniqueSample(
      kExportResultHistogram,
      LoginDbDeprecationExportResult::kErrorFetchingPasswords, 1);
  histogram_tester.ExpectTotalCount(kExportLatencyHistogram, 0);
  password_store->ShutdownOnUIThread();
}

TEST_F(LoginDbDeprecationPasswordExporterTest, ExportsBackendPasswords) {
  base::HistogramTester histogram_tester;
  std::pair<PasswordForm, std::string> form_expected_data_pair =
      GetTestFormWithExpectedExportData();
  PasswordForm form = std::move(form_expected_data_pair.first);
  std::string expected_exported_data =
      std::move(form_expected_data_pair.second);

  password_store()->AddLogin(form, task_env_.QuitClosure());
  task_env_.RunUntilQuit();

  const base::TimeDelta kExportLatency = base::Minutes(1);
  exporter()->Start(password_store(), task_env_.QuitClosure());
  task_env_.AdvanceClock(kExportLatency);
  task_env_.RunUntilQuit();

  base::FilePath expected_file_path =
      export_dir().Append(FILE_PATH_LITERAL(kExportFilename));

  EXPECT_TRUE(base::PathExists(expected_file_path));
  EXPECT_TRUE(
      pref_service()->GetBoolean(prefs::kUpmUnmigratedPasswordsExported));

  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(expected_file_path, &contents));
  EXPECT_TRUE(!contents.empty());
  EXPECT_EQ(expected_exported_data, contents);
  histogram_tester.ExpectUniqueSample(
      kExportResultHistogram, LoginDbDeprecationExportResult::kSuccess, 1);
  histogram_tester.ExpectUniqueTimeSample(kExportLatencyHistogram,
                                          kExportLatency, 1);
}

TEST_F(LoginDbDeprecationPasswordExporterTest, ExportFailure) {
  base::HistogramTester histogram_tester;
  PasswordForm form = GetTestFormWithExpectedExportData().first;
  password_store()->AddLogin(form, task_env_.QuitClosure());
  task_env_.RunUntilQuit();

  PasswordManagerExporter* internal_exporter =
      exporter()->GetInternalExporterForTesting(passkey);
  StrictMock<base::MockCallback<PasswordManagerExporter::WriteCallback>>
      mock_write_callback;

  // Fake a failed write.
  internal_exporter->SetWriteForTesting(mock_write_callback.Get());
  EXPECT_CALL(mock_write_callback, Run).WillOnce(Return(false));

  exporter()->Start(password_store(), task_env_.QuitClosure());
  task_env_.RunUntilQuit();

  EXPECT_FALSE(
      pref_service()->GetBoolean(prefs::kUpmUnmigratedPasswordsExported));
  histogram_tester.ExpectUniqueSample(
      kExportResultHistogram, LoginDbDeprecationExportResult::kFileWriteError,
      1);
  histogram_tester.ExpectTotalCount(kExportLatencyHistogram, 0);
}

TEST_F(LoginDbDeprecationPasswordExporterTest, RemovesLoginsOnExportSuccess) {
  std::pair<PasswordForm, std::string> form_expected_data_pair =
      GetTestFormWithExpectedExportData();
  PasswordForm form = std::move(form_expected_data_pair.first);
  std::string expected_exported_data =
      std::move(form_expected_data_pair.second);

  password_store()->AddLogin(form, task_env_.QuitClosure());
  task_env_.RunUntilQuit();

  exporter()->Start(password_store(), task_env_.QuitClosure());
  task_env_.RunUntilQuit();

  ASSERT_TRUE(
      pref_service()->GetBoolean(prefs::kUpmUnmigratedPasswordsExported));

  password_manager::PasswordStoreResultsObserver results_observer;
  password_store()->GetAllLogins(results_observer.GetWeakPtr());
  std::vector<std::unique_ptr<PasswordForm>> forms =
      results_observer.WaitForResults();
  EXPECT_TRUE(forms.empty());
}

TEST_F(LoginDbDeprecationPasswordExporterTest,
       DoesntRemoveLoginsOnExportFailure) {
  PasswordForm form = GetTestFormWithExpectedExportData().first;
  password_store()->AddLogin(form, task_env_.QuitClosure());
  task_env_.RunUntilQuit();

  PasswordManagerExporter* internal_exporter =
      exporter()->GetInternalExporterForTesting(passkey);
  StrictMock<base::MockCallback<PasswordManagerExporter::WriteCallback>>
      mock_write_callback;

  // Fake a failed write.
  internal_exporter->SetWriteForTesting(mock_write_callback.Get());
  EXPECT_CALL(mock_write_callback, Run).WillOnce(Return(false));

  exporter()->Start(password_store(), task_env_.QuitClosure());
  task_env_.RunUntilQuit();

  ASSERT_FALSE(
      pref_service()->GetBoolean(prefs::kUpmUnmigratedPasswordsExported));

  password_manager::PasswordStoreResultsObserver results_observer;
  password_store()->GetAllLogins(results_observer.GetWeakPtr());
  std::vector<std::unique_ptr<PasswordForm>> forms =
      results_observer.WaitForResults();
  EXPECT_FALSE(forms.empty());
}

}  // namespace password_manager
