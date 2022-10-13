// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/basic_interactions.h"
#include "base/guid.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/icu_test_util.h"
#include "base/test/mock_callback.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill_assistant/browser/actions/action_test_utils.h"
#include "components/autofill_assistant/browser/fake_script_executor_ui_delegate.h"
#include "components/autofill_assistant/browser/generic_ui.pb.h"
#include "components/autofill_assistant/browser/mock_execution_delegate.h"
#include "components/autofill_assistant/browser/test_util.h"
#include "components/autofill_assistant/browser/user_action.h"
#include "components/autofill_assistant/browser/user_model.h"
#include "components/autofill_assistant/browser/value_util.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Property;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StrEq;
namespace {
DateProto CreateDateProto(int year, int month, int day) {
  DateProto proto;
  proto.set_year(year);
  proto.set_month(month);
  proto.set_day(day);
  return proto;
}
}  // namespace

class BasicInteractionsTest : public testing::Test {
 public:
  void SetUp() override {
    ON_CALL(execution_delegate_, GetClientSettings)
        .WillByDefault(ReturnRef(settings_));
    ON_CALL(execution_delegate_, GetUserModel)
        .WillByDefault(Return(&user_model_));
  }

 protected:
  BasicInteractionsTest() {}
  ~BasicInteractionsTest() override {}

  FakeScriptExecutorUiDelegate ui_delegate_;
  MockExecutionDelegate execution_delegate_;
  ClientSettings settings_;
  UserModel user_model_;
  BasicInteractions basic_interactions_{&ui_delegate_, &execution_delegate_};
};

TEST_F(BasicInteractionsTest, GetWeakPtr) {
  EXPECT_EQ(basic_interactions_.GetWeakPtr().get(), &basic_interactions_);
}

TEST_F(BasicInteractionsTest, SetValue) {
  SetModelValueProto proto;
  *proto.mutable_value()->mutable_value() = SimpleValue(std::string("success"));

  // Model identifier not set.
  EXPECT_FALSE(basic_interactions_.SetValue(proto));

  proto.set_model_identifier("output");
  EXPECT_TRUE(basic_interactions_.SetValue(proto));
  EXPECT_EQ(user_model_.GetValue("output"), proto.value().value());
}

TEST_F(BasicInteractionsTest, SetUserActions) {
  SetUserActionsProto proto;
  // UserActions not set
  EXPECT_FALSE(basic_interactions_.SetUserActions(proto));

  // UserActions value not set
  proto.mutable_user_actions();
  EXPECT_FALSE(basic_interactions_.SetUserActions(proto));

  // UserActions value doesn't have any actions
  ValueProto done_user_actions_value;
  *proto.mutable_user_actions()->mutable_value() = done_user_actions_value;
  EXPECT_FALSE(basic_interactions_.SetUserActions(proto));

  // Successfully set user action
  UserActionProto done_user_action;
  done_user_action.set_identifier("done_identifier");
  done_user_action.mutable_chip()->set_type(ChipType::HIGHLIGHTED_ACTION);
  done_user_action.mutable_chip()->set_text("Done");
  done_user_action.set_enabled(true);
  *done_user_actions_value.mutable_user_actions()->add_values() =
      done_user_action;
  *proto.mutable_user_actions()->mutable_value() = done_user_actions_value;
  EXPECT_TRUE(basic_interactions_.SetUserActions(proto));

  EXPECT_THAT(*ui_delegate_.GetUserActions(),
              ElementsAre(AllOf(
                  Property(&UserAction::identifier, StrEq("done_identifier")),
                  Property(&UserAction::has_chip, Eq(true)),
                  Property(&UserAction::enabled, Eq(true)))));

  // Successfully replace user actions
  ValueProto cancel_user_actions_value;
  UserActionProto cancel_user_action;
  cancel_user_action.set_identifier("cancel_identifier");
  cancel_user_action.mutable_chip()->set_type(ChipType::CANCEL_ACTION);
  cancel_user_action.mutable_chip()->set_text("Cancel");
  cancel_user_action.set_enabled(false);
  *cancel_user_actions_value.mutable_user_actions()->add_values() =
      cancel_user_action;
  *proto.mutable_user_actions()->mutable_value() = cancel_user_actions_value;
  EXPECT_TRUE(basic_interactions_.SetUserActions(proto));

  EXPECT_THAT(*ui_delegate_.GetUserActions(),
              ElementsAre(AllOf(
                  Property(&UserAction::identifier, StrEq("cancel_identifier")),
                  Property(&UserAction::has_chip, Eq(true)),
                  Property(&UserAction::enabled, Eq(false)))));
}

