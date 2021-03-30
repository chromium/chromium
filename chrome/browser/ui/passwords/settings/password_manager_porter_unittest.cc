// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/settings/password_manager_porter.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/passwords/settings/password_manager_presenter.h"
#include "chrome/browser/ui/passwords/settings/password_ui_view.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/export/password_manager_exporter.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/browser/ui/credential_provider_interface.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::IsEmpty;
using ::testing::StrictMock;
using ::testing::UnorderedPointwise;

namespace {

#if defined(OS_WIN)
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

 protected:
  ~TestSelectFileDialog() override = default;

  void SelectFileImpl(Type type,
                      const std::u16string& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      void* params) override {
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

  DISALLOW_COPY_AND_ASSIGN(TestSelectFileDialog);
};

class TestSelectFilePolicy : public ui::SelectFilePolicy {
 public:
  bool CanOpenSelectFileDialog() override { return true; }
  void SelectFileDenied() override {}

 private:
  DISALLOW_ASSIGN(TestSelectFilePolicy);
};

class TestSelectFileDialogFactory : public ui::SelectFileDialogFactory {
 public:
  explicit TestSelectFileDialogFactory(const base::FilePath& forced_path)
      : forced_path_(forced_path) {}

  ui::SelectFileDialog* Create(
      ui::SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) override {
    return new TestSelectFileDialog(
        listener, std::make_unique<TestSelectFilePolicy>(), forced_path_);
  }

 private:
  // The path that will be selected by created dialogs.
  base::FilePath forced_path_;

  DISALLOW_ASSIGN(TestSelectFileDialogFactory);
};

// A fake ui::SelectFileDialog, which will cancel the file selection instead of
// selecting a file.
class FakeCancellingSelectFileDialog : public ui::SelectFileDialog {
 public:
  FakeCancellingSelectFileDialog(Listener* listener,
                                 std::unique_ptr<ui::SelectFilePolicy> policy)
      : ui::SelectFileDialog(listener, std::move(policy)) {}

 protected:
  void SelectFileImpl(Type type,
                      const std::u16string& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      void* params) override {
    listener_->FileSelectionCanceled(params);
  }

  bool IsRunning(gfx::NativeWindow owning_window) const override {
    return false;
  }
  void ListenerDestroyed() override {}
  bool HasMultipleFileTypeChoicesImpl() override { return false; }

 private:
  ~FakeCancellingSelectFileDialog() override = default;

  DISALLOW_COPY_AND_ASSIGN(FakeCancellingSelectFileDialog);
};

class FakeCancellingSelectFileDialogFactory
    : public ui::SelectFileDialogFactory {
 public:
  FakeCancellingSelectFileDialogFactory() {}

  ui::SelectFileDialog* Create(
      ui::SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) override {
    return new FakeCancellingSelectFileDialog(
        listener, std::make_unique<TestSelectFilePolicy>());
  }

 private:
  DISALLOW_ASSIGN(TestSelectFileDialogFactory);
};

class TestPasswordManagerPorter : public PasswordManagerPorter {
 public:
  TestPasswordManagerPorter()
      : PasswordManagerPorter(nullptr, ProgressCallback()) {}

  MOCK_METHOD1(ImportPasswordsFromPath, void(const base::FilePath& path));

  MOCK_METHOD1(ExportPasswordsToPath, void(const base::FilePath& path));

 private:
  DISALLOW_COPY_AND_ASSIGN(TestPasswordManagerPorter);
};

class MockPasswordManagerExporter
    : public password_manager::PasswordManagerExporter {
 public:
  MockPasswordManagerExporter()
      : password_manager::PasswordManagerExporter(
            nullptr,
            base::BindRepeating([](password_manager::ExportProgressStatus,
                                   const std::string&) -> void {})) {}
  ~MockPasswordManagerExporter() override = default;

  MOCK_METHOD0(PreparePasswordsForExport, void());
  MOCK_METHOD0(Cancel, void());
  MOCK_METHOD1(SetDestination, void(const base::FilePath&));
  MOCK_METHOD0(GetExportProgressStatus,
               password_manager::ExportProgressStatus());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPasswordManagerExporter);
};

