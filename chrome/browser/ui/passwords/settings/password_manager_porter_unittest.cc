// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/settings/password_manager_porter.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/affiliation/mock_affiliation_service.h"
#include "components/password_manager/core/browser/export/password_manager_exporter.h"
#include "components/password_manager/core/browser/import/csv_password_sequence.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/browser/ui/credential_provider_interface.h"
#include "components/password_manager/core/browser/ui/import_results.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::IsEmpty;
using ::testing::StrictMock;
using ::testing::UnorderedPointwise;

namespace {

#if BUILDFLAG(IS_WIN)
const base::FilePath::CharType kNullFileName[] = FILE_PATH_LITERAL("/nul");
#else
const base::FilePath::CharType kNullFileName[] = FILE_PATH_LITERAL("/dev/null");
#endif

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
                      void* params,
                      const GURL* caller) override {
    listener_->FileSelected(forced_path_, file_type_index, params);
  }
  bool IsRunning(gfx::NativeWindow owning_window) const override {
    return false;
  }
  void ListenerDestroyed() override {}
  bool HasMultipleFileTypeChoicesImpl() override { return false; }

 private:
  // The path that will be selected by this dialog.
  base::FilePath forced_path_;
};

class TestSelectFilePolicy : public ui::SelectFilePolicy {
 public:
  TestSelectFilePolicy& operator=(const TestSelectFilePolicy&) = delete;

  bool CanOpenSelectFileDialog() override { return true; }
  void SelectFileDenied() override {}
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
    return new TestSelectFileDialog(
        listener, std::make_unique<TestSelectFilePolicy>(), forced_path_);
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
                      void* params,
                      const GURL* caller) override {
    listener_->FileSelectionCanceled(params);
  }

  bool IsRunning(gfx::NativeWindow owning_window) const override {
    return false;
  }
  void ListenerDestroyed() override {}
  bool HasMultipleFileTypeChoicesImpl() override { return false; }

 private:
  ~FakeCancellingSelectFileDialog() override = default;
};

class FakeCancellingSelectFileDialogFactory
    : public ui::SelectFileDialogFactory {
 public:
  FakeCancellingSelectFileDialogFactory() {}

  TestSelectFileDialogFactory& operator=(const TestSelectFileDialogFactory&) =
      delete;

  ui::SelectFileDialog* Create(
      ui::SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) override {
    return new FakeCancellingSelectFileDialog(
        listener, std::make_unique<TestSelectFilePolicy>());
  }
};

class MockPasswordManagerExporter
    : public password_manager::PasswordManagerExporter {
 public:
  MockPasswordManagerExporter()
      : password_manager::PasswordManagerExporter(
            nullptr,
            base::BindRepeating([](password_manager::ExportProgressStatus,
                                   const std::string&) -> void {}),
            base::MockOnceClosure().Get()) {}

  MockPasswordManagerExporter(const MockPasswordManagerExporter&) = delete;
  MockPasswordManagerExporter& operator=(const MockPasswordManagerExporter&) =
      delete;

  ~MockPasswordManagerExporter() override = default;

  MOCK_METHOD0(PreparePasswordsForExport, void());
  MOCK_METHOD0(Cancel, void());
  MOCK_METHOD1(SetDestination, void(const base::FilePath&));
  MOCK_METHOD0(GetExportProgressStatus,
               password_manager::ExportProgressStatus());
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
      for (const auto& pwd : seq)
        result->csv_passwords.push_back(pwd);
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

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    // SelectFileDialog::SetFactory is responsible for freeing the memory
    // associated with a new factory.
    selected_file_ = base::FilePath(kNullFileName);
    ui::SelectFileDialog::SetFactory(
        new TestSelectFileDialogFactory(selected_file_));
  }

  // The file that our fake file selector returns.
  // This file should not actually be used by the test.
  base::FilePath selected_file_;
};

// Password importing and exporting using a |SelectFileDialog| is not yet
// supported on Android.
#if !BUILDFLAG(IS_ANDROID)

