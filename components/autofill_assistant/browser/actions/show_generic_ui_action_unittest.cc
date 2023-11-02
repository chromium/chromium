// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/show_generic_ui_action.h"

#include "base/guid.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/mock_personal_data_manager.h"
#include "components/autofill_assistant/browser/public/password_change/mock_website_login_manager.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/user_model.h"
#include "components/autofill_assistant/browser/value_util.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {
const char kFakeUrl[] = "https://www.example.com";
const char kFakeUsername[] = "user@example.com";

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Not;
using ::testing::Property;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

class ShowGenericUiActionTest : public testing::Test {
 public:
  void SetUp() override {
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        &browser_context_, nullptr);
    content::WebContentsTester::For(web_contents_.get())
        ->SetLastCommittedURL(GURL(kFakeUrl));

    ON_CALL(mock_action_delegate_, SetGenericUi(_, _, _, _, _))
        .WillByDefault(Invoke(
            [&](std::unique_ptr<GenericUserInterfaceProto> generic_ui,
                base::OnceCallback<void(const ClientStatus&)>
                    end_action_callback,
                base::OnceCallback<void(const ClientStatus&)>
                    view_inflation_finished_callback,
                base::RepeatingCallback<void(const RequestBackendDataProto&)>
                    request_backend_data_callback,
                base::RepeatingCallback<void(const ShowAccountScreenProto&)>
                    show_account_screen_callback) {
              std::move(view_inflation_finished_callback)
                  .Run(ClientStatus(ACTION_APPLIED));
              std::move(end_action_callback).Run(ClientStatus(ACTION_APPLIED));
            }));
    ON_CALL(mock_action_delegate_, ClearGenericUi()).WillByDefault(Return());
    ON_CALL(mock_action_delegate_, GetUserModel())
        .WillByDefault(Return(&user_model_));
    ON_CALL(mock_action_delegate_, GetUserData())
        .WillByDefault(Return(&user_data_));
    ON_CALL(mock_action_delegate_, GetPersonalDataManager)
        .WillByDefault(Return(&mock_personal_data_manager_));
    ON_CALL(mock_action_delegate_, GetWebsiteLoginManager)
        .WillByDefault(Return(&mock_website_login_manager_));
    ON_CALL(mock_website_login_manager_, GetLoginsForUrl(_, _))
        .WillByDefault(
            RunOnceCallback<1>(std::vector<WebsiteLoginManager::Login>{
                WebsiteLoginManager::Login(GURL(kFakeUrl), kFakeUsername)}));
    ON_CALL(mock_action_delegate_, GetWebContents())
        .WillByDefault(Return(web_contents_.get()));
  }

 protected:
  std::unique_ptr<ShowGenericUiAction> Run() {
    // Apply initial model values as specified by proto.
    user_model_.MergeWithProto(proto_.generic_user_interface().model(), false);
    ActionProto action_proto;
    *action_proto.mutable_show_generic_ui() = proto_;
    auto action = std::make_unique<ShowGenericUiAction>(&mock_action_delegate_,
                                                        action_proto);
    action->ProcessAction(callback_.Get());
    return action;
  }

  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  content::TestBrowserContext browser_context_;
  std::unique_ptr<content::WebContents> web_contents_;
  UserData user_data_;
  UserModel user_model_;
  MockPersonalDataManager mock_personal_data_manager_;
  MockWebsiteLoginManager mock_website_login_manager_;
  MockActionDelegate mock_action_delegate_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  ShowGenericUiProto proto_;
};

TEST_F(ShowGenericUiActionTest, FailedViewInflationEndsAction) {
  ON_CALL(mock_action_delegate_, SetGenericUi(_, _, _, _, _))
      .WillByDefault(Invoke(
          [&](std::unique_ptr<GenericUserInterfaceProto> generic_ui,
              base::OnceCallback<void(const ClientStatus&)> end_action_callback,
              base::OnceCallback<void(const ClientStatus&)>
                  view_inflation_finished_callback,
              base::RepeatingCallback<void(const RequestBackendDataProto&)>
                  request_backend_data_callback,
              base::RepeatingCallback<void(const ShowAccountScreenProto&)>
                  show_account_screen_callback) {
            std::move(view_inflation_finished_callback)
                .Run(ClientStatus(INVALID_ACTION));
          }));

  EXPECT_CALL(mock_action_delegate_, ClearGenericUi()).Times(1);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));

  Run();
}

