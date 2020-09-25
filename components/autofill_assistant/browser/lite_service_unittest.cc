// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/lite_service.h"
#include "components/autofill_assistant/browser/mock_service.h"
#include "components/autofill_assistant/browser/service.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/test_util.h"
#include "components/autofill_assistant/browser/trigger_context.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/notreached.h"
#include "base/test/bind_test_util.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gtest_util.h"
#include "base/test/mock_callback.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {
const char kFakeScriptPath[] = "localhost/chrome/test";
const char kFakeUrl[] = "https://www.example.com";
}  // namespace

using ::testing::_;
using ::testing::AtMost;
using ::testing::Eq;

class LiteServiceTest : public testing::Test {
 protected:
  LiteServiceTest() {
    auto service_impl = std::make_unique<MockService>();
    mock_native_service_ = service_impl.get();

    lite_service_ = std::make_unique<LiteService>(
        std::move(service_impl), kFakeScriptPath, mock_finished_callback_.Get(),
        mock_script_running_callback_.Get());
    EXPECT_CALL(*mock_native_service_, OnGetScriptsForUrl).Times(0);
    EXPECT_CALL(*mock_native_service_, OnGetNextActions).Times(0);

    EXPECT_CALL(*mock_native_service_,
                OnGetActions(kFakeScriptPath, _, _, _, _, _))
        .Times(AtMost(1))
        .WillOnce([&](const std::string& script_path, const GURL& url,
                      const TriggerContext& trigger_context,
                      const std::string& global_payload,
                      const std::string& script_payload,
                      Service::ResponseCallback& callback) {
          std::string serialized_response;
          get_actions_response_.SerializeToString(&serialized_response);
          std::move(callback).Run(true, serialized_response);
        });
  }
  ~LiteServiceTest() override {}

  void SetSecondScriptPartForTest(std::unique_ptr<ActionsResponseProto> proto) {
    lite_service_->trigger_script_second_part_ = std::move(proto);
  }

  ActionsResponseProto* GetSecondScriptPartForTest() const {
    return lite_service_->trigger_script_second_part_.get();
  }

  void ExpectStopWithFinishedState(Metrics::LiteScriptFinishedState state) {
    ActionsResponseProto stop;
    stop.add_actions()->mutable_stop();
    std::string serialized_stop;
    stop.SerializeToString(&serialized_stop);
    EXPECT_CALL(mock_response_callback_, Run(true, serialized_stop)).Times(1);
    EXPECT_CALL(mock_finished_callback_, Run(state));
  }

  base::MockCallback<base::OnceCallback<void(Metrics::LiteScriptFinishedState)>>
      mock_finished_callback_;
  base::MockCallback<base::RepeatingCallback<void(bool)>>
      mock_script_running_callback_;
  base::MockCallback<base::OnceCallback<void(bool, const std::string&)>>
      mock_response_callback_;
  MockService* mock_native_service_ = nullptr;
  std::unique_ptr<LiteService> lite_service_;
  ActionsResponseProto get_actions_response_;
};

TEST_F(LiteServiceTest, RunsNotificationOnDelete) {
  base::MockCallback<base::OnceCallback<void(Metrics::LiteScriptFinishedState)>>
      notification_callback;
  base::MockCallback<base::RepeatingCallback<void(bool)>>
      script_running_callback;
  EXPECT_CALL(
      notification_callback,
      Run(Metrics::LiteScriptFinishedState::LITE_SCRIPT_SERVICE_DELETED));
  {
    LiteService lite_service(std::make_unique<MockService>(), kFakeScriptPath,
                             notification_callback.Get(),
                             script_running_callback.Get());
  }
}

TEST_F(LiteServiceTest, GetScriptsForUrl) {
  EXPECT_CALL(mock_response_callback_, Run(true, _))
      .Times(1)
      .WillOnce([](bool success, const std::string& response) {
        SupportsScriptResponseProto proto;
        ASSERT_TRUE(proto.ParseFromString(response));

        ASSERT_THAT(proto.scripts().size(), Eq(1));
        EXPECT_THAT(proto.scripts(0).path(), Eq(kFakeScriptPath));
        EXPECT_TRUE(proto.scripts(0).presentation().autostart());
        EXPECT_FALSE(proto.scripts(0).presentation().needs_ui());
        EXPECT_FALSE(proto.scripts(0).presentation().chip().text().empty());
      });
  lite_service_->GetScriptsForUrl(GURL(kFakeUrl), TriggerContextImpl(),
                                  mock_response_callback_.Get());
}

