// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/fallback_handler/required_fields_fallback_handler.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/guid.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill_assistant/browser/action_value.pb.h"
#include "components/autofill_assistant/browser/actions/action_test_utils.h"
#include "components/autofill_assistant/browser/actions/fallback_handler/required_field.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/web/element_finder.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Expectation;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Return;

RequiredField CreateRequiredField(int key,
                                  const std::vector<std::string>& selector) {
  RequiredField required_field;
  required_field.proto.mutable_value_expression()->add_chunk()->set_key(key);
  required_field.selector = Selector(selector);
  required_field.status = RequiredField::EMPTY;
  return required_field;
}

RequiredField CreateRequiredField(const ValueExpression& value_expression,
                                  const std::vector<std::string>& selector) {
  RequiredField required_field;
  *required_field.proto.mutable_value_expression() = value_expression;
  required_field.selector = Selector(selector);
  required_field.status = RequiredField::EMPTY;
  return required_field;
}

class RequiredFieldsFallbackHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    ON_CALL(mock_action_delegate_, RunElementChecks)
        .WillByDefault(Invoke([this](BatchElementChecker* checker) {
          checker->Run(&mock_web_controller_);
        }));
    test_util::MockFindAnyElement(mock_web_controller_);
    ON_CALL(mock_action_delegate_, GetWebController)
        .WillByDefault(Return(&mock_web_controller_));
    ON_CALL(mock_web_controller_, GetElementTag(_, _))
        .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "INPUT"));
    ON_CALL(mock_web_controller_, SetValueAttribute(_, _, _))
        .WillByDefault(RunOnceCallback<2>(OkClientStatus()));
    ON_CALL(mock_action_delegate_, WaitUntilDocumentIsInReadyState(_, _, _, _))
        .WillByDefault(RunOnceCallback<3>(OkClientStatus(), base::Seconds(0)));
    ON_CALL(mock_web_controller_, ScrollIntoView(_, _, _, _, _))
        .WillByDefault(RunOnceCallback<4>(OkClientStatus()));
    ON_CALL(mock_web_controller_, WaitUntilElementIsStable(_, _, _, _))
        .WillByDefault(RunOnceCallback<3>(OkClientStatus(), base::Seconds(0)));
  }

 protected:
  MockActionDelegate mock_action_delegate_;
  MockWebController mock_web_controller_;
};

TEST_F(RequiredFieldsFallbackHandlerTest,
       AutofillFailureExitsEarlyForEmptyRequiredFields) {
  RequiredFieldsFallbackHandler fallback_handler(
      /* required_fields = */ {},
      /* fallback_values = */ {}, &mock_action_delegate_);

  fallback_handler.CheckAndFallbackRequiredFields(
      ClientStatus(OTHER_ACTION_STATUS),
      base::BindOnce([](const ClientStatus& status) {
        EXPECT_EQ(status.proto_status(), OTHER_ACTION_STATUS);
        EXPECT_FALSE(status.details().has_autofill_error_info());
      }));
}

TEST_F(RequiredFieldsFallbackHandlerTest, AutofillFailureGetsForwarded) {
  // Everything is full, no need to do work. Required fields succeed by
  // default.
  ON_CALL(mock_web_controller_, GetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "value"));

  std::vector<RequiredField> required_fields = {
      CreateRequiredField(51, {"#card_name"})};

  RequiredFieldsFallbackHandler fallback_handler(required_fields, {},
                                                 &mock_action_delegate_);
  fallback_handler.CheckAndFallbackRequiredFields(
      ClientStatus(OTHER_ACTION_STATUS),
      base::BindOnce([](const ClientStatus& status) {
        EXPECT_EQ(status.proto_status(), OTHER_ACTION_STATUS);
        EXPECT_EQ(
            status.details().autofill_error_info().autofill_error_status(),
            OTHER_ACTION_STATUS);
      }));
}

TEST_F(RequiredFieldsFallbackHandlerTest,
       AutofillFailureReturnedOverFallbackError) {
  // Everything is empty. Required fields fail by default.
  ON_CALL(mock_web_controller_, GetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), std::string()));

  std::vector<RequiredField> required_fields = {
      CreateRequiredField(51, {"#card_name"})};

  RequiredFieldsFallbackHandler fallback_handler(required_fields, {},
                                                 &mock_action_delegate_);
  fallback_handler.CheckAndFallbackRequiredFields(
      ClientStatus(OTHER_ACTION_STATUS),
      base::BindOnce([](const ClientStatus& status) {
        EXPECT_EQ(status.proto_status(), OTHER_ACTION_STATUS);
        EXPECT_EQ(
            status.details().autofill_error_info().autofill_error_status(),
            OTHER_ACTION_STATUS);
      }));
}

