// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/fallback_handler/required_fields_fallback_handler.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/guid.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
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
using ::testing::Invoke;

RequiredField CreateRequiredField(const std::string& value_expression,
                                  const std::vector<std::string>& selector) {
  RequiredField required_field;
  required_field.value_expression = value_expression;
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
    ON_CALL(mock_action_delegate_, GetElementTag(_, _))
        .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "INPUT"));
    ON_CALL(mock_action_delegate_, OnSetFieldValue(_, _, _))
        .WillByDefault(RunOnceCallback<2>(OkClientStatus()));
    ON_CALL(mock_action_delegate_, WaitForDocumentToBecomeInteractive(_, _))
        .WillByDefault(RunOnceCallback<1>(OkClientStatus()));
    ON_CALL(mock_action_delegate_, ScrollIntoView(_, _))
        .WillByDefault(RunOnceCallback<1>(OkClientStatus()));
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

  base::OnceCallback<void(const ClientStatus&,
                          const base::Optional<ClientStatus>&)>
      callback =
          base::BindOnce([](const ClientStatus& status,
                            const base::Optional<ClientStatus>& detail_status) {
            EXPECT_EQ(status.proto_status(), OTHER_ACTION_STATUS);
            EXPECT_FALSE(detail_status.has_value());
          });

  fallback_handler.CheckAndFallbackRequiredFields(
      ClientStatus(OTHER_ACTION_STATUS), std::move(callback));
}

TEST_F(RequiredFieldsFallbackHandlerTest,
       AddsMissingOrEmptyFallbackValuesToError) {
  ON_CALL(mock_web_controller_, OnGetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), ""));

  std::vector<RequiredField> required_fields = {
      CreateRequiredField("${51}", {"#card_name"}),
      CreateRequiredField("${52}", {"#card_number"}),
      CreateRequiredField("${-3}", {"#card_network"})};

  std::map<std::string, std::string> fallback_values = {
      {base::NumberToString(
           static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_NAME_FULL)),
       "John Doe"},
      {base::NumberToString(
           static_cast<int>(AutofillFormatProto::CREDIT_CARD_NETWORK)),
       ""}};

  RequiredFieldsFallbackHandler fallback_handler(
      required_fields, fallback_values, &mock_action_delegate_);

  base::OnceCallback<void(const ClientStatus&,
                          const base::Optional<ClientStatus>&)>
      callback =
          base::BindOnce([](const ClientStatus& status,
                            const base::Optional<ClientStatus>& detail_status) {
            EXPECT_EQ(status.proto_status(), AUTOFILL_INCOMPLETE);
            ASSERT_TRUE(detail_status.has_value());
            ASSERT_EQ(detail_status.value().proto_status(),
                      AUTOFILL_INCOMPLETE);
            ASSERT_EQ(detail_status.value()
                          .details()
                          .autofill_error_info()
                          .autofill_field_error_size(),
                      3);
            EXPECT_EQ(detail_status.value()
                          .details()
                          .autofill_error_info()
                          .autofill_field_error(0)
                          .value_expression(),
                      "${52}");
            EXPECT_TRUE(detail_status.value()
                            .details()
                            .autofill_error_info()
                            .autofill_field_error(0)
                            .no_fallback_value());
            EXPECT_EQ(detail_status.value()
                          .details()
                          .autofill_error_info()
                          .autofill_field_error(1)
                          .value_expression(),
                      "${-3}");
            EXPECT_TRUE(detail_status.value()
                            .details()
                            .autofill_error_info()
                            .autofill_field_error(1)
                            .no_fallback_value());
            EXPECT_EQ(detail_status.value()
                          .details()
                          .autofill_error_info()
                          .autofill_field_error(2)
                          .value_expression(),
                      "${51}");
            EXPECT_TRUE(detail_status.value()
                            .details()
                            .autofill_error_info()
                            .autofill_field_error(2)
                            .empty_after_fallback());
          });

  fallback_handler.CheckAndFallbackRequiredFields(OkClientStatus(),
                                                  std::move(callback));
}

