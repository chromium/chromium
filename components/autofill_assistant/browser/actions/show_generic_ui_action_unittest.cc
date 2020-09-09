// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/show_generic_ui_action.h"

#include "base/guid.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/mock_personal_data_manager.h"
#include "components/autofill_assistant/browser/mock_website_login_manager.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/user_model.h"
#include "components/autofill_assistant/browser/value_util.h"
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
using ::testing::IsNull;
using ::testing::Not;
using ::testing::Property;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

class ShowGenericUiActionTest : public content::RenderViewHostTestHarness {
 public:
  ShowGenericUiActionTest() {}

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    ON_CALL(mock_action_delegate_, OnSetGenericUi(_, _, _))
        .WillByDefault(
            Invoke([&](std::unique_ptr<GenericUserInterfaceProto> generic_ui,
                       base::OnceCallback<void(const ClientStatus&)>&
                           end_action_callback,
                       base::OnceCallback<void(const ClientStatus&)>&
                           view_inflation_finished_callback) {
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
    ON_CALL(mock_website_login_manager_, OnGetLoginsForUrl(_, _))
        .WillByDefault(
            RunOnceCallback<1>(std::vector<WebsiteLoginManager::Login>{
                WebsiteLoginManager::Login(GURL(kFakeUrl), kFakeUsername)}));
    content::WebContentsTester::For(web_contents())
        ->SetLastCommittedURL(GURL(kFakeUrl));
    ON_CALL(mock_action_delegate_, GetWebContents())
        .WillByDefault(Return(web_contents()));
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

  UserData user_data_;
  UserModel user_model_;
  MockPersonalDataManager mock_personal_data_manager_;
  MockWebsiteLoginManager mock_website_login_manager_;
  MockActionDelegate mock_action_delegate_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  ShowGenericUiProto proto_;
};

TEST_F(ShowGenericUiActionTest, FailedViewInflationEndsAction) {
  ON_CALL(mock_action_delegate_, OnSetGenericUi(_, _, _))
      .WillByDefault(
          Invoke([&](std::unique_ptr<GenericUserInterfaceProto> generic_ui,
                     base::OnceCallback<void(const ClientStatus&)>&
                         end_action_callback,
                     base::OnceCallback<void(const ClientStatus&)>&
                         view_inflation_finished_callback) {
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
  EXPECT_CALL(mock_action_delegate_, OnSetGenericUi(_, _, _)).Times(1);
  EXPECT_CALL(mock_action_delegate_, ClearGenericUi()).Times(1);
  EXPECT_CALL(mock_action_delegate_, CleanUpAfterPrompt()).Times(1);
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

  ON_CALL(mock_action_delegate_, OnSetGenericUi(_, _, _))
      .WillByDefault(
          Invoke([this](std::unique_ptr<GenericUserInterfaceProto> generic_ui,
                        base::OnceCallback<void(const ClientStatus&)>&
                            end_action_callback,
                        base::OnceCallback<void(const ClientStatus&)>&
                            view_inflation_finished_callback) {
            std::move(view_inflation_finished_callback)
                .Run(ClientStatus(ACTION_APPLIED));
            user_model_.SetValue("value_2", SimpleValue(std::string("change")));
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

  EXPECT_CALL(mock_action_delegate_, OnSetGenericUi(_, _, _)).Times(0);
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
  *input_value_a->mutable_value() = SimpleValue(std::string("regular"));
  auto* input_value_b =
      proto_.mutable_generic_user_interface()->mutable_model()->add_values();
  input_value_b->set_identifier("sensitive_value");
  *input_value_b->mutable_value() = SimpleValue(std::string("secret"));
  input_value_b->mutable_value()->set_is_client_side_only(true);

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

TEST_F(ShowGenericUiActionTest, RequestProfiles) {
  autofill::AutofillProfile profile_a(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetProfileInfo(
      &profile_a, "Marion", "Mitchell", "Morrison", "marion@me.xyz", "Fox",
      "123 Zoo St.", "unit 5", "Hollywood", "CA", "91601", "US", "16505678910");
  ON_CALL(mock_personal_data_manager_, IsAutofillProfileEnabled)
      .WillByDefault(Return(true));
  ON_CALL(mock_personal_data_manager_, GetProfiles)
      .WillByDefault(
          Return(std::vector<autofill::AutofillProfile*>({&profile_a})));

  proto_.mutable_request_profiles()->set_model_identifier("profiles");
  // Keep action alive by storing it in local variable.
  auto action = Run();

  EXPECT_THAT(user_model_.GetProfile(profile_a.guid())->Compare(profile_a),
              Eq(0));
  ValueProto expected_value;
  expected_value.set_is_client_side_only(true);
  expected_value.mutable_profiles()->add_values()->set_guid(profile_a.guid());
  EXPECT_EQ(*user_model_.GetValue("profiles"), expected_value);

  // Add second profile.
  autofill::AutofillProfile profile_b(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetProfileInfo(&profile_b, "John", "", "Doe",
                                 "editor@gmail.com", "", "203 Barfield Lane",
                                 "", "Mountain View", "CA", "94043", "US",
                                 "+12345678901");
  ON_CALL(mock_personal_data_manager_, GetProfiles)
      .WillByDefault(Return(
          std::vector<autofill::AutofillProfile*>({&profile_a, &profile_b})));
  mock_personal_data_manager_.NotifyPersonalDataObserver();
  EXPECT_THAT(user_model_.GetProfile(profile_a.guid())->Compare(profile_a),
              Eq(0));
  EXPECT_THAT(user_model_.GetProfile(profile_b.guid())->Compare(profile_b),
              Eq(0));
  expected_value.mutable_profiles()->add_values()->set_guid(profile_b.guid());
  EXPECT_THAT(user_model_.GetValue("profiles")->profiles().values(),
              UnorderedElementsAreArray(expected_value.profiles().values()));

  // Remove profile_a.
  ON_CALL(mock_personal_data_manager_, GetProfiles)
      .WillByDefault(
          Return(std::vector<autofill::AutofillProfile*>({&profile_b})));
  mock_personal_data_manager_.NotifyPersonalDataObserver();
  EXPECT_EQ(user_model_.GetProfile(profile_a.guid()), nullptr);
  EXPECT_THAT(user_model_.GetProfile(profile_b.guid())->Compare(profile_b),
              Eq(0));
  expected_value.Clear();
  expected_value.set_is_client_side_only(true);
  expected_value.mutable_profiles()->add_values()->set_guid(profile_b.guid());
  EXPECT_THAT(user_model_.GetValue("profiles")->profiles().values(),
              UnorderedElementsAreArray(expected_value.profiles().values()));

  // After the action has ended, updates to the PDM are ignored.
  action.reset();
  ON_CALL(mock_personal_data_manager_, GetProfiles)
      .WillByDefault(Return(
          std::vector<autofill::AutofillProfile*>({&profile_a, &profile_b})));
  mock_personal_data_manager_.NotifyPersonalDataObserver();
  EXPECT_EQ(user_model_.GetProfile(profile_a.guid()), nullptr);
  EXPECT_THAT(user_model_.GetProfile(profile_b.guid())->Compare(profile_b),
              Eq(0));
  expected_value.Clear();
  expected_value.set_is_client_side_only(true);
  expected_value.mutable_profiles()->add_values()->set_guid(profile_b.guid());
  EXPECT_THAT(user_model_.GetValue("profiles")->profiles().values(),
              UnorderedElementsAreArray(expected_value.profiles().values()));
}

TEST_F(ShowGenericUiActionTest, RequestCreditCards) {
  ON_CALL(mock_personal_data_manager_, IsAutofillCreditCardEnabled)
      .WillByDefault(Return(true));
  ON_CALL(mock_personal_data_manager_, ShouldSuggestServerCards)
      .WillByDefault(Return(true));
  ON_CALL(mock_personal_data_manager_, IsAutofillProfileEnabled)
      .WillByDefault(Return(true));

  autofill::AutofillProfile profile_a(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetProfileInfo(
      &profile_a, "Marion", "Mitchell", "Morrison", "marion@me.xyz", "Fox",
      "123 Zoo St.", "unit 5", "Hollywood", "CA", "91601", "US", "16505678910");

  autofill::CreditCard credit_card_a(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetCreditCardInfo(&credit_card_a, "Marion Mitchell",
                                    "4111 1111 1111 1111", "01", "2050",
                                    profile_a.guid());
  ON_CALL(mock_personal_data_manager_, GetCreditCards)
      .WillByDefault(
          Return(std::vector<autofill::CreditCard*>({&credit_card_a})));

  proto_.mutable_request_credit_cards()->set_model_identifier("cards");
  // Keep action alive by storing it in local variable.
  auto action = Run();

  EXPECT_THAT(
      user_model_.GetCreditCard(credit_card_a.guid())->Compare(credit_card_a),
      Eq(0));
  ValueProto expected_value;
  expected_value.set_is_client_side_only(true);
  expected_value.mutable_credit_cards()->add_values()->set_guid(
      credit_card_a.guid());
  EXPECT_EQ(*user_model_.GetValue("cards"), expected_value);

  // Add second card.
  autofill::AutofillProfile profile_b(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetProfileInfo(&profile_b, "John", "", "Doe",
                                 "editor@gmail.com", "", "203 Barfield Lane",
                                 "", "Mountain View", "CA", "94043", "US",
                                 "+12345678901");
  autofill::CreditCard credit_card_b(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetCreditCardInfo(&credit_card_b, "John Doe",
                                    "4111 1111 1111 1111", "01", "2050",
                                    profile_b.guid());
  ON_CALL(mock_personal_data_manager_, GetCreditCards)
      .WillByDefault(Return(std::vector<autofill::CreditCard*>(
          {&credit_card_a, &credit_card_b})));
  mock_personal_data_manager_.NotifyPersonalDataObserver();
  EXPECT_THAT(
      user_model_.GetCreditCard(credit_card_a.guid())->Compare(credit_card_a),
      Eq(0));
  EXPECT_THAT(
      user_model_.GetCreditCard(credit_card_b.guid())->Compare(credit_card_b),
      Eq(0));
  expected_value.mutable_credit_cards()->add_values()->set_guid(
      credit_card_b.guid());
  EXPECT_THAT(
      user_model_.GetValue("cards")->credit_cards().values(),
      UnorderedElementsAreArray(expected_value.credit_cards().values()));

  // Remove credit_card_a.
  ON_CALL(mock_personal_data_manager_, GetCreditCards)
      .WillByDefault(
          Return(std::vector<autofill::CreditCard*>({&credit_card_b})));
  mock_personal_data_manager_.NotifyPersonalDataObserver();
  EXPECT_EQ(user_model_.GetCreditCard(credit_card_a.guid()), nullptr);
  EXPECT_THAT(
      user_model_.GetCreditCard(credit_card_b.guid())->Compare(credit_card_b),
      Eq(0));
  expected_value.Clear();
  expected_value.set_is_client_side_only(true);
  expected_value.mutable_credit_cards()->add_values()->set_guid(
      credit_card_b.guid());
  EXPECT_THAT(
      user_model_.GetValue("cards")->credit_cards().values(),
      UnorderedElementsAreArray(expected_value.credit_cards().values()));

  // After the action has ended, updates to the PDM are ignored.
  action.reset();
  ON_CALL(mock_personal_data_manager_, GetCreditCards)
      .WillByDefault(Return(std::vector<autofill::CreditCard*>(
          {&credit_card_a, &credit_card_b})));
  mock_personal_data_manager_.NotifyPersonalDataObserver();
  EXPECT_EQ(user_model_.GetCreditCard(credit_card_a.guid()), nullptr);
  EXPECT_THAT(
      user_model_.GetCreditCard(credit_card_b.guid())->Compare(credit_card_b),
      Eq(0));
  expected_value.Clear();
  expected_value.set_is_client_side_only(true);
  expected_value.mutable_credit_cards()->add_values()->set_guid(
      credit_card_b.guid());
  EXPECT_THAT(
      user_model_.GetValue("cards")->credit_cards().values(),
      UnorderedElementsAreArray(expected_value.credit_cards().values()));
}

TEST_F(ShowGenericUiActionTest, RequestLogins) {
  auto* request_login_options = proto_.mutable_request_login_options();
  request_login_options->set_model_identifier("login_options");
  auto* login_option_a =
      request_login_options->add_login_options()->mutable_custom_login_option();
  login_option_a->set_label("label_a");
  login_option_a->set_sublabel("sublabel_a");
  login_option_a->set_payload("payload_a");

  auto* login_option_b = request_login_options->add_login_options()
                             ->mutable_password_manager_logins();
  login_option_b->set_sublabel("sublabel_b");
  login_option_b->set_payload("payload_b");

  Run();

  ValueProto expected_value;
  expected_value.set_is_client_side_only(true);
  *expected_value.mutable_login_options()->add_values() = *login_option_a;
  auto* expected_b = expected_value.mutable_login_options()->add_values();
  expected_b->set_label("user@example.com");
  expected_b->set_sublabel("sublabel_b");
  expected_b->set_payload("payload_b");
  EXPECT_EQ(*user_model_.GetValue("login_options"), expected_value);
}

TEST_F(ShowGenericUiActionTest, ElementPreconditionMissesIdentifier) {
  auto* element_check =
      proto_.mutable_periodic_element_checks()->add_element_checks();
  element_check->mutable_element_condition()
      ->mutable_match()
      ->add_filters()
      ->set_css_selector("selector");

  EXPECT_CALL(mock_action_delegate_, OnSetGenericUi(_, _, _)).Times(0);
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
  ON_CALL(mock_action_delegate_, OnSetGenericUi(_, _, _))
      .WillByDefault(
          Invoke([&](std::unique_ptr<GenericUserInterfaceProto> generic_ui,
                     base::OnceCallback<void(const ClientStatus&)>&
                         end_action_callback,
                     base::OnceCallback<void(const ClientStatus&)>&
                         view_inflation_finished_callback) {
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
  EXPECT_CALL(mock_action_delegate_, CleanUpAfterPrompt()).Times(1);
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
  ON_CALL(mock_action_delegate_, OnSetGenericUi(_, _, _))
      .WillByDefault(
          Invoke([&](std::unique_ptr<GenericUserInterfaceProto> generic_ui,
                     base::OnceCallback<void(const ClientStatus&)>&
                         end_action_callback,
                     base::OnceCallback<void(const ClientStatus&)>&
                         view_inflation_finished_callback) {
            std::move(view_inflation_finished_callback)
                .Run(ClientStatus(ACTION_APPLIED));
            // Also end action when UI is set. At this point, the action should
            // have terminated already.
            std::move(end_action_callback)
                .Run(ClientStatus(OTHER_ACTION_STATUS));
          }));
  EXPECT_CALL(mock_action_delegate_, CleanUpAfterPrompt()).Times(1);
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

  user_data_.additional_values_["client_memory_1"] =
      SimpleValue(std::string("value_1"));
  user_data_.additional_values_["client_memory_2"] = SimpleValue(123);

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

// TODO(b/161652848): Add test coverage for element checks and interrupts.

}  // namespace
}  // namespace autofill_assistant