TEST_F(PasswordManagerPorterTest, PasswordExport) {
  PasswordManagerPorter porter(
      /*profile=*/nullptr, /*presenter=*/nullptr,
      /*on_export_progress_callback=*/base::DoNothing());
  std::unique_ptr<MockPasswordManagerExporter> mock_password_manager_exporter_ =
      std::make_unique<StrictMock<MockPasswordManagerExporter>>();

  EXPECT_CALL(*mock_password_manager_exporter_, PreparePasswordsForExport());
  EXPECT_CALL(*mock_password_manager_exporter_, SetDestination(selected_file_));

  porter.SetExporterForTesting(std::move(mock_password_manager_exporter_));
  porter.Export(web_contents());
}

TEST_F(PasswordManagerPorterTest, CancelExportFileSelection) {
  ui::SelectFileDialog::SetFactory(new FakeCancellingSelectFileDialogFactory());

  std::unique_ptr<MockPasswordManagerExporter> mock_password_manager_exporter_ =
      std::make_unique<StrictMock<MockPasswordManagerExporter>>();
  PasswordManagerPorter porter(
      /*profile=*/nullptr, /*presenter=*/nullptr,
      /*on_export_progress_callback=*/base::DoNothing());

  EXPECT_CALL(*mock_password_manager_exporter_, PreparePasswordsForExport());
  EXPECT_CALL(*mock_password_manager_exporter_, Cancel());

  porter.SetExporterForTesting(std::move(mock_password_manager_exporter_));
  porter.Export(web_contents());
}

TEST_F(PasswordManagerPorterTest, CancelExport) {
  std::unique_ptr<MockPasswordManagerExporter> mock_password_manager_exporter_ =
      std::make_unique<StrictMock<MockPasswordManagerExporter>>();
  PasswordManagerPorter porter(
      /*profile=*/nullptr, /*presenter=*/nullptr,
      /*on_export_progress_callback=*/base::DoNothing());

  EXPECT_CALL(*mock_password_manager_exporter_, PreparePasswordsForExport());
  EXPECT_CALL(*mock_password_manager_exporter_, SetDestination(_));
  EXPECT_CALL(*mock_password_manager_exporter_, Cancel());

  porter.SetExporterForTesting(std::move(mock_password_manager_exporter_));
  porter.Export(web_contents());
  porter.CancelExport();
}

TEST_F(PasswordManagerPorterTest, ImportDismissedOnCanceledFileSelection) {
  std::unique_ptr<TestingProfile> profile = CreateTestingProfile();
  PasswordStoreFactory::GetInstance()->SetTestingFactoryAndUse(
      profile.get(),
      base::BindRepeating(
          &password_manager::BuildPasswordStore<
              content::BrowserContext, password_manager::TestPasswordStore>));
  scoped_refptr<password_manager::PasswordStoreInterface> store(
      PasswordStoreFactory::GetForProfile(profile.get(),
                                          ServiceAccessType::EXPLICIT_ACCESS));
  auto* test_password_store =
      static_cast<password_manager::TestPasswordStore*>(store.get());
  EXPECT_THAT(test_password_store->stored_passwords(), IsEmpty());
  password_manager::MockAffiliationService affiliation_service;
  password_manager::SavedPasswordsPresenter presenter{
      &affiliation_service, test_password_store,
      /*account_store=*/nullptr};
  presenter.Init();

  PasswordManagerPorter porter(
      profile.get(), &presenter,
      /*on_export_progress_callback=*/base::DoNothing());

  auto importer =
      std::make_unique<password_manager::PasswordImporter>(&presenter);

  FakePasswordParserService service;
  mojo::Receiver<password_manager::mojom::CSVPasswordParser> receiver{&service};
  mojo::PendingRemote<password_manager::mojom::CSVPasswordParser>
      pending_remote{receiver.BindNewPipeAndPassRemote()};
  importer->SetServiceForTesting(std::move(pending_remote));

  porter.SetImporterForTesting(std::move(importer));

  ui::SelectFileDialog::SetFactory(new FakeCancellingSelectFileDialogFactory());

  base::MockCallback<PasswordManagerPorter::ImportResultsCallback> callback;
  EXPECT_CALL(
      callback,
      Run(::testing::Field(&password_manager::ImportResults::status,
                           password_manager::ImportResults::Status::DISMISSED)))
      .Times(1);
  porter.Import(web_contents(),
                password_manager::PasswordForm::Store::kProfileStore,
                callback.Get());
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(test_password_store->stored_passwords(), IsEmpty());
  store->ShutdownOnUIThread();
}