TEST_F(RequiredFieldsFallbackHandlerTest, AddsFirstFieldFillingError) {
  ON_CALL(mock_web_controller_, OnGetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), ""));
  ON_CALL(mock_action_delegate_, OnSetFieldValue(_, _, _))
      .WillByDefault(RunOnceCallback<2>(ClientStatus(OTHER_ACTION_STATUS)));

  std::vector<RequiredField> required_fields = {
      CreateRequiredField("${51}", {"#card_name"}),
      CreateRequiredField("${52}", {"#card_number"})};

  std::map<std::string, std::string> fallback_values = {
      {base::NumberToString(
           static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_NAME_FULL)),
       "John Doe"},
      {base::NumberToString(
           static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_NUMBER)),
       "4111111111111111"}};

  RequiredFieldsFallbackHandler fallback_handler(
      required_fields, fallback_values, &mock_action_delegate_);

  base::OnceCallback<void(const ClientStatus&,
                          const base::Optional<ClientStatus>&)>
      callback =
          base::BindOnce([](const ClientStatus& status,
                            const base::Optional<ClientStatus>& detail_status) {
            EXPECT_EQ(status.proto_status(), AUTOFILL_INCOMPLETE);
            ASSERT_TRUE(detail_status.has_value());
            ASSERT_EQ(detail_status.value().proto_status(),
                      AUTOFILL_INCOMPLETE);
            ASSERT_EQ(detail_status.value()
                          .details()
                          .autofill_error_info()
                          .autofill_field_error_size(),
                      1);
            EXPECT_EQ(detail_status.value()
                          .details()
                          .autofill_error_info()
                          .autofill_field_error(0)
                          .value_expression(),
                      "${51}");
            EXPECT_EQ(detail_status.value()
                          .details()
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
  ON_CALL(mock_web_controller_, OnGetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), ""));

  std::vector<RequiredField> required_fields = {
      CreateRequiredField("${51}", {"#card_name"}),
      CreateRequiredField("${52}", {"#card_number"})};

  std::map<std::string, std::string> fallback_values = {
      {base::NumberToString(
           static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_NAME_FULL)),
       "John Doe"},
      {base::NumberToString(
           static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_NUMBER)),
       "4111111111111111"}};

  RequiredFieldsFallbackHandler fallback_handler(
      required_fields, fallback_values, &mock_action_delegate_);

  base::OnceCallback<void(const ClientStatus&,
                          const base::Optional<ClientStatus>&)>
      callback =
          base::BindOnce([](const ClientStatus& status,
                            const base::Optional<ClientStatus>& detail_status) {
            EXPECT_EQ(status.proto_status(), AUTOFILL_INCOMPLETE);
            ASSERT_TRUE(detail_status.has_value());
            ASSERT_EQ(detail_status.value().proto_status(),
                      AUTOFILL_INCOMPLETE);
            ASSERT_EQ(detail_status.value()
                          .details()
                          .autofill_error_info()
                          .autofill_field_error_size(),
                      1);
            EXPECT_EQ(detail_status.value()
                          .details()
                          .autofill_error_info()
                          .autofill_field_error(0)
                          .value_expression(),
                      "${51}");
            EXPECT_TRUE(detail_status.value()
                            .details()
                            .autofill_error_info()
                            .autofill_field_error(0)
                            .empty_after_fallback());
          });

  fallback_handler.CheckAndFallbackRequiredFields(OkClientStatus(),
                                                  std::move(callback));
}

TEST_F(RequiredFieldsFallbackHandlerTest, DoesNotFallbackIfFieldsAreFilled) {
  ON_CALL(mock_web_controller_, OnGetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "value"));
  EXPECT_CALL(mock_action_delegate_, OnSetFieldValue(_, _, _)).Times(0);

  std::vector<RequiredField> required_fields = {
      CreateRequiredField("${51}", {"#card_name"})};

  RequiredFieldsFallbackHandler fallback_handler(required_fields, {},
                                                 &mock_action_delegate_);
  fallback_handler.CheckAndFallbackRequiredFields(
      OkClientStatus(),
      base::BindOnce([](const ClientStatus& status,
                        const base::Optional<ClientStatus>& detail_status) {
        EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
      }));
}

TEST_F(RequiredFieldsFallbackHandlerTest, FillsEmptyRequiredField) {
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(_, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), ""));
  Selector expected_selector({"#card_name"});
  Expectation set_value =
      EXPECT_CALL(
          mock_action_delegate_,
          OnSetFieldValue("John Doe",
                          EqualsElement(test_util::MockFindElement(
                              mock_action_delegate_, expected_selector)),
                          _))
          .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(_, _))
      .After(set_value)
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "John Doe"));

  std::vector<RequiredField> required_fields = {
      CreateRequiredField("${51}", {"#card_name"})};

  std::map<std::string, std::string> fallback_values = {
      {base::NumberToString(
           static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_NAME_FULL)),
       "John Doe"}};

  RequiredFieldsFallbackHandler fallback_handler(
      required_fields, fallback_values, &mock_action_delegate_);
  fallback_handler.CheckAndFallbackRequiredFields(
      OkClientStatus(),
      base::BindOnce([](const ClientStatus& status,
                        const base::Optional<ClientStatus>& detail_status) {
        EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
      }));
}

