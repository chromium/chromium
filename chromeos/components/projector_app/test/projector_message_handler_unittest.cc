// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/projector_app/projector_message_handler.h"
#include "ash/public/cpp/projector/projector_controller.h"
#include "ash/public/cpp/test/mock_projector_controller.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/components/projector_app/test/mock_app_client.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestUserEmail[] = "testuser1@gmail.com";

const char kWebUIResponse[] = "cr.webUIResponse";
const char kGetAccountsCallback[] = "getAccountsCallback";
const char kCanStartProjectorSessionCallback[] =
    "canStartProjectorSessionCallback";
const char kStartProjectorSessionCallback[] = "startProjectorSessionCallback";
const char kGetOAuthTokenCallback[] = "getOAuthTokenCallback";

}  // namespace

namespace chromeos {

class ProjectorMessageHandlerUnitTest : public testing::Test {
 public:
  ProjectorMessageHandlerUnitTest() = default;
  ProjectorMessageHandlerUnitTest(const ProjectorMessageHandlerUnitTest&) =
      delete;
  ProjectorMessageHandlerUnitTest& operator=(
      const ProjectorMessageHandlerUnitTest&) = delete;
  ~ProjectorMessageHandlerUnitTest() override = default;

  // testing::Test
  void SetUp() override {
    message_handler_ = std::make_unique<ProjectorMessageHandler>();
    message_handler_->set_web_ui_for_test(&web_ui());
    message_handler_->RegisterMessages();
  }

  void TearDown() override { message_handler_.reset(); }

  ProjectorMessageHandler* message_handler() { return message_handler_.get(); }
  content::TestWebUI& web_ui() { return web_ui_; }
  ash::MockProjectorController& controller() { return mock_controller_; }
  MockAppClient& mock_app_client() { return mock_app_client_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  std::unique_ptr<ProjectorMessageHandler> message_handler_;
  ash::MockProjectorController mock_controller_;
  MockAppClient mock_app_client_;
  content::TestWebUI web_ui_;
};

TEST_F(ProjectorMessageHandlerUnitTest, GetAccounts) {
  base::ListValue list_args;
  list_args.Append(kGetAccountsCallback);

  web_ui().HandleReceivedMessage("getAccounts", &list_args);

  // We expect that there was only one callback to the WebUI.
  EXPECT_EQ(web_ui().call_data().size(), 1u);

  const content::TestWebUI::CallData& call_data = *(web_ui().call_data()[0]);

  EXPECT_EQ(call_data.function_name(), kWebUIResponse);
  EXPECT_EQ(call_data.arg1()->GetString(), kGetAccountsCallback);

  // Whether the callback was rejected or not.
  EXPECT_TRUE(call_data.arg2()->GetBool());
  ASSERT_TRUE(call_data.arg3()->is_list());

  const auto& list_view = call_data.arg3()->GetList();
  // There is only one account in the identity manager.
  EXPECT_EQ(list_view.size(), 1u);

  // Ensure that the entry is an account with a the valid email.
  const auto& account = list_view[0];
  const std::string* email = account.FindStringPath("email");
  ASSERT_NE(email, nullptr);
  EXPECT_EQ(*email, kTestUserEmail);
}

TEST_F(ProjectorMessageHandlerUnitTest, CanStartProjectorSession) {
  EXPECT_CALL(controller(), CanStartNewSession());
  ON_CALL(controller(), CanStartNewSession)
      .WillByDefault(testing::Return(true));

  base::ListValue list_args;
  list_args.Append(kCanStartProjectorSessionCallback);

  web_ui().HandleReceivedMessage("canStartProjectorSession", &list_args);

  // We expect that there was only one callback to the WebUI.
  EXPECT_EQ(web_ui().call_data().size(), 1u);

  const content::TestWebUI::CallData& call_data = *(web_ui().call_data()[0]);
  EXPECT_EQ(call_data.function_name(), kWebUIResponse);
  EXPECT_EQ(call_data.arg1()->GetString(), kCanStartProjectorSessionCallback);
  EXPECT_TRUE(call_data.arg2()->GetBool());
  EXPECT_TRUE(call_data.arg3()->GetBool());
}

TEST_F(ProjectorMessageHandlerUnitTest, GetOAuthTokenForAccount) {
  mock_app_client().SetAutomaticIssueOfAccessTokens(false);

  base::ListValue list_args;
  list_args.Append(kGetOAuthTokenCallback);
  base::ListValue args;
  args.Append(kTestUserEmail);
  list_args.Append(std::move(args));

  web_ui().HandleReceivedMessage("getOAuthTokenForAccount", &list_args);
  mock_app_client().WaitForAccessRequest(kTestUserEmail);

  EXPECT_EQ(web_ui().call_data().size(), 1u);

  const content::TestWebUI::CallData& call_data = *(web_ui().call_data()[0]);
  EXPECT_EQ(call_data.function_name(), kWebUIResponse);
  EXPECT_EQ(call_data.arg1()->GetString(), kGetOAuthTokenCallback);
}

class ProjectorSessionStartUnitTest
    : public ::testing::WithParamInterface<bool>,
      public ProjectorMessageHandlerUnitTest {
 public:
  ProjectorSessionStartUnitTest() = default;
  ProjectorSessionStartUnitTest(const ProjectorSessionStartUnitTest&) = delete;
  ProjectorSessionStartUnitTest& operator=(
      const ProjectorSessionStartUnitTest&) = delete;
  ~ProjectorSessionStartUnitTest() override = default;
};

TEST_P(ProjectorSessionStartUnitTest, ProjectorSessionTest) {
  bool success = GetParam();
  EXPECT_CALL(controller(), CanStartNewSession());
  ON_CALL(controller(), CanStartNewSession)
      .WillByDefault(testing::Return(success));

  base::ListValue list_args;
  list_args.Append(kStartProjectorSessionCallback);
  base::ListValue args;
  args.Append("folderId");
  list_args.Append(std::move(args));

  web_ui().HandleReceivedMessage("startProjectorSession", &list_args);

  // We expect that there was only one callback to the WebUI.
  EXPECT_EQ(web_ui().call_data().size(), 1u);
  const content::TestWebUI::CallData& call_data = *(web_ui().call_data()[0]);

  EXPECT_EQ(call_data.function_name(), kWebUIResponse);
  EXPECT_EQ(call_data.arg1()->GetString(), kStartProjectorSessionCallback);
  EXPECT_TRUE(call_data.arg2()->GetBool());
  EXPECT_EQ(call_data.arg3()->GetBool(), success);
}

INSTANTIATE_TEST_CASE_P(SessionStartSuccessFailTest,
                        ProjectorSessionStartUnitTest,
                        ::testing::Values(true, false));

}  // namespace chromeos
