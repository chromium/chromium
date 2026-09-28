// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/network_ui/onc_import_message_handler.h"

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/values.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/shell_dialogs/fake_select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/select_file_policy.h"

namespace ash {

namespace {

class TrackingSelectFilePolicy : public ui::SelectFilePolicy {
 public:
  explicit TrackingSelectFilePolicy(bool* can_open_called)
      : can_open_called_(can_open_called) {}

  bool CanOpenSelectFileDialog() override {
    *can_open_called_ = true;
    return true;
  }

  void SelectFileDenied() override {}

 private:
  raw_ptr<bool> can_open_called_;
};

class DenyingSelectFilePolicy : public ui::SelectFilePolicy {
 public:
  bool CanOpenSelectFileDialog() override { return false; }
  void SelectFileDenied() override {}
};

}  // namespace

class OncImportMessageHandlerTest : public testing::Test {
 public:
  OncImportMessageHandlerTest() = default;
  ~OncImportMessageHandlerTest() override = default;

  void SetUp() override {
    ui::DeviceDataManager::CreateInstance();
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("test@example.com");
    web_contents_ = web_contents_factory_.CreateWebContents(profile_);
    web_ui_.set_web_contents(web_contents_);

    fake_factory_ = ui::FakeSelectFileDialog::RegisterFactory();
    fake_factory_->SetOpenCallback(base::DoNothing());

    auto handler = std::make_unique<OncImportMessageHandler>(
        OncImportMessageHandler::SelectFilePolicyCreator());
    handler_ = handler.get();
    web_ui_.AddMessageHandler(std::move(handler));
    handler_->AllowJavascriptForTesting();
  }

  void TearDown() override {
    ui::DeviceDataManager::DeleteInstance();
    fake_factory_ = nullptr;
    ui::SelectFileDialog::SetFactory(nullptr);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<TestingProfile> profile_;
  raw_ptr<content::WebContents> web_contents_;
  content::TestWebUI web_ui_;
  raw_ptr<OncImportMessageHandler> handler_;
  raw_ptr<ui::FakeSelectFileDialog::Factory> fake_factory_;
};

TEST_F(OncImportMessageHandlerTest, ImportONCCanceled) {
  base::ListValue args;
  args.Append("callback-id-1");
  web_ui_.HandleReceivedMessage("importONC", args);

  ui::FakeSelectFileDialog* fake_dialog = fake_factory_->GetLastDialog();
  ASSERT_TRUE(fake_dialog);
  fake_dialog->CallFileSelectionCanceled();

  const std::vector<std::unique_ptr<content::TestWebUI::CallData>>& call_data =
      web_ui_.call_data();
  ASSERT_EQ(1u, call_data.size());
  EXPECT_EQ("cr.webUIResponse", call_data[0]->function_name());
  ASSERT_TRUE(call_data[0]->arg1()->is_string());
  EXPECT_EQ("callback-id-1", call_data[0]->arg1()->GetString());

  // ResolveJavascriptCallback injects a true boolean for success by default
  ASSERT_TRUE(call_data[0]->arg2()->is_bool());
  EXPECT_TRUE(call_data[0]->arg2()->GetBool());

  // Verify the payload list returned to the WebUI
  ASSERT_TRUE(call_data[0]->arg3()->is_list());
  const auto& response_list = call_data[0]->arg3()->GetList();
  ASSERT_EQ(2u, response_list.size());
  EXPECT_EQ("File selection canceled", response_list[0].GetString());

  // Cancellation resolves cleanly without raising a WebUI error.
  EXPECT_FALSE(response_list[1].GetBool());
}

TEST_F(OncImportMessageHandlerTest, ImportONCEmptyFileSelected) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath empty_file_path = temp_dir.GetPath().AppendASCII("empty.onc");
  ASSERT_TRUE(base::WriteFile(empty_file_path, ""));

  base::ListValue args;
  args.Append("callback-id-2");
  web_ui_.HandleReceivedMessage("importONC", args);

  ui::FakeSelectFileDialog* fake_dialog = fake_factory_->GetLastDialog();
  ASSERT_TRUE(fake_dialog);
  ASSERT_TRUE(fake_dialog->CallFileSelected(empty_file_path, "onc"));

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return web_ui_.call_data().size() > 0; }));

  const std::vector<std::unique_ptr<content::TestWebUI::CallData>>& call_data =
      web_ui_.call_data();
  ASSERT_EQ(1u, call_data.size());
  EXPECT_EQ("cr.webUIResponse", call_data[0]->function_name());
  ASSERT_TRUE(call_data[0]->arg1()->is_string());
  EXPECT_EQ("callback-id-2", call_data[0]->arg1()->GetString());
  ASSERT_TRUE(call_data[0]->arg2()->is_bool());
  EXPECT_TRUE(call_data[0]->arg2()->GetBool());
  ASSERT_TRUE(call_data[0]->arg3()->is_list());
  const auto& response_list = call_data[0]->arg3()->GetList();
  ASSERT_EQ(2u, response_list.size());
  EXPECT_EQ("File not read", response_list[0].GetString());

  // Empty file read resolves cleanly without raising a WebUI error.
  EXPECT_FALSE(response_list[1].GetBool());

  // Verify that internal state is reset and a subsequent call works.
  base::ListValue args2;
  args2.Append("callback-id-2-subsequent");
  web_ui_.HandleReceivedMessage("importONC", args2);

  ui::FakeSelectFileDialog* fake_dialog2 = fake_factory_->GetLastDialog();
  ASSERT_TRUE(fake_dialog2);

  // If the internal state wasn't reset, HandleReceivedMessage would have
  // synchronously rejected the promise with "File selection dialog already
  // open", increasing the call_data size to 2. Since it's still 1, a new dialog
  // was successfully opened.
  EXPECT_EQ(1u, web_ui_.call_data().size());
}