TEST_F(RequiredFieldsFallbackHandlerTest, FallsBackForForcedFilledField) {
  ON_CALL(mock_web_controller_, OnGetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "value"));
  Selector expected_selector({"#card_name"});
  EXPECT_CALL(mock_action_delegate_,
              OnSetFieldValue("John Doe",
                              EqualsElement(test_util::MockFindElement(
                                  mock_action_delegate_, expected_selector)),
                              _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));

  std::vector<RequiredField> required_fields = {
      CreateRequiredField("${51}", {"#card_name"})};
  required_fields[0].forced = true;

  std::map<std::string, std::string> fallback_values = {
      {base::NumberToString(
           static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_NAME_FULL)),
       "John Doe"}};

  RequiredFieldsFallbackHandler fallback_handler(
      required_fields, fallback_values, &mock_action_delegate_);
  fallback_handler.CheckAndFallbackRequiredFields(
      OkClientStatus(),
      base::BindOnce([](const ClientStatus& status,
                        const base::Optional<ClientStatus>& detail_status) {
        EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
      }));
}

TEST_F(RequiredFieldsFallbackHandlerTest, FailsIfForcedFieldDidNotGetFilled) {
  ON_CALL(mock_web_controller_, OnGetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "value"));
  EXPECT_CALL(mock_action_delegate_, OnSetFieldValue(_, _, _)).Times(0);

  std::vector<RequiredField> required_fields = {
      CreateRequiredField("${51}", {"#card_name"})};
  required_fields[0].forced = true;

  RequiredFieldsFallbackHandler fallback_handler(required_fields, {},
                                                 &mock_action_delegate_);

  base::OnceCallback<void(const ClientStatus&,
                          const base::Optional<ClientStatus>&)>
      callback =
          base::BindOnce([](const ClientStatus& status,
                            const base::Optional<ClientStatus>& detail_status) {
            EXPECT_EQ(status.proto_status(), AUTOFILL_INCOMPLETE);
            ASSERT_TRUE(detail_status.has_value());
            ASSERT_EQ(detail_status.value().proto_status(),
                      AUTOFILL_INCOMPLETE);
            ASSERT_EQ(detail_status.value()
                          .details()
                          .autofill_error_info()
                          .autofill_field_error_size(),
                      1);
            EXPECT_EQ(detail_status.value()
                          .details()
                          .autofill_error_info()
                          .autofill_field_error(0)
                          .value_expression(),
                      "${51}");
            EXPECT_TRUE(detail_status.value()
                            .details()
                            .autofill_error_info()
                            .autofill_field_error(0)
                            .no_fallback_value());
          });

  fallback_handler.CheckAndFallbackRequiredFields(OkClientStatus(),
                                                  std::move(callback));
}

TEST_F(RequiredFieldsFallbackHandlerTest, FillsFieldWithPattern) {
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(_, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), ""));
  Selector expected_selector({"#card_expiry"});
  Expectation set_value =
      EXPECT_CALL(
          mock_action_delegate_,
          OnSetFieldValue("08/2050",
                          EqualsElement(test_util::MockFindElement(
                              mock_action_delegate_, expected_selector)),
                          _))
          .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(_, _))
      .After(set_value)
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "not empty"));

  std::vector<RequiredField> required_fields = {
      CreateRequiredField("${53}/${55}", {"#card_expiry"})};

  std::map<std::string, std::string> fallback_values = {
      {base::NumberToString(
           static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_EXP_MONTH)),
       "08"},
      {base::NumberToString(static_cast<int>(
           autofill::ServerFieldType::CREDIT_CARD_EXP_4_DIGIT_YEAR)),
       "2050"}};

  RequiredFieldsFallbackHandler fallback_handler(
      required_fields, fallback_values, &mock_action_delegate_);
  fallback_handler.CheckAndFallbackRequiredFields(
      OkClientStatus(),
      base::BindOnce([](const ClientStatus& status,
                        const base::Optional<ClientStatus>& detail_status) {
        EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
      }));
}