TEST_F(RequiredFieldsFallbackHandlerTest,
       AddsMissingOrEmptyFallbackValuesToError) {
  // The checks should only run once (initially). There should not be a
  // "non-empty" validation because it failed before that.
  Selector card_name_selector({"#card_name"});
  Selector card_number_selector({"#card_number"});
  Selector card_network_selector({"#card_network"});
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(test_util::MockFindElement(
                                mock_web_controller_, card_name_selector)),
                            _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), std::string()));
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(test_util::MockFindElement(
                                mock_web_controller_, card_number_selector)),
                            _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), std::string()));
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(test_util::MockFindElement(
                                mock_web_controller_, card_network_selector)),
                            _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), std::string()));

  std::vector<RequiredField> required_fields = {
      CreateRequiredField(51, {"#card_name"}),
      CreateRequiredField(52, {"#card_number"}),
      CreateRequiredField(-3, {"#card_network"})};

  base::flat_map<field_formatter::Key, std::string> fallback_values = {
      {field_formatter::Key(
           static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_NAME_FULL)),
       "John Doe"},
      {field_formatter::Key(
           static_cast<int>(AutofillFormatProto::CREDIT_CARD_NETWORK)),
       std::string()}};

  RequiredFieldsFallbackHandler fallback_handler(
      required_fields, fallback_values, &mock_action_delegate_);

  base::OnceCallback<void(const ClientStatus&)> callback =
      base::BindOnce([](const ClientStatus& status) {
        EXPECT_EQ(status.proto_status(), AUTOFILL_INCOMPLETE);
        ASSERT_EQ(
            status.details().autofill_error_info().autofill_field_error_size(),
            2);
        EXPECT_EQ(status.details()
                      .autofill_error_info()
                      .autofill_field_error(0)
                      .value_expression(),
                  "${52}");
        EXPECT_TRUE(status.details()
                        .autofill_error_info()
                        .autofill_field_error(0)
                        .no_fallback_value());
        EXPECT_EQ(status.details()
                      .autofill_error_info()
                      .autofill_field_error(1)
                      .value_expression(),
                  "${-3}");
        EXPECT_TRUE(status.details()
                        .autofill_error_info()
                        .autofill_field_error(1)
                        .no_fallback_value());
      });

  fallback_handler.CheckAndFallbackRequiredFields(OkClientStatus(),
                                                  std::move(callback));
}

TEST_F(RequiredFieldsFallbackHandlerTest, AddsFirstFieldFillingError) {
  ON_CALL(mock_web_controller_, GetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), ""));
  ON_CALL(mock_web_controller_, SetValueAttribute(_, _, _))
      .WillByDefault(RunOnceCallback<2>(ClientStatus(OTHER_ACTION_STATUS)));

  std::vector<RequiredField> required_fields = {
      CreateRequiredField(51, {"#card_name"}),
      CreateRequiredField(52, {"#card_number"})};

  base::flat_map<field_formatter::Key, std::string> fallback_values = {
      {field_formatter::Key(
           static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_NAME_FULL)),
       "John Doe"},
      {field_formatter::Key(
           static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_NUMBER)),
       "4111111111111111"}};

  RequiredFieldsFallbackHandler fallback_handler(
      required_fields, fallback_values, &mock_action_delegate_);

  base::OnceCallback<void(const ClientStatus&)> callback =
      base::BindOnce([](const ClientStatus& status) {
        EXPECT_EQ(status.proto_status(), AUTOFILL_INCOMPLETE);
        ASSERT_EQ(
            status.details().autofill_error_info().autofill_field_error_size(),
            1);
        EXPECT_EQ(status.details()
                      .autofill_error_info()
                      .autofill_field_error(0)
                      .value_expression(),
                  "${51}");
        EXPECT_EQ(status.details()
                      .autofill_error_info()
                      .autofill_field_error(0)
                      .status(),
                  OTHER_ACTION_STATUS);
      });

  fallback_handler.CheckAndFallbackRequiredFields(OkClientStatus(),
                                                  std::move(callback));
}

