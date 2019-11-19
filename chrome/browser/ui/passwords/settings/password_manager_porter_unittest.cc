// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/settings/password_manager_porter.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/passwords/settings/password_manager_presenter.h"
#include "chrome/browser/ui/passwords/settings/password_ui_view.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/password_manager/core/browser/export/password_manager_exporter.h"
#include "components/password_manager/core/browser/ui/credential_provider_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"

using ::testing::_;
using ::testing::StrictMock;

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
  ~TestSelectFileDialog() override {}

  void SelectFileImpl(Type type,
                      const base::string16& title,
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
                      const base::string16& title,
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

#endif

}  // namespace