TEST_F(RequiredFieldsFallbackHandlerTest,
       FailsToFillFieldWithUnknownOrEmptyKey) {
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(_, _))
      .Times(2)
      .WillRepeatedly(RunOnceCallback<1>(OkClientStatus(), ""));
  EXPECT_CALL(mock_action_delegate_, OnSetFieldValue(_, _, _)).Times(0);

  std::vector<RequiredField> required_fields = {
      CreateRequiredField("${53}", {"#card_expiry"}),
      CreateRequiredField("${-3}", {"#card_network"})};

  std::map<std::string, std::string> fallback_values;
  fallback_values.emplace(base::NumberToString(static_cast<int>(
                              AutofillFormatProto::CREDIT_CARD_NETWORK)),
                          "");

  RequiredFieldsFallbackHandler fallback_handler(
      required_fields, fallback_values, &mock_action_delegate_);

  base::OnceCallback<void(const ClientStatus&,
                          const base::Optional<ClientStatus>&)>
      callback =
          base::BindOnce([](const ClientStatus& status,
                            const base::Optional<ClientStatus>& detail_status) {
            EXPECT_EQ(status.proto_status(), AUTOFILL_INCOMPLETE);
            ASSERT_TRUE(detail_status.has_value());
            ASSERT_EQ(detail_status.value().proto_status(),
                      AUTOFILL_INCOMPLETE);
            ASSERT_EQ(detail_status.value()
                          .details()
                          .autofill_error_info()
                          .autofill_field_error_size(),
                      2);
            EXPECT_EQ(detail_status.value()
                          .details()
                          .autofill_error_info()
                          .autofill_field_error(0)
                          .value_expression(),
                      "${53}");
            EXPECT_TRUE(detail_status.value()
                            .details()
                            .autofill_error_info()
                            .autofill_field_error(0)
                            .no_fallback_value());
            EXPECT_EQ(detail_status.value()
                          .details()
                          .autofill_error_info()
                          .autofill_field_error(1)
                          .value_expression(),
                      "${-3}");
            EXPECT_TRUE(detail_status.value()
                            .details()
                            .autofill_error_info()
                            .autofill_field_error(1)
                            .no_fallback_value());
          });

  fallback_handler.CheckAndFallbackRequiredFields(OkClientStatus(),
                                                  std::move(callback));
}

TEST_F(RequiredFieldsFallbackHandlerTest, UsesSelectOptionForDropdowns) {
  Selector expected_selector({"#year"});
  const ElementFinder::Result& expected_element =
      test_util::MockFindElement(mock_action_delegate_, expected_selector);
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(expected_selector, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), ""));
  EXPECT_CALL(mock_action_delegate_,
              GetElementTag(EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "SELECT"));
  Expectation select_option =
      EXPECT_CALL(
          mock_action_delegate_,
          SelectOption("2050", DropdownSelectStrategy::LABEL_STARTS_WITH,
                       EqualsElement(expected_element), _))
          .WillOnce(RunOnceCallback<3>(OkClientStatus()));
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(expected_selector, _))
      .After(select_option)
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "2050"));

  std::vector<RequiredField> required_fields = {
      CreateRequiredField("${55}", {"#year"})};

  std::map<std::string, std::string> fallback_values = {
      {base::NumberToString(static_cast<int>(
           autofill::ServerFieldType::CREDIT_CARD_EXP_4_DIGIT_YEAR)),
       "2050"}};

  RequiredFieldsFallbackHandler fallback_handler(
      required_fields, fallback_values, &mock_action_delegate_);
  fallback_handler.CheckAndFallbackRequiredFields(
      OkClientStatus(),
      base::BindOnce([](const ClientStatus& status,
                        const base::Optional<ClientStatus>& detail_status) {
        EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
      }));
}

