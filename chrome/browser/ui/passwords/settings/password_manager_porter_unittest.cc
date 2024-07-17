// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/settings/password_manager_porter.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "components/password_manager/core/browser/export/password_manager_exporter.h"
#include "components/password_manager/core/browser/import/csv_password_sequence.h"
#include "components/password_manager/core/browser/import/import_results.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/ui/credential_provider_interface.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::IsEmpty;
using ::testing::SizeIs;
using ::testing::StrictMock;
using ::testing::UnorderedPointwise;

namespace {

class TestSelectFileDialog : public ui::SelectFileDialog {
 public:
  TestSelectFileDialog(Listener* listener,
                       std::unique_ptr<ui::SelectFilePolicy> policy,
                       const base::FilePath& forced_path)
      : ui::SelectFileDialog(listener, std::move(policy)),
        forced_path_(forced_path) {}

  TestSelectFileDialog(const TestSelectFileDialog&) = delete;
  TestSelectFileDialog& operator=(const TestSelectFileDialog&) = delete;

 protected:
  ~TestSelectFileDialog() override = default;

  void SelectFileImpl(Type type,
                      const std::u16string& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      const GURL* caller) override {
    listener_->FileSelected(ui::SelectedFileInfo(forced_path_),
                            file_type_index);
  }
  bool IsRunning(gfx::NativeWindow owning_window) const override {
    return false;
  }
  void ListenerDestroyed() override { listener_ = nullptr; }
  bool HasMultipleFileTypeChoicesImpl() override { return false; }

 private:
  // The path that will be selected by this dialog.
  base::FilePath forced_path_;
};

class TestSelectFileDialogFactory : public ui::SelectFileDialogFactory {
 public:
  explicit TestSelectFileDialogFactory(const base::FilePath& forced_path)
      : forced_path_(forced_path) {}

  TestSelectFileDialogFactory& operator=(const TestSelectFileDialogFactory&) =
      delete;

  ui::SelectFileDialog* Create(
      ui::SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) override {
    return new TestSelectFileDialog(listener, nullptr, forced_path_);
  }

 private:
  // The path that will be selected by created dialogs.
  base::FilePath forced_path_;
};

// A fake ui::SelectFileDialog, which will cancel the file selection instead of
// selecting a file.
class FakeCancellingSelectFileDialog : public ui::SelectFileDialog {
 public:
  FakeCancellingSelectFileDialog(Listener* listener,
                                 std::unique_ptr<ui::SelectFilePolicy> policy)
      : ui::SelectFileDialog(listener, std::move(policy)) {}

  FakeCancellingSelectFileDialog(const FakeCancellingSelectFileDialog&) =
      delete;
  FakeCancellingSelectFileDialog& operator=(
      const FakeCancellingSelectFileDialog&) = delete;

 protected:
  void SelectFileImpl(Type type,
                      const std::u16string& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      const GURL* caller) override {
    listener_->FileSelectionCanceled();
  }

  bool IsRunning(gfx::NativeWindow owning_window) const override {
    return false;
  }
  void ListenerDestroyed() override { listener_ = nullptr; }
  bool HasMultipleFileTypeChoicesImpl() override { return false; }

 private:
  ~FakeCancellingSelectFileDialog() override = default;
};

class FakeCancellingSelectFileDialogFactory
    : public ui::SelectFileDialogFactory {
 public:
  FakeCancellingSelectFileDialogFactory() = default;

  TestSelectFileDialogFactory& operator=(const TestSelectFileDialogFactory&) =
      delete;

  ui::SelectFileDialog* Create(
      ui::SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) override {
    return new FakeCancellingSelectFileDialog(listener, nullptr);
  }
};

class MockPasswordManagerExporter
    : public password_manager::PasswordManagerExporter {
 public:
  MockPasswordManagerExporter()
      : password_manager::PasswordManagerExporter(
            nullptr,
            base::BindRepeating(
                [](const password_manager::PasswordExportInfo&) -> void {}),
            base::MockOnceClosure().Get()) {}

  MockPasswordManagerExporter(const MockPasswordManagerExporter&) = delete;
  MockPasswordManagerExporter& operator=(const MockPasswordManagerExporter&) =
      delete;

  ~MockPasswordManagerExporter() override = default;

  MOCK_METHOD(void, PreparePasswordsForExport, (), (override));
  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD(void, SetDestination, (const base::FilePath&), (override));
};

class FakePasswordParserService
    : public password_manager::mojom::CSVPasswordParser {
 public:
  void ParseCSV(const std::string& raw_json,
                ParseCSVCallback callback) override {
    password_manager::mojom::CSVPasswordSequencePtr result = nullptr;
    password_manager::CSVPasswordSequence seq(raw_json);
    if (seq.result() == password_manager::CSVPassword::Status::kOK) {
      result = password_manager::mojom::CSVPasswordSequence::New();
      for (const auto& pwd : seq) {
        result->csv_passwords.push_back(pwd);
      }
    }
    std::move(callback).Run(std::move(result));
  }
};

class PasswordManagerPorterTest : public ChromeRenderViewHostTestHarness {
 public:
  PasswordManagerPorterTest(const PasswordManagerPorterTest&) = delete;
  PasswordManagerPorterTest& operator=(const PasswordManagerPorterTest&) =
      delete;

