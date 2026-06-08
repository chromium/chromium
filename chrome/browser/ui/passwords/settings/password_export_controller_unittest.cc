// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/settings/password_export_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/ui/select_file_policy/chrome_select_file_policy.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "components/password_manager/core/browser/export/export_progress_status.h"
#include "components/password_manager/core/browser/export/password_manager_exporter.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::Return;
using ::testing::StrictMock;

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

  FakeCancellingSelectFileDialogFactory(
      const FakeCancellingSelectFileDialogFactory&) = delete;
  FakeCancellingSelectFileDialogFactory& operator=(
      const FakeCancellingSelectFileDialogFactory&) = delete;

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
  MOCK_METHOD(password_manager::ExportProgressStatus,
              GetProgressStatus,
              (),
              (override));
};

class PasswordExportControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  PasswordExportControllerTest(const PasswordExportControllerTest&) = delete;
  PasswordExportControllerTest& operator=(const PasswordExportControllerTest&) =
      delete;

 protected:
  PasswordExportControllerTest() = default;

  ~PasswordExportControllerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    ASSERT_TRUE(directory_.CreateUniqueTempDir());
    // SelectFileDialog::SetFactory is responsible for freeing the memory
    // associated with a new factory.
    ui::SelectFileDialog::SetFactory(
        std::make_unique<TestSelectFileDialogFactory>(temp_file_path()));
    controller_ = std::make_unique<PasswordExportController>(
        &presenter(),
        /*on_export_progress_callback=*/base::DoNothing());

    store_->Init();

    base::test::TestFuture<void> init_future;
    presenter_.Init(init_future.GetCallback());
    ASSERT_TRUE(init_future.Wait());
  }

  void TearDown() override {
    controller_.reset();
    store_->ShutdownOnUIThread();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  PasswordExportController& controller() { return *controller_; }
  password_manager::SavedPasswordsPresenter& presenter() { return presenter_; }
  // The file that our fake file selector returns.
  const base::FilePath temp_file_path() {
    return directory_.GetPath().Append(FILE_PATH_LITERAL("test.csv"));
  }

 private:
  std::unique_ptr<PasswordExportController> controller_;
  scoped_refptr<password_manager::TestPasswordStore> store_ =
      base::MakeRefCounted<password_manager::TestPasswordStore>();
  affiliations::FakeAffiliationService affiliation_service_;
  password_manager::SavedPasswordsPresenter presenter_{
      &affiliation_service_, store_,
      /*account_store=*/nullptr};
  base::ScopedTempDir directory_;
};

TEST_F(PasswordExportControllerTest, PasswordExport) {
  std::unique_ptr<MockPasswordManagerExporter> mock_password_manager_exporter =
      std::make_unique<StrictMock<MockPasswordManagerExporter>>();

  EXPECT_CALL(*mock_password_manager_exporter, GetProgressStatus())
      .WillRepeatedly(
          Return(password_manager::ExportProgressStatus::kNotStarted));
  EXPECT_CALL(*mock_password_manager_exporter, PreparePasswordsForExport());
  EXPECT_CALL(*mock_password_manager_exporter,
              SetDestination(temp_file_path()));

  controller().SetExporterForTesting(std::move(mock_password_manager_exporter));
  EXPECT_TRUE(controller().Export(web_contents()->GetWeakPtr()));
}

TEST_F(PasswordExportControllerTest, ExportInProgressPreventsSubsequentExport) {
  auto mock_exporter_ptr =
      std::make_unique<StrictMock<MockPasswordManagerExporter>>();
  auto* mock_exporter = mock_exporter_ptr.get();

  // Set up the mock to claim it's already working.
  EXPECT_CALL(*mock_exporter, GetProgressStatus())
      .WillRepeatedly(
          Return(password_manager::ExportProgressStatus::kInProgress));

  controller().SetExporterForTesting(std::move(mock_exporter_ptr));

  // Try to start another export. It should return false immediately.
  EXPECT_FALSE(controller().Export(web_contents()->GetWeakPtr()));
}

TEST_F(PasswordExportControllerTest, CancelExportFileSelection) {
  ui::SelectFileDialog::SetFactory(
      std::make_unique<FakeCancellingSelectFileDialogFactory>());

  std::unique_ptr<MockPasswordManagerExporter> mock_password_manager_exporter =
      std::make_unique<StrictMock<MockPasswordManagerExporter>>();

  EXPECT_CALL(*mock_password_manager_exporter, GetProgressStatus())
      .WillRepeatedly(
          Return(password_manager::ExportProgressStatus::kNotStarted));
  EXPECT_CALL(*mock_password_manager_exporter, PreparePasswordsForExport());
  EXPECT_CALL(*mock_password_manager_exporter, Cancel());

  controller().SetExporterForTesting(std::move(mock_password_manager_exporter));
  controller().Export(web_contents()->GetWeakPtr());
}

TEST_F(PasswordExportControllerTest, CancelExport) {
  std::unique_ptr<MockPasswordManagerExporter> mock_password_manager_exporter =
      std::make_unique<StrictMock<MockPasswordManagerExporter>>();

  EXPECT_CALL(*mock_password_manager_exporter, GetProgressStatus())
      .WillRepeatedly(
          Return(password_manager::ExportProgressStatus::kNotStarted));
  EXPECT_CALL(*mock_password_manager_exporter, PreparePasswordsForExport());
  EXPECT_CALL(*mock_password_manager_exporter, SetDestination(_));
  EXPECT_CALL(*mock_password_manager_exporter, Cancel());

  controller().SetExporterForTesting(std::move(mock_password_manager_exporter));
  controller().Export(web_contents()->GetWeakPtr());
  controller().CancelExport();
}

}  // namespace