TEST_F(RequiredFieldsFallbackHandlerTest,
       AddsFirstEmptyFieldAfterFillingToError) {
  ON_CALL(mock_web_controller_, GetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), ""));

  std::vector<RequiredField> required_fields = {
      CreateRequiredField(51, {"#card_name"}),
      CreateRequiredField(52, {"#card_number"})};

  base::flat_map<field_formatter::Key, std::string> fallback_values = {
      {field_formatter::Key(
           static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_NAME_FULL)),
       "John Doe"},
      {field_formatter::Key(
           static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_NUMBER)),
       "4111111111111111"}};

  RequiredFieldsFallbackHandler fallback_handler(
      required_fields, fallback_values, &mock_action_delegate_);

  base::OnceCallback<void(const ClientStatus&)> callback =
      base::BindOnce([](const ClientStatus& status) {
        EXPECT_EQ(status.proto_status(), AUTOFILL_INCOMPLETE);
        ASSERT_EQ(
            status.details().autofill_error_info().autofill_field_error_size(),
            1);
        EXPECT_EQ(status.details()
                      .autofill_error_info()
                      .autofill_field_error(0)
                      .value_expression(),
                  "${51}");
        EXPECT_TRUE(status.details()
                        .autofill_error_info()
                        .autofill_field_error(0)
                        .empty_after_fallback());
      });

  fallback_handler.CheckAndFallbackRequiredFields(OkClientStatus(),
                                                  std::move(callback));
}

TEST_F(RequiredFieldsFallbackHandlerTest, DoesNotFallbackIfFieldsAreFilled) {
  ON_CALL(mock_web_controller_, GetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "value"));
  EXPECT_CALL(mock_web_controller_, SetValueAttribute(_, _, _)).Times(0);

  std::vector<RequiredField> required_fields = {
      CreateRequiredField(51, {"#card_name"})};

  RequiredFieldsFallbackHandler fallback_handler(required_fields, {},
                                                 &mock_action_delegate_);
  fallback_handler.CheckAndFallbackRequiredFields(
      OkClientStatus(), base::BindOnce([](const ClientStatus& status) {
        EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
      }));
}

TEST_F(RequiredFieldsFallbackHandlerTest, FillsEmptyRequiredField) {
  EXPECT_CALL(mock_web_controller_, GetFieldValue(_, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), ""));
  Selector expected_selector({"#card_name"});
  Expectation set_value =
      EXPECT_CALL(
          mock_web_controller_,
          SetValueAttribute("John Doe",
                            EqualsElement(test_util::MockFindElement(
                                mock_action_delegate_, expected_selector)),
                            _))
          .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(mock_web_controller_, GetFieldValue(_, _))
      .After(set_value)
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "John Doe"));

  std::vector<RequiredField> required_fields = {
      CreateRequiredField(51, {"#card_name"})};

  base::flat_map<field_formatter::Key, std::string> fallback_values = {
      {field_formatter::Key(
           static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_NAME_FULL)),
       "John Doe"}};

  RequiredFieldsFallbackHandler fallback_handler(
      required_fields, fallback_values, &mock_action_delegate_);
  fallback_handler.CheckAndFallbackRequiredFields(
      OkClientStatus(), base::BindOnce([](const ClientStatus& status) {
        EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
      }));
}

TEST_F(RequiredFieldsFallbackHandlerTest, FallsBackForForcedFilledField) {
  ON_CALL(mock_web_controller_, GetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "value"));
  Selector expected_selector({"#card_name"});
  EXPECT_CALL(mock_web_controller_,
              SetValueAttribute("John Doe",
                                EqualsElement(test_util::MockFindElement(
                                    mock_action_delegate_, expected_selector)),
                                _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));

  std::vector<RequiredField> required_fields = {
      CreateRequiredField(51, {"#card_name"})};
  required_fields[0].proto.set_forced(true);

  base::flat_map<field_formatter::Key, std::string> fallback_values = {
      {field_formatter::Key(
           static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_NAME_FULL)),
       "John Doe"}};

  RequiredFieldsFallbackHandler fallback_handler(
      required_fields, fallback_values, &mock_action_delegate_);
  fallback_handler.CheckAndFallbackRequiredFields(
      OkClientStatus(), base::BindOnce([](const ClientStatus& status) {
        EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
      }));
}