 protected:
  PasswordManagerPorterTest() = default;

  ~PasswordManagerPorterTest() override = default;

  using MockImportFileDeletion = StrictMock<base::MockCallback<
      password_manager::PasswordImporter::DeleteFileCallback>>;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    ASSERT_TRUE(directory_.CreateUniqueTempDir());
    // SelectFileDialog::SetFactory is responsible for freeing the memory
    // associated with a new factory.
    ui::SelectFileDialog::SetFactory(
        std::make_unique<TestSelectFileDialogFactory>(temp_file_path()));

    profile_ = CreateTestingProfile();
    porter_ = std::make_unique<PasswordManagerPorter>(
        profile_.get(), &presenter(),
        /*on_export_progress_callback=*/base::DoNothing());

    auto importer =
        std::make_unique<password_manager::PasswordImporter>(&presenter_);
    mojo::PendingRemote<password_manager::mojom::CSVPasswordParser>
        pending_remote{receiver.BindNewPipeAndPassRemote()};
    importer->SetServiceForTesting(std::move(pending_remote));
    importer->SetDeleteFileForTesting(import_file_deletion_callback_.Get());
    porter_->SetImporterForTesting(std::move(importer));

    store_->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);
    presenter_.Init();
    task_environment()->RunUntilIdle();
  }

  void TearDown() override {
    porter_.reset();
    profile_.reset();
    store_->ShutdownOnUIThread();
    task_environment()->RunUntilIdle();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  PasswordManagerPorter& porter() { return *porter_; }
  password_manager::TestPasswordStore& store() { return *store_; }
  password_manager::SavedPasswordsPresenter& presenter() { return presenter_; }
  // The file that our fake file selector returns.
  const base::FilePath temp_file_path() {
    return directory_.GetPath().Append(FILE_PATH_LITERAL("test"));
  }
  MockImportFileDeletion& import_file_deletion_callback() {
    return import_file_deletion_callback_;
  }

  bool AddPasswordForm(const password_manager::PasswordForm& form) {
    bool result =
        presenter_.AddCredential(password_manager::CredentialUIEntry(form));

    task_environment()->RunUntilIdle();
    return result;
  }

 private:
  FakePasswordParserService service;
  mojo::Receiver<password_manager::mojom::CSVPasswordParser> receiver{&service};
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<PasswordManagerPorter> porter_;
  scoped_refptr<password_manager::TestPasswordStore> store_ =
      base::MakeRefCounted<password_manager::TestPasswordStore>();
  affiliations::FakeAffiliationService affiliation_service_;
  password_manager::SavedPasswordsPresenter presenter_{
      &affiliation_service_, store_,
      /*account_store=*/nullptr};
  base::ScopedTempDir directory_;
  MockImportFileDeletion import_file_deletion_callback_;
};

// Password importing and exporting using a |SelectFileDialog| is not yet
// supported on Android.
#if !BUILDFLAG(IS_ANDROID)

TEST_F(PasswordManagerPorterTest, PasswordExport) {
  std::unique_ptr<MockPasswordManagerExporter> mock_password_manager_exporter_ =
      std::make_unique<StrictMock<MockPasswordManagerExporter>>();

  EXPECT_CALL(*mock_password_manager_exporter_, PreparePasswordsForExport());
  EXPECT_CALL(*mock_password_manager_exporter_,
              SetDestination(temp_file_path()));

  porter().SetExporterForTesting(std::move(mock_password_manager_exporter_));
  porter().Export(web_contents()->GetWeakPtr());
}