class PasswordManagerPorterTest : public ChromeRenderViewHostTestHarness {
 protected:
  PasswordManagerPorterTest() = default;
  ~PasswordManagerPorterTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    password_manager_porter_ = std::make_unique<TestPasswordManagerPorter>();
    // SelectFileDialog::SetFactory is responsible for freeing the memory
    // associated with a new factory.
    selected_file_ = base::FilePath(kNullFileName);
    ui::SelectFileDialog::SetFactory(
        new TestSelectFileDialogFactory(selected_file_));
  }

  TestPasswordManagerPorter* password_manager_porter() const {
    return password_manager_porter_.get();
  }

  // The file that our fake file selector returns.
  // This file should not actually be used by the test.
  base::FilePath selected_file_;

 private:
  std::unique_ptr<TestPasswordManagerPorter> password_manager_porter_;

  DISALLOW_COPY_AND_ASSIGN(PasswordManagerPorterTest);
};

// Password importing and exporting using a |SelectFileDialog| is not yet
// supported on Android.
#if !defined(OS_ANDROID)

TEST_F(PasswordManagerPorterTest, PasswordImport) {
  EXPECT_CALL(*password_manager_porter(), ImportPasswordsFromPath(_));

  password_manager_porter()->set_web_contents(web_contents());
  password_manager_porter()->Load();
}

TEST_F(PasswordManagerPorterTest, PasswordExport) {
  PasswordManagerPorter porter(nullptr,
                               PasswordManagerPorter::ProgressCallback());
  std::unique_ptr<MockPasswordManagerExporter> mock_password_manager_exporter_ =
      std::make_unique<StrictMock<MockPasswordManagerExporter>>();

  EXPECT_CALL(*mock_password_manager_exporter_, PreparePasswordsForExport());
  EXPECT_CALL(*mock_password_manager_exporter_, SetDestination(selected_file_));

  porter.set_web_contents(web_contents());
  porter.SetExporterForTesting(std::move(mock_password_manager_exporter_));
  porter.Store();
}

TEST_F(PasswordManagerPorterTest, CancelExportFileSelection) {
  ui::SelectFileDialog::SetFactory(new FakeCancellingSelectFileDialogFactory());

  std::unique_ptr<MockPasswordManagerExporter> mock_password_manager_exporter_ =
      std::make_unique<StrictMock<MockPasswordManagerExporter>>();
  PasswordManagerPorter porter(nullptr,
                               PasswordManagerPorter::ProgressCallback());

  EXPECT_CALL(*mock_password_manager_exporter_, PreparePasswordsForExport());
  EXPECT_CALL(*mock_password_manager_exporter_, Cancel());

  porter.set_web_contents(web_contents());
  porter.SetExporterForTesting(std::move(mock_password_manager_exporter_));
  porter.Store();
}

TEST_F(PasswordManagerPorterTest, CancelStore) {
  std::unique_ptr<MockPasswordManagerExporter> mock_password_manager_exporter_ =
      std::make_unique<StrictMock<MockPasswordManagerExporter>>();
  PasswordManagerPorter porter(nullptr,
                               PasswordManagerPorter::ProgressCallback());

  EXPECT_CALL(*mock_password_manager_exporter_, PreparePasswordsForExport());
  EXPECT_CALL(*mock_password_manager_exporter_, SetDestination(_));
  EXPECT_CALL(*mock_password_manager_exporter_, Cancel());

  porter.set_web_contents(web_contents());
  porter.SetExporterForTesting(std::move(mock_password_manager_exporter_));
  porter.Store();
  porter.CancelStore();
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
  PasswordStoreFactory::GetInstance()->SetTestingFactory(
      profile.get(),
      base::BindRepeating(
          &password_manager::BuildPasswordStore<
              content::BrowserContext, password_manager::TestPasswordStore>));
  scoped_refptr<password_manager::PasswordStore> store(
      PasswordStoreFactory::GetForProfile(profile.get(),
                                          ServiceAccessType::EXPLICIT_ACCESS));
  auto* test_password_store =
      static_cast<password_manager::TestPasswordStore*>(store.get());

  EXPECT_THAT(test_password_store->stored_passwords(), IsEmpty());

  base::FilePath temp_file_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&temp_file_path));
  ASSERT_TRUE(base::WriteFile(temp_file_path, tc.csv));

  // No credential provider needed, because this |porter| won't be used for
  // exporting. No progress callback needed, because UI interaction will be
  // skipped.
  PasswordManagerPorter porter(/*credential_provider_interface=*/nullptr,
                               PasswordManagerPorter::ProgressCallback());
  porter.ImportPasswordsFromPathForTesting(temp_file_path, profile.get());
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