TEST_F(RequiredFieldsFallbackHandlerTest, FailsIfForcedFieldDidNotGetFilled) {
  ON_CALL(mock_web_controller_, GetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "value"));
  EXPECT_CALL(mock_web_controller_, SetValueAttribute(_, _, _)).Times(0);

  std::vector<RequiredField> required_fields = {
      CreateRequiredField(51, {"#card_name"})};
  required_fields[0].proto.set_forced(true);

  RequiredFieldsFallbackHandler fallback_handler(required_fields, {},
                                                 &mock_action_delegate_);

  base::OnceCallback<void(const ClientStatus&)> callback =
      base::BindOnce([](const ClientStatus& status) {
        EXPECT_EQ(status.proto_status(), AUTOFILL_INCOMPLETE);
        ASSERT_EQ(
            status.details().autofill_error_info().autofill_field_error_size(),
            1);
        EXPECT_EQ(status.details()
                      .autofill_error_info()
                      .autofill_field_error(0)
                      .value_expression(),
                  "${51}");
        EXPECT_TRUE(status.details()
                        .autofill_error_info()
                        .autofill_field_error(0)
                        .no_fallback_value());
      });

  fallback_handler.CheckAndFallbackRequiredFields(OkClientStatus(),
                                                  std::move(callback));
}

TEST_F(RequiredFieldsFallbackHandlerTest, FillsFieldWithPattern) {
  EXPECT_CALL(mock_web_controller_, GetFieldValue(_, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), ""));
  Selector expected_selector({"#card_expiry"});
  Expectation set_value =
      EXPECT_CALL(
          mock_web_controller_,
          SetValueAttribute("08/2050",
                            EqualsElement(test_util::MockFindElement(
                                mock_action_delegate_, expected_selector)),
                            _))
          .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(mock_web_controller_, GetFieldValue(_, _))
      .After(set_value)
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "not empty"));

  ValueExpression value_expression;
  value_expression.add_chunk()->set_key(53);
  value_expression.add_chunk()->set_text("/");
  value_expression.add_chunk()->set_key(55);
  std::vector<RequiredField> required_fields = {
      CreateRequiredField(value_expression, {"#card_expiry"})};

  base::flat_map<field_formatter::Key, std::string> fallback_values = {
      {field_formatter::Key(autofill::ServerFieldType::CREDIT_CARD_EXP_MONTH),
       "08"},
      {field_formatter::Key(
           autofill::ServerFieldType::CREDIT_CARD_EXP_4_DIGIT_YEAR),
       "2050"}};

  RequiredFieldsFallbackHandler fallback_handler(
      required_fields, fallback_values, &mock_action_delegate_);
  fallback_handler.CheckAndFallbackRequiredFields(
      OkClientStatus(), base::BindOnce([](const ClientStatus& status) {
        EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
      }));
}

TEST_F(RequiredFieldsFallbackHandlerTest,
       FailsToFillFieldWithUnknownOrEmptyKey) {
  EXPECT_CALL(mock_web_controller_, GetFieldValue(_, _))
      .Times(2)
      .WillRepeatedly(RunOnceCallback<1>(OkClientStatus(), ""));
  EXPECT_CALL(mock_web_controller_, SetValueAttribute(_, _, _)).Times(0);

  std::vector<RequiredField> required_fields = {
      CreateRequiredField(53, {"#card_expiry"}),
      CreateRequiredField(-3, {"#card_network"})};

  base::flat_map<field_formatter::Key, std::string> fallback_values;
  fallback_values.emplace(
      static_cast<int>(AutofillFormatProto::CREDIT_CARD_NETWORK), "");

  RequiredFieldsFallbackHandler fallback_handler(
      required_fields, fallback_values, &mock_action_delegate_);

  base::OnceCallback<void(const ClientStatus&)> callback =
      base::BindOnce([](const ClientStatus& status) {
        EXPECT_EQ(status.proto_status(), AUTOFILL_INCOMPLETE);
        ASSERT_EQ(
            status.details().autofill_error_info().autofill_field_error_size(),
            2);
        EXPECT_EQ(status.details()
                      .autofill_error_info()
                      .autofill_field_error(0)
                      .value_expression(),
                  "${53}");
        EXPECT_TRUE(status.details()
                        .autofill_error_info()
                        .autofill_field_error(0)
                        .no_fallback_value());
        EXPECT_EQ(status.details()
                      .autofill_error_info()
                      .autofill_field_error(1)
                      .value_expression(),
                  "${-3}");
        EXPECT_TRUE(status.details()
                        .autofill_error_info()
                        .autofill_field_error(1)
                        .no_fallback_value());
      });

  fallback_handler.CheckAndFallbackRequiredFields(OkClientStatus(),
                                                  std::move(callback));
}