TEST_F(LiteServiceTest, GetActionsOnlySendsScriptPath) {
  TriggerContextImpl non_empty_context;
  non_empty_context.SetCallerAccountHash("something");

  EXPECT_CALL(*mock_native_service_,
              OnGetActions(kFakeScriptPath, _, _, _, _, _))
      .Times(1)
      .WillOnce([&](const std::string& script_path, const GURL& url,
                    const TriggerContext& trigger_context,
                    const std::string& global_payload,
                    const std::string& script_payload,
                    Service::ResponseCallback& callback) {
        EXPECT_THAT(script_path, Eq(kFakeScriptPath));
        // All other information has been cleared.
        EXPECT_THAT(url, Eq(GURL()));
        EXPECT_THAT(trigger_context.get_caller_account_hash(), Eq(""));
        EXPECT_THAT(global_payload, Eq(""));
        EXPECT_THAT(script_payload, Eq(""));
      });

  lite_service_->GetActions(kFakeScriptPath, GURL(kFakeUrl), non_empty_context,
                            "payload", "payload",
                            mock_response_callback_.Get());
}

TEST_F(LiteServiceTest, GetActionsOnlyFetchesFromScriptPath) {
  ExpectStopWithFinishedState(
      Metrics::LiteScriptFinishedState::LITE_SCRIPT_PATH_MISMATCH);
  lite_service_->GetActions("localhost/chrome/different", GURL(kFakeUrl),
                            TriggerContextImpl(), "", "",
                            mock_response_callback_.Get());
}

TEST_F(LiteServiceTest, StopsOnGetActionsFailed) {
  ExpectStopWithFinishedState(
      Metrics::LiteScriptFinishedState::LITE_SCRIPT_GET_ACTIONS_FAILED);
  EXPECT_CALL(*mock_native_service_,
              OnGetActions(kFakeScriptPath, _, _, _, _, _))
      .Times(1)
      .WillOnce([&](const std::string& script_path, const GURL& url,
                    const TriggerContext& trigger_context,
                    const std::string& global_payload,
                    const std::string& script_payload,
                    Service::ResponseCallback& callback) {
        std::move(callback).Run(false, std::string());
      });

  EXPECT_CALL(mock_script_running_callback_, Run).Times(0);
  lite_service_->GetActions(kFakeScriptPath, GURL(kFakeUrl),
                            TriggerContextImpl(), "", "",
                            mock_response_callback_.Get());
}

TEST_F(LiteServiceTest, StopsOnGetActionsParsingError) {
  ExpectStopWithFinishedState(
      Metrics::LiteScriptFinishedState::LITE_SCRIPT_GET_ACTIONS_PARSE_ERROR);
  EXPECT_CALL(*mock_native_service_,
              OnGetActions(kFakeScriptPath, _, _, _, _, _))
      .Times(1)
      .WillOnce([&](const std::string& script_path, const GURL& url,
                    const TriggerContext& trigger_context,
                    const std::string& global_payload,
                    const std::string& script_payload,
                    Service::ResponseCallback& callback) {
        std::move(callback).Run(true, std::string("invalid proto"));
      });
  EXPECT_CALL(mock_script_running_callback_, Run).Times(0);
  lite_service_->GetActions(kFakeScriptPath, GURL(kFakeUrl),
                            TriggerContextImpl(), "", "",
                            mock_response_callback_.Get());
}

TEST_F(LiteServiceTest, StopsOnGetActionsContainsUnsafeActions) {
  get_actions_response_.add_actions()->mutable_prompt()->set_browse_mode(true);
  get_actions_response_.add_actions()->mutable_click();
  get_actions_response_.add_actions()->mutable_prompt();
  ExpectStopWithFinishedState(
      Metrics::LiteScriptFinishedState::LITE_SCRIPT_UNSAFE_ACTIONS);
  EXPECT_CALL(mock_script_running_callback_, Run).Times(0);
  lite_service_->GetActions(kFakeScriptPath, GURL(kFakeUrl),
                            TriggerContextImpl(), "", "",
                            mock_response_callback_.Get());
}

TEST_F(LiteServiceTest, GetActionsSucceedsForMinimalViableScript) {
  get_actions_response_.add_actions()->mutable_prompt()->set_browse_mode(true);
  get_actions_response_.add_actions()->mutable_prompt();
  EXPECT_CALL(mock_response_callback_, Run(true, _));
  EXPECT_CALL(mock_script_running_callback_, Run(/*ui_shown =*/false)).Times(1);
  lite_service_->GetActions(kFakeScriptPath, GURL(kFakeUrl),
                            TriggerContextImpl(), "", "",
                            mock_response_callback_.Get());
}