TEST_F(ShowGenericUiActionTest, GoesIntoPromptState) {
  InSequence seq;
  EXPECT_CALL(mock_action_delegate_, Prompt(_, _, _, _, _)).Times(1);
  EXPECT_CALL(mock_action_delegate_, SetGenericUi(_, _, _, _, _)).Times(1);
  EXPECT_CALL(mock_action_delegate_, ClearGenericUi()).Times(1);
  EXPECT_CALL(mock_action_delegate_, CleanUpAfterPrompt(Eq(true))).Times(1);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));

  Run();
}

TEST_F(ShowGenericUiActionTest, EmptyOutputModel) {
  auto* input_value =
      proto_.mutable_generic_user_interface()->mutable_model()->add_values();
  input_value->set_identifier("input");
  *input_value->mutable_value() = SimpleValue(std::string("InputValue"));

  EXPECT_CALL(mock_action_delegate_, ClearGenericUi()).Times(1);
  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(&ProcessedActionProto::show_generic_ui_result,
                   Property(&ShowGenericUiProto::Result::model,
                            Property(&ModelProto::values, SizeIs(0))))))));

  Run();
}

TEST_F(ShowGenericUiActionTest, NonEmptyOutputModel) {
  auto* input_value_a =
      proto_.mutable_generic_user_interface()->mutable_model()->add_values();
  input_value_a->set_identifier("value_1");
  *input_value_a->mutable_value() = SimpleValue(std::string("input-only"));
  auto* input_value_b =
      proto_.mutable_generic_user_interface()->mutable_model()->add_values();
  input_value_b->set_identifier("value_2");
  *input_value_b->mutable_value() = SimpleValue(std::string("Preset value"));

  proto_.add_output_model_identifiers("value_2");

  ON_CALL(mock_action_delegate_, SetGenericUi(_, _, _, _, _))
      .WillByDefault(Invoke(
          [this](
              std::unique_ptr<GenericUserInterfaceProto> generic_ui,
              base::OnceCallback<void(const ClientStatus&)> end_action_callback,
              base::OnceCallback<void(const ClientStatus&)>
                  view_inflation_finished_callback,
              base::RepeatingCallback<void(const RequestBackendDataProto&)>
                  request_backend_data_callback,
              base::RepeatingCallback<void(const ShowAccountScreenProto&)>
                  show_account_screen_callback) {
            std::move(view_inflation_finished_callback)
                .Run(ClientStatus(ACTION_APPLIED));
            user_model_.SetValue(
                "value_2", SimpleValue(std::string("change"),
                                       /* is_client_side_only = */ false));
            std::move(end_action_callback).Run(ClientStatus(ACTION_APPLIED));
          }));

  EXPECT_CALL(mock_action_delegate_, ClearGenericUi()).Times(1);
  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(&ProcessedActionProto::show_generic_ui_result,
                   Property(&ShowGenericUiProto::Result::model,
                            Property(&ModelProto::values,
                                     UnorderedElementsAre(SimpleModelValue(
                                         "value_2", SimpleValue(std::string(
                                                        "change")))))))))));

  Run();
}

TEST_F(ShowGenericUiActionTest, OutputModelNotSubsetOfInputModel) {
  auto* input_value_a =
      proto_.mutable_generic_user_interface()->mutable_model()->add_values();
  input_value_a->set_identifier("value_1");
  *input_value_a->mutable_value() = SimpleValue(std::string("input-only"));
  auto* input_value_b =
      proto_.mutable_generic_user_interface()->mutable_model()->add_values();
  input_value_b->set_identifier("value_2");
  *input_value_b->mutable_value() = SimpleValue(std::string("Preset value"));

  user_model_.SetValue("value_3",
                       SimpleValue(std::string("from_previous_action")));
  proto_.add_output_model_identifiers("value_2");
  proto_.add_output_model_identifiers("value_3");

  EXPECT_CALL(mock_action_delegate_, SetGenericUi(_, _, _, _, _)).Times(0);
  EXPECT_CALL(mock_action_delegate_, ClearGenericUi()).Times(1);
  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, INVALID_ACTION),
          Property(&ProcessedActionProto::show_generic_ui_result,
                   Property(&ShowGenericUiProto::Result::model,
                            Property(&ModelProto::values, SizeIs(0))))))));

  Run();
}