TEST_F(RequiredFieldsFallbackHandlerTest, UsesSelectOptionForDropdowns) {
  InSequence sequence;

  Selector expected_selector({"#exp"});

  // First validation fails.
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(test_util::MockFindElement(
                                mock_web_controller_, expected_selector)),
                            _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), std::string()));

  // Fill field.
  const ElementFinderResult& expected_element =
      test_util::MockFindElement(mock_action_delegate_, expected_selector);
  EXPECT_CALL(mock_web_controller_,
              GetElementTag(EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "SELECT"));
  EXPECT_CALL(mock_web_controller_,
              SelectOption("^05\\/2050$", /* case_sensitive= */ false,
                           SelectOptionProto::VALUE, /* strict= */ false,
                           EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<5>(OkClientStatus()));

  // Second validation succeeds.
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(test_util::MockFindElement(
                                mock_web_controller_, expected_selector)),
                            _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "05/2050"));

  std::vector<RequiredField> required_fields = {
      CreateRequiredField(57, {"#exp"})};
  required_fields[0].proto.set_select_strategy(
      DropdownSelectStrategy::VALUE_MATCH);

  base::flat_map<field_formatter::Key, std::string> fallback_values = {
      {field_formatter::Key(
           autofill::ServerFieldType::CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR),
       "05/2050"}};

  RequiredFieldsFallbackHandler fallback_handler(
      required_fields, fallback_values, &mock_action_delegate_);
  fallback_handler.CheckAndFallbackRequiredFields(
      OkClientStatus(), base::BindOnce([](const ClientStatus& status) {
        EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
      }));
}

TEST_F(RequiredFieldsFallbackHandlerTest,
       UsesSelectOptionForFullDropdownDefinition) {
  InSequence sequence;

  Selector expected_selector({"#exp"});

  // First validation fails.
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(test_util::MockFindElement(
                                mock_web_controller_, expected_selector)),
                            _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), std::string()));

  // Fill field.
  const ElementFinderResult& expected_element =
      test_util::MockFindElement(mock_action_delegate_, expected_selector);
  EXPECT_CALL(mock_web_controller_,
              GetElementTag(EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "SELECT"));
  EXPECT_CALL(mock_web_controller_,
              SelectOption("^05\\/2050", /* case_sensitive= */ true,
                           SelectOptionProto::VALUE, /* strict= */ false,
                           EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<5>(OkClientStatus()));

  // Second validation succeeds.
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(test_util::MockFindElement(
                                mock_web_controller_, expected_selector)),
                            _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "05/2050"));

  RequiredField required_field = CreateRequiredField(57, {"#exp"});
  required_field.proto.set_option_comparison_attribute(
      SelectOptionProto::VALUE);
  ValueExpressionRegexp value_expression_re2;
  value_expression_re2.mutable_value_expression()->add_chunk()->set_text("^");
  value_expression_re2.mutable_value_expression()->add_chunk()->set_key(57);
  value_expression_re2.set_case_sensitive(true);
  *required_field.proto.mutable_option_comparison_value_expression_re2() =
      value_expression_re2;
  std::vector<RequiredField> required_fields = {required_field};

  base::flat_map<field_formatter::Key, std::string> fallback_values = {
      {field_formatter::Key(
           autofill::ServerFieldType::CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR),
       "05/2050"}};

  RequiredFieldsFallbackHandler fallback_handler(
      required_fields, fallback_values, &mock_action_delegate_);
  fallback_handler.CheckAndFallbackRequiredFields(
      OkClientStatus(), base::BindOnce([](const ClientStatus& status) {
        EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
      }));
}