TEST_F(OncImportMessageHandlerTest, ListenerDestroyedOnHandlerDestruction) {
  base::ListValue args;
  args.Append("callback-id-3");
  web_ui_.HandleReceivedMessage("importONC", args);

  ui::FakeSelectFileDialog* fake_dialog = fake_factory_->GetLastDialog();
  ASSERT_TRUE(fake_dialog);
  scoped_refptr<ui::SelectFileDialog> keep_alive(fake_dialog);

  EXPECT_EQ(handler_, fake_dialog->listener());

  // Destroy the handler and verify the listener is cleared.
  handler_ = nullptr;
  web_ui_.GetHandlersForTesting()->clear();
  EXPECT_EQ(nullptr, fake_dialog->listener());

  // Simulate user interaction on the orphaned dialog to ensure no UAF crashes
  // occur.
  fake_dialog->CallFileSelectionCanceled();
  EXPECT_FALSE(
      fake_dialog->CallFileSelected(base::FilePath("test.onc"), "onc"));
}

TEST_F(OncImportMessageHandlerTest, ImportONCWhileDialogOpen) {
  // First import call opens the dialog.
  base::ListValue args1;
  args1.Append("callback-id-first");
  web_ui_.HandleReceivedMessage("importONC", args1);

  // Expect no response yet, since the dialog is just open.
  EXPECT_TRUE(web_ui_.call_data().empty());

  ui::FakeSelectFileDialog* fake_dialog = fake_factory_->GetLastDialog();
  ASSERT_TRUE(fake_dialog);

  // Second import call while the dialog is still open.
  base::ListValue args2;
  args2.Append("callback-id-second");
  web_ui_.HandleReceivedMessage("importONC", args2);

  // The second call should immediately be rejected.
  const std::vector<std::unique_ptr<content::TestWebUI::CallData>>& call_data =
      web_ui_.call_data();
  ASSERT_EQ(1u, call_data.size());
  EXPECT_EQ("cr.webUIResponse", call_data[0]->function_name());
  EXPECT_EQ("callback-id-second", call_data[0]->arg1()->GetString());
  EXPECT_TRUE(call_data[0]->arg2()->GetBool());

  ASSERT_TRUE(call_data[0]->arg3()->is_list());
  const auto& response_list = call_data[0]->arg3()->GetList();
  ASSERT_EQ(2u, response_list.size());
  EXPECT_EQ("File selection dialog already open", response_list[0].GetString());
  EXPECT_TRUE(response_list[1].GetBool());

  // Cancel the active dialog to verify it was not corrupted by the concurrent
  // request.
  fake_dialog->CallFileSelectionCanceled();

  // A second response should have been added for the first callback.
  ASSERT_EQ(2u, call_data.size());
  EXPECT_EQ("cr.webUIResponse", call_data[1]->function_name());
  EXPECT_EQ("callback-id-first", call_data[1]->arg1()->GetString());
  EXPECT_TRUE(call_data[1]->arg2()->GetBool());

  ASSERT_TRUE(call_data[1]->arg3()->is_list());
  const auto& response_list_first = call_data[1]->arg3()->GetList();
  ASSERT_EQ(2u, response_list_first.size());
  EXPECT_EQ("File selection canceled", response_list_first[0].GetString());
  EXPECT_FALSE(response_list_first[1].GetBool());
}

TEST_F(OncImportMessageHandlerTest, ImportONCSequentialReinvocation) {
  // First import call.
  base::ListValue args1;
  args1.Append("callback-id-1");
  web_ui_.HandleReceivedMessage("importONC", args1);

  ui::FakeSelectFileDialog* fake_dialog1 = fake_factory_->GetLastDialog();
  ASSERT_TRUE(fake_dialog1);

  // Cancel it.
  fake_dialog1->CallFileSelectionCanceled();
  ASSERT_EQ(1u, web_ui_.call_data().size());

  // Second import call should succeed in opening a new dialog.
  base::ListValue args2;
  args2.Append("callback-id-2");
  web_ui_.HandleReceivedMessage("importONC", args2);

  ui::FakeSelectFileDialog* fake_dialog2 = fake_factory_->GetLastDialog();
  ASSERT_TRUE(fake_dialog2);

  // Cancel the second dialog to verify it is fully functional.
  fake_dialog2->CallFileSelectionCanceled();
  ASSERT_EQ(2u, web_ui_.call_data().size());
  EXPECT_EQ("callback-id-2", web_ui_.call_data()[1]->arg1()->GetString());
}