TEST_F(LiteServiceTest, GetActionsSplitsActionsResponseAtLastBrowse) {
  ActionsResponseProto expected_first_part;
  expected_first_part.add_actions()->mutable_prompt()->set_browse_mode(true);
  expected_first_part.add_actions()->mutable_wait_for_dom();
  auto* last_browse_mode_action =
      expected_first_part.add_actions()->mutable_prompt();
  last_browse_mode_action->set_browse_mode(true);
  last_browse_mode_action->add_choices()->mutable_auto_select_when();

  ActionsResponseProto expected_second_part;
  expected_second_part.add_actions()->mutable_prompt();

  for (const auto& action : expected_first_part.actions()) {
    *get_actions_response_.add_actions() = action;
  }
  for (const auto& action : expected_second_part.actions()) {
    *get_actions_response_.add_actions() = action;
  }

  std::vector<ProcessedActionProto> processed_actions;
  EXPECT_CALL(mock_response_callback_, Run(true, _))
      .Times(1)
      .WillOnce([&](bool result, const std::string& response) {
        // Can't directly compare the protos here, because the lite service
        // will have automatically assigned unique payloads to prompts.
        ActionsResponseProto proto;
        ASSERT_TRUE(proto.ParseFromString(response));
        ASSERT_TRUE(proto.actions().size() == 4);
        // The first action is a ConfigureUiState, which is automatically added
        // by the lite_service.
        EXPECT_TRUE(proto.actions(0).action_info_case() ==
                    ActionProto::kConfigureUiState);
        EXPECT_TRUE(proto.actions(0).configure_ui_state().overlay_behavior() ==
                    ConfigureUiStateProto::HIDDEN);
        EXPECT_TRUE(proto.actions(1).action_info_case() ==
                    ActionProto::kPrompt);
        EXPECT_TRUE(proto.actions(1).prompt().browse_mode());
        EXPECT_TRUE(proto.actions(2).action_info_case() ==
                    ActionProto::kWaitForDom);
        EXPECT_TRUE(proto.actions(3).action_info_case() ==
                    ActionProto::kPrompt);
        EXPECT_TRUE(proto.actions(3).prompt().browse_mode());

        for (const auto& action : proto.actions()) {
          processed_actions.emplace_back(ProcessedActionProto());
          *processed_actions.back().mutable_action() = action;
          processed_actions.back().set_status(ACTION_APPLIED);
        }
        processed_actions.back().mutable_prompt_choice()->set_server_payload(
            proto.actions(3).prompt().choices(0).server_payload());
      });
  EXPECT_CALL(mock_script_running_callback_, Run(/*ui_shown =*/false)).Times(1);
  lite_service_->GetActions(kFakeScriptPath, GURL(kFakeUrl),
                            TriggerContextImpl(), "", "",
                            mock_response_callback_.Get());

  EXPECT_CALL(mock_response_callback_, Run(true, _))
      .Times(1)
      .WillOnce([&](bool result, const std::string& response) {
        ActionsResponseProto proto;
        ASSERT_TRUE(proto.ParseFromString(response));
        ASSERT_TRUE(proto.actions().size() == 1);
        EXPECT_TRUE(proto.actions(0).action_info_case() ==
                    ActionProto::kPrompt);
        EXPECT_FALSE(proto.actions(0).prompt().browse_mode());
      });

  EXPECT_CALL(mock_script_running_callback_, Run(/*ui_shown =*/true)).Times(1);
  lite_service_->GetNextActions(TriggerContextImpl(), "", "", processed_actions,
                                mock_response_callback_.Get());
}

TEST_F(LiteServiceTest, GetNextActionsFirstPartStopsOnUserNavigateAway) {
  SetSecondScriptPartForTest(std::make_unique<ActionsResponseProto>());
  std::vector<ProcessedActionProto> processed_actions(1);
  processed_actions.back().set_status(ACTION_APPLIED);
  processed_actions.back().mutable_action()->mutable_prompt();
  processed_actions.back().mutable_prompt_choice()->set_navigation_ended(true);

  ExpectStopWithFinishedState(
      Metrics::LiteScriptFinishedState::LITE_SCRIPT_BROWSE_FAILED_NAVIGATE);
  EXPECT_CALL(mock_script_running_callback_, Run(/*ui_shown =*/true)).Times(0);
  lite_service_->GetNextActions(TriggerContextImpl(), "", "", processed_actions,
                                mock_response_callback_.Get());
}

TEST_F(LiteServiceTest, GetNextActionsFirstPartSucceedsOnAutoSelectChoice) {
  SetSecondScriptPartForTest(std::make_unique<ActionsResponseProto>());
  std::vector<ProcessedActionProto> processed_actions(1);
  auto* choice = processed_actions.back()
                     .mutable_action()
                     ->mutable_prompt()
                     ->add_choices();
  choice->set_server_payload("payload");
  choice->mutable_auto_select_when();
  processed_actions.back().set_status(ACTION_APPLIED);
  processed_actions.back().mutable_prompt_choice()->set_server_payload(
      "payload");

  EXPECT_CALL(mock_response_callback_, Run(true, ""));
  EXPECT_CALL(mock_script_running_callback_, Run(/*ui_shown =*/true)).Times(1);
  lite_service_->GetNextActions(TriggerContextImpl(), "", "", processed_actions,
                                mock_response_callback_.Get());
}

