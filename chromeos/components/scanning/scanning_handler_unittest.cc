// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/scanning/scanning_handler.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "base/values.h"
#include "chromeos/components/scanning/scanning_paths_provider.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/select_file_policy.h"

namespace chromeos {

class TestSelectFilePolicy : public ui::SelectFilePolicy {
 public:
  TestSelectFilePolicy& operator=(const TestSelectFilePolicy&) = delete;

  bool CanOpenSelectFileDialog() override { return true; }
  void SelectFileDenied() override {}
};

std::unique_ptr<ui::SelectFilePolicy> CreateTestSelectFilePolicy(
    content::WebContents* web_contents) {
  return std::make_unique<TestSelectFilePolicy>();
}

// A test ui::SelectFileDialog.
class TestSelectFileDialog : public ui::SelectFileDialog {
 public:
  TestSelectFileDialog(Listener* listener,
                       std::unique_ptr<ui::SelectFilePolicy> policy,
                       base::FilePath selected_path)
      : ui::SelectFileDialog(listener, std::move(policy)),
        selected_path_(selected_path) {}

  TestSelectFileDialog(const TestSelectFileDialog&) = delete;
  TestSelectFileDialog& operator=(const TestSelectFileDialog&) = delete;

 protected:
  void SelectFileImpl(Type type,
                      const base::string16& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      void* params) override {
    if (selected_path_.empty()) {
      listener_->FileSelectionCanceled(params);
      return;
    }

    listener_->FileSelected(selected_path_, 0 /* index */,
                            nullptr /* params */);
  }

  bool IsRunning(gfx::NativeWindow owning_window) const override {
    return true;
  }
  void ListenerDestroyed() override {}
  bool HasMultipleFileTypeChoicesImpl() override { return false; }

 private:
  ~TestSelectFileDialog() override = default;

  // The simulatd file path selected by the user.
  base::FilePath selected_path_;
};

// A factory associated with the artificial file picker.
class TestSelectFileDialogFactory : public ui::SelectFileDialogFactory {
 public:
  explicit TestSelectFileDialogFactory(base::FilePath selected_path)
      : selected_path_(selected_path) {}

  ui::SelectFileDialog* Create(
      ui::SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) override {
    return new TestSelectFileDialog(listener, std::move(policy),
                                    selected_path_);
  }

  TestSelectFileDialogFactory(const TestSelectFileDialogFactory&) = delete;
  TestSelectFileDialogFactory& operator=(const TestSelectFileDialogFactory&) =
      delete;

 private:
  // The simulated file path selected by the user.
  base::FilePath selected_path_;
};

// A test impl of ScanningPathsProvider.
class TestScanningPathsProvider : public ScanningPathsProvider {
 public:
  TestScanningPathsProvider() = default;

  TestScanningPathsProvider(const TestScanningPathsProvider&) = delete;
  TestScanningPathsProvider& operator=(const TestScanningPathsProvider&) =
      delete;

  std::string GetBaseNameFromPath(content::WebUI* web_ui,
                                  const base::FilePath& path) override {
    return path.BaseName().value();
  }
};

class ScanningHandlerTest : public testing::Test {
 public:
  ScanningHandlerTest()
      : task_environment_(content::BrowserTaskEnvironment::REAL_IO_THREAD),
        web_ui_(),
        scanning_handler_() {}
  ~ScanningHandlerTest() override = default;

  void SetUp() override {
    scanning_handler_ = std::make_unique<ScanningHandler>(
        base::BindRepeating(&CreateTestSelectFilePolicy),
        std::make_unique<chromeos::TestScanningPathsProvider>());
    scanning_handler_->SetWebUIForTest(&web_ui_);
    scanning_handler_->RegisterMessages();

    base::ListValue args;
    web_ui_.HandleReceivedMessage("initialize", &args);
  }

  void TearDown() override { ui::SelectFileDialog::SetFactory(nullptr); }

  const content::TestWebUI::CallData& CallDataAtIndex(size_t index) {
    return *web_ui_.call_data()[index];
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::TestWebUI web_ui_;
  std::unique_ptr<ScanningHandler> scanning_handler_;
};

// Validates that invoking the requestScanToLocation Web UI event opens the
// select dialog, and if a directory is chosen, returns the selected file path
// and base name.
TEST_F(ScanningHandlerTest, SelectDirectory) {
  const base::FilePath base_file_path("/this/is/a/test/directory/Base Name");
  ui::SelectFileDialog::SetFactory(
      new TestSelectFileDialogFactory(base_file_path));

  const size_t call_data_count_before_call = web_ui_.call_data().size();
  base::ListValue args;
  args.Append("handlerFunctionName");
  web_ui_.HandleReceivedMessage("requestScanToLocation", &args);

  EXPECT_EQ(call_data_count_before_call + 1u, web_ui_.call_data().size());
  const content::TestWebUI::CallData& call_data =
      CallDataAtIndex(call_data_count_before_call);
  EXPECT_EQ("cr.webUIResponse", call_data.function_name());
  EXPECT_EQ("handlerFunctionName", call_data.arg1()->GetString());
  EXPECT_TRUE(call_data.arg2()->GetBool());

  const base::DictionaryValue* selected_path_dict;
  EXPECT_TRUE(call_data.arg3()->GetAsDictionary(&selected_path_dict));
  EXPECT_EQ(base_file_path.value(),
            *selected_path_dict->FindStringPath("filePath"));
  EXPECT_EQ("Base Name", *selected_path_dict->FindStringPath("baseName"));
}

// Validates that invoking the requestScanToLocation Web UI event opens the
// select dialog, and if the dialog is canceled, returns an empty file path and
// base name.
TEST_F(ScanningHandlerTest, CancelDialog) {
  ui::SelectFileDialog::SetFactory(
      new TestSelectFileDialogFactory(base::FilePath()));

  const size_t call_data_count_before_call = web_ui_.call_data().size();
  base::ListValue args;
  args.Append("handlerFunctionName");
  web_ui_.HandleReceivedMessage("requestScanToLocation", &args);

  EXPECT_EQ(call_data_count_before_call + 1u, web_ui_.call_data().size());
  const content::TestWebUI::CallData& call_data =
      CallDataAtIndex(call_data_count_before_call);
  EXPECT_EQ("cr.webUIResponse", call_data.function_name());
  EXPECT_EQ("handlerFunctionName", call_data.arg1()->GetString());
  EXPECT_TRUE(call_data.arg2()->GetBool());

  const base::DictionaryValue* selected_path_dict;
  EXPECT_TRUE(call_data.arg3()->GetAsDictionary(&selected_path_dict));
  EXPECT_EQ("", *selected_path_dict->FindStringPath("filePath"));
  EXPECT_EQ("", *selected_path_dict->FindStringPath("baseName"));
}

}  // namespace chromeos.