TEST_F(OncImportMessageHandlerTest, ImportONCValidFileSelected) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath valid_file_path = temp_dir.GetPath().AppendASCII("valid.onc");
  const std::string kValidOncBlob = "{\"Type\": \"UnencryptedConfiguration\"}";
  ASSERT_TRUE(base::WriteFile(valid_file_path, kValidOncBlob));

  base::ListValue args;
  args.Append("callback-id-valid");
  web_ui_.HandleReceivedMessage("importONC", args);

  ui::FakeSelectFileDialog* fake_dialog = fake_factory_->GetLastDialog();
  ASSERT_TRUE(fake_dialog);
  ASSERT_TRUE(fake_dialog->CallFileSelected(valid_file_path, "onc"));

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return web_ui_.call_data().size() > 0; }));

  const std::vector<std::unique_ptr<content::TestWebUI::CallData>>& call_data =
      web_ui_.call_data();
  ASSERT_EQ(1u, call_data.size());
  EXPECT_EQ("cr.webUIResponse", call_data[0]->function_name());
  EXPECT_EQ("callback-id-valid", call_data[0]->arg1()->GetString());
  EXPECT_TRUE(call_data[0]->arg2()->GetBool());

  ASSERT_TRUE(call_data[0]->arg3()->is_list());
  const auto& response_list = call_data[0]->arg3()->GetList();
  ASSERT_EQ(2u, response_list.size());

  // Verify that the payload was handed off to the IO thread and reached
  // the expected error path due to the missing UserManager in the test.
  EXPECT_EQ("User not found.", response_list[0].GetString());
  EXPECT_TRUE(response_list[1].GetBool());
}

TEST_F(OncImportMessageHandlerTest, PolicyCreatorInvoked) {
  content::TestWebUI test_web_ui;
  test_web_ui.set_web_contents(web_contents_);

  content::WebContents* captured_web_contents = nullptr;
  bool can_open_called = false;
  auto creator =
      base::BindLambdaForTesting([&captured_web_contents, &can_open_called](
                                     content::WebContents* web_contents)
                                     -> std::unique_ptr<ui::SelectFilePolicy> {
        captured_web_contents = web_contents;
        return std::make_unique<TrackingSelectFilePolicy>(&can_open_called);
      });

  auto handler = std::make_unique<OncImportMessageHandler>(std::move(creator));
  auto* handler_ptr = handler.get();
  test_web_ui.AddMessageHandler(std::move(handler));
  handler_ptr->AllowJavascriptForTesting();

  base::ListValue args;
  args.Append("callback-id-policy");
  test_web_ui.HandleReceivedMessage("importONC", args);

  // Verify the callback received the WebContents.
  EXPECT_EQ(web_contents_, captured_web_contents);

  // Verify the resulting policy was consulted by
  // SelectFileDialog::SelectFile().
  EXPECT_TRUE(can_open_called);
}

TEST_F(OncImportMessageHandlerTest, PolicyDeniesFileSelection) {
  content::TestWebUI test_web_ui;
  test_web_ui.set_web_contents(web_contents_);

  auto creator =
      base::BindLambdaForTesting([](content::WebContents* web_contents)
                                     -> std::unique_ptr<ui::SelectFilePolicy> {
        return std::make_unique<DenyingSelectFilePolicy>();
      });

  auto handler = std::make_unique<OncImportMessageHandler>(std::move(creator));
  auto* handler_ptr = handler.get();
  test_web_ui.AddMessageHandler(std::move(handler));
  handler_ptr->AllowJavascriptForTesting();

  base::ListValue args;
  args.Append("callback-id-denied");
  test_web_ui.HandleReceivedMessage("importONC", args);

  // SelectFileDialog posts a task to cancel the selection if the policy
  // denies it. Wait for that task to run.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return test_web_ui.call_data().size() > 0; }));

  ASSERT_EQ(1u, test_web_ui.call_data().size());
  // The policy should prevent the dialog from opening, and the promise
  // should be resolved cleanly with a "File selection canceled" response.
  const content::TestWebUI::CallData& call_data =
      *test_web_ui.call_data().back();
  EXPECT_EQ("cr.webUIResponse", call_data.function_name());
  EXPECT_EQ("callback-id-denied", call_data.arg1()->GetString());
  EXPECT_TRUE(call_data.arg2()->GetBool());  // is_success / !is_error

  // arg3 is the response object, which `Respond()` packages as a list:
  // [result_string, is_error_bool]
  ASSERT_TRUE(call_data.arg3()->is_list());
  const auto& response_list = call_data.arg3()->GetList();
  ASSERT_EQ(2u, response_list.size());
  EXPECT_EQ("File selection canceled", response_list[0].GetString());
  EXPECT_FALSE(response_list[1].GetBool());  // is_error should be false
}

}  // namespace ash
