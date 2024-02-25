// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/main/send_search_feedback_handler.h"

#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::settings {

namespace {

class TestSendSearchFeedbackHandler : public SendSearchFeedbackHandler {
 public:
  TestSendSearchFeedbackHandler() = default;
  ~TestSendSearchFeedbackHandler() override = default;

  // Make public for testing.
  using SendSearchFeedbackHandler::set_web_ui;

  MOCK_METHOD(void,
              OpenFeedbackDialogWrapper,
              (const std::string& description_template));
};

class SendSearchFeedbackHandlerTest : public testing::Test {
 public:
  SendSearchFeedbackHandlerTest() = default;
  ~SendSearchFeedbackHandlerTest() override = default;
  SendSearchFeedbackHandlerTest(const SendSearchFeedbackHandlerTest&) = delete;
  SendSearchFeedbackHandlerTest& operator=(
      const SendSearchFeedbackHandlerTest&) = delete;

  void SetUp() override {
    handler_ = std::make_unique<TestSendSearchFeedbackHandler>();
    handler_->set_web_ui(&web_ui_);
    handler_->RegisterMessages();
    handler_->AllowJavascriptForTesting();
  }

  void TearDown() override { handler_.reset(); }

 protected:
  content::TestWebUI web_ui_;
  std::unique_ptr<TestSendSearchFeedbackHandler> handler_;
};

TEST_F(SendSearchFeedbackHandlerTest, TestHandleOpenFeedbackDialog) {
  base::Value::List args;
  std::string description_template = "#Settings foo bar";
  args.Append(description_template);
  EXPECT_CALL(*handler_, OpenFeedbackDialogWrapper(description_template))
      .Times(1);
  web_ui_.HandleReceivedMessage("openSearchFeedbackDialog", args);
}

}  // namespace

}  // namespace ash::settings