TEST_F(RequiredFieldsFallbackHandlerTest, ClicksOnCustomDropdown) {
  EXPECT_CALL(mock_web_controller_, GetFieldValue(_, _)).Times(0);
  EXPECT_CALL(mock_web_controller_, SetValueAttribute(_, _, _)).Times(0);
  Selector expected_main_selector({"#card_expiry"});
  EXPECT_CALL(
      mock_web_controller_,
      ClickOrTapElement(ClickType::TAP,
                        EqualsElement(test_util::MockFindElement(
                            mock_action_delegate_, expected_main_selector)),
                        _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  Selector expected_option_selector({".option"});
  expected_option_selector.MatchingInnerText("05\\/2050",
                                             /* case_sensitive= */ false);
  EXPECT_CALL(mock_action_delegate_,
              OnShortWaitForElement(expected_option_selector, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), base::Seconds(0)));
  EXPECT_CALL(
      mock_web_controller_,
      ClickOrTapElement(ClickType::TAP,
                        EqualsElement(test_util::MockFindElement(
                            mock_action_delegate_, expected_option_selector)),
                        _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));

  std::vector<RequiredField> required_fields = {
      CreateRequiredField(57, {"#card_expiry"})};
  *required_fields[0].proto.mutable_option_element_to_click() =
      ToSelectorProto(".option");

  base::flat_map<field_formatter::Key, std::string> fallback_values = {
      {field_formatter::Key(
           autofill::ServerFieldType::CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR),
       "05/2050"}};

  RequiredFieldsFallbackHandler fallback_handler(
      required_fields, fallback_values, &mock_action_delegate_);
  fallback_handler.CheckAndFallbackRequiredFields(
      OkClientStatus(), base::BindOnce([](const ClientStatus& status) {
        EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
      }));
}

TEST_F(RequiredFieldsFallbackHandlerTest, SkipsOptionalCustomDropdown) {
  EXPECT_CALL(mock_web_controller_, GetFieldValue(_, _)).Times(0);
  EXPECT_CALL(mock_web_controller_, SetValueAttribute(_, _, _)).Times(0);
  Selector expected_main_selector({"#card_expiry"});
  EXPECT_CALL(mock_action_delegate_, FindElement(expected_main_selector, _))
      .WillOnce(
          RunOnceCallback<1>(ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr));
  EXPECT_CALL(mock_web_controller_, ClickOrTapElement(ClickType::TAP, _, _))
      .Times(0);

  std::vector<RequiredField> required_fields = {
      CreateRequiredField(57, {"#card_expiry"})};
  *required_fields[0].proto.mutable_option_element_to_click() =
      ToSelectorProto(".option");
  required_fields[0].proto.set_is_optional(true);

  base::flat_map<field_formatter::Key, std::string> fallback_values = {
      {field_formatter::Key(
           autofill::ServerFieldType::CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR),
       "05/2050"}};

  RequiredFieldsFallbackHandler fallback_handler(
      required_fields, fallback_values, &mock_action_delegate_);
  fallback_handler.CheckAndFallbackRequiredFields(
      OkClientStatus(), base::BindOnce([](const ClientStatus& status) {
        EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
        ASSERT_EQ(
            status.details().autofill_error_info().autofill_field_error_size(),
            1);
        EXPECT_EQ(status.details()
                      .autofill_error_info()
                      .autofill_field_error(0)
                      .value_expression(),
                  "${57}");
        EXPECT_EQ(status.details()
                      .autofill_error_info()
                      .autofill_field_error(0)
                      .status(),
                  ELEMENT_RESOLUTION_FAILED);
      }));
}

TEST_F(RequiredFieldsFallbackHandlerTest, CustomDropdownClicksStopOnError) {
  EXPECT_CALL(mock_web_controller_, GetFieldValue(_, _)).Times(0);
  EXPECT_CALL(mock_web_controller_, SetValueAttribute(_, _, _)).Times(0);
  Selector expected_main_selector({"#card_expiry"});
  Expectation main_click =
      EXPECT_CALL(
          mock_web_controller_,
          ClickOrTapElement(ClickType::TAP,
                            EqualsElement(test_util::MockFindElement(
                                mock_action_delegate_, expected_main_selector)),
                            _))
          .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  Selector expected_option_selector({".option"});
  expected_option_selector.MatchingInnerText("05\\/2050",
                                             /* case_sensitive= */ false);
  EXPECT_CALL(mock_action_delegate_,
              OnShortWaitForElement(expected_option_selector, _))
      .WillOnce(RunOnceCallback<1>(ClientStatus(ELEMENT_RESOLUTION_FAILED),
                                   base::Seconds(0)));
  EXPECT_CALL(mock_action_delegate_, FindElement(_, _))
      .Times(0)
      .After(main_click);
  EXPECT_CALL(mock_web_controller_, ClickOrTapElement(_, _, _))
      .Times(0)
      .After(main_click);

  std::vector<RequiredField> required_fields = {
      CreateRequiredField(57, {"#card_expiry"})};
  *required_fields[0].proto.mutable_option_element_to_click() =
      ToSelectorProto(".option");

  base::flat_map<field_formatter::Key, std::string> fallback_values = {
      {field_formatter::Key(
           autofill::ServerFieldType::CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR),
       "05/2050"}};

  RequiredFieldsFallbackHandler fallback_handler(
      required_fields, fallback_values, &mock_action_delegate_);
  fallback_handler.CheckAndFallbackRequiredFields(
      OkClientStatus(), base::BindOnce([](const ClientStatus& status) {
        EXPECT_EQ(status.proto_status(), AUTOFILL_INCOMPLETE);
        ASSERT_EQ(
            status.details().autofill_error_info().autofill_field_error_size(),
            1);
        EXPECT_EQ(status.details()
                      .autofill_error_info()
                      .autofill_field_error(0)
                      .value_expression(),
                  "${57}");
        EXPECT_EQ(status.details()
                      .autofill_error_info()
                      .autofill_field_error(0)
                      .status(),
                  OPTION_VALUE_NOT_FOUND);
      }));
}

TEST_F(RequiredFieldsFallbackHandlerTest, ClearsFilledField) {
  InSequence sequence;

  Selector expected_selector({"#field"});

  // First validation fails
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(test_util::MockFindElement(
                                mock_web_controller_, expected_selector)),
                            _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "value"));

  // Clears field.
  EXPECT_CALL(mock_web_controller_,
              SetValueAttribute(std::string(),
                                EqualsElement(test_util::MockFindElement(
                                    mock_action_delegate_, expected_selector)),
                                _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));

  // Second validation succeeds.
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(test_util::MockFindElement(
                                mock_web_controller_, expected_selector)),
                            _))
      .WillRepeatedly(RunOnceCallback<1>(OkClientStatus(), std::string()));

  std::vector<RequiredField> required_fields = {
      CreateRequiredField(ValueExpression(), {"#field"})};
  base::flat_map<field_formatter::Key, std::string> fallback_values;

  RequiredFieldsFallbackHandler fallback_handler(
      required_fields, fallback_values, &mock_action_delegate_);
  fallback_handler.CheckAndFallbackRequiredFields(
      OkClientStatus(), base::BindOnce([](const ClientStatus& status) {
        EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
      }));
}