TEST_F(PasswordManagerPorterTest, CancelExportFileSelection) {
  ui::SelectFileDialog::SetFactory(
      std::make_unique<FakeCancellingSelectFileDialogFactory>());

  std::unique_ptr<MockPasswordManagerExporter> mock_password_manager_exporter_ =
      std::make_unique<StrictMock<MockPasswordManagerExporter>>();

  EXPECT_CALL(*mock_password_manager_exporter_, PreparePasswordsForExport());
  EXPECT_CALL(*mock_password_manager_exporter_, Cancel());

  porter().SetExporterForTesting(std::move(mock_password_manager_exporter_));
  porter().Export(web_contents()->GetWeakPtr());
}

TEST_F(PasswordManagerPorterTest, CancelExport) {
  std::unique_ptr<MockPasswordManagerExporter> mock_password_manager_exporter_ =
      std::make_unique<StrictMock<MockPasswordManagerExporter>>();

  EXPECT_CALL(*mock_password_manager_exporter_, PreparePasswordsForExport());
  EXPECT_CALL(*mock_password_manager_exporter_, SetDestination(_));
  EXPECT_CALL(*mock_password_manager_exporter_, Cancel());

  porter().SetExporterForTesting(std::move(mock_password_manager_exporter_));
  porter().Export(web_contents()->GetWeakPtr());
  porter().CancelExport();
}

TEST_F(PasswordManagerPorterTest, ImportDismissedOnCanceledFileSelection) {
  ui::SelectFileDialog::SetFactory(
      std::make_unique<FakeCancellingSelectFileDialogFactory>());

  base::MockCallback<PasswordManagerPorter::ImportResultsCallback> callback;
  EXPECT_CALL(
      callback,
      Run(::testing::Field(&password_manager::ImportResults::status,
                           password_manager::ImportResults::Status::DISMISSED)))
      .Times(1);
  porter().Import(web_contents(),
                  password_manager::PasswordForm::Store::kProfileStore,
                  callback.Get());
  task_environment()->RunUntilIdle();

  EXPECT_THAT(store().stored_passwords(), IsEmpty());
}

TEST_F(PasswordManagerPorterTest, ContinueImportWithConflicts) {
  password_manager::PasswordForm test_form;
  test_form.url = GURL("https://test.com");
  test_form.signon_realm = test_form.url.spec();
  test_form.username_value = u"username";
  test_form.password_value = u"old_password";
  test_form.in_store = password_manager::PasswordForm::Store::kProfileStore;

  AddPasswordForm(test_form);
  ASSERT_EQ(1u, store().stored_passwords().size());

  ASSERT_TRUE(base::WriteFile(temp_file_path(),
                              "origin,username,password\n"
                              "https://test.com,username,new_password\n"));

  const size_t expected_displayed_entires_size = 1;
  base::MockCallback<PasswordManagerPorter::ImportResultsCallback> callback;
  EXPECT_CALL(
      callback,
      Run(AllOf(
          ::testing::Field(&password_manager::ImportResults::status,
                           password_manager::ImportResults::Status::CONFLICTS),
          ::testing::Field(&password_manager::ImportResults::displayed_entries,
                           SizeIs(expected_displayed_entires_size)))))
      .Times(1);
  porter().Import(web_contents(),
                  password_manager::PasswordForm::Store::kProfileStore,
                  callback.Get());
  task_environment()->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&callback);

  EXPECT_CALL(callback, Run(::testing::Field(
                            &password_manager::ImportResults::status,
                            password_manager::ImportResults::Status::SUCCESS)))
      .Times(1);
  // Should overwrite conflicting passwords
  porter().ContinueImport(/*selected_ids=*/{0}, callback.Get());
  task_environment()->RunUntilIdle();

  ASSERT_EQ(1u, store().stored_passwords().size());
  password_manager::PasswordForm stored_form =
      store().stored_passwords().begin()->second[0];
  EXPECT_EQ(u"new_password", stored_form.password_value);
}