TEST_F(BasicInteractionsTest, ComputeValueNoKindOrValue) {
  // Kind not set
  ComputeValueProto proto_no_kind_or_value;
  proto_no_kind_or_value.set_result_model_identifier("output");
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto_no_kind_or_value));

  // Kind set but no value was added
  ComputeValueProto proto_boolean_and;
  proto_boolean_and.set_result_model_identifier("output");
  proto_boolean_and.mutable_boolean_and();
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto_boolean_and));

  ComputeValueProto proto_boolean_or;
  proto_boolean_or.set_result_model_identifier("output");
  proto_boolean_or.mutable_boolean_or();
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto_boolean_or));

  ComputeValueProto proto_boolean_not;
  proto_boolean_not.set_result_model_identifier("output");
  proto_boolean_not.mutable_boolean_not();
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto_boolean_not));

  ComputeValueProto proto_to_string;
  proto_to_string.set_result_model_identifier("output");
  proto_to_string.mutable_to_string();
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto_to_string));

  ComputeValueProto proto_comparison;
  proto_comparison.set_result_model_identifier("output");
  proto_comparison.mutable_comparison();
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto_comparison));

  ComputeValueProto proto_integer_sum;
  proto_integer_sum.set_result_model_identifier("output");
  proto_integer_sum.mutable_integer_sum();
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto_integer_sum));

  ComputeValueProto proto_credit_card_response;
  proto_credit_card_response.set_result_model_identifier("output");
  proto_credit_card_response.mutable_create_credit_card_response();
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto_credit_card_response));

  ComputeValueProto proto_login_options_response;
  proto_login_options_response.set_result_model_identifier("output");
  proto_login_options_response.mutable_create_login_option_response();
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto_login_options_response));

  ComputeValueProto proto_string_empty;
  proto_string_empty.set_result_model_identifier("output");
  proto_string_empty.mutable_string_empty();
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto_string_empty));
}

TEST_F(BasicInteractionsTest, ComputeValueBooleanAnd) {
  ComputeValueProto proto;
  proto.mutable_boolean_and()->add_values()->set_model_identifier("value_1");
  *proto.mutable_boolean_and()->add_values()->mutable_value() =
      SimpleValue(true);
  proto.mutable_boolean_and()->add_values()->set_model_identifier("value_3");

  user_model_.SetValue("value_1", SimpleValue(true));
  user_model_.SetValue("value_3", SimpleValue(false));

  // Result model identifier not set.
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  proto.set_result_model_identifier("output");
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("output"), SimpleValue(false));

  user_model_.SetValue("value_3", SimpleValue(true));
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("output"), SimpleValue(true));

  // is_client_side_only flag is sticky.
  user_model_.SetValue("value_3",
                       SimpleValue(true, /* is_client_side_only = */ true));
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_TRUE(user_model_.GetValue("output")->is_client_side_only());

  // Mixing types is not allowed.
  user_model_.SetValue("value_1", ValueProto());
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  // All input values must have size 1.
  ValueProto multi_bool;
  multi_bool.mutable_booleans()->add_values(true);
  multi_bool.mutable_booleans()->add_values(false);
  user_model_.SetValue("value_3", multi_bool);
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));
}

TEST_F(BasicInteractionsTest, ComputeValueBooleanOr) {
  ComputeValueProto proto;
  proto.mutable_boolean_or()->add_values()->set_model_identifier("value_1");
  proto.mutable_boolean_or()->add_values()->set_model_identifier("value_2");
  *proto.mutable_boolean_or()->add_values()->mutable_value() =
      SimpleValue(false);

  user_model_.SetValue("value_1", SimpleValue(false));
  user_model_.SetValue("value_2", SimpleValue(false));

  // Result model identifier not set.
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  proto.set_result_model_identifier("output");
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("output"), SimpleValue(false));

  user_model_.SetValue("value_2", SimpleValue(true));
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("output"), SimpleValue(true));

  // is_client_side_only flag is sticky.
  user_model_.SetValue("value_2",
                       SimpleValue(true, /* is_client_side_only = */ true));
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_TRUE(user_model_.GetValue("output")->is_client_side_only());

  // Mixing types is not allowed.
  user_model_.SetValue("value_1", ValueProto());
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  // All input values must have size 1.
  ValueProto multi_bool;
  multi_bool.mutable_booleans()->add_values(true);
  multi_bool.mutable_booleans()->add_values(false);
  user_model_.SetValue("value_1", multi_bool);
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));
}

TEST_F(BasicInteractionsTest, ComputeValueBooleanNot) {
  ComputeValueProto proto;
  proto.mutable_boolean_not()->mutable_value()->set_model_identifier("value");
  user_model_.SetValue("value", SimpleValue(false));

  // Result model identifier not set.
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  proto.set_result_model_identifier("value");
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("value"), SimpleValue(true));

  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("value"), SimpleValue(false));

  // is_client_side_only flag is sticky.
  user_model_.SetValue("value",
                       SimpleValue(false, /* is_client_side_only = */ true));
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_TRUE(user_model_.GetValue("value")->is_client_side_only());

  // Not a boolean.
  user_model_.SetValue("value", ValueProto());
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  // Size != 1.
  ValueProto multi_bool;
  multi_bool.mutable_booleans()->add_values(true);
  multi_bool.mutable_booleans()->add_values(false);
  user_model_.SetValue("value", multi_bool);
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));
}