TEST_F(RequiredFieldsFallbackHandlerTest, SkipsForcedFieldCheckOnFirstRun) {
  InSequence sequence;

  Selector forced_field_selector({"#forced_field"});

  // First validation skips forced fields.
  EXPECT_CALL(mock_web_controller_, GetFieldValue(_, _)).Times(0);

  // Fills field.
  EXPECT_CALL(
      mock_web_controller_,
      SetValueAttribute("value",
                        EqualsElement(test_util::MockFindElement(
                            mock_action_delegate_, forced_field_selector)),
                        _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));

  // Second validation checks the field.
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(test_util::MockFindElement(
                                mock_web_controller_, forced_field_selector)),
                            _))
      .WillRepeatedly(RunOnceCallback<1>(OkClientStatus(), "value"));

  ValueExpression value_expression;
  value_expression.add_chunk()->set_text("value");
  auto forced_field = CreateRequiredField(value_expression, {"#forced_field"});
  forced_field.proto.set_forced(true);
  std::vector<RequiredField> required_fields = {forced_field};

  base::flat_map<field_formatter::Key, std::string> fallback_values;

  RequiredFieldsFallbackHandler fallback_handler(
      required_fields, fallback_values, &mock_action_delegate_);
  fallback_handler.CheckAndFallbackRequiredFields(
      OkClientStatus(), base::BindOnce([](const ClientStatus& status) {
        EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
      }));
}

TEST_F(RequiredFieldsFallbackHandlerTest,
       EmptyValueDoesNotFailForFieldNotNeedingToBeFilled) {
  Selector card_name_selector({"#card_name"});
  Selector card_number_selector({"#card_number"});
  auto card_name_element =
      test_util::MockFindElement(mock_web_controller_, card_name_selector, 2);
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(card_name_element), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), std::string()))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "value"));
  auto card_number_element =
      test_util::MockFindElement(mock_web_controller_, card_number_selector, 2);
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(card_number_element), _))
      .Times(2)
      .WillRepeatedly(RunOnceCallback<1>(OkClientStatus(), "value"));
  EXPECT_CALL(mock_web_controller_,
              SetValueAttribute("John Doe",
                                EqualsElement(test_util::MockFindElement(
                                    mock_action_delegate_, card_name_selector)),
                                _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));

  std::vector<RequiredField> required_fields = {
      CreateRequiredField(51, {"#card_name"}),
      CreateRequiredField(52, {"#card_number"})};

  base::flat_map<field_formatter::Key, std::string> fallback_values = {
      {field_formatter::Key(
           static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_NAME_FULL)),
       "John Doe"}};

  RequiredFieldsFallbackHandler fallback_handler(
      required_fields, fallback_values, &mock_action_delegate_);

  base::OnceCallback<void(const ClientStatus&)> callback =
      base::BindOnce([](const ClientStatus& status) {
        EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
        EXPECT_EQ(
            status.details().autofill_error_info().autofill_field_error_size(),
            0);
      });

  fallback_handler.CheckAndFallbackRequiredFields(OkClientStatus(),
                                                  std::move(callback));
}