TEST_F(ShowGenericUiActionTest, ClientOnlyValuesDoNotLeaveDevice) {
  auto* input_value_a =
      proto_.mutable_generic_user_interface()->mutable_model()->add_values();
  input_value_a->set_identifier("regular_value");
  *input_value_a->mutable_value() =
      SimpleValue(std::string("regular"), /* is_client_side_only = */ false);
  auto* input_value_b =
      proto_.mutable_generic_user_interface()->mutable_model()->add_values();
  input_value_b->set_identifier("sensitive_value");
  *input_value_b->mutable_value() =
      SimpleValue(std::string("secret"), /* is_client_side_only = */ true);

  proto_.add_output_model_identifiers("regular_value");
  proto_.add_output_model_identifiers("sensitive_value");

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::show_generic_ui_result,
              Property(&ShowGenericUiProto::Result::model,
                       Property(&ModelProto::values,
                                UnorderedElementsAre(
                                    SimpleModelValue(
                                        "regular_value",
                                        SimpleValue(std::string("regular"))),
                                    SimpleModelValue("sensitive_value",
                                                     ValueProto())))))))));

  Run();
}

TEST_F(ShowGenericUiActionTest, ElementPreconditionMissesIdentifier) {
  auto* element_check =
      proto_.mutable_periodic_element_checks()->add_element_checks();
  element_check->mutable_element_condition()
      ->mutable_match()
      ->add_filters()
      ->set_css_selector("selector");

  EXPECT_CALL(mock_action_delegate_, SetGenericUi(_, _, _, _, _)).Times(0);
  EXPECT_CALL(mock_action_delegate_, ClearGenericUi()).Times(1);
  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, INVALID_ACTION),
          Property(&ProcessedActionProto::show_generic_ui_result,
                   Property(&ShowGenericUiProto::Result::model,
                            Property(&ModelProto::values, SizeIs(0))))))));

  Run();
}

TEST_F(ShowGenericUiActionTest, EndActionOnNavigation) {
  ON_CALL(mock_action_delegate_, SetGenericUi(_, _, _, _, _))
      .WillByDefault(Invoke(
          [&](std::unique_ptr<GenericUserInterfaceProto> generic_ui,
              base::OnceCallback<void(const ClientStatus&)> end_action_callback,
              base::OnceCallback<void(const ClientStatus&)>
                  view_inflation_finished_callback,
              base::RepeatingCallback<void(const RequestBackendDataProto&)>
                  request_backend_data_callback,
              base::RepeatingCallback<void(const ShowAccountScreenProto&)>
                  show_account_screen_callback) {
            std::move(view_inflation_finished_callback)
                .Run(ClientStatus(ACTION_APPLIED));
          }));
  EXPECT_CALL(mock_action_delegate_, Prompt(_, _, _, _, _))
      .WillOnce([](std::unique_ptr<std::vector<UserAction>> user_actions,
                   bool disable_force_expand_sheet,
                   base::OnceCallback<void()> end_navigation_callback,
                   bool browse_mode, bool browse_mode_invisible) {
        std::move(end_navigation_callback).Run();
      });
  EXPECT_CALL(mock_action_delegate_, CleanUpAfterPrompt(Eq(true))).Times(1);
  EXPECT_CALL(
      callback_,
      Run(Pointee(
          AllOf(Property(&ProcessedActionProto::status, ACTION_APPLIED),
                Property(&ProcessedActionProto::show_generic_ui_result,
                         Property(&ShowGenericUiProto::Result::navigation_ended,
                                  true))))));

  proto_.set_end_on_navigation(true);
  Run();
}