TEST_F(RequiredFieldsFallbackHandlerTest, ClicksOnCustomDropdown) {
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(_, _)).Times(0);
  EXPECT_CALL(mock_action_delegate_, OnSetFieldValue(_, _, _)).Times(0);
  Selector expected_main_selector({"#card_expiry"});
  EXPECT_CALL(
      mock_action_delegate_,
      ClickOrTapElement(ClickType::TAP,
                        EqualsElement(test_util::MockFindElement(
                            mock_action_delegate_, expected_main_selector)),
                        _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  Selector expected_option_selector({".option"});
  expected_option_selector.MatchingInnerText("08");
  expected_option_selector.MustBeVisible();
  EXPECT_CALL(mock_action_delegate_,
              OnShortWaitForElement(expected_option_selector, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus()));
  EXPECT_CALL(
      mock_action_delegate_,
      ClickOrTapElement(ClickType::TAP,
                        EqualsElement(test_util::MockFindElement(
                            mock_action_delegate_, expected_option_selector)),
                        _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));

  std::vector<RequiredField> required_fields = {
      CreateRequiredField("${53}", {"#card_expiry"})};
  required_fields[0].fallback_click_element = Selector({".option"});

  std::map<std::string, std::string> fallback_values = {
      {base::NumberToString(
           static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_EXP_MONTH)),
       "08"}};

  RequiredFieldsFallbackHandler fallback_handler(
      required_fields, fallback_values, &mock_action_delegate_);
  fallback_handler.CheckAndFallbackRequiredFields(
      OkClientStatus(),
      base::BindOnce([](const ClientStatus& status,
                        const base::Optional<ClientStatus>& detail_status) {
        EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
      }));
}

TEST_F(RequiredFieldsFallbackHandlerTest, CustomDropdownClicksStopOnError) {
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(_, _)).Times(0);
  EXPECT_CALL(mock_action_delegate_, OnSetFieldValue(_, _, _)).Times(0);
  Selector expected_main_selector({"#card_expiry"});
  Expectation main_click =
      EXPECT_CALL(
          mock_action_delegate_,
          ClickOrTapElement(ClickType::TAP,
                            EqualsElement(test_util::MockFindElement(
                                mock_action_delegate_, expected_main_selector)),
                            _))
          .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  Selector expected_option_selector({".option"});
  expected_option_selector.MatchingInnerText("08");
  expected_option_selector.MustBeVisible();
  EXPECT_CALL(mock_action_delegate_,
              OnShortWaitForElement(expected_option_selector, _))
      .WillOnce(RunOnceCallback<1>(ClientStatus(ELEMENT_RESOLUTION_FAILED)));
  EXPECT_CALL(mock_action_delegate_, FindElement(_, _))
      .Times(0)
      .After(main_click);
  EXPECT_CALL(mock_action_delegate_, ClickOrTapElement(_, _, _))
      .Times(0)
      .After(main_click);

  std::vector<RequiredField> required_fields = {
      CreateRequiredField("${53}", {"#card_expiry"})};
  required_fields[0].fallback_click_element = Selector({".option"});

  std::map<std::string, std::string> fallback_values = {
      {base::NumberToString(
           static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_EXP_MONTH)),
       "08"}};

  RequiredFieldsFallbackHandler fallback_handler(
      required_fields, fallback_values, &mock_action_delegate_);
  fallback_handler.CheckAndFallbackRequiredFields(
      OkClientStatus(),
      base::BindOnce([](const ClientStatus& status,
                        const base::Optional<ClientStatus>& detail_status) {
        EXPECT_EQ(status.proto_status(), AUTOFILL_INCOMPLETE);
      }));
}

TEST_F(RequiredFieldsFallbackHandlerTest, ClearsFilledFields) {
  Selector full_field_selector({"#full_field"});
  Selector empty_field_selector({"#empty_field"});
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(full_field_selector, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "value"));
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(empty_field_selector, _))
      .Times(0);

  Expectation clear_full_value =
      EXPECT_CALL(
          mock_action_delegate_,
          OnSetFieldValue("",
                          EqualsElement(test_util::MockFindElement(
                              mock_action_delegate_, full_field_selector)),
                          _))
          .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(full_field_selector, _))
      .After(clear_full_value)
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), ""));
  Expectation clear_empty_value =
      EXPECT_CALL(
          mock_action_delegate_,
          OnSetFieldValue("",
                          EqualsElement(test_util::MockFindElement(
                              mock_action_delegate_, empty_field_selector)),
                          _))
          .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(empty_field_selector, _))
      .After(clear_empty_value)
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), ""));

  auto non_forced_field = CreateRequiredField("", {"#full_field"});
  auto forced_field = CreateRequiredField("", {"#empty_field"});
  forced_field.forced = true;
  std::vector<RequiredField> required_fields = {non_forced_field, forced_field};

  std::map<std::string, std::string> fallback_values;

  RequiredFieldsFallbackHandler fallback_handler(
      required_fields, fallback_values, &mock_action_delegate_);
  fallback_handler.CheckAndFallbackRequiredFields(
      OkClientStatus(),
      base::BindOnce([](const ClientStatus& status,
                        const base::Optional<ClientStatus>& detail_status) {
        EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
      }));
}

}  // namespace
}  // namespace autofill_assistant