TEST_F(BasicInteractionsTest, ComputeValueToString) {
  ComputeValueProto proto;
  proto.mutable_to_string()->mutable_value()->set_model_identifier("value");
  user_model_.SetValue("value", SimpleValue(1,
                                            /* is_client_side_only = */ true));

  // Result model identifier not set.
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  // Integer
  proto.set_result_model_identifier("output");
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(*user_model_.GetValue("output"),
            SimpleValue(std::string("1"), /* is_client_side_only = */ true));

  // Boolean
  user_model_.SetValue("value",
                       SimpleValue(true, /* is_client_side_only = */ true));
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(*user_model_.GetValue("output"),
            SimpleValue(std::string("true"), /* is_client_side_only = */ true));

  // String
  user_model_.SetValue("value", SimpleValue(std::string("test asd"),
                                            /* is_client_side_only = */ true));
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(
      *user_model_.GetValue("output"),
      SimpleValue(std::string("test asd"), /* is_client_side_only = */ true));

  // Date without format fails.
  user_model_.SetValue("value", SimpleValue(CreateDateProto(2020, 10, 23),
                                            /* is_client_side_only = */ true));
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  // Date with format succeeds.
  {
    base::test::ScopedRestoreICUDefaultLocale locale(std::string("en_US"));
    proto.mutable_to_string()->mutable_date_format()->set_date_format(
        "EEE, MMM d y");
    EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
    EXPECT_EQ(*user_model_.GetValue("output"),
              SimpleValue(std::string("Fri, Oct 23, 2020"),
                          /* is_client_side_only = */ true));
  }

  // Date in german locale.
  {
    base::test::ScopedRestoreICUDefaultLocale locale(std::string("de_DE"));
    EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
    EXPECT_EQ(*user_model_.GetValue("output"),
              SimpleValue(std::string("Fr., 23. Okt. 2020"),
                          /* is_client_side_only = */ true));
  }

  // Credit cards
  autofill::CreditCard credit_card_a(base::GenerateGUID(),
                                     "https://www.example.com");
  autofill::test::SetCreditCardInfo(&credit_card_a, "Marion Mitchell",
                                    "4111 1111 1111 1111", "01", "2050", "");
  autofill::CreditCard credit_card_b(base::GenerateGUID(),
                                     "https://www.example.com");
  autofill::test::SetCreditCardInfo(&credit_card_b, "John Doe",
                                    "4111 1111 1111 2222", "02", "2051", "");
  auto credit_cards =
      std::make_unique<std::vector<std::unique_ptr<autofill::CreditCard>>>();
  credit_cards->emplace_back(
      std::make_unique<autofill::CreditCard>(credit_card_a));
  credit_cards->emplace_back(
      std::make_unique<autofill::CreditCard>(credit_card_b));
  user_model_.SetAutofillCreditCards(std::move(credit_cards));
  ValueProto credit_cards_value;
  credit_cards_value.mutable_credit_cards()->add_values()->set_guid(
      credit_card_a.guid());
  credit_cards_value.mutable_credit_cards()->add_values()->set_guid(
      credit_card_b.guid());
  user_model_.SetValue("value", credit_cards_value);
  // Formatting credit cards fails if value_expression or locale are not set.
  proto.mutable_to_string()->mutable_autofill_format()->set_locale("en-US");
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));
  // {name} {network} **** {last-4-digits} ({month/year})
  *proto.mutable_to_string()
       ->mutable_autofill_format()
       ->mutable_value_expression() = test_util::ValueExpressionBuilder()
                                          .addChunk(51)
                                          .addChunk(". ")
                                          .addChunk(-5)
                                          .addChunk(" **** ")
                                          .addChunk(-4)
                                          .addChunk(" (")
                                          .addChunk(53)
                                          .addChunk("/")
                                          .addChunk(54)
                                          .addChunk(")")
                                          .toProto();
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  ValueProto expected_result;
  expected_result.mutable_strings()->add_values(
      "Marion Mitchell. Visa **** 1111 (01/50)");
  expected_result.mutable_strings()->add_values(
      "John Doe. Visa **** 2222 (02/51)");
  EXPECT_EQ(*user_model_.GetValue("output"), expected_result);

  // Profiles
  autofill::AutofillProfile profile_a(base::GenerateGUID(),
                                      "https://www.example.com");
  autofill::test::SetProfileInfo(
      &profile_a, "Marion", "Mitchell", "Morrison", "marion@me.xyz", "Fox",
      "123 Zoo St.", "unit 5", "Hollywood", "CA", "91601", "US", "16505678910");
  autofill::AutofillProfile profile_b(base::GenerateGUID(),
                                      "https://www.example.com");
  autofill::test::SetProfileInfo(&profile_b, "John", "", "Doe",
                                 "editor@gmail.com", "", "203 Barfield Lane",
                                 "apt A", "Mountain View", "CA", "94043", "US",
                                 "+12345678901");
  auto profiles = std::make_unique<
      std::vector<std::unique_ptr<autofill::AutofillProfile>>>();
  profiles->emplace_back(
      std::make_unique<autofill::AutofillProfile>(profile_a));
  profiles->emplace_back(
      std::make_unique<autofill::AutofillProfile>(profile_b));
  user_model_.SetAutofillProfiles(std::move(profiles));
  ValueProto profiles_value;
  profiles_value.mutable_profiles()->add_values()->set_guid(profile_a.guid());
  profiles_value.mutable_profiles()->add_values()->set_guid(profile_b.guid());
  user_model_.SetValue("value", profiles_value);
  // Formatting profiles fails if value_expression is empty.
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));
  // {name_full}, {address_line_1} {address_line_2} {zip code} {city} {country}
  *proto.mutable_to_string()
       ->mutable_autofill_format()
       ->mutable_value_expression() = test_util::ValueExpressionBuilder()
                                          .addChunk(7)
                                          .addChunk(" ")
                                          .addChunk(30)
                                          .addChunk(" ")
                                          .addChunk(31)
                                          .addChunk(" ")
                                          .addChunk(35)
                                          .addChunk(" ")
                                          .addChunk(33)
                                          .addChunk(" ")
                                          .addChunk(36)
                                          .toProto();
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  expected_result.Clear();
  expected_result.mutable_strings()->add_values(
      "Marion Mitchell Morrison 123 Zoo St. unit 5 91601 Hollywood United "
      "States");
  expected_result.mutable_strings()->add_values(
      "John Doe 203 Barfield Lane apt A 94043 Mountain View United States");
  EXPECT_EQ(*user_model_.GetValue("output"), expected_result);

  // Different locale.
  proto.mutable_to_string()->mutable_autofill_format()->set_locale("de-DE");
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  expected_result.Clear();
  expected_result.mutable_strings()->add_values(
      "Marion Mitchell Morrison 123 Zoo St. unit 5 91601 Hollywood Vereinigte "
      "Staaten");
  expected_result.mutable_strings()->add_values(
      "John Doe 203 Barfield Lane apt A 94043 Mountain View Vereinigte "
      "Staaten");
  EXPECT_EQ(*user_model_.GetValue("output"), expected_result);

  // Empty value fails.
  user_model_.SetValue("value", ValueProto());
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  // Multi value succeeds.
  ValueProto multi_value;
  multi_value.mutable_booleans()->add_values(true);
  multi_value.mutable_booleans()->add_values(false);
  user_model_.SetValue("value", multi_value);
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  expected_result.Clear();
  expected_result.mutable_strings()->add_values("true");
  expected_result.mutable_strings()->add_values("false");
  EXPECT_EQ(*user_model_.GetValue("output"), expected_result);

  multi_value.Clear();
  multi_value.mutable_ints()->add_values(5);
  multi_value.mutable_ints()->add_values(13);
  user_model_.SetValue("value", multi_value);
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  expected_result.Clear();
  expected_result.mutable_strings()->add_values("5");
  expected_result.mutable_strings()->add_values("13");
  EXPECT_EQ(*user_model_.GetValue("output"), expected_result);

  // is_client_side_only flag is sticky.
  multi_value.set_is_client_side_only(true);
  user_model_.SetValue("value", multi_value);
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_TRUE(user_model_.GetValue("output")->is_client_side_only());
}