TEST_F(LiteServiceTest, GetNextActionsSecondPartOnlyServedOnce) {
  SetSecondScriptPartForTest(std::make_unique<ActionsResponseProto>());
  std::vector<ProcessedActionProto> processed_actions(1);
  auto* choice = processed_actions.back()
                     .mutable_action()
                     ->mutable_prompt()
                     ->add_choices();
  choice->set_server_payload("payload");
  choice->mutable_auto_select_when();
  processed_actions.back().set_status(ACTION_APPLIED);
  processed_actions.back().mutable_prompt_choice()->set_server_payload(
      "payload");

  EXPECT_CALL(mock_response_callback_, Run(true, "")).Times(1);
  lite_service_->GetNextActions(TriggerContextImpl(), "", "", processed_actions,
                                mock_response_callback_.Get());

  ExpectStopWithFinishedState(
      Metrics::LiteScriptFinishedState::
          LITE_SCRIPT_PROMPT_FAILED_CONDITION_NO_LONGER_TRUE);
  lite_service_->GetNextActions(TriggerContextImpl(), "", "", processed_actions,
                                mock_response_callback_.Get());
}

TEST_F(LiteServiceTest, GetNextActionsSecondPartStopsWhenUserCancels) {
  std::vector<ProcessedActionProto> processed_actions(1);
  auto* choice = processed_actions.back()
                     .mutable_action()
                     ->mutable_prompt()
                     ->add_choices();
  choice->set_server_payload("payload");
  choice->mutable_chip()->set_type(CLOSE_ACTION);
  processed_actions.back().set_status(ACTION_APPLIED);
  processed_actions.back().mutable_prompt_choice()->set_server_payload(
      "payload");

  ExpectStopWithFinishedState(
      Metrics::LiteScriptFinishedState::LITE_SCRIPT_PROMPT_FAILED_CLOSE);
  lite_service_->GetNextActions(TriggerContextImpl(), "", "", processed_actions,
                                mock_response_callback_.Get());
}

TEST_F(LiteServiceTest, GetNextActionsSecondPartStopsWhenUserNavigatesBack) {
  std::vector<ProcessedActionProto> processed_actions(1);
  auto* choice = processed_actions.back()
                     .mutable_action()
                     ->mutable_prompt()
                     ->add_choices();
  choice->set_server_payload("payload");
  choice->mutable_auto_select_when();
  processed_actions.back().set_status(ACTION_APPLIED);
  processed_actions.back().mutable_prompt_choice()->set_server_payload(
      "payload");

  ExpectStopWithFinishedState(
      Metrics::LiteScriptFinishedState::
          LITE_SCRIPT_PROMPT_FAILED_CONDITION_NO_LONGER_TRUE);
  lite_service_->GetNextActions(TriggerContextImpl(), "", "", processed_actions,
                                mock_response_callback_.Get());
}

TEST_F(LiteServiceTest, GetNextActionsSecondPartStopsWhenUserNavigatesAway) {
  std::vector<ProcessedActionProto> processed_actions(1);
  processed_actions.back().set_status(ACTION_APPLIED);
  processed_actions.back().mutable_action()->mutable_prompt();
  processed_actions.back().mutable_prompt_choice()->set_navigation_ended(true);

  ExpectStopWithFinishedState(
      Metrics::LiteScriptFinishedState::LITE_SCRIPT_PROMPT_FAILED_NAVIGATE);
  lite_service_->GetNextActions(TriggerContextImpl(), "", "", processed_actions,
                                mock_response_callback_.Get());
}

TEST_F(LiteServiceTest, GetNextActionsSecondPartStopsWhenUserAgrees) {
  std::vector<ProcessedActionProto> processed_actions(1);
  auto* choice = processed_actions.back()
                     .mutable_action()
                     ->mutable_prompt()
                     ->add_choices();
  choice->set_server_payload("payload");
  choice->mutable_chip()->set_type(DONE_ACTION);
  processed_actions.back().set_status(ACTION_APPLIED);
  processed_actions.back().mutable_prompt_choice()->set_server_payload(
      "payload");

  ExpectStopWithFinishedState(
      Metrics::LiteScriptFinishedState::LITE_SCRIPT_PROMPT_SUCCEEDED);
  lite_service_->GetNextActions(TriggerContextImpl(), "", "", processed_actions,
                                mock_response_callback_.Get());
}

}  // namespace autofill_assistant
