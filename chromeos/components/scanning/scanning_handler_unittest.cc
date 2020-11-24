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

namespace {

constexpr char kHandlerFunctionName[] = "handlerFunctionName";
constexpr char kTestFilePath[] = "/test/file/path";

}  // namespace

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

bool ShowFileInFilesApp(const base::FilePath& drive_path,
                        const base::FilePath& my_files_path,
                        content::WebUI* web_ui,
                        const base::FilePath& path_to_file) {
  return kTestFilePath == path_to_file.value();
}

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
        std::make_unique<chromeos::TestScanningPathsProvider>(),
        base::BindRepeating(&ShowFileInFilesApp, base::FilePath(),
                            base::FilePath()));
    scanning_handler_->SetWebUIForTest(&web_ui_);
    scanning_handler_->RegisterMessages();

    base::ListValue args;
    web_ui_.HandleReceivedMessage("initialize", &args);
  }

  void TearDown() override { ui::SelectFileDialog::SetFactory(nullptr); }

  // Gets the call data after a ScanningHandler WebUI call and asserts the
  // expected response.
  const content::TestWebUI::CallData& GetCallData(int size_before_call) {
    const std::vector<std::unique_ptr<content::TestWebUI::CallData>>&
        call_data_list = web_ui_.call_data();
    EXPECT_EQ(size_before_call + 1u, call_data_list.size());

    const content::TestWebUI::CallData& call_data = *call_data_list.back();
    EXPECT_EQ("cr.webUIResponse", call_data.function_name());
    EXPECT_EQ(kHandlerFunctionName, call_data.arg1()->GetString());
    // True if ResolveJavascriptCallback and false if RejectJavascriptCallback
    // is called by the handler.
    EXPECT_TRUE(call_data.arg2()->GetBool());
    return call_data;
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
  args.Append(kHandlerFunctionName);
  web_ui_.HandleReceivedMessage("requestScanToLocation", &args);

  const content::TestWebUI::CallData& call_data =
      GetCallData(call_data_count_before_call);
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
  args.Append(kHandlerFunctionName);
  web_ui_.HandleReceivedMessage("requestScanToLocation", &args);

  const content::TestWebUI::CallData& call_data =
      GetCallData(call_data_count_before_call);
  const base::DictionaryValue* selected_path_dict;
  EXPECT_TRUE(call_data.arg3()->GetAsDictionary(&selected_path_dict));
  EXPECT_EQ("", *selected_path_dict->FindStringPath("filePath"));
  EXPECT_EQ("", *selected_path_dict->FindStringPath("baseName"));
}

// Validates that invoking the showFileInLocation Web UI event calls the
// OpenFilesAppFunction function and returns the callback with the boolean.
TEST_F(ScanningHandlerTest, ShowFileInLocation) {
  const size_t call_data_count_before_call = web_ui_.call_data().size();
  base::ListValue args;
  args.Append(kHandlerFunctionName);
  args.Append(kTestFilePath);
  web_ui_.HandleReceivedMessage("showFileInLocation", &args);

  const content::TestWebUI::CallData& call_data =
      GetCallData(call_data_count_before_call);
  // Expect true from call to ShowFileInFilesApp().
  EXPECT_TRUE(call_data.arg3()->GetBool());
}

}  // namespace chromeos.