TEST_F(BasicInteractionsTest, ComputeValueIntegerSum) {
  ComputeValueProto proto;
  proto.mutable_integer_sum();
  user_model_.SetValue("value_a", SimpleValue(1));
  user_model_.SetValue("value_b", SimpleValue(2));

  // Missing fields.
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));
  proto.mutable_integer_sum()->add_values()->set_model_identifier("value_a");
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));
  proto.mutable_integer_sum()->add_values()->set_model_identifier("value_b");
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));
  proto.set_result_model_identifier("result");
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));

  // Size != 1.
  ValueProto value;
  value.mutable_ints()->add_values(1);
  value.mutable_ints()->add_values(2);
  user_model_.SetValue("value_a", value);
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  // Check results.
  user_model_.SetValue("value_a", SimpleValue(1));
  user_model_.SetValue("value_b", SimpleValue(2));
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("result"), SimpleValue(3));

  user_model_.SetValue("value_a", SimpleValue(-1));
  user_model_.SetValue("value_b", SimpleValue(5));
  *proto.mutable_integer_sum()->add_values()->mutable_value() = SimpleValue(3);
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("result"), SimpleValue(7));

  // is_client_side_only flag is sticky.
  user_model_.SetValue("value_b",
                       SimpleValue(5, /* is_client_side_only = */ true));
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_TRUE(user_model_.GetValue("result")->is_client_side_only());
}

TEST_F(BasicInteractionsTest, ComputeValueArrayLength) {
  ComputeValueProto proto;
  proto.set_result_model_identifier("result");
  proto.mutable_array_length();

  // Missing fields.
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  // Add missing field, but |value| doesn't exist in |user_model_|
  proto.mutable_array_length()->mutable_value()->set_model_identifier("value");
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  // Empty value
  user_model_.SetValue("value", ValueProto());
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("result"), SimpleValue(0));

  // Empty array
  ValueProto value;
  value.mutable_ints();
  user_model_.SetValue("value", value);
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("result"), SimpleValue(0));

  // Non-Empty array
  value.mutable_ints()->add_values(5);
  value.mutable_ints()->add_values(6);
  user_model_.SetValue("value", value);
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("result"), SimpleValue(2));
}

TEST_F(BasicInteractionsTest, RequestBackendDataWithoutCallbackFails) {
  EndActionProto proto;
  EXPECT_FALSE(
      basic_interactions_.RequestBackendData(RequestBackendDataProto{}));
}