struct FormDescription {
  std::string origin;
  std::string username;
  std::string password;
};

struct TestCase {
  base::StringPiece csv;
  std::vector<FormDescription> descriptions;
};

// PasswordManagerPorterStoreTest, unlike PasswordManagerPorterTest, does not
// mock out the part of PasswordManagerPorter which is responsible for storing
// the passwords.
class PasswordManagerPorterStoreTest
    : public testing::WithParamInterface<TestCase>,
      public ChromeRenderViewHostTestHarness {};

MATCHER(FormHasDescription, "") {
  const auto& form = std::get<0>(arg);
  const auto& desc = std::get<1>(arg);
  return form.url == GURL(desc.origin) &&
         form.username_value == base::ASCIIToUTF16(desc.username) &&
         form.password_value == base::ASCIIToUTF16(desc.password);
}

TEST_P(PasswordManagerPorterStoreTest, Import) {
  const TestCase& tc = GetParam();
  // Set up the profile and grab the TestPasswordStore. The latter is provided
  // by the GetTestingFactories override in the harness.
  std::unique_ptr<TestingProfile> profile = CreateTestingProfile();
  PasswordStoreFactory::GetInstance()->SetTestingFactoryAndUse(
      profile.get(),
      base::BindRepeating(
          &password_manager::BuildPasswordStore<
              content::BrowserContext, password_manager::TestPasswordStore>));
  scoped_refptr<password_manager::PasswordStoreInterface> store(
      PasswordStoreFactory::GetForProfile(profile.get(),
                                          ServiceAccessType::EXPLICIT_ACCESS));
  auto* test_password_store =
      static_cast<password_manager::TestPasswordStore*>(store.get());

  EXPECT_THAT(test_password_store->stored_passwords(), IsEmpty());

  base::FilePath temp_file_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&temp_file_path));
  ASSERT_TRUE(base::WriteFile(temp_file_path, tc.csv));

  password_manager::MockAffiliationService affiliation_service;
  password_manager::SavedPasswordsPresenter presenter{
      &affiliation_service, test_password_store,
      /*account_store=*/nullptr};
  presenter.Init();

  PasswordManagerPorter porter(
      profile.get(), &presenter,
      /*on_export_progress_callback=*/base::DoNothing());

  auto importer =
      std::make_unique<password_manager::PasswordImporter>(&presenter);

  FakePasswordParserService service;
  mojo::Receiver<password_manager::mojom::CSVPasswordParser> receiver{&service};
  mojo::PendingRemote<password_manager::mojom::CSVPasswordParser>
      pending_remote{receiver.BindNewPipeAndPassRemote()};

  importer->SetServiceForTesting(std::move(pending_remote));

  porter.SetImporterForTesting(std::move(importer));
  ui::SelectFileDialog::SetFactory(
      new TestSelectFileDialogFactory(temp_file_path));
  base::MockCallback<PasswordManagerPorter::ImportResultsCallback> callback;
  EXPECT_CALL(callback, Run).Times(1);
  porter.Import(web_contents(),
                password_manager::PasswordForm::Store::kProfileStore,
                callback.Get());
  base::RunLoop().RunUntilIdle();
  if (tc.descriptions.empty()) {
    EXPECT_THAT(test_password_store->stored_passwords(), IsEmpty());
    return;
  }
  // Note: The code below assumes that all the credentials in tc.csv have the
  // same signon realm, and that it is https://example.com/.
  ASSERT_EQ(1u, test_password_store->stored_passwords().size());
  const auto& credentials = *test_password_store->stored_passwords().begin();
  EXPECT_EQ("https://example.com/", credentials.first);
  EXPECT_THAT(credentials.second,
              UnorderedPointwise(FormHasDescription(), tc.descriptions));

  base::DeleteFile(temp_file_path);
  store->ShutdownOnUIThread();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PasswordManagerPorterStoreTest,
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