TEST_F(ShowGenericUiActionTest, BreakingNavigationBeforeUiIsSet) {
  // End action immediately with ACTION_APPLIED after it goes into prompt.
  EXPECT_CALL(mock_action_delegate_, Prompt(_, _, _, _, _))
      .WillOnce([](std::unique_ptr<std::vector<UserAction>> user_actions,
                   bool disable_force_expand_sheet,
                   base::OnceCallback<void()> end_navigation_callback,
                   bool browse_mode, bool browse_mode_invisible) {
        std::move(end_navigation_callback).Run();
      });
  ON_CALL(mock_action_delegate_, SetGenericUi(_, _, _, _, _))
      .WillByDefault(Invoke(
          [&](std::unique_ptr<GenericUserInterfaceProto> generic_ui,
              base::OnceCallback<void(const ClientStatus&)> end_action_callback,
              base::OnceCallback<void(const ClientStatus&)>
                  view_inflation_finished_callback,
              base::RepeatingCallback<void(const RequestBackendDataProto&)>
                  request_backend_data_callback,
              base::RepeatingCallback<void(const ShowAccountScreenProto&)>
                  show_account_screen_callback) {
            std::move(view_inflation_finished_callback)
                .Run(ClientStatus(ACTION_APPLIED));
            // Also end action when UI is set. At this point, the action should
            // have terminated already.
            std::move(end_action_callback)
                .Run(ClientStatus(OTHER_ACTION_STATUS));
          }));
  EXPECT_CALL(mock_action_delegate_, CleanUpAfterPrompt(Eq(true))).Times(1);
  EXPECT_CALL(
      callback_,
      Run(Pointee(
          AllOf(Property(&ProcessedActionProto::status, ACTION_APPLIED),
                Property(&ProcessedActionProto::show_generic_ui_result,
                         Property(&ShowGenericUiProto::Result::navigation_ended,
                                  true))))));

  proto_.set_end_on_navigation(true);
  Run();
}

TEST_F(ShowGenericUiActionTest, RequestUserDataFailsOnMissingValues) {
  auto* request_user_data = proto_.mutable_request_user_data();
  auto* additional_value = request_user_data->add_additional_values();
  additional_value->set_source_identifier("client_memory");
  additional_value->set_model_identifier("target");

  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              PRECONDITION_FAILED))));
  Run();
}

TEST_F(ShowGenericUiActionTest, RequestUserData) {
  auto* request_user_data = proto_.mutable_request_user_data();
  auto* additional_value = request_user_data->add_additional_values();
  additional_value->set_source_identifier("client_memory_1");
  additional_value->set_model_identifier("target_1");
  additional_value = request_user_data->add_additional_values();
  additional_value->set_source_identifier("client_memory_2");
  additional_value->set_model_identifier("target_2");

  user_data_.SetAdditionalValue("client_memory_1",
                                SimpleValue(std::string("value_1")));
  user_data_.SetAdditionalValue("client_memory_2", SimpleValue(123));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();

  auto expected_value_1 = SimpleValue(std::string("value_1"));
  expected_value_1.set_is_client_side_only(true);
  auto expected_value_2 = SimpleValue(123);
  expected_value_2.set_is_client_side_only(true);

  EXPECT_EQ(*user_model_.GetValue("target_1"), expected_value_1);
  EXPECT_EQ(*user_model_.GetValue("target_2"), expected_value_2);
}