TEST_F(BasicInteractionsTest, RequestBackendDataWithCallbackSucceeds) {
  base::MockCallback<
      base::RepeatingCallback<void(const RequestBackendDataProto&)>>
      callback;
  basic_interactions_.SetRequestBackendDataCallback(callback.Get());
  RequestBackendDataProto request;
  request.set_output_success_model_identifier("output_success");
  EXPECT_CALL(
      callback,
      Run(Property(&RequestBackendDataProto::output_success_model_identifier,
                   "output_success")));
  EXPECT_TRUE(basic_interactions_.RequestBackendData(request));
}

TEST_F(BasicInteractionsTest, RequestBackendDataFailOnClearedCallbacks) {
  base::MockCallback<
      base::RepeatingCallback<void(const RequestBackendDataProto&)>>
      callback;
  basic_interactions_.SetRequestBackendDataCallback(callback.Get());
  RequestBackendDataProto request;
  request.set_output_success_model_identifier("output_success");
  EXPECT_CALL(
      callback,
      Run(Property(&RequestBackendDataProto::output_success_model_identifier,
                   "output_success")));
  EXPECT_TRUE(basic_interactions_.RequestBackendData(request));

  EXPECT_CALL(callback, Run(request)).Times(0);
  basic_interactions_.ClearCallbacks();
  EXPECT_FALSE(basic_interactions_.RequestBackendData(request));
}

TEST_F(BasicInteractionsTest, ShowAccountScreenFails) {
  EndActionProto proto;
  EXPECT_FALSE(basic_interactions_.ShowAccountScreen(ShowAccountScreenProto{}));
}

TEST_F(BasicInteractionsTest, ShowAccountScreenSucceeds) {
  base::MockCallback<
      base::RepeatingCallback<void(const ShowAccountScreenProto&)>>
      callback;
  basic_interactions_.SetShowAccountScreenCallback(callback.Get());
  ShowAccountScreenProto proto;
  proto.set_gms_account_intent_screen_id(10004);
  EXPECT_CALL(callback, Run(proto));
  EXPECT_TRUE(basic_interactions_.ShowAccountScreen(proto));
}

TEST_F(BasicInteractionsTest, ShowAccountScreenFailOnClearedCallbacks) {
  base::MockCallback<
      base::RepeatingCallback<void(const ShowAccountScreenProto&)>>
      callback;
  basic_interactions_.SetShowAccountScreenCallback(callback.Get());
  ShowAccountScreenProto proto;
  proto.set_gms_account_intent_screen_id(10004);
  EXPECT_CALL(callback, Run(proto));
  EXPECT_TRUE(basic_interactions_.ShowAccountScreen(proto));

  EXPECT_CALL(callback, Run(proto)).Times(0);
  basic_interactions_.ClearCallbacks();
  EXPECT_FALSE(basic_interactions_.ShowAccountScreen(proto));
}

TEST_F(BasicInteractionsTest, EndActionWithoutCallbackFails) {
  EndActionProto proto;
  EXPECT_FALSE(basic_interactions_.EndAction(ClientStatus(INVALID_ACTION)));
}

TEST_F(BasicInteractionsTest, EndActionWithCallbackSucceeds) {
  base::MockCallback<base::OnceCallback<void(const ClientStatus&)>> callback;
  basic_interactions_.SetEndActionCallback(callback.Get());

  EXPECT_CALL(callback,
              Run(Property(&ClientStatus::proto_status, ACTION_APPLIED)));
  EXPECT_TRUE(basic_interactions_.EndAction(ClientStatus(ACTION_APPLIED)));
}

TEST_F(BasicInteractionsTest, NotifyViewInflationFinishedRunsCallback) {
  base::MockCallback<base::OnceCallback<void(const ClientStatus&)>> callback;
  basic_interactions_.SetViewInflationFinishedCallback(callback.Get());

  EXPECT_CALL(callback,
              Run(Property(&ClientStatus::proto_status, ACTION_APPLIED)));
  EXPECT_TRUE(basic_interactions_.NotifyViewInflationFinished(
      ClientStatus(ACTION_APPLIED)));
}

TEST_F(BasicInteractionsTest, NotifyPersistentViewInflationFinishedCallback) {
  base::MockCallback<base::OnceCallback<void(const ClientStatus&)>> callback;

  // |persistent_view_inflation_finished_callback_| not set
  EXPECT_FALSE(basic_interactions_.NotifyPersistentViewInflationFinished(
      ClientStatus(ACTION_APPLIED)));

  basic_interactions_.SetPersistentViewInflationFinishedCallback(
      callback.Get());
  EXPECT_CALL(callback,
              Run(Property(&ClientStatus::proto_status, ACTION_APPLIED)));

  EXPECT_TRUE(basic_interactions_.NotifyPersistentViewInflationFinished(
      ClientStatus(ACTION_APPLIED)));

  // |persistent_view_inflation_finished_callback_| cleared
  basic_interactions_.SetPersistentViewInflationFinishedCallback(
      callback.Get());
  basic_interactions_.ClearPersistentUiCallbacks();
  EXPECT_FALSE(basic_interactions_.NotifyPersistentViewInflationFinished(
      ClientStatus(ACTION_APPLIED)));
}