TEST_F(RequiredFieldsFallbackHandlerTest,
       AddsMissingFallbackValueToDetailsForOptionalFieldsWithoutFailing) {
  Selector card_name_selector({"#card_name"});
  Selector card_number_selector({"#card_number"});
  auto card_name_element =
      test_util::MockFindElement(mock_web_controller_, card_name_selector, 2);
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(card_name_element), _))
      .Times(2)
      .WillRepeatedly(RunOnceCallback<1>(OkClientStatus(), std::string()));
  EXPECT_CALL(mock_web_controller_, SetValueAttribute(_, _, _)).Times(0);
  auto card_number_element =
      test_util::MockFindElement(mock_web_controller_, card_number_selector, 2);
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(card_number_element), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), std::string()))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "4111111111111111"));
  EXPECT_CALL(
      mock_web_controller_,
      SetValueAttribute("4111111111111111",
                        EqualsElement(test_util::MockFindElement(
                            mock_action_delegate_, card_number_selector)),
                        _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));

  std::vector<RequiredField> required_fields = {
      CreateRequiredField(51, {"#card_name"}),
      CreateRequiredField(52, {"#card_number"})};
  required_fields[0].proto.set_is_optional(true);

  base::flat_map<field_formatter::Key, std::string> fallback_values = {
      {field_formatter::Key(
           static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_NUMBER)),
       "4111111111111111"}};

  RequiredFieldsFallbackHandler fallback_handler(
      required_fields, fallback_values, &mock_action_delegate_);

  base::OnceCallback<void(const ClientStatus&)> callback =
      base::BindOnce([](const ClientStatus& status) {
        EXPECT_TRUE(status.ok());
        ASSERT_EQ(
            status.details().autofill_error_info().autofill_field_error_size(),
            1);
        EXPECT_EQ(status.details()
                      .autofill_error_info()
                      .autofill_field_error(0)
                      .value_expression(),
                  "${51}");
        EXPECT_TRUE(status.details()
                        .autofill_error_info()
                        .autofill_field_error(0)
                        .no_fallback_value());
      });

  fallback_handler.CheckAndFallbackRequiredFields(OkClientStatus(),
                                                  std::move(callback));
}

TEST_F(RequiredFieldsFallbackHandlerTest,
       AddsNotFoundElementToDetailsForOptionalFieldsWithoutFailing) {
  Selector card_name_selector({"#card_name"});
  Selector card_number_selector({"#card_number"});
  EXPECT_CALL(mock_web_controller_, FindElement(card_name_selector, _, _))
      .Times(2)
      .WillRepeatedly(
          RunOnceCallback<2>(ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr));
  EXPECT_CALL(mock_action_delegate_, FindElement(card_name_selector, _))
      .WillOnce(
          RunOnceCallback<1>(ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr));
  auto card_number_element =
      test_util::MockFindElement(mock_web_controller_, card_number_selector, 2);
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(card_number_element), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), std::string()))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "4111111111111111"));
  EXPECT_CALL(
      mock_web_controller_,
      SetValueAttribute("4111111111111111",
                        EqualsElement(test_util::MockFindElement(
                            mock_action_delegate_, card_number_selector)),
                        _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));

  std::vector<RequiredField> required_fields = {
      CreateRequiredField(51, {"#card_name"}),
      CreateRequiredField(52, {"#card_number"})};
  required_fields[0].proto.set_is_optional(true);

  base::flat_map<field_formatter::Key, std::string> fallback_values = {
      {field_formatter::Key(
           static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_NAME_FULL)),
       "John Doe"},
      {field_formatter::Key(
           static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_NUMBER)),
       "4111111111111111"}};

  RequiredFieldsFallbackHandler fallback_handler(
      required_fields, fallback_values, &mock_action_delegate_);

  base::OnceCallback<void(const ClientStatus&)> callback =
      base::BindOnce([](const ClientStatus& status) {
        EXPECT_TRUE(status.ok());
        ASSERT_EQ(
            status.details().autofill_error_info().autofill_field_error_size(),
            1);
        EXPECT_EQ(status.details()
                      .autofill_error_info()
                      .autofill_field_error(0)
                      .value_expression(),
                  "${51}");
        EXPECT_EQ(status.details()
                      .autofill_error_info()
                      .autofill_field_error(0)
                      .status(),
                  ELEMENT_RESOLUTION_FAILED);
      });

  fallback_handler.CheckAndFallbackRequiredFields(OkClientStatus(),
                                                  std::move(callback));
}

}  // namespace
}  // namespace autofill_assistant
