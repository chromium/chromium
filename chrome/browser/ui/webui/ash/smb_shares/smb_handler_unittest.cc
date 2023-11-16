// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/smb_shares/smb_handler.h"

#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/ash/smb_client/smb_service_test_base.h"
#include "content/public/test/test_web_ui.h"

namespace ash::smb_dialog {

class TestSmbHandler : public SmbHandler {
 public:
  explicit TestSmbHandler(Profile* profile)
      : SmbHandler(profile, base::DoNothing()) {}
  ~TestSmbHandler() override = default;

  // Make public for testing.
  using SmbHandler::HandleHasAnySmbMountedBefore;
  using SmbHandler::set_web_ui;
};

class SmbHandlerTest : public ash::smb_client::SmbServiceBaseTest {
 public:
  SmbHandlerTest() = default;
  ~SmbHandlerTest() override = default;

 protected:
  void VerifyHasSmbMountedBeforeResult(bool expected_result) {
    base::Value::List args;
    args.Append("callback-id");
    handler()->HandleHasAnySmbMountedBefore(args);

    const content::TestWebUI::CallData& call_data =
        *web_ui()->call_data().back();

    EXPECT_EQ("cr.webUIResponse", call_data.function_name());
    EXPECT_EQ("callback-id", call_data.arg1()->GetString());
    EXPECT_TRUE(call_data.arg2()->GetBool());
    EXPECT_EQ(expected_result, call_data.arg3()->GetBool());
  }

  TestSmbHandler* handler() { return handler_.get(); }
  content::TestWebUI* web_ui() { return &web_ui_; }

  std::unique_ptr<TestSmbHandler> handler_;
  content::TestWebUI web_ui_;
};

TEST_F(SmbHandlerTest, NoSmbMountedBeforeWithSmbServiceNotAvailable) {
  handler_ = std::make_unique<TestSmbHandler>(profile());
  handler_->set_web_ui(&web_ui_);
  handler_->RegisterMessages();
  handler_->AllowJavascriptForTesting();

  VerifyHasSmbMountedBeforeResult(false);
}

TEST_F(SmbHandlerTest, NoSmbMountedBeforeWithSmbServiceAvailable) {
  handler_ = std::make_unique<TestSmbHandler>(profile());
  if (!smb_service) {
    // Create smb service.
    smb_service = std::make_unique<smb_client::SmbService>(
        profile(), std::make_unique<base::SimpleTestTickClock>());
  }

  handler_->SetSmbServiceForTesting(smb_service.get());
  handler_->set_web_ui(&web_ui_);
  handler_->RegisterMessages();
  handler_->AllowJavascriptForTesting();

  VerifyHasSmbMountedBeforeResult(false);
}

TEST_F(SmbHandlerTest, SmbMountedBeforeWithSmbServiceAvailable) {
  handler_ = std::make_unique<TestSmbHandler>(profile());
  CreateService(profile());
  WaitForSetupComplete();

  // Add a share
  std::ignore =
      MountBasicShare(smb_client::kSharePath, smb_client::kMountPath,
                      base::BindOnce([](smb_client::SmbMountResult result) {
                        EXPECT_EQ(smb_client::SmbMountResult::kSuccess, result);
                      }));

  handler_->SetSmbServiceForTesting(smb_service.get());
  handler_->set_web_ui(&web_ui_);
  handler_->RegisterMessages();
  handler_->AllowJavascriptForTesting();

  VerifyHasSmbMountedBeforeResult(true);
}

}  // namespace ash::smb_dialog