TEST_F(BasicInteractionsTest, EndActionResetsViewInflationCallback) {
  base::MockCallback<base::OnceCallback<void(const ClientStatus&)>>
      view_inflation_finished_callback;
  base::MockCallback<base::OnceCallback<void(const ClientStatus&)>>
      end_action_callback;
  basic_interactions_.SetViewInflationFinishedCallback(
      view_inflation_finished_callback.Get());
  basic_interactions_.SetEndActionCallback(end_action_callback.Get());

  EXPECT_CALL(end_action_callback,
              Run(Property(&ClientStatus::proto_status, INVALID_ACTION)));
  EXPECT_CALL(view_inflation_finished_callback, Run(_)).Times(0);
  EXPECT_TRUE(basic_interactions_.EndAction(ClientStatus(INVALID_ACTION)));
  EXPECT_FALSE(basic_interactions_.NotifyViewInflationFinished(
      ClientStatus(ACTION_APPLIED)));
}

TEST_F(BasicInteractionsTest, ComputeValueCompare) {
  user_model_.SetValue("value_a", ValueProto());
  user_model_.SetValue("value_b", ValueProto());

  ComputeValueProto proto;
  proto.mutable_comparison();

  // Fields are missing.
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));
  proto.mutable_comparison()->mutable_value_a()->set_model_identifier(
      "value_a");
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));
  proto.mutable_comparison()->mutable_value_b()->set_model_identifier(
      "value_b");
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));
  proto.set_result_model_identifier("result");
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  // EQUAL and NOT_EQUAL supported for all value types.
  ValueComparisonProto::Mode support_all_types_modes[] = {
      ValueComparisonProto::EQUAL, ValueComparisonProto::NOT_EQUAL};
  for (const auto mode : support_all_types_modes) {
    proto.mutable_comparison()->set_mode(mode);
    EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
    user_model_.SetValue("value_a", SimpleValue(std::string("string_a")));
    user_model_.SetValue("value_b", SimpleValue(std::string("string_b")));
    EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
    user_model_.SetValue("value_a", SimpleValue(true));
    user_model_.SetValue("value_b", SimpleValue(false));
    EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
    user_model_.SetValue("value_a", SimpleValue(1));
    user_model_.SetValue("value_b", SimpleValue(2));
    EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
    user_model_.SetValue("value_a", SimpleValue(CreateDateProto(2020, 8, 7)));
    user_model_.SetValue("value_b", SimpleValue(CreateDateProto(2020, 11, 5)));
    EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
    ValueProto user_actions_value;
    user_actions_value.mutable_user_actions();
    user_model_.SetValue("value_a", user_actions_value);
    user_model_.SetValue("value_b", user_actions_value);
    EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  }

  // Some types are not supported for modes other than EQUAL and NOT_EQUAL.
  proto.mutable_comparison()->set_mode(ValueComparisonProto::LESS);
  user_model_.SetValue("value_a", ValueProto());
  user_model_.SetValue("value_b", ValueProto());
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));
  user_model_.SetValue("value_a", SimpleValue(true));
  user_model_.SetValue("value_b", SimpleValue(false));
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));
  ValueProto user_actions_value;
  user_actions_value.mutable_user_actions();
  user_model_.SetValue("value_a", user_actions_value);
  user_model_.SetValue("value_b", user_actions_value);
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  // Different types fail for mode != EQUAL.
  user_model_.SetValue("value_a", SimpleValue(1));
  user_model_.SetValue("value_b", SimpleValue(std::string("a")));
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  // Size != 1 fails for mode != EQUAL.
  ValueProto multi_value;
  multi_value.mutable_booleans()->add_values(true);
  multi_value.mutable_booleans()->add_values(false);
  user_model_.SetValue("value_a", multi_value);
  user_model_.SetValue("value_b", multi_value);
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  // Different types succeed for mode == NOT_EQUAL.
  proto.mutable_comparison()->set_mode(ValueComparisonProto::NOT_EQUAL);
  user_model_.SetValue("value_a", SimpleValue(1));
  user_model_.SetValue("value_b", SimpleValue(std::string("a")));
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));

  // Check comparison results.
  proto.mutable_comparison()->set_mode(ValueComparisonProto::LESS);
  user_model_.SetValue("value_a", SimpleValue(1));
  user_model_.SetValue("value_b", SimpleValue(2));
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("result"), SimpleValue(true));

  user_model_.SetValue("value_a", SimpleValue(1));
  user_model_.SetValue("value_b", SimpleValue(1));
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("result"), SimpleValue(false));

  proto.mutable_comparison()->set_mode(ValueComparisonProto::LESS_OR_EQUAL);
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("result"), SimpleValue(true));

  proto.mutable_comparison()->set_mode(ValueComparisonProto::GREATER_OR_EQUAL);
  user_model_.SetValue("value_a", SimpleValue(1));
  user_model_.SetValue("value_b", SimpleValue(2));
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("result"), SimpleValue(false));

  user_model_.SetValue("value_a", SimpleValue(1));
  user_model_.SetValue("value_b", SimpleValue(1));
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("result"), SimpleValue(true));

  proto.mutable_comparison()->set_mode(ValueComparisonProto::GREATER);
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("result"), SimpleValue(false));

  user_model_.SetValue("value_a", SimpleValue(2));
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("result"), SimpleValue(true));

  proto.mutable_comparison()->set_mode(ValueComparisonProto::GREATER_OR_EQUAL);
  user_model_.SetValue("value_a",
                       SimpleValue(1, /* is_client_side_only = */ false));
  user_model_.SetValue("value_b",
                       SimpleValue(1, /* is_client_side_only = */ true));
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(*user_model_.GetValue("result"), SimpleValue(true, true));

  proto.mutable_comparison()->set_mode(ValueComparisonProto::NOT_EQUAL);
  user_model_.SetValue("value_a", SimpleValue(1));
  user_model_.SetValue("value_b", SimpleValue(std::string("a")));
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("result"), SimpleValue(true));

  user_model_.SetValue("value_a", SimpleValue(1));
  user_model_.SetValue("value_b", SimpleValue(2));
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("result"), SimpleValue(true));

  user_model_.SetValue("value_b", SimpleValue(1));
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("result"), SimpleValue(false));
}