TEST_F(ShowGenericUiActionTest, RequestPhoneNumbersSuccess) {
  RequestBackendDataProto request;
  request.set_output_success_model_identifier("output_success");
  request.mutable_request_phone_numbers()->set_output_profiles_model_identifier(
      "output_profiles");
  GetUserDataResponseProto fake_response;
  fake_response.add_available_phone_numbers()->mutable_value()->set_value(
      "+91-9999999999");
  fake_response.add_available_phone_numbers()->mutable_value()->set_value(
      "+91-9999999990");
  EXPECT_CALL(mock_action_delegate_, RequestUserData)
      .WillOnce(RunOnceCallback<1>(true, fake_response));

  ActionProto action_proto;
  *action_proto.mutable_show_generic_ui() = proto_;
  auto action = std::make_unique<ShowGenericUiAction>(&mock_action_delegate_,
                                                      action_proto);

  action->OnRequestBackendUserData(request);

  AutofillProfileProto profile_proto1;
  profile_proto1.set_phone_number_index(0);
  auto* autofill_profile1 = user_model_.GetProfile(profile_proto1);

  AutofillProfileProto profile_proto2;
  profile_proto2.set_phone_number_index(1);
  auto* autofill_profile2 = user_model_.GetProfile(profile_proto2);

  EXPECT_NE(autofill_profile1, nullptr);
  EXPECT_NE(autofill_profile2, nullptr);

  EXPECT_EQ(autofill_profile1->GetRawInfo(
                autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER),
            u"+91-9999999999");
  EXPECT_EQ(autofill_profile2->GetRawInfo(
                autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER),
            u"+91-9999999990");

  auto phone_meta_data = user_model_.GetValue("output_profiles");
  EXPECT_TRUE(phone_meta_data.has_value());
  EXPECT_TRUE(phone_meta_data->is_client_side_only());
  EXPECT_EQ(phone_meta_data->profiles().values_size(), 2);
  EXPECT_EQ(phone_meta_data->profiles().values(0), profile_proto1);
  EXPECT_EQ(phone_meta_data->profiles().values(1), profile_proto2);
  EXPECT_EQ(user_model_.GetValue("output_success"), SimpleValue(true));

  // We already have data under the output_profiles key.
  // Requesting data again should overwrite the data.
  fake_response.add_available_phone_numbers()->mutable_value()->set_value(
      "+91-9999999991");
  EXPECT_CALL(mock_action_delegate_, RequestUserData)
      .WillOnce(RunOnceCallback<1>(true, fake_response));
  action->OnRequestBackendUserData(request);
  auto phone_meta_data_new = user_model_.GetValue("output_profiles");
  EXPECT_TRUE(phone_meta_data_new.has_value());
  EXPECT_TRUE(phone_meta_data_new->is_client_side_only());
  EXPECT_EQ(phone_meta_data_new->profiles().values_size(), 3);
  AutofillProfileProto profile_proto3;
  profile_proto3.set_phone_number_index(2);
  auto* autofill_profile3 = user_model_.GetProfile(profile_proto3);
  EXPECT_NE(autofill_profile3, nullptr);
  EXPECT_EQ(autofill_profile3->GetRawInfo(
                autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER),
            u"+91-9999999991");
}

TEST_F(ShowGenericUiActionTest, RequestPhoneNumbersNoBackendCall) {
  RequestBackendDataProto request;
  EXPECT_CALL(mock_action_delegate_, RequestUserData).Times(0);

  ActionProto action_proto;
  *action_proto.mutable_show_generic_ui() = proto_;
  auto action = std::make_unique<ShowGenericUiAction>(&mock_action_delegate_,
                                                      action_proto);

  action->OnRequestBackendUserData(request);
  request.set_output_success_model_identifier("output_success");
  action->OnRequestBackendUserData(request);
}

TEST_F(ShowGenericUiActionTest, OnShowAccountScreenSucceeds) {
  ShowAccountScreenProto proto;
  proto.set_gms_account_intent_screen_id(10004);
  EXPECT_CALL(
      mock_action_delegate_,
      ShowAccountScreen(
          Property(&ShowAccountScreenProto::gms_account_intent_screen_id,
                   10004),
          _))
      .Times(1);

  ActionProto action_proto;
  *action_proto.mutable_show_generic_ui() = proto_;
  auto action = std::make_unique<ShowGenericUiAction>(&mock_action_delegate_,
                                                      action_proto);

  action->OnShowAccountScreen(proto);
}

TEST_F(ShowGenericUiActionTest, OnShowAccountScreenFail) {
  EXPECT_CALL(mock_action_delegate_, ShowAccountScreen).Times(0);

  ActionProto action_proto;
  *action_proto.mutable_show_generic_ui() = proto_;
  auto action = std::make_unique<ShowGenericUiAction>(&mock_action_delegate_,
                                                      action_proto);

  ShowAccountScreenProto proto;
  action->OnShowAccountScreen(proto);
}

TEST_F(ShowGenericUiActionTest, OnInterruptFinished) {
  EXPECT_CALL(mock_action_delegate_, SetGenericUi(_, _, _, _, _)).Times(1);

  ActionProto action_proto;
  *action_proto.mutable_show_generic_ui() = proto_;
  auto action = std::make_unique<ShowGenericUiAction>(&mock_action_delegate_,
                                                      action_proto);

  action->OnInterruptFinished();
}

// TODO(b/161652848): Add test coverage for element checks and interrupts.

}  // namespace
}  // namespace autofill_assistant