TEST_F(PasswordManagerPorterTest, RejectNewImportsWhenConflictsNotResolved) {
  password_manager::PasswordForm test_form;
  test_form.url = GURL("https://test.com");
  test_form.signon_realm = test_form.url.spec();
  test_form.username_value = u"username";
  test_form.password_value = u"old_password";
  test_form.in_store = password_manager::PasswordForm::Store::kProfileStore;

  AddPasswordForm(test_form);
  ASSERT_EQ(1u, store().stored_passwords().size());

  ASSERT_TRUE(base::WriteFile(temp_file_path(),
                              "origin,username,password\n"
                              "https://test.com,username,new_password\n"));

  const size_t expected_displayed_entires_size = 1;
  base::MockCallback<PasswordManagerPorter::ImportResultsCallback> callback;
  EXPECT_CALL(
      callback,
      Run(AllOf(
          ::testing::Field(&password_manager::ImportResults::status,
                           password_manager::ImportResults::Status::CONFLICTS),
          ::testing::Field(&password_manager::ImportResults::displayed_entries,
                           SizeIs(expected_displayed_entires_size)))))
      .Times(1);
  porter().Import(web_contents(),
                  password_manager::PasswordForm::Store::kProfileStore,
                  callback.Get());
  task_environment()->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&callback);

  EXPECT_CALL(
      callback,
      Run(::testing::Field(
          &password_manager::ImportResults::status,
          password_manager::ImportResults::Status::IMPORT_ALREADY_ACTIVE)))
      .Times(1);
  porter().Import(web_contents(),
                  password_manager::PasswordForm::Store::kProfileStore,
                  callback.Get());
  task_environment()->RunUntilIdle();
}

TEST_F(PasswordManagerPorterTest, ResetImporterTriggersFileDeletion) {
  ASSERT_TRUE(base::WriteFile(temp_file_path(),
                              "origin,username,password\n"
                              "https://test.com,username,secret\n"));

  base::MockCallback<PasswordManagerPorter::ImportResultsCallback> callback;
  EXPECT_CALL(callback, Run(::testing::Field(
                            &password_manager::ImportResults::status,
                            password_manager::ImportResults::Status::SUCCESS)))
      .Times(1);
  porter().Import(web_contents(),
                  password_manager::PasswordForm::Store::kProfileStore,
                  callback.Get());
  task_environment()->RunUntilIdle();

  EXPECT_CALL(import_file_deletion_callback(), Run(temp_file_path())).Times(1);
  porter().ResetImporter(/*delete_file=*/true);
  task_environment()->RunUntilIdle();

  ASSERT_EQ(1u, store().stored_passwords().size());
}

struct FormDescription {
  std::string origin;
  std::string username;
  std::string password;
};

struct TestCase {
  std::string_view csv;
  std::vector<FormDescription> descriptions;
};

class PasswordManagerPorterImportTest
    : public testing::WithParamInterface<TestCase>,
      public PasswordManagerPorterTest {};

MATCHER(FormHasDescription, "") {
  const auto& form = std::get<0>(arg);
  const auto& desc = std::get<1>(arg);
  return form.url == GURL(desc.origin) &&
         form.username_value == base::ASCIIToUTF16(desc.username) &&
         form.password_value == base::ASCIIToUTF16(desc.password);
}

TEST_P(PasswordManagerPorterImportTest, Import) {
  const TestCase& tc = GetParam();

  ASSERT_TRUE(base::WriteFile(temp_file_path(), tc.csv));

  base::MockCallback<PasswordManagerPorter::ImportResultsCallback> callback;
  EXPECT_CALL(callback, Run).Times(1);
  porter().Import(web_contents(),
                  password_manager::PasswordForm::Store::kProfileStore,
                  callback.Get());
  task_environment()->RunUntilIdle();
  if (tc.descriptions.empty()) {
    EXPECT_THAT(store().stored_passwords(), IsEmpty());
    return;
  }
  // Note: The code below assumes that all the credentials in tc.csv have the
  // same signon realm, and that it is https://example.com/.
  ASSERT_EQ(1u, store().stored_passwords().size());
  const std::pair<std::string /* signon_realm */,
                  std::vector<password_manager::PasswordForm>>& credentials =
      *store().stored_passwords().begin();
  EXPECT_EQ("https://example.com/", credentials.first);
  EXPECT_THAT(credentials.second,
              UnorderedPointwise(FormHasDescription(), tc.descriptions));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PasswordManagerPorterImportTest,
    ::testing::Values(
        // Empty TestCase describes a valid answer to an empty CSV input.
        TestCase(),
        TestCase{"invalid header\n"
                 "https://example.com,u,p",
                 {}},
        TestCase{"origin,username,password\n"
                 "https://example.com/somepath,x,y\n"
                 "invalid to be ignored\n"
                 "https://example.com,u,p",
                 {{"https://example.com/somepath", "x", "y"},
                  {"https://example.com/", "u", "p"}}}));

#endif

}  // namespace