TEST_F(BasicInteractionsTest, ComputeValueCreateCreditCardResponse) {
  ComputeValueProto proto;
  proto.mutable_create_credit_card_response();

  // Missing fields.
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));
  proto.mutable_create_credit_card_response()
      ->mutable_value()
      ->set_model_identifier("value");
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));
  proto.set_result_model_identifier("result");
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  autofill::CreditCard credit_card(base::GenerateGUID(),
                                   "https://www.example.com");
  autofill::test::SetCreditCardInfo(&credit_card, "Marion Mitchell",
                                    "4111 1111 1111 1111", "01", "2050", "");
  auto credit_cards =
      std::make_unique<std::vector<std::unique_ptr<autofill::CreditCard>>>();
  credit_cards->emplace_back(
      std::make_unique<autofill::CreditCard>(credit_card));
  user_model_.SetAutofillCreditCards(std::move(credit_cards));

  ValueProto value_wrong_guid;
  value_wrong_guid.mutable_credit_cards()->add_values()->set_guid("wrong");
  user_model_.SetValue("value", value_wrong_guid);
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  ValueProto value_correct_guid;
  value_correct_guid.mutable_credit_cards()->add_values()->set_guid(
      credit_card.guid());
  user_model_.SetValue("value", value_correct_guid);
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  ValueProto expected_response_value;
  expected_response_value.mutable_credit_card_response()->set_network("visa");
  expected_response_value.set_is_client_side_only(false);
  EXPECT_EQ(user_model_.GetValue("result"), expected_response_value);

  // CreateCreditCardResponse is allowed to extract the card network from
  // client-only values.
  value_correct_guid.set_is_client_side_only(true);
  user_model_.SetValue("value", value_correct_guid);
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("result"), expected_response_value);

  // Size != 1.
  ValueProto value;
  value.mutable_credit_cards()->add_values()->set_guid(credit_card.guid());
  value.mutable_credit_cards()->add_values()->set_guid(credit_card.guid());
  user_model_.SetValue("value", value);
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));
}

TEST_F(BasicInteractionsTest, ComputeValueCreateLoginOptionResponse) {
  ComputeValueProto proto;
  proto.mutable_create_login_option_response();

  // Missing fields.
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));
  proto.mutable_create_login_option_response()
      ->mutable_value()
      ->set_model_identifier("value");
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));
  proto.set_result_model_identifier("result");
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  ValueProto value;
  value.mutable_login_options()->add_values()->set_payload("payload");
  value.set_is_client_side_only(true);
  user_model_.SetValue("value", value);
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));

  // LoginOptionResponseProto is allowed to extract the payload from
  // client-only values.
  ValueProto expected_response_value;
  expected_response_value.set_server_payload("payload");
  expected_response_value.set_is_client_side_only(false);
  EXPECT_EQ(user_model_.GetValue("result"), expected_response_value);
}

TEST_F(BasicInteractionsTest, ComputeValueCreateLoginOptionResponseWithTag) {
  ComputeValueProto proto;
  proto.mutable_create_login_option_response();

  // Missing fields.
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));
  proto.mutable_create_login_option_response()
      ->mutable_value()
      ->set_model_identifier("value");
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));
  proto.set_result_model_identifier("result");
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  ValueProto value;
  value.mutable_login_options()->add_values()->set_tag("tag");
  value.set_is_client_side_only(true);
  user_model_.SetValue("value", value);
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));

  // LoginOptionResponseProto is allowed to extract the payload from
  // client-only values.
  ValueProto expected_response_value;
  expected_response_value.mutable_strings()->add_values("tag");
  expected_response_value.set_is_client_side_only(false);
  EXPECT_EQ(user_model_.GetValue("result"), expected_response_value);
}

TEST_F(BasicInteractionsTest, ComputeStringEmpty) {
  ComputeValueProto proto;
  proto.set_result_model_identifier("result");
  proto.mutable_string_empty()->mutable_value()->set_model_identifier("value");

  // Missing value.
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  // Multiple strings.
  ValueProto multiple_strings;
  multiple_strings.mutable_strings()->add_values("Hello");
  multiple_strings.mutable_strings()->add_values("World");
  user_model_.SetValue("value", multiple_strings);
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  // Single empty string.
  user_model_.SetValue("value", SimpleValue(std::string()));
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("result"), SimpleValue(true));

  // Single non-empty string.
  user_model_.SetValue("value", SimpleValue(std::string("Hello")));
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("result"), SimpleValue(false));
}

TEST_F(BasicInteractionsTest, ToggleUserAction) {
  ToggleUserActionProto proto;
  ValueProto user_actions_value;
  UserActionProto cancel_user_action;
  cancel_user_action.set_identifier("cancel_identifier");
  cancel_user_action.mutable_chip()->set_type(ChipType::CANCEL_ACTION);
  cancel_user_action.mutable_chip()->set_text("Cancel");
  *user_actions_value.mutable_user_actions()->add_values() = cancel_user_action;
  UserActionProto done_user_action;
  done_user_action.set_identifier("done_identifier");
  done_user_action.mutable_chip()->set_type(ChipType::HIGHLIGHTED_ACTION);
  done_user_action.mutable_chip()->set_text("Done");
  *user_actions_value.mutable_user_actions()->add_values() = done_user_action;
  user_model_.SetValue("chips", user_actions_value);
  user_model_.SetValue("enabled", SimpleValue(false));

  // Missing fields.
  EXPECT_FALSE(basic_interactions_.ToggleUserAction(proto));
  proto.set_user_actions_model_identifier("chips");
  EXPECT_FALSE(basic_interactions_.ToggleUserAction(proto));
  proto.mutable_enabled()->set_model_identifier("enabled");
  EXPECT_FALSE(basic_interactions_.ToggleUserAction(proto));
  proto.set_user_action_identifier("done_identifier");
  EXPECT_TRUE(basic_interactions_.ToggleUserAction(proto));

  // Wrong value types/sizes.
  user_model_.SetValue("chips", SimpleValue(3));
  EXPECT_FALSE(basic_interactions_.ToggleUserAction(proto));
  user_model_.SetValue("chips", user_actions_value);
  user_model_.SetValue("enabled", SimpleValue(2));
  EXPECT_FALSE(basic_interactions_.ToggleUserAction(proto));
  ValueProto multi_bool;
  multi_bool.mutable_booleans()->add_values(true);
  multi_bool.mutable_booleans()->add_values(false);
  user_model_.SetValue("enabled", multi_bool);
  EXPECT_FALSE(basic_interactions_.ToggleUserAction(proto));
  user_model_.SetValue("enabled", SimpleValue(false));
  EXPECT_TRUE(basic_interactions_.ToggleUserAction(proto));

  // Wrong user action identifier.
  proto.set_user_action_identifier("wrong");
  EXPECT_FALSE(basic_interactions_.ToggleUserAction(proto));
  proto.set_user_action_identifier("cancel_identifier");
  EXPECT_TRUE(basic_interactions_.ToggleUserAction(proto));

  // Check changes to values.
  user_model_.SetValue("chips", user_actions_value);
  user_model_.SetValue("enabled", SimpleValue(false));
  proto.set_user_action_identifier("done_identifier");
  EXPECT_THAT(
      user_model_.GetValue("chips")->user_actions().values(),
      ElementsAre(AllOf(Property(&UserActionProto::identifier,
                                 StrEq("cancel_identifier")),
                        Property(&UserActionProto::enabled, Eq(true))),
                  AllOf(Property(&UserActionProto::identifier,
                                 StrEq("done_identifier")),
                        Property(&UserActionProto::enabled, Eq(true)))));
  EXPECT_TRUE(basic_interactions_.ToggleUserAction(proto));
  EXPECT_THAT(
      user_model_.GetValue("chips")->user_actions().values(),
      ElementsAre(AllOf(Property(&UserActionProto::identifier,
                                 StrEq("cancel_identifier")),
                        Property(&UserActionProto::enabled, Eq(true))),
                  AllOf(Property(&UserActionProto::identifier,
                                 StrEq("done_identifier")),
                        Property(&UserActionProto::enabled, Eq(false)))));
}

TEST_F(BasicInteractionsTest, RunConditionalCallback) {
  InSequence seq;
  base::MockCallback<base::RepeatingCallback<void()>> callback;

  EXPECT_CALL(callback, Run()).Times(0);
  EXPECT_FALSE(
      basic_interactions_.RunConditionalCallback("condition", callback.Get()));

  ValueProto multi_bool;
  multi_bool.mutable_booleans()->add_values(true);
  multi_bool.mutable_booleans()->add_values(false);
  user_model_.SetValue("condition", multi_bool);
  EXPECT_FALSE(
      basic_interactions_.RunConditionalCallback("condition", callback.Get()));

  user_model_.SetValue("condition", SimpleValue(false));
  EXPECT_TRUE(
      basic_interactions_.RunConditionalCallback("condition", callback.Get()));

  EXPECT_CALL(callback, Run()).Times(1);
  user_model_.SetValue("condition", SimpleValue(true));
  EXPECT_TRUE(
      basic_interactions_.RunConditionalCallback("condition", callback.Get()));
}

TEST_F(BasicInteractionsTest, GetClientSettings) {
  settings_.display_strings_locale = "hi-IN";
  EXPECT_EQ(basic_interactions_.GetClientSettings().display_strings_locale,
            "hi-IN");
  settings_.display_strings_locale = "";
  EXPECT_TRUE(
      basic_interactions_.GetClientSettings().display_strings_locale.empty());
}

}  // namespace autofill_assistant
