// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_trigger_source.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/filling/test_form_filler.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map_test_utils.h"
#include "components/autofill/core/browser/heuristic_source.h"
#include "components/autofill/core/browser/payments/credit_card_cvc_authenticator.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/credit_card_network_identifiers.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-data-view.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::testing::AtLeast;
using ::testing::Contains;
using ::testing::DoAll;
using ::testing::Each;
using ::testing::Eq;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::Optional;
using ::testing::Property;
using ::testing::Return;
using ::testing::SaveArg;

constexpr char kAppLocale[] = "en-US";

// Action `SaveArgElementsTo<k>(pointer)` saves the value pointed to by the
// `k`th (0-based) argument of the mock function by moving it to `*pointer`.
ACTION_TEMPLATE(SaveArgElementsTo,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(pointer)) {
  auto span = testing::get<k>(args);
  pointer->assign(span.begin(), span.end());
}

class MockAutofillClient : public TestAutofillClient {
 public:
  MockAutofillClient() = default;
  MockAutofillClient(const MockAutofillClient&) = delete;
  MockAutofillClient& operator=(const MockAutofillClient&) = delete;
  ~MockAutofillClient() override = default;

  MOCK_METHOD(void,
              DidFillForm,
              (AutofillTriggerSource trigger_source, bool is_refill),
              (override));
};

class MockAutofillDriver : public TestAutofillDriver {
 public:
  using TestAutofillDriver::TestAutofillDriver;
  MockAutofillDriver(const MockAutofillDriver&) = delete;
  MockAutofillDriver& operator=(const MockAutofillDriver&) = delete;

  // Mock methods to enable testability.
  MOCK_METHOD((base::flat_set<FieldGlobalId>),
              ApplyFormAction,
              (mojom::FormActionType action_type,
               mojom::ActionPersistence action_persistence,
               base::span<const FormFieldData> data,
               const url::Origin& triggered_origin,
               (const base::flat_map<FieldGlobalId, FieldType>&),
               (const Section&)),
              (override));
  MOCK_METHOD(void,
              ApplyFieldAction,
              (mojom::FieldActionType text_replacement,
               mojom::ActionPersistence action_persistence,
               const FieldGlobalId& field_id,
               const std::u16string& value),
              (override));
};

auto HasValue(std::u16string value) {
  return Property("FormFieldData::value", &FormFieldData::value,
                  std::move(value));
}

// Takes a FormFieldData argument.
auto AutofilledWith(std::u16string value) {
  return AllOf(Property("FormFieldData::is_autofilled",
                        &FormFieldData::is_autofilled, true),
               Property("FormFieldData::value", &FormFieldData::value,
                        std::move(value)));
}

// Takes an AutofillField argument.
MATCHER_P(AutofilledWithProfile, profile, "") {
  return arg->is_autofilled() && arg->autofill_source_profile_guid() &&
         *arg->autofill_source_profile_guid() == profile.guid();
}

}  // namespace
// The anonymous namespace needs to end here because of `friend`ships between
// the tests and the production code.

class FormFillerTest
    : public testing::Test,
      public WithTestAutofillClientDriverManager<NiceMock<MockAutofillClient>,
                                                 MockAutofillDriver> {
 public:
  void SetUp() override {
    // Advance the mock clock to a fixed, arbitrary, somewhat recent date.
    // This is needed to fill CC expiry dates.
    base::Time year2020;
    ASSERT_TRUE(base::Time::FromString("01/01/20", &year2020));
    task_environment_.FastForwardBy(year2020 - AutofillClock::Now());

    InitAutofillClient();
    payments_autofill_client().set_payments_network_interface(
        std::make_unique<payments::TestPaymentsNetworkInterface>(
            autofill_client().GetURLLoaderFactory(),
            autofill_client().GetIdentityManager(),
            &autofill_client().GetPersonalDataManager()));
    CreateAutofillDriver();

    // Mandatory re-auth is required for credit card autofill on automotive, so
    // the authenticator response needs to be properly mocked.
#if BUILDFLAG(IS_ANDROID)
    payments_autofill_client()
        .SetUpDeviceBiometricAuthenticatorSuccessOnAutomotive();
#endif
  }

  void TearDown() override { DeleteAllAutofillDrivers(); }

  void FormsSeen(const std::vector<FormData>& forms) {
    autofill_manager().OnFormsSeen(/*updated_forms=*/forms,
                                   /*removed_forms=*/{});
  }

  FormData FormSeen(test::FormDescription form_description) {
    FormData form = test::GetFormData(form_description);
    autofill_manager().AddSeenForm(form,
                                   test::GetHeuristicTypes(form_description),
                                   test::GetServerTypes(form_description));
    return form;
  }

  FormFiller& form_filler() {
    return test_api(autofill_manager()).form_filler();
  }

  FormStructure* GetFormStructure(const FormData& form) {
    return autofill_manager().FindCachedFormById(form.global_id());
  }

  AutofillField* GetAutofillField(const FormGlobalId& form_id,
                                  const FieldGlobalId& field_id) {
    return autofill_manager().GetAutofillField(form_id, field_id);
  }

  // Lets `BrowserAutofillManager` fill `form` using `trigger`` and
  // returns `form` as it would be extracted from the renderer afterwards, i.e.,
  // with the autofilled `FormFieldData::value`s.
  FormData ApplyFormAction(
      FormData form,
      base::FunctionRef<void(const FormData& form)> trigger) {
    std::vector<FormFieldData> filled_fields;
    std::vector<FieldGlobalId> global_ids;
    for (const FormFieldData& field : form.fields()) {
      global_ids.push_back(field.global_id());
    }
    // After the call, `filled_fields` will only contain the fields that were
    // autofilled in this call of FillOrPreviewForm (% fields not filled due
    // to the iframe security policy).
    EXPECT_CALL(autofill_driver(), ApplyFormAction)
        .WillOnce(
            DoAll(SaveArgElementsTo<2>(&filled_fields), Return(global_ids)))
        .WillRepeatedly({});
    trigger(form);
    // Copy the filled data into the form.
    for (FormFieldData& field : test_api(form).fields()) {
      if (auto it = std::ranges::find(filled_fields, field.global_id(),
                                      &FormFieldData::global_id);
          it != filled_fields.end()) {
        field = *it;
      }
    }
    return form;
  }

  // Lets `BrowserAutofillManager` fill `form` with `filling_payload` and
  // returns `form` as it would be extracted from the renderer afterwards, i.e.,
  // with the autofilled `FormFieldData::value`s.
  FormData AutofillForm(
      FormData form,
      const FormFieldData& trigger_field,
      FillingPayload filling_payload,
      AutofillTriggerSource trigger_source = AutofillTriggerSource::kPopup) {
    return ApplyFormAction(std::move(form), [&](const FormData& form) {
      form_filler().FillOrPreviewForm(
          mojom::ActionPersistence::kFill, form, filling_payload,
          *GetFormStructure(form),
          *GetAutofillField(form.global_id(), trigger_field.global_id()),
          trigger_source);
    });
  }

  // Lets `BrowserAutofillManager` undo the last filling operation performed on
  // `trigger_field`, which belongs to `form`, and returns `form` as it would be
  // extracted from the renderer afterwards, i.e., with the autofilled
  // `FormFieldData::value`s.
  FormData UndoAutofill(FormData form, const FormFieldData& trigger_field) {
    return ApplyFormAction(std::move(form), [&](const FormData& form) {
      autofill_manager().UndoAutofill(mojom::ActionPersistence::kFill, form,
                                      trigger_field);
    });
  }

  // Lets `BrowserAutofillManager` fill `trigger_field` with `value` and
  // modifies `form` to reflect this filling.
  FormData FillField(FormData form,
                     const FormFieldData& trigger_field,
                     FillingProduct filling_product,
                     std::u16string value) {
    form_filler().FillOrPreviewField(
        mojom::ActionPersistence::kFill, mojom::FieldActionType::kReplaceAll,
        trigger_field,
        GetAutofillField(form.global_id(), trigger_field.global_id()), value,
        filling_product, /*field_type_used=*/std::nullopt);

    FormFieldData& field =
        *std::ranges::find(test_api(form).fields(), trigger_field.global_id(),
                           &FormFieldData::global_id);
    field.set_is_autofilled(true);
    field.set_value(value);
    return form;
  }

  std::vector<FormFieldData> PreviewVirtualCardDataAndGetResults(
      const FormData& form,
      const FormFieldData& field,
      const CreditCard& virtual_card) {
    std::vector<FormFieldData> filled_fields;
    EXPECT_CALL(autofill_driver(), ApplyFormAction)
        .WillOnce((DoAll(SaveArgElementsTo<2>(&filled_fields),
                         Return(std::vector<FieldGlobalId>{}))));
    form_filler().FillOrPreviewForm(
        mojom::ActionPersistence::kPreview, form, &virtual_card,
        *GetFormStructure(form),
        *GetAutofillField(form.global_id(), field.global_id()),
        AutofillTriggerSource::kPopup);
    return filled_fields;
  }

  // Convenience method to cast the FullCardRequest into a CardUnmaskDelegate.
  CardUnmaskDelegate* full_card_unmask_delegate() {
    payments::FullCardRequest* full_card_request =
        payments_autofill_client()
            .GetCvcAuthenticator()
            .full_card_request_.get();
    DCHECK(full_card_request);
    return static_cast<CardUnmaskDelegate*>(full_card_request);
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::AutofillUnitTestEnvironment autofill_test_environment_;
};

// Test that the correct section is filled.
TEST_F(FormFillerTest, FillTriggeredSection) {
  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FULL, .autocomplete_attribute = "name"},
                  {.role = NAME_FULL, .autocomplete_attribute = "name"}}});
  FormsSeen({form});
  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);

  // Assign different sections to the fields.
  base::flat_map<LocalFrameToken, size_t> frame_token_ids;
  for (const std::unique_ptr<AutofillField>& field : form_structure->fields()) {
    field->set_section(Section::FromFieldIdentifier(*field, frame_token_ids));
  }

  AutofillProfile profile = test::GetFullProfile();
  AutofillForm(form, form.fields()[1], &profile);

  form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);
  EXPECT_FALSE(form_structure->field(0)->is_autofilled());
  EXPECT_TRUE(form_structure->field(1)->is_autofilled());
}

// Test that if the form cache is outdated because a field has changed, filling
// is aborted after that field.
TEST_F(FormFillerTest, DoNotFillIfFormFieldChanged) {
  FormData form = test::CreateTestAddressFormData();
  FormsSeen({form});
  test_api(form).field(-1) = FormFieldData();

  AutofillProfile profile = test::GetFullProfile();
  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields().front(), &profile).fields();

  EXPECT_THAT(filled_fields.back(), HasValue(u""));
  filled_fields.pop_back();
  EXPECT_THAT(filled_fields, Each(Not(HasValue(u""))));
}

// Test that if the form cache is outdated because the form has changed, filling
// is aborted because of that change.
TEST_F(FormFillerTest, DoNotFillIfFormChanged) {
  FormData form = test::CreateTestAddressFormData();
  FormsSeen({form});
  test_api(form).Remove(-1);

  EXPECT_CALL(autofill_driver(), ApplyFormAction).Times(0);
  AutofillProfile profile = test::GetFullProfile();
  form_filler().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, form, &profile, *GetFormStructure(form),
      *GetAutofillField(form.global_id(), form.fields().front().global_id()),
      AutofillTriggerSource::kPopup);
}

TEST_F(FormFillerTest, SkipPreFilledFields) {
  base::test::ScopedFeatureList placeholders_features(
      features::kAutofillSkipPreFilledFields);

  AutofillProfile profile = test::GetFullProfile();
  const std::u16string kToBeFilledState =
      profile.GetInfo(ADDRESS_HOME_STATE, kAppLocale);
  const std::u16string kSelectedState = u"NC (filled)";
  FormData form = test::GetFormData(
      {.fields = {
           {.role = NAME_FIRST, .value = u"Triggering field (filled)"},
           {.role = NAME_LAST, .value = u"Placeholder (skipped)"},
           {.role = EMAIL_ADDRESS, .value = u"No data (skipped)"},
           {.role = ADDRESS_HOME_LINE1, .value = u"No placeholder (skipped)"},
           {.role = ADDRESS_HOME_STATE,
            .value = kSelectedState,
            .form_control_type = FormControlType::kSelectOne,
            .select_options = {SelectOption{.value = kSelectedState,
                                            .text = kSelectedState},
                               SelectOption{.value = kToBeFilledState,
                                            .text = kToBeFilledState}}},
           // Value initialized with whitespace-only, expect field to be filled.
           {.role = ADDRESS_HOME_COUNTRY, .value = u" "}}});
  FormsSeen({form});

  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields().front(), &profile).fields();

  EXPECT_THAT(filled_fields[0],
              AutofilledWith(profile.GetInfo(NAME_FIRST, kAppLocale)));
  EXPECT_FALSE(filled_fields[1].is_autofilled());
  EXPECT_EQ(filled_fields[1].value(), form.fields()[1].value());
  EXPECT_FALSE(filled_fields[2].is_autofilled());
  EXPECT_EQ(filled_fields[2].value(), form.fields()[2].value());
  EXPECT_FALSE(filled_fields[3].is_autofilled());
  EXPECT_EQ(filled_fields[3].value(), form.fields()[3].value());
  EXPECT_THAT(filled_fields[4], AutofilledWith(kToBeFilledState));
  EXPECT_THAT(filled_fields[5], AutofilledWith(profile.GetInfo(
                                    ADDRESS_HOME_COUNTRY, kAppLocale)));
}

TEST_F(FormFillerTest, UndoSavesFormFillingData) {
  FormData form = test::CreateTestAddressFormData();
  FormsSeen({form});

  base::flat_set<FieldGlobalId> safe_fields{form.fields().front().global_id()};
  EXPECT_CALL(autofill_driver(), ApplyFormAction)
      .Times(2)
      .WillRepeatedly(Return(safe_fields));

  AutofillProfile profile = test::GetFullProfile();
  form_filler().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, form, &profile, *GetFormStructure(form),
      *GetAutofillField(form.global_id(), form.fields().front().global_id()),
      AutofillTriggerSource::kPopup);
  // Undo early returns if it has no filling history for the trigger field,
  // which is initially empty, therefore calling the driver is proof that data
  // was successfully stored.
  autofill_manager().UndoAutofill(mojom::ActionPersistence::kFill, form,
                                  form.fields().front());
}

TEST_F(FormFillerTest, UndoSavesFormFillingDataForAutofillAi) {
  FormData form = FormSeen(
      {.fields = {
           {.server_type = PASSPORT_NUMBER},
           {.server_type = NO_SERVER_DATA, .heuristic_type = NAME_FULL},
           {.server_type = PASSPORT_ISSUING_COUNTRY,
            .heuristic_type = ADDRESS_HOME_COUNTRY},
           {.server_type = IBAN_VALUE, .heuristic_type = IBAN_VALUE},
           {.server_type = UNKNOWN_TYPE, .heuristic_type = UNKNOWN_TYPE}}});

  auto safe_fields = base::MakeFlatSet<FieldGlobalId>(
      form.fields(), {}, &FormFieldData::global_id);
  EXPECT_CALL(autofill_driver(), ApplyFormAction)
      .Times(2)
      .WillRepeatedly(Return(safe_fields));

  EntityInstance passport = test::GetPassportEntityInstance();
  autofill_manager().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, form, form.fields().front().global_id(),
      &passport, AutofillTriggerSource::kAutofillAi);
  autofill_manager().UndoAutofill(mojom::ActionPersistence::kFill, form,
                                  form.fields().front());
}

TEST_F(FormFillerTest, UndoPreviewDoesNotChangeTheCache) {
  FormData form = test::CreateTestAddressFormData();
  FormsSeen({form});
  AutofillField* autofill_field =
      GetAutofillField(form.global_id(), form.fields().front().global_id());
  AutofillProfile profile = test::GetFullProfile();

  EXPECT_CALL(autofill_driver(), ApplyFormAction)
      .WillRepeatedly(
          Return(base::flat_set<FieldGlobalId>{autofill_field->global_id()}));

  form_filler().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, form, &profile, *GetFormStructure(form),
      *autofill_field, AutofillTriggerSource::kPopup);
  ASSERT_TRUE(autofill_field->is_autofilled());

  // A preview of the undo operation won't reset the autofill state.
  autofill_manager().UndoAutofill(mojom::ActionPersistence::kPreview, form,
                                  form.fields().front());
  EXPECT_TRUE(autofill_field->is_autofilled());

  // An actual undo operation will reset the autofill state.
  autofill_manager().UndoAutofill(mojom::ActionPersistence::kFill, form,
                                  form.fields().front());
  EXPECT_FALSE(autofill_field->is_autofilled());
}

TEST_F(FormFillerTest, UndoSavesFieldByFieldFillingData) {
  FormData form = test::CreateTestAddressFormData();
  FormsSeen({form});

  EXPECT_CALL(autofill_driver(), ApplyFieldAction);
  autofill_manager().FillOrPreviewField(
      mojom::ActionPersistence::kFill, mojom::FieldActionType::kReplaceAll,
      form, form.fields().front(), u"Some Name",
      SuggestionType::kAddressFieldByFieldFilling, NAME_FULL);
  // Undo early returns if it has no filling history for the trigger field,
  // which is initially empty, therefore calling the driver is proof that data
  // was successfully stored.
  EXPECT_CALL(autofill_driver(), ApplyFormAction);
  autofill_manager().UndoAutofill(mojom::ActionPersistence::kFill, form,
                                  form.fields().front());
}

TEST_F(FormFillerTest, UndoResetsCachedAutofillState) {
  FormData form = test::CreateTestAddressFormData();

  AutofillField filled_autofill_field(form.fields().front());
  test_api(form).field(0).set_is_autofilled(false);
  test_api(form_filler())
      .AddFormFillingEntry(
          std::to_array<const FormFieldData*>({&form.fields().front()}),
          std::to_array<const AutofillField*>({&filled_autofill_field}),
          FillingProduct::kAddress, /*is_refill=*/false);

  test_api(form).field(0).set_is_autofilled(true);
  FormsSeen({form});

  const AutofillField* autofill_field =
      GetAutofillField(form.global_id(), form.fields().front().global_id());
  ASSERT_TRUE(autofill_field->is_autofilled());
  autofill_manager().UndoAutofill(mojom::ActionPersistence::kFill, form,
                                  form.fields().front());
  EXPECT_FALSE(autofill_field->is_autofilled());
}

TEST_F(FormFillerTest, FillOrPreviewFormCallsDidFillForm) {
  FormData form = test::CreateTestAddressFormData();
  FormsSeen({form});

  AutofillProfile profile = test::GetFullProfile();
  EXPECT_CALL(autofill_client(), DidFillForm);
  AutofillForm(form, form.fields().front(), &profile);
}

// Tests that for autocomplete=unrecognized fields are not filled by default,
// but are filled if they are the filling trigger field.
TEST_F(FormFillerTest,
       FillAddressForm_AutocompleteUnrecognizedFillingBehavior) {
  // Create a form where the middle name field has autocomplete=unrecognized.
  FormData form = test::GetFormData(
      {.fields = {
           {.role = NAME_FIRST, .autocomplete_attribute = "given-name"},
           {.role = NAME_MIDDLE, .autocomplete_attribute = "unrecognized"},
           {.role = NAME_LAST, .autocomplete_attribute = "family-name"}}});
  FormsSeen({form});
  FormStructure* form_structure = GetFormStructure(form);
  form_structure->field(1)->SetTypeTo(AutofillType(NAME_MIDDLE),
                                      AutofillPredictionSource::kHeuristics);
  ASSERT_EQ(form_structure->field(1)->html_type(),
            HtmlFieldType::kUnrecognized);

  // Fill `form` from the first name field and expect that the middle name field
  // isn't filled and the rest is.
  AutofillProfile profile = test::GetFullProfile();
  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields()[0], &profile).fields();
  EXPECT_THAT(filled_fields[0],
              AutofilledWith(profile.GetInfo(NAME_FIRST, kAppLocale)));
  EXPECT_FALSE(filled_fields[1].is_autofilled());
  EXPECT_THAT(filled_fields[2],
              AutofilledWith(profile.GetInfo(NAME_LAST, kAppLocale)));

  // Fill `form` from the middle name field and expect that all fields are
  // filled.
  filled_fields = AutofillForm(form, form.fields()[1], &profile).fields();

  EXPECT_THAT(filled_fields[0],
              AutofilledWith(profile.GetInfo(NAME_FIRST, kAppLocale)));
  EXPECT_THAT(filled_fields[1],
              AutofilledWith(profile.GetInfo(NAME_MIDDLE, kAppLocale)));
  EXPECT_THAT(filled_fields[2],
              AutofilledWith(profile.GetInfo(NAME_LAST, kAppLocale)));
}

// Test that we correctly fill a credit card form.
TEST_F(FormFillerTest, FillCreditCardForm_Simple) {
  FormData form = test::CreateTestCreditCardFormData(/*is_https=*/true,
                                                     /*use_month_type=*/false);
  FormsSeen({form});

  CreditCard credit_card = test::GetCreditCard();
  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields().front(), &credit_card).fields();
  ASSERT_EQ(filled_fields.size(), 5u);
  EXPECT_THAT(filled_fields[0], AutofilledWith(credit_card.GetInfo(
                                    CREDIT_CARD_NAME_FULL, kAppLocale)));
  EXPECT_THAT(filled_fields[1], AutofilledWith(credit_card.GetInfo(
                                    CREDIT_CARD_NUMBER, kAppLocale)));
  EXPECT_THAT(filled_fields[2], AutofilledWith(credit_card.GetInfo(
                                    CREDIT_CARD_EXP_MONTH, kAppLocale)));
  EXPECT_THAT(filled_fields[3], AutofilledWith(credit_card.GetInfo(
                                    CREDIT_CARD_EXP_4_DIGIT_YEAR, kAppLocale)));
  EXPECT_FALSE(filled_fields[4].is_autofilled());
}

// Test that whitespace and separators are stripped from the credit card number.
TEST_F(FormFillerTest, FillCreditCardForm_StripCardNumber) {
  CreditCard credit_card_whitespace;
  test::SetCreditCardInfo(&credit_card_whitespace, "Elvis Presley",
                          "4234 5678 9012 3456",  // Visa
                          "04", "2999", "1");
  CreditCard credit_card_separator;
  test::SetCreditCardInfo(&credit_card_separator, "Elvis Presley",
                          "4234-5678-9012-3456",  // Visa
                          "04", "2999", "1");
  FormData form =
      test::GetFormData({.fields = {{.autocomplete_attribute = "cc-number"}}});
  FormsSeen({form});

  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields().front(), &credit_card_whitespace)
          .fields();
  EXPECT_THAT(filled_fields[0], AutofilledWith(u"4234567890123456"));

  filled_fields =
      AutofillForm(form, form.fields().front(), &credit_card_separator)
          .fields();
  EXPECT_THAT(filled_fields[0], AutofilledWith(u"4234567890123456"));
}

// Tests that when payment form fields are autofilled and payment swapping is
// enabled, the autofilled values can be replaced with empty values.
TEST_F(FormFillerTest, PaymentsSwappingWithPartiallyEmptyData) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillPaymentsFieldSwapping);

  FormData form = test::CreateTestCreditCardFormData(/*is_https=*/true,
                                                     /*use_month_type=*/false);
  FormsSeen({form});

  CreditCard credit_card_full;
  test::SetCreditCardInfo(&credit_card_full, "Elvis Presley",
                          "4234 5678 9012 3456",  // Visa
                          "04", "2999", "1");

  CreditCard credit_card_with_empty_data;
  test::SetCreditCardInfo(&credit_card_with_empty_data, "Elvis Presley New",
                          "4234-5678-9012-3456",  // Visa
                          "04", "", "1");

  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields().front(), &credit_card_full).fields();

  EXPECT_THAT(filled_fields[0], AutofilledWith(credit_card_full.GetInfo(
                                    CREDIT_CARD_NAME_FULL, kAppLocale)));
  EXPECT_THAT(filled_fields[3], AutofilledWith(credit_card_full.GetInfo(
                                    CREDIT_CARD_EXP_4_DIGIT_YEAR, kAppLocale)));
  EXPECT_TRUE(filled_fields[3].is_autofilled());

  filled_fields =
      AutofillForm(form, form.fields().front(), &credit_card_with_empty_data)
          .fields();
  EXPECT_THAT(filled_fields[0],
              AutofilledWith(credit_card_with_empty_data.GetInfo(
                  CREDIT_CARD_NAME_FULL, kAppLocale)));
  EXPECT_EQ(filled_fields[3].value(), u"");
  EXPECT_FALSE(filled_fields[3].is_autofilled());
}

struct PartialCreditCardDateParams {
  const char* cc_month = "";
  const char* cc_year = "";
  std::u16string expected_filled_date;
};

class PartialCreditCardDateTest
    : public FormFillerTest,
      public testing::WithParamInterface<PartialCreditCardDateParams> {};

INSTANTIATE_TEST_SUITE_P(
    CreditCardFormFillerTest,
    PartialCreditCardDateTest,
    testing::ValuesIn({PartialCreditCardDateParams{.expected_filled_date = u""},
                       PartialCreditCardDateParams{.cc_month = "04",
                                                   .expected_filled_date = u""},
                       PartialCreditCardDateParams{.cc_year = "2999",
                                                   .expected_filled_date = u""},
                       PartialCreditCardDateParams{
                           .cc_month = "04",
                           .cc_year = "2999",
                           .expected_filled_date = u"2999-04"}}));

// Test that we correctly fill a credit card form with month input type.
TEST_P(PartialCreditCardDateTest, FillWithPartialDate) {
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "Elvis Presley",
                          "4234567890123456",  // Visa
                          GetParam().cc_month, GetParam().cc_year, "");
  FormData form = test::CreateTestCreditCardFormData(/*is_https=*/true,
                                                     /*use_month_type=*/true);
  FormsSeen({form});

  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields().front(), &credit_card).fields();
  ASSERT_EQ(filled_fields.size(), 4u);
  EXPECT_THAT(filled_fields[0], AutofilledWith(credit_card.GetInfo(
                                    CREDIT_CARD_NAME_FULL, kAppLocale)));
  EXPECT_THAT(filled_fields[1], AutofilledWith(credit_card.GetInfo(
                                    CREDIT_CARD_NUMBER, kAppLocale)));
  EXPECT_EQ(filled_fields[2].value(), GetParam().expected_filled_date);
  EXPECT_FALSE(filled_fields[3].is_autofilled());
}

// Test that only the first 19 credit card number fields are filled.
TEST_F(FormFillerTest, FillOnlyFirstNineteenCreditCardNumberFields) {
  // Create a form with 20 credit card number fields.
  FormData form =
      test::GetFormData({.fields = std::vector<test::FieldDescription>(
                             20, {.autocomplete_attribute = "cc-number"})});
  FormsSeen({form});

  CreditCard credit_card = test::GetCreditCard();
  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields().front(), &credit_card).fields();

  // Verify that the first 19 credit card number fields are filled.
  for (size_t i = 0; i < 19; i++) {
    EXPECT_THAT(filled_fields[i], AutofilledWith(credit_card.GetInfo(
                                      CREDIT_CARD_NUMBER, kAppLocale)))
        << i;
  }
  // Verify that the 20th. credit card number field is not filled.
  EXPECT_FALSE(filled_fields.back().is_autofilled());
}

// Test the credit card number is filled correctly into single-digit fields.
TEST_F(FormFillerTest, FillCreditCardNumberIntoSingleDigitFields) {
  // Create a form with 20 credit card number fields.
  FormData form =
      test::GetFormData({.fields = std::vector<test::FieldDescription>(
                             20, {.autocomplete_attribute = "cc-number"})});
  // Set the size limit of the first nineteen fields to 1.
  for (size_t i = 0; i < 19; i++) {
    test_api(form).field(i).set_max_length(1);
  }
  FormsSeen({form});

  CreditCard credit_card = test::GetCreditCard();
  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields().front(), &credit_card).fields();

  // Verify that the first 19 card number fields are filled.
  std::u16string card_number =
      credit_card.GetInfo(CREDIT_CARD_NUMBER, kAppLocale);
  for (size_t i = 0; i < 19; i++) {
    EXPECT_THAT(filled_fields[i], AutofilledWith(i < card_number.length()
                                                     ? card_number.substr(i, 1)
                                                     : card_number))
        << i;
  }
  EXPECT_FALSE(filled_fields[19].is_autofilled());
  EXPECT_TRUE(filled_fields[19].value().empty());
}

// Test that we correctly fill a credit card form with first and last cardholder
// name.
TEST_F(FormFillerTest, FillCreditCardForm_SplitName) {
  FormData form = test::CreateTestCreditCardFormData(
      /*is_https=*/true, /*use_month_type=*/false, /*split_names=*/true);
  FormsSeen({form});

  CreditCard credit_card = test::GetCreditCard();
  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields().front(), &credit_card).fields();
  ASSERT_EQ(form.fields().size(), 6u);
  EXPECT_THAT(filled_fields[0], AutofilledWith(credit_card.GetInfo(
                                    CREDIT_CARD_NAME_FIRST, kAppLocale)));
  EXPECT_THAT(filled_fields[1], AutofilledWith(credit_card.GetInfo(
                                    CREDIT_CARD_NAME_LAST, kAppLocale)));
  EXPECT_THAT(filled_fields[2], AutofilledWith(credit_card.GetInfo(
                                    CREDIT_CARD_NUMBER, kAppLocale)));
  EXPECT_THAT(filled_fields[3], AutofilledWith(credit_card.GetInfo(
                                    CREDIT_CARD_EXP_MONTH, kAppLocale)));
  EXPECT_THAT(filled_fields[4], AutofilledWith(credit_card.GetInfo(
                                    CREDIT_CARD_EXP_4_DIGIT_YEAR, kAppLocale)));
  EXPECT_FALSE(filled_fields[5].is_autofilled());
  EXPECT_TRUE(filled_fields[5].value().empty());
}

// Test that only filled selection boxes are counted for the type filling limit.
TEST_F(FormFillerTest, OnlyCountFilledSelectionBoxesForTypeFillingLimit) {
  test::PopulateAlternativeStateNameMapForTesting(
      "US", "California",
      {{.canonical_name = "California",
        .abbreviations = {"CA"},
        .alternative_names = {}}});

  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FULL, .autocomplete_attribute = "name"}}});
  // Add 20 selection boxes that should be fillable since the correct
  // entry is present.
  for (int i = 0; i < 20; i++) {
    test_api(form).Append(
        test::CreateTestSelectField("State", "state", "", "address-level1",
                                    {"AA", "BB", "CA"}, {"AA", "BB", "CA"}));
  }
  // Add 10 other a selection box for the country.
  for (int i = 0; i < 10; ++i) {
    test_api(form).Append(
        test::CreateTestSelectField("Country", "country", "", "country",
                                    {"DE", "FR", "US"}, {"DE", "FR", "US"}));
  }
  FormsSeen({form});

  AutofillProfile profile = test::GetFullProfile();
  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields().front(), &profile).fields();

  ASSERT_EQ(filled_fields.size(), 31u);
  for (size_t i = 1; i <= 20; ++i) {
    EXPECT_THAT(filled_fields[i], AutofilledWith(u"CA")) << i;
  }
  for (size_t i = 21; i < 30; ++i) {
    EXPECT_THAT(filled_fields[i], AutofilledWith(u"US")) << i;
  }
  EXPECT_FALSE(filled_fields[30].is_autofilled());
  EXPECT_TRUE(filled_fields[30].value().empty());
}

// Test that fields with the autocomplete attribute set to off are filled.
TEST_F(FormFillerTest, FillAddressForm_AutocompleteOffFillingBehavior) {
  // Create a form where the middle name field has autocomplete=off.
  FormData form = test::GetFormData(
      {.fields = {
           {.role = NAME_FIRST, .autocomplete_attribute = "given-name"},
           {.role = NAME_MIDDLE, .autocomplete_attribute = "off"},
           {.role = NAME_LAST, .autocomplete_attribute = "family-name"}}});
  FormsSeen({form});
  FormStructure* form_structure = GetFormStructure(form);
  form_structure->field(1)->set_heuristic_type(GetActiveHeuristicSource(),
                                               NAME_MIDDLE);
  ASSERT_THAT(form_structure->field(1)->Type().GetTypes(),
              Contains(NAME_MIDDLE));

  AutofillProfile profile = test::GetFullProfile();
  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields()[0], &profile).fields();
  ASSERT_EQ(filled_fields.size(), 3u);
  EXPECT_THAT(filled_fields[0],
              AutofilledWith(profile.GetInfo(NAME_FIRST, kAppLocale)));
  EXPECT_THAT(filled_fields[1],
              AutofilledWith(profile.GetInfo(NAME_MIDDLE, kAppLocale)));
  EXPECT_THAT(filled_fields[2],
              AutofilledWith(profile.GetInfo(NAME_LAST, kAppLocale)));
}

// Test that fields with value equal to their placeholder attribute are filled.
TEST_F(FormFillerTest, FillAddressForm_PlaceholderEqualsValue) {
  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FIRST,
                   .value = u"First Name",
                   .placeholder = u"First Name",
                   .autocomplete_attribute = "given-name"},
                  {.role = NAME_MIDDLE,
                   .value = u"Middle Name",
                   .placeholder = u"Middle Name",
                   .autocomplete_attribute = "additional-name"},
                  {.role = NAME_LAST,
                   .value = u"Last Name",
                   .placeholder = u"Last Name",
                   .autocomplete_attribute = "family-name"}}});
  FormsSeen({form});

  AutofillProfile profile = test::GetFullProfile();
  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields()[0], &profile).fields();
  ASSERT_EQ(filled_fields.size(), 3u);
  EXPECT_THAT(filled_fields[0],
              AutofilledWith(profile.GetInfo(NAME_FIRST, kAppLocale)));
  EXPECT_THAT(filled_fields[1],
              AutofilledWith(profile.GetInfo(NAME_MIDDLE, kAppLocale)));
  EXPECT_THAT(filled_fields[2],
              AutofilledWith(profile.GetInfo(NAME_LAST, kAppLocale)));
}

// Test that credit card fields with unrecognized autocomplete attribute are
// filled.
TEST_F(FormFillerTest,
       FillCreditCardForm_AutocompleteUnrecognizedFillingBehavior) {
  // Create a form where the middle name field has autocomplete=off.
  FormData form = test::CreateTestCreditCardFormData(/*is_https=*/true,
                                                     /*use_month_type=*/false);
  test_api(form).field(0).set_autocomplete_attribute("unrecognized");
  FormsSeen({form});

  CreditCard credit_card = test::GetCreditCard();
  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields().front(), &credit_card).fields();
  EXPECT_THAT(filled_fields.front(), AutofilledWith(credit_card.GetInfo(
                                         CREDIT_CARD_NAME_FULL, kAppLocale)));
}

// Test that credit card fields are filled even if they have the autocomplete
// attribute set to off.
TEST_F(FormFillerTest, FillCreditCardForm_AutocompleteOffBehavior) {
  FormData form = test::CreateTestCreditCardFormData(/*is_https=*/true,
                                                     /*use_month_type=*/false);
  test_api(form).field(0).set_autocomplete_attribute("off");
  FormsSeen({form});

  CreditCard credit_card = test::GetCreditCard();
  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields().front(), &credit_card).fields();
  EXPECT_THAT(filled_fields.front(), AutofilledWith(credit_card.GetInfo(
                                         CREDIT_CARD_NAME_FULL, kAppLocale)));
}

// Test that selecting an expired credit card fills everything except the
// expiration date.
TEST_F(FormFillerTest, FillCreditCardForm_ExpiredCard) {
  CreditCard expired_card;
  test::SetCreditCardInfo(&expired_card, "Homer Simpson",
                          "4234567890654321",  // Visa
                          "05", "2000", "1");
  FormData form = test::CreateTestCreditCardFormData(/*is_https=*/true,
                                                     /*use_month_type=*/false);
  FormsSeen({form});

  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, *form.fields().begin(), &expired_card).fields();
  ASSERT_EQ(filled_fields.size(), 5u);
  EXPECT_THAT(filled_fields[0], AutofilledWith(expired_card.GetInfo(
                                    CREDIT_CARD_NAME_FULL, kAppLocale)));
  EXPECT_THAT(filled_fields[1], AutofilledWith(expired_card.GetInfo(
                                    CREDIT_CARD_NUMBER, kAppLocale)));
  EXPECT_FALSE(filled_fields[2].is_autofilled());
  EXPECT_TRUE(filled_fields[2].value().empty());
  EXPECT_FALSE(filled_fields[3].is_autofilled());
  EXPECT_TRUE(filled_fields[3].value().empty());
  EXPECT_FALSE(filled_fields[4].is_autofilled());
  EXPECT_TRUE(filled_fields[4].value().empty());
}

TEST_F(FormFillerTest, PreviewCreditCardForm_VirtualCard) {
  FormData form = test::CreateTestCreditCardFormData(/*is_https=*/true,
                                                     /*use_month_type=*/false);
  FormsSeen({form});

  CreditCard virtual_card = test::GetVirtualCard();
  std::vector<FormFieldData> filled_fields =
      PreviewVirtualCardDataAndGetResults(form, form.fields()[1], virtual_card);

  std::u16string expected_cardholder_name = u"Lorem Ipsum";
  // Virtual card number using obfuscated dots only: Virtual card Mastercard
  // ••••4444
  std::u16string expected_card_number =
      u"Virtual card Mastercard  " +
      virtual_card.ObfuscatedNumberWithVisibleLastFourDigits();
  // Virtual card expiration month using obfuscated dots: ••
  std::u16string expected_exp_month =
      CreditCard::GetMidlineEllipsisPlainDots(/*num_dots=*/2);
  // Virtual card expiration year using obfuscated dots: ••••
  std::u16string expected_exp_year =
      CreditCard::GetMidlineEllipsisPlainDots(/*num_dots=*/4);
  // Virtual card cvc using obfuscated dots: •••
  std::u16string expected_cvc =
      CreditCard::GetMidlineEllipsisPlainDots(/*num_dots=*/3);

  EXPECT_EQ(filled_fields[0].value(), expected_cardholder_name);
  EXPECT_EQ(filled_fields[1].value(), expected_card_number);
  EXPECT_EQ(filled_fields[2].value(), expected_exp_month);
  EXPECT_EQ(filled_fields[3].value(), expected_exp_year);
  EXPECT_EQ(filled_fields[4].value(), expected_cvc);
}

// Test that unfocusable fields aren't filled, except for <select> fields.
TEST_F(FormFillerTest, DoNotFillUnfocusableFieldsExceptForSelect) {
  // Create a form with both focusable and non-focusable fields.
  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FULL, .autocomplete_attribute = "name"},
                  {.role = ADDRESS_HOME_COUNTRY,
                   .autocomplete_attribute = "country"}}});
  test_api(form).field(-1).set_is_focusable(false);
  test_api(form).Append(test::CreateTestSelectField(
      "Country", "country", "", "country", {"CA", "US"},
      {"Canada", "United States"}, FormControlType::kSelectOne));
  test_api(form).field(-1).set_is_focusable(false);
  FormsSeen({form});

  AutofillProfile profile = test::GetFullProfile();
  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields()[0], &profile).fields();

  ASSERT_EQ(3u, filled_fields.size());
  EXPECT_THAT(filled_fields[0],
              AutofilledWith(profile.GetInfo(NAME_FULL, kAppLocale)));
  EXPECT_FALSE(filled_fields[1].is_autofilled());
  EXPECT_TRUE(filled_fields[1].value().empty());
  EXPECT_THAT(filled_fields[2], AutofilledWith(u"US"));
}

// Test that we correctly fill a form that has author-specified sections, which
// might not match our expected section breakdown.
TEST_F(FormFillerTest, FillFormWithAuthorSpecifiedSections) {
  // Create a form with a billing section and an unnamed section, interleaved.
  // The billing section includes both address and credit card fields.
  FormData form = test::GetFormData(
      {.fields = {
           {.role = ADDRESS_HOME_COUNTRY, .autocomplete_attribute = "country"},
           {.role = ADDRESS_HOME_LINE1,
            .autocomplete_attribute = "section-billing address-line1"},
           {.role = CREDIT_CARD_NAME_FULL,
            .autocomplete_attribute = "section-billing cc-name"},
           {.role = CREDIT_CARD_NUMBER,
            .autocomplete_attribute = "section-billing cc-number"},
           {.role = EMAIL_ADDRESS, .autocomplete_attribute = "email"}}});
  FormsSeen({form});

  // Fill the unnamed section.
  AutofillProfile profile = test::GetFullProfile();
  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields()[0], &profile).fields();
  ASSERT_EQ(filled_fields.size(), 5u);
  // TODO(crbug.com/40264633): Replace with GetInfo.
  EXPECT_THAT(filled_fields[0],
              AutofilledWith(profile.GetRawInfo(ADDRESS_HOME_COUNTRY)));
  EXPECT_FALSE(filled_fields[1].is_autofilled());
  EXPECT_TRUE(filled_fields[1].value().empty());
  EXPECT_FALSE(filled_fields[2].is_autofilled());
  EXPECT_TRUE(filled_fields[2].value().empty());
  EXPECT_FALSE(filled_fields[3].is_autofilled());
  EXPECT_TRUE(filled_fields[3].value().empty());
  EXPECT_THAT(filled_fields[4],
              AutofilledWith(profile.GetInfo(EMAIL_ADDRESS, kAppLocale)));

  // Fill the address portion of the billing section.
  filled_fields = AutofillForm(form, form.fields()[1], &profile).fields();
  ASSERT_EQ(filled_fields.size(), 5u);
  EXPECT_FALSE(filled_fields[0].is_autofilled());
  EXPECT_TRUE(filled_fields[0].value().empty());
  EXPECT_THAT(filled_fields[1],
              AutofilledWith(profile.GetInfo(ADDRESS_HOME_LINE1, kAppLocale)));
  EXPECT_FALSE(filled_fields[2].is_autofilled());
  EXPECT_TRUE(filled_fields[2].value().empty());
  EXPECT_FALSE(filled_fields[3].is_autofilled());
  EXPECT_TRUE(filled_fields[3].value().empty());
  EXPECT_FALSE(filled_fields[4].is_autofilled());
  EXPECT_TRUE(filled_fields[4].value().empty());

  // Fill the credit card portion of the billing section.
  CreditCard credit_card = test::GetCreditCard();
  filled_fields = AutofillForm(form, form.fields()[2], &credit_card).fields();
  ASSERT_EQ(filled_fields.size(), 5u);
  EXPECT_FALSE(filled_fields[0].is_autofilled());
  EXPECT_TRUE(filled_fields[0].value().empty());
  EXPECT_FALSE(filled_fields[1].is_autofilled());
  EXPECT_TRUE(filled_fields[1].value().empty());
  EXPECT_THAT(filled_fields[2], AutofilledWith(credit_card.GetInfo(
                                    CREDIT_CARD_NAME_FULL, kAppLocale)));
  EXPECT_THAT(filled_fields[3], AutofilledWith(credit_card.GetInfo(
                                    CREDIT_CARD_NUMBER, kAppLocale)));
  EXPECT_FALSE(filled_fields[4].is_autofilled());
  EXPECT_TRUE(filled_fields[4].value().empty());
}

// Test that we correctly fill a form that has a single logical section with
// multiple email address fields.
TEST_F(FormFillerTest, FillFormWithMultipleEmails) {
  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FULL, .autocomplete_attribute = "name"},
                  {.role = EMAIL_ADDRESS, .autocomplete_attribute = "email"},
                  {.role = EMAIL_ADDRESS, .autocomplete_attribute = "email"}}});
  FormsSeen({form});

  AutofillProfile profile = test::GetFullProfile();
  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields()[0], &profile).fields();
  ASSERT_EQ(filled_fields.size(), 3u);
  EXPECT_THAT(filled_fields[0],
              AutofilledWith(profile.GetInfo(NAME_FULL, kAppLocale)));
  EXPECT_THAT(filled_fields[1],
              AutofilledWith(profile.GetInfo(EMAIL_ADDRESS, kAppLocale)));
  EXPECT_THAT(filled_fields[2],
              AutofilledWith(profile.GetInfo(EMAIL_ADDRESS, kAppLocale)));
}

// Test that we correctly fill a previously autofilled address form.
TEST_F(FormFillerTest, FillAutofilledAddressForm) {
  // Set up our form data.
  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FULL, .autocomplete_attribute = "name"},
                  {.role = EMAIL_ADDRESS, .autocomplete_attribute = "email"}}});
  for (FormFieldData& field : test_api(form).fields()) {
    field.set_is_autofilled(true);
  }
  FormsSeen({form});

  AutofillProfile profile = test::GetFullProfile();
  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields().front(), &profile).fields();
  ASSERT_EQ(filled_fields.size(), 2u);
  EXPECT_THAT(filled_fields[0],
              AutofilledWith(profile.GetInfo(NAME_FULL, kAppLocale)));
  EXPECT_TRUE(filled_fields[1].value().empty());
}

// Test that we correctly fill a previously autofilled credit card form.
TEST_F(FormFillerTest, FillAutofilledCreditCardForm) {
  // Set up our form data.
  FormData form =
      test::GetFormData({.fields = {{.role = CREDIT_CARD_NAME_FULL,
                                     .autocomplete_attribute = "cc-name"},
                                    {.role = CREDIT_CARD_NUMBER,
                                     .autocomplete_attribute = "cc-number"}}});
  for (FormFieldData& field : test_api(form).fields()) {
    field.set_is_autofilled(true);
  }
  FormsSeen({form});

  CreditCard credit_card = test::GetCreditCard();
  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields().front(), &credit_card).fields();
  ASSERT_EQ(filled_fields.size(), 2u);
  EXPECT_THAT(filled_fields[0], AutofilledWith(credit_card.GetInfo(
                                    CREDIT_CARD_NAME_FULL, kAppLocale)));
  EXPECT_TRUE(filled_fields[1].value().empty());
}

// Test that we correctly fill a partly manually filled address form.
TEST_F(FormFillerTest, FillPartlyManuallyFilledAddressForm) {
  FormData form = test::GetFormData(
      {.fields = {
           {.role = NAME_FIRST, .autocomplete_attribute = "given-name"},
           {.role = NAME_MIDDLE, .autocomplete_attribute = "additional-name"},
           {.role = NAME_LAST, .autocomplete_attribute = "family-name"}}});
  // Michael will be overridden with Elvis because Autofill is triggered from
  // the first field.
  test_api(form).field(0).set_value(u"Michael");
  test_api(form).field(0).set_properties_mask(
      form.fields()[0].properties_mask() | kUserTyped);
  // Jackson will be preserved.
  test_api(form).field(2).set_value(u"Jackson");
  test_api(form).field(2).set_properties_mask(
      form.fields()[2].properties_mask() | kUserTyped);
  FormsSeen({form});

  AutofillProfile profile = test::GetFullProfile();
  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields().front(), &profile).fields();
  ASSERT_EQ(filled_fields.size(), 3u);
  EXPECT_THAT(filled_fields[0],
              AutofilledWith(profile.GetInfo(NAME_FIRST, kAppLocale)));
  EXPECT_THAT(filled_fields[1],
              AutofilledWith(profile.GetInfo(NAME_MIDDLE, kAppLocale)));
  EXPECT_FALSE(filled_fields[2].is_autofilled());
}

// Test that we correctly fill a partly manually filled credit card form.
TEST_F(FormFillerTest, FillPartlyManuallyFilledCreditCardForm) {
  FormData form = test::GetFormData(
      {.fields = {{.role = CREDIT_CARD_NAME_FIRST,
                   .autocomplete_attribute = "cc-given-name"},
                  {.role = CREDIT_CARD_NAME_LAST,
                   .autocomplete_attribute = "cc-family-name"},
                  {.role = CREDIT_CARD_NUMBER,
                   .autocomplete_attribute = "cc-number"}}});
  // Michael will be overridden with Elvis because Autofill is triggered from
  // the first field.
  test_api(form).field(0).set_value(u"Michael");
  test_api(form).field(0).set_properties_mask(
      form.fields()[0].properties_mask() | kUserTyped);
  // Jackson will be preserved.
  test_api(form).field(1).set_value(u"Jackson");
  test_api(form).field(1).set_properties_mask(
      form.fields()[1].properties_mask() | kUserTyped);
  FormsSeen({form});

  // First fill the address data.
  CreditCard credit_card = test::GetCreditCard();
  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields().front(), &credit_card).fields();
  ASSERT_EQ(filled_fields.size(), 3u);
  EXPECT_THAT(filled_fields[0], AutofilledWith(credit_card.GetInfo(
                                    CREDIT_CARD_NAME_FIRST, kAppLocale)));
  EXPECT_FALSE(filled_fields[1].is_autofilled());
  EXPECT_THAT(filled_fields[2], AutofilledWith(credit_card.GetInfo(
                                    CREDIT_CARD_NUMBER, kAppLocale)));
}

// Test that we correctly fill a phone number split across multiple fields.
TEST_F(FormFillerTest, FillPhoneNumber) {
  // In one form, rely on the max length attribute to imply US phone number
  // parts. In the other form, rely on the autocomplete type attribute.
  FormData form_with_us_number_max_length;
  form_with_us_number_max_length.set_renderer_id(test::MakeFormRendererId());
  form_with_us_number_max_length.set_name(u"MyMaxlengthPhoneForm");
  form_with_us_number_max_length.set_url(
      GURL("https://myform.com/phone_form.html"));
  form_with_us_number_max_length.set_action(
      GURL("https://myform.com/phone_submit.html"));
  FormData form_with_autocompletetype = form_with_us_number_max_length;
  form_with_autocompletetype.set_renderer_id(test::MakeFormRendererId());
  form_with_autocompletetype.set_name(u"MyAutocompletetypePhoneForm");

  struct {
    const char* label;
    const char* name;
    size_t max_length;
    const char* autocomplete_attribute;
  } test_fields[] = {{"country code", "country_code", 1, "tel-country-code"},
                     {"area code", "area_code", 3, "tel-area-code"},
                     {"phone", "phone_prefix", 3, "tel-local-prefix"},
                     {"-", "phone_suffix", 4, "tel-local-suffix"}};

  constexpr uint64_t default_max_length = 0;
  for (const auto& test_field : test_fields) {
    FormFieldData field = test::CreateTestFormField(
        test_field.label, test_field.name, "", FormControlType::kInputText, "",
        test_field.max_length);
    test_api(form_with_us_number_max_length).Append(field);

    field.set_max_length(default_max_length);
    field.set_autocomplete_attribute(test_field.autocomplete_attribute);
    field.set_parsed_autocomplete(
        ParseAutocompleteAttribute(test_field.autocomplete_attribute));
    test_api(form_with_autocompletetype).Append(field);
  }

  FormsSeen({form_with_us_number_max_length, form_with_autocompletetype});

  // We should be able to fill prefix and suffix fields for US numbers.
  AutofillProfile profile = test::GetFullProfile();
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"16505554567");
  FormData filled_form1 =
      AutofillForm(form_with_us_number_max_length,
                   form_with_us_number_max_length.fields().front(), &profile);
  ASSERT_EQ(4u, filled_form1.fields().size());
  EXPECT_EQ(u"1", filled_form1.fields()[0].value());
  EXPECT_EQ(u"650", filled_form1.fields()[1].value());
  EXPECT_EQ(u"555", filled_form1.fields()[2].value());
  EXPECT_EQ(u"4567", filled_form1.fields()[3].value());

  FormData filled_form2 =
      AutofillForm(form_with_autocompletetype,
                   form_with_autocompletetype.fields().front(), &profile);
  ASSERT_EQ(4u, filled_form2.fields().size());
  EXPECT_EQ(u"1", filled_form2.fields()[0].value());
  EXPECT_EQ(u"650", filled_form2.fields()[1].value());
  EXPECT_EQ(u"555", filled_form2.fields()[2].value());
  EXPECT_EQ(u"4567", filled_form2.fields()[3].value());

  // For other countries, fill prefix and suffix fields with best effort.
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"GB");
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"447700954321");
  FormData filled_form3 =
      AutofillForm(form_with_us_number_max_length,
                   form_with_us_number_max_length.fields().front(), &profile);
  ASSERT_EQ(4u, filled_form3.fields().size());
  EXPECT_EQ(u"4", filled_form3.fields()[0].value());
  EXPECT_EQ(u"700", filled_form3.fields()[1].value());
  EXPECT_EQ(u"95", filled_form3.fields()[2].value());
  EXPECT_EQ(u"4321", filled_form3.fields()[3].value());

  FormData filled_form4 =
      AutofillForm(form_with_autocompletetype,
                   form_with_autocompletetype.fields().front(), &profile);
  ASSERT_EQ(4u, filled_form4.fields().size());
  EXPECT_EQ(u"44", filled_form4.fields()[0].value());
  EXPECT_EQ(u"7700", filled_form4.fields()[1].value());
  EXPECT_EQ(u"95", filled_form4.fields()[2].value());
  EXPECT_EQ(u"4321", filled_form4.fields()[3].value());
}

TEST_F(FormFillerTest, FillPhoneNumber_ForPhonePrefixOrSuffix) {
  FormData form = test::GetFormData(
      {.fields = {{.role = PHONE_HOME_COUNTRY_CODE,
                   .max_length = 1,
                   .autocomplete_attribute = "tel-country-code"},
                  {.role = PHONE_HOME_CITY_CODE,
                   .max_length = 3,
                   .autocomplete_attribute = "tel-area-code"},
                  {.role = PHONE_HOME_NUMBER_PREFIX,
                   .max_length = 3,
                   .autocomplete_attribute = "tel-local-prefix"},
                  {.role = PHONE_HOME_NUMBER_SUFFIX,
                   .max_length = 4,
                   .autocomplete_attribute = "tel-local-suffix"}}});

  FormsSeen({form});

  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"1800FLOWERS");
  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields().front(), &profile).fields();

  ASSERT_EQ(4U, filled_fields.size());
  EXPECT_THAT(filled_fields[2], AutofilledWith(u"356"));
  EXPECT_THAT(filled_fields[3], AutofilledWith(u"9377"));
}

// Tests filling a phone number field with max length limit.
TEST_F(FormFillerTest, FillPhoneNumber_WithMaxLengthLimit) {
  FormData form =
      test::GetFormData({.fields = {{.role = PHONE_HOME_WHOLE_NUMBER,
                                     .max_length = 10,
                                     .autocomplete_attribute = "tel"}}});
  FormsSeen({form});

  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"+886123456789");
  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields().front(), &profile).fields();

  ASSERT_EQ(1u, filled_fields.size());
  EXPECT_THAT(filled_fields[0], AutofilledWith(u"123456789"));
}

// Tests that only the first complete number is filled when there are multiple
// componentized number fields.
TEST_F(FormFillerTest, FillFirstPhoneNumber_ComponentizedNumbers) {
  // Create a form with multiple componentized phone number fields.
  FormData form = test::GetFormData(
      {.fields = {
           {.role = NAME_FULL, .autocomplete_attribute = "name"},
           {.role = PHONE_HOME_COUNTRY_CODE,
            .autocomplete_attribute = "tel-country-code"},
           {.role = PHONE_HOME_CITY_CODE,
            .autocomplete_attribute = "tel-area-code"},
           {.role = PHONE_HOME_NUMBER, .autocomplete_attribute = "tel-local"},
           {.role = PHONE_HOME_COUNTRY_CODE,
            .autocomplete_attribute = "tel-country-code"},
           {.role = PHONE_HOME_CITY_CODE,
            .autocomplete_attribute = "tel-area-code"},
           {.role = PHONE_HOME_NUMBER,
            .autocomplete_attribute = "tel-local"}}});
  FormsSeen({form});

  AutofillProfile profile = test::GetFullProfile();
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"16505554567");
  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields().front(), &profile).fields();
  // Verify only the first complete set of phone number fields are filled.
  ASSERT_EQ(7u, filled_fields.size());
  EXPECT_EQ(u"John H. Doe", filled_fields[0].value());
  EXPECT_EQ(u"1", filled_fields[1].value());
  EXPECT_EQ(u"650", filled_fields[2].value());
  EXPECT_EQ(u"5554567", filled_fields[3].value());
  EXPECT_EQ(std::u16string(), filled_fields[4].value());
  EXPECT_EQ(std::u16string(), filled_fields[5].value());
  EXPECT_EQ(std::u16string(), filled_fields[6].value());
}

TEST_F(FormFillerTest, FillFirstPhoneNumber_WholeNumbers) {
  // Create a form with multiple complete phone number fields.
  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FULL, .autocomplete_attribute = "name"},
                  {.role = PHONE_HOME_CITY_AND_NUMBER,
                   .autocomplete_attribute = "tel-national"},
                  {.role = PHONE_HOME_CITY_AND_NUMBER,
                   .autocomplete_attribute = "tel-national"}}});
  FormsSeen({form});

  AutofillProfile profile = test::GetFullProfile();
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"16505554567");
  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields().front(), &profile).fields();
  // Verify only the first complete set of phone number fields are filled.
  ASSERT_EQ(3u, filled_fields.size());
  EXPECT_EQ(u"John H. Doe", filled_fields[0].value());
  EXPECT_EQ(u"6505554567", filled_fields[1].value());
  EXPECT_EQ(std::u16string(), filled_fields[2].value());
}

TEST_F(FormFillerTest, FillFirstPhoneNumber_FillPartsOnceOnly) {
  // Create a form with a redundant componentized phone number field.
  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FULL, .autocomplete_attribute = "name"},
                  {.role = PHONE_HOME_COUNTRY_CODE,
                   .autocomplete_attribute = "tel-country-code"},
                  {.role = PHONE_HOME_CITY_CODE,
                   .autocomplete_attribute = "tel-area-code"},
                  {.role = PHONE_HOME_CITY_AND_NUMBER,
                   .autocomplete_attribute = "tel-national"}}});
  FormsSeen({form});

  AutofillProfile profile = test::GetFullProfile();
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"16505554567");
  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields().front(), &profile).fields();
  // Verify only the first complete set of phone number fields are filled,
  // and phone components are not filled more than once.
  ASSERT_EQ(4u, filled_fields.size());
  EXPECT_EQ(u"John H. Doe", filled_fields[0].value());
  EXPECT_EQ(u"1", filled_fields[1].value());
  EXPECT_EQ(std::u16string(), filled_fields[2].value());
  EXPECT_EQ(u"6505554567", filled_fields[3].value());
}

// Verify when extension is misclassified, and there is a complete
// phone field, we do not fill anything to extension field.
TEST_F(FormFillerTest, FillFirstPhoneNumber_NotFillMisclassifiedExtention) {
  // Create a field with a misclassified phone extension field.
  FormData form = test::GetFormData(
      {.fields = {
           {.role = NAME_FULL, .autocomplete_attribute = "name"},
           {.role = PHONE_HOME_NUMBER, .autocomplete_attribute = "tel-local"},
           {.role = PHONE_HOME_EXTENSION,
            .autocomplete_attribute = "tel-local"}}});
  FormsSeen({form});

  AutofillProfile profile = test::GetFullProfile();
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"16505554567");
  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields().front(), &profile).fields();
  // Verify the misclassified extension field is not filled.
  ASSERT_EQ(3u, filled_fields.size());
  EXPECT_EQ(u"John H. Doe", filled_fields[0].value());
  EXPECT_EQ(u"5554567", filled_fields[1].value());
  EXPECT_EQ(std::u16string(), filled_fields[2].value());
}

// Verify that phone number fields annotated with the autocomplete attribute
// are filled best-effort. Phone number local heuristics only succeed if a
// PHONE_HOME_NUMBER field is present.
TEST_F(FormFillerTest, FillFirstPhoneNumber_BestEffortFilling) {
  // Create a field with incomplete phone number fields.
  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FULL, .autocomplete_attribute = "name"},
                  {.role = PHONE_HOME_CITY_CODE,
                   .autocomplete_attribute = "tel-area-code"}}});
  FormsSeen({form});

  AutofillProfile profile = test::GetFullProfile();
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"16505554567");
  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields().front(), &profile).fields();
  // Verify that we fill with best effort.
  ASSERT_EQ(2U, filled_fields.size());
  EXPECT_EQ(u"John H. Doe", filled_fields[0].value());
  EXPECT_EQ(u"650", filled_fields[1].value());
}

// When the focus is on second phone field explicitly, we will fill the
// entire form, both first phone field and second phone field included.
TEST_F(FormFillerTest, FillFirstPhoneNumber_FocusOnSecondPhoneNumber) {
  // Create a form with two complete phone number fields.
  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FULL, .autocomplete_attribute = "name"},
                  {.role = PHONE_HOME_CITY_AND_NUMBER,
                   .autocomplete_attribute = "tel-national"},
                  {.role = PHONE_HOME_CITY_AND_NUMBER,
                   .autocomplete_attribute = "tel-national"}}});
  FormsSeen({form});

  AutofillProfile profile = test::GetFullProfile();
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"16505554567");
  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields()[2], &profile).fields();
  // Verify when the second phone number field is being focused, we fill
  // that field *AND* the first phone number field.
  ASSERT_EQ(3u, filled_fields.size());
  EXPECT_EQ(u"John H. Doe", filled_fields[0].value());
  EXPECT_EQ(u"6505554567", filled_fields[1].value());
  EXPECT_EQ(u"6505554567", filled_fields[2].value());
}

TEST_F(FormFillerTest, FillFirstPhoneNumber_HiddenFieldShouldNotCount) {
  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FULL, .autocomplete_attribute = "name"},
                  {.role = PHONE_HOME_CITY_AND_NUMBER,
                   .is_focusable = false,
                   .autocomplete_attribute = "tel-national"},
                  {.role = PHONE_HOME_CITY_AND_NUMBER,
                   .autocomplete_attribute = "tel-national"}}});
  FormsSeen({form});

  AutofillProfile profile = test::GetFullProfile();
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"16505554567");
  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields().front(), &profile).fields();
  // Verify hidden/non-focusable phone field is set to only_fill_when_focused.
  ASSERT_EQ(3u, filled_fields.size());
  EXPECT_EQ(u"John H. Doe", filled_fields[0].value());
  EXPECT_EQ(std::u16string(), filled_fields[1].value());
  EXPECT_EQ(u"6505554567", filled_fields[2].value());
}

// Tests that non-focusable fields are not filled unless they are select
// elements.
TEST_F(FormFillerTest, FillNonFocusableFields) {
  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FULL, .autocomplete_attribute = "name"}}});

  // <input role="presentation"> were considered unfillable in the past but are
  // now fillable.
  test_api(form).Append(
      test::CreateTestSelectField("Country", "country", "", "country",
                                  {"CA", "US"}, {"Canada", "United States"}));
  test_api(form).field(-1).set_is_focusable(false);
  test_api(form).Append(
      test::CreateTestSelectField("State", "state", "", "address-level1",
                                  {"NY", "CA"}, {"New York", "California"}));
  test_api(form).field(-1).set_role(
      FormFieldData::RoleAttribute::kPresentation);

  test_api(form).Append(test::CreateTestFormField("City", "city", "",
                                                  FormControlType::kInputText));
  test_api(form).field(-1).set_is_focusable(false);
  test_api(form).Append(test::CreateTestFormField(
      "Street Address", "address", "", FormControlType::kInputText, "address"));
  test_api(form).field(-1).set_role(
      FormFieldData::RoleAttribute::kPresentation);
  FormsSeen({form});

  AutofillProfile profile = test::GetFullProfile();
  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields()[0], &profile).fields();

  ASSERT_EQ(filled_fields.size(), 5u);
  EXPECT_THAT(filled_fields[0],
              AutofilledWith(profile.GetInfo(NAME_FULL, kAppLocale)));
  EXPECT_THAT(filled_fields[1], AutofilledWith(u"US"));
  EXPECT_THAT(filled_fields[2], AutofilledWith(u"CA"));
  EXPECT_FALSE(filled_fields[3].is_autofilled());
  EXPECT_TRUE(filled_fields[4].is_autofilled());
}

TEST_F(FormFillerTest, FillFirstPhoneNumber_MultipleSectionFilledCorrectly) {
  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FULL, .autocomplete_attribute = "name"},
                  {.role = PHONE_HOME_CITY_AND_NUMBER,
                   .autocomplete_attribute = "tel-national"},
                  {.role = PHONE_HOME_CITY_AND_NUMBER,
                   .autocomplete_attribute = "tel-national"},
                  {.role = NAME_FULL, .autocomplete_attribute = "name"},
                  {.role = PHONE_HOME_CITY_AND_NUMBER,
                   .autocomplete_attribute = "tel-national"},
                  {.role = PHONE_HOME_CITY_AND_NUMBER,
                   .autocomplete_attribute = "tel-national"}}});
  FormsSeen({form});

  AutofillProfile profile = test::GetFullProfile();
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"16505554567");
  // Fill first section.
  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields()[0], &profile).fields();
  // Verify first section is filled with rationalization.
  ASSERT_EQ(6u, filled_fields.size());
  EXPECT_EQ(u"John H. Doe", filled_fields[0].value());
  EXPECT_EQ(u"6505554567", filled_fields[1].value());
  EXPECT_EQ(std::u16string(), filled_fields[2].value());
  EXPECT_EQ(std::u16string(), filled_fields[3].value());
  EXPECT_EQ(std::u16string(), filled_fields[4].value());
  EXPECT_EQ(std::u16string(), filled_fields[5].value());

  // Fill the second section.
  filled_fields = AutofillForm(form, form.fields()[3], &profile).fields();
  // Verify second section is filled with rationalization.
  ASSERT_EQ(6u, filled_fields.size());
  EXPECT_EQ(std::u16string(), filled_fields[0].value());
  EXPECT_EQ(std::u16string(), filled_fields[1].value());
  EXPECT_EQ(std::u16string(), filled_fields[2].value());
  EXPECT_EQ(u"John H. Doe", filled_fields[3].value());
  EXPECT_EQ(u"6505554567", filled_fields[4].value());
  EXPECT_EQ(std::u16string(), filled_fields[5].value());
}

TEST_F(FormFillerTest, FillPassportEntity) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillAiWithDataSchema);
  FormData form = test::GetFormData({.fields = {
                                         // Passport number:
                                         {.role = UNKNOWN_TYPE},
                                         // Passport first name:
                                         {.role = NAME_FIRST},
                                         // Passport last name:
                                         {.role = NAME_LAST},
                                         // Issuing country:
                                         {.role = ADDRESS_HOME_COUNTRY},
                                         // Issue date:
                                         {.role = UNKNOWN_TYPE},
                                         // Expiration date:
                                         {.role = UNKNOWN_TYPE},
                                     }});
  FormsSeen({form});

  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);
  auto set_server_type = [&](size_t field_index, auto... types) {
    form_structure->fields()[field_index]->set_server_predictions(
        {test::CreateFieldPrediction(types)...});
    std::vector<FieldType> expected_types = {types...};
    std::vector<FieldType> actual_types = base::ToVector(
        form_structure->fields()[field_index]->server_predictions(),
        [](const auto& p) {
          return ToSafeFieldType(p.type(), NO_SERVER_DATA);
        });
    CHECK(expected_types == actual_types);
  };
  auto set_format_string = [&](size_t field_index,
                               std::string_view format_string) {
    form_structure->fields()[field_index]->set_format_string_unless_overruled(
        AutofillFormatString(base::UTF8ToUTF16(format_string),
                             FormatString_Type_DATE),
        AutofillFormatStringSource::kServer);
  };
  set_server_type(0, PASSPORT_NUMBER);
  set_server_type(1, NAME_FIRST);
  set_server_type(2, NAME_LAST);
  set_server_type(3, PASSPORT_ISSUING_COUNTRY);
  set_server_type(4, PASSPORT_ISSUE_DATE);
  set_format_string(4, "M/YY");
  set_server_type(5, PASSPORT_EXPIRATION_DATE);
  set_format_string(5, "DD/MM/YYYY");

  EntityInstance passport = test::GetPassportEntityInstance();

  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields()[0], &passport).fields();
  EXPECT_EQ(filled_fields[0].value(), u"LR1234567");
  EXPECT_EQ(filled_fields[1].value(), u"Pippi");
  EXPECT_EQ(filled_fields[2].value(), u"Långstrump");
  EXPECT_EQ(filled_fields[3].value(), u"Sweden");
  EXPECT_EQ(filled_fields[4].value(), u"9/10");
  EXPECT_EQ(filled_fields[5].value(), u"30/08/2019");
}

// Test that we can still fill a form when a field has been removed from it.
TEST_F(FormFillerTest, FormChangesRemoveField) {
  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FULL, .autocomplete_attribute = "name"},
                  {.role = EMAIL_ADDRESS, .autocomplete_attribute = "email"},
                  {.role = PHONE_HOME_WHOLE_NUMBER,
                   .autocomplete_attribute = "tel"}}});
  FormsSeen({form});
  test_api(form).Remove(-1);
  FormsSeen({form});

  AutofillProfile profile = test::GetFullProfile();
  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields()[0], &profile).fields();
  ASSERT_EQ(filled_fields.size(), 2u);
  EXPECT_THAT(filled_fields[0],
              AutofilledWith(profile.GetInfo(NAME_FULL, kAppLocale)));
  EXPECT_THAT(filled_fields[1],
              AutofilledWith(profile.GetInfo(EMAIL_ADDRESS, kAppLocale)));
}

// Test that we can still fill a form when a field has been added to it.
TEST_F(FormFillerTest, FormChangesAddField) {
  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FULL, .autocomplete_attribute = "name"}}});
  FormsSeen({form});
  test_api(form).Append(test::CreateTestFormField(
      "email", "email", "", FormControlType::kInputText, "email"));
  FormsSeen({form});

  AutofillProfile profile = test::GetFullProfile();
  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields()[0], &profile).fields();
  ASSERT_EQ(filled_fields.size(), 2u);
  EXPECT_THAT(filled_fields[0],
              AutofilledWith(profile.GetInfo(NAME_FULL, kAppLocale)));
  EXPECT_THAT(filled_fields[1],
              AutofilledWith(profile.GetInfo(EMAIL_ADDRESS, kAppLocale)));
}

// Test that we can still fill a form when the visibility of some fields
// changes.
TEST_F(FormFillerTest, FormChangesVisibilityOfFields) {
  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FULL, .autocomplete_attribute = "name"},
                  {.role = ADDRESS_HOME_LINE1,
                   .autocomplete_attribute = "address-line1"},
                  {.role = ADDRESS_HOME_ZIP,
                   .is_focusable = false,
                   .autocomplete_attribute = "postal-code"},
                  {.role = ADDRESS_HOME_COUNTRY,
                   .is_focusable = false,
                   .autocomplete_attribute = "country"}}});
  FormsSeen({form});

  // Fill the form with the first profile. The hidden fields will not get
  // filled.
  AutofillProfile profile = test::GetFullProfile();
  FormData filled_form = AutofillForm(form, form.fields()[0], &profile);

  ASSERT_EQ(4u, filled_form.fields().size());
  EXPECT_THAT(filled_form.fields()[0],
              AutofilledWith(profile.GetInfo(NAME_FULL, kAppLocale)));
  EXPECT_THAT(filled_form.fields()[1],
              AutofilledWith(profile.GetInfo(ADDRESS_HOME_LINE1, kAppLocale)));
  EXPECT_FALSE(filled_form.fields()[2].is_autofilled());
  EXPECT_FALSE(filled_form.fields()[3].is_autofilled());

  // Two other fields will show up. Select the second profile. The fields that
  // were already filled, would be left unchanged, and the rest would be filled
  // with the second profile.
  test_api(filled_form).field(2).set_is_focusable(true);
  test_api(filled_form).field(3).set_is_focusable(true);

  // Reparse the form to validate the visibility changes. Fast forward so that
  // no refill is triggered automatically.
  task_environment_.FastForwardBy(base::Seconds(5));
  FormsSeen({filled_form});

  AutofillProfile profile2 = test::GetFullProfile2();
  std::vector<FormFieldData> later_filled_fields =
      AutofillForm(filled_form, filled_form.fields()[2], &profile2).fields();
  ASSERT_EQ(4u, later_filled_fields.size());
  EXPECT_THAT(later_filled_fields[0],
              AutofilledWith(profile.GetInfo(NAME_FULL, kAppLocale)));
  EXPECT_THAT(later_filled_fields[1],
              AutofilledWith(profile.GetInfo(ADDRESS_HOME_LINE1, kAppLocale)));
  EXPECT_THAT(later_filled_fields[2],
              AutofilledWith(profile2.GetInfo(ADDRESS_HOME_ZIP, kAppLocale)));
  // TODO(crbug.com/40264633): Replace with GetInfo.
  EXPECT_THAT(later_filled_fields[3],
              AutofilledWith(profile2.GetRawInfo(ADDRESS_HOME_COUNTRY)));
}

// Test that fields will be assigned with the source profile that was used for
// autofill.
TEST_F(FormFillerTest, TrackFillingOrigin) {
  FormData form = test::GetFormData(
      {.fields = {
           {.role = NAME_FIRST, .autocomplete_attribute = "given-name"},
           {.role = NAME_MIDDLE, .autocomplete_attribute = "additional-name"},
           {.role = NAME_LAST, .autocomplete_attribute = "family-name"},
           {.role = EMAIL_ADDRESS, .autocomplete_attribute = "email"}}});
  FormsSeen({form});

  AutofillProfile profile = test::GetFullProfile();
  AutofillForm(form, form.fields()[0], &profile);

  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);
  ASSERT_EQ(form_structure->field_count(), 4u);
  EXPECT_THAT(form_structure->field(0), AutofilledWithProfile(profile));
  EXPECT_THAT(form_structure->field(1), AutofilledWithProfile(profile));
  EXPECT_THAT(form_structure->field(2), AutofilledWithProfile(profile));
  EXPECT_THAT(form_structure->field(3), AutofilledWithProfile(profile));
}

// Test that filling with multiple autofill profiles will set different source
// profiles for fields.
TEST_F(FormFillerTest, TrackFillingOriginWithUsingMultipleProfiles) {
  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FIRST, .autocomplete_attribute = "given-name"},
                  {.role = NAME_LAST, .autocomplete_attribute = "family-name"},
                  {.role = EMAIL_ADDRESS, .autocomplete_attribute = "email"}}});
  FormsSeen({form});

  // Fill the form with a profile without email
  AutofillProfile profile1 = test::GetFullProfile();
  profile1.ClearFields({EMAIL_ADDRESS});
  FormData filled_form = AutofillForm(form, form.fields()[0], &profile1);

  // Check that the email field has no filling source.
  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);
  ASSERT_EQ(form.fields()[2].label(), u"E-mail address");
  EXPECT_EQ(form_structure->field(2)->autofill_source_profile_guid(),
            std::nullopt);

  // Then fill the email field using the second profile
  AutofillProfile profile2 = test::GetFullProfile2();
  AutofillForm(filled_form, form.fields()[2], &profile2);

  // Check that the first three fields have the first profile as filling source
  // and the last field has the second profile.
  form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);
  ASSERT_EQ(form_structure->field_count(), 3u);
  EXPECT_THAT(form_structure->field(0), AutofilledWithProfile(profile1));
  EXPECT_THAT(form_structure->field(1), AutofilledWithProfile(profile1));
  EXPECT_THAT(form_structure->field(2), AutofilledWithProfile(profile2));
}

// Test that an autofilled and edited field will be assigned with the autofill
// profile.
TEST_F(FormFillerTest, TrackFillingOriginOnEditedField) {
  FormData form = test::GetFormData(
      {.fields = {
           {.role = NAME_FIRST, .autocomplete_attribute = "given-name"},
           {.role = NAME_LAST, .autocomplete_attribute = "family-name"}}});
  FormsSeen({form});

  AutofillProfile profile = test::GetFullProfile();
  FormData filled_form = AutofillForm(form, form.fields()[0], &profile);

  // Simulate editing the first field.
  test_api(filled_form).field(0).set_value(u"");
  autofill_manager().OnTextFieldValueChanged(
      filled_form, filled_form.fields()[0].global_id(), base::TimeTicks::Now());

  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);
  ASSERT_TRUE(form_structure->field(0)->previously_autofilled());
  EXPECT_FALSE(form_structure->field(0)->is_autofilled());
  EXPECT_THAT(form_structure->field(0)->autofill_source_profile_guid(),
              Optional(profile.guid()));
  EXPECT_THAT(form_structure->field(1), AutofilledWithProfile(profile));
}

// Regression test that a field with an unrelated type doesn't cause a crash
// (crbug.com/324811625).
TEST_F(FormFillerTest, PreFilledCCFieldInAddressFormDoesNotCauseCrash) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillSkipPreFilledFields);
  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FULL,
                   .value = u"pre-filled",
                   .autocomplete_attribute = "section-billing name"},
                  {.role = CREDIT_CARD_NUMBER,
                   .value = u"pre-filled",
                   .autocomplete_attribute = "section-billing cc-number"}}});
  FormsSeen({form});

  AutofillProfile profile = test::GetFullProfile();
  AutofillForm(form, form.fields().front(), &profile);
  // Expect that this test doesn't cause a crash.
}

class MockFormFiller : public TestFormFiller {
 public:
  MockFormFiller(BrowserAutofillManager& manager) : TestFormFiller(manager) {}
  MOCK_METHOD(void,
              ScheduleRefill,
              (const FormData& form,
               RefillContext& refill_context,
               AutofillTriggerSource trigger_source,
               RefillTriggerReason refill_trigger_reason),
              (override));
};

class RefillTest : public FormFillerTest {
 public:
  void SetUp() override {
    FormFillerTest::SetUp();
    test_api(autofill_manager())
        .set_form_filler(std::make_unique<MockFormFiller>(autofill_manager()));
  }

  MockFormFiller& mock_form_filler() {
    return static_cast<MockFormFiller&>(form_filler());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(RefillTest, SelectOptionsChanged_IrrelevantSelectField) {
  AutofillProfile profile = test::GetFullProfile();
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndDisableFeature(
        features::kAutofillFewerTrivialRefills);
    FormData form = test::GetFormData(
        {.fields = {
             {.role = NAME_FULL, .autocomplete_attribute = "name"},
             {.form_control_type = mojom::FormControlType::kSelectOne}}});
    FormsSeen({form});
    AutofillForm(form, form.fields().front(), &profile);
    EXPECT_CALL(mock_form_filler(), ScheduleRefill).Times(1);
    autofill_manager().OnSelectFieldOptionsDidChange(
        form, form.fields().back().global_id());
  }
  {
    base::test::ScopedFeatureList scoped_feature_list{
        features::kAutofillFewerTrivialRefills};
    FormData form = test::GetFormData(
        {.fields = {
             {.role = NAME_FULL, .autocomplete_attribute = "name"},
             {.form_control_type = mojom::FormControlType::kSelectOne}}});
    FormsSeen({form});
    AutofillForm(form, form.fields().front(), &profile);
    EXPECT_CALL(mock_form_filler(), ScheduleRefill).Times(0);
    autofill_manager().OnSelectFieldOptionsDidChangeImpl(
        form, form.fields().back().global_id());
  }
}

// The following Refill Tests ensure that Autofill can handle the situation
// where it fills a credit card form with an expiration date like 04/2999
// and the website tries to reformat the input with whitespaces around the
// slash and then sacrifices the wrong digits in the expiration date. I.e.,
// the website replaces "04/2099" with "04 / 20". The tests ensure that this
// triggers a refill with "04 / 29".
struct RefillTestCase {
  // The value that JavaScript owned by the website sets for the expiration
  // date filed.
  std::u16string exp_date_from_js;
  // Whether we expect a refill from in this test case.
  bool triggers_refill;
  // What value we expect in the refill.
  std::u16string refilled_exp_date = u"";
};

class ExpirationDateRefillTest
    : public FormFillerTest,
      public testing::WithParamInterface<RefillTestCase> {};

TEST_P(ExpirationDateRefillTest, RefillJavascriptModifiedExpirationDates) {
  RefillTestCase test_case = GetParam();
  FormData form = test::GetFormData(
      {.fields = {
           {.role = CREDIT_CARD_NAME_FULL, .autocomplete_attribute = "cc-name"},
           {.role = CREDIT_CARD_NUMBER, .autocomplete_attribute = "cc-number"},
           {.role = CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
            .autocomplete_attribute = "cc-exp"}}});
  FormsSeen({form});

  // Simulate filling and store the data to be filled in |first_fill_data|.
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "Elvis Presley",
                          "4234567890123456",  // Visa
                          "04", "2999", "1");
  FormData first_fill_data =
      AutofillForm(form, form.fields().front(), &credit_card);
  ASSERT_EQ(3u, first_fill_data.fields().size());
  EXPECT_THAT(first_fill_data.fields()[0], AutofilledWith(u"Elvis Presley"));
  EXPECT_THAT(first_fill_data.fields()[1], AutofilledWith(u"4234567890123456"));
  EXPECT_THAT(first_fill_data.fields()[2], AutofilledWith(u"04/2999"));

  std::vector<FormFieldData> refilled_fields;
  if (test_case.triggers_refill) {
    // Prepare intercepting the filling operation to the driver and capture
    // the re-filled form data.
    EXPECT_CALL(autofill_driver(), ApplyFormAction)
        .WillOnce(DoAll(SaveArgElementsTo<2>(&refilled_fields),
                        Return(std::vector<FieldGlobalId>{})));
  } else {
    EXPECT_CALL(autofill_driver(), ApplyFormAction).Times(0);
  }

  // Simulate that JavaScript modifies the expiration date field.
  FormData form_after_js_modification = first_fill_data;
  test_api(form_after_js_modification)
      .field(2)
      .set_value(test_case.exp_date_from_js);
  autofill_manager().OnJavaScriptChangedAutofilledValue(
      form_after_js_modification,
      form_after_js_modification.fields()[2].global_id(), u"04/2999");

  testing::Mock::VerifyAndClearExpectations(&autofill_driver());

  if (test_case.triggers_refill) {
    ASSERT_EQ(1u, refilled_fields.size());
    // The first two fields aren't filled since their values do not change, so
    // they're removed from refilled_fields`. Therefore the only field in
    // `refilled_fields` corresponds the the third field in `form`.
    EXPECT_EQ(refilled_fields[0].global_id(), form.fields()[2].global_id());
    EXPECT_THAT(refilled_fields[0],
                AutofilledWith(test_case.refilled_exp_date));
    EXPECT_TRUE(refilled_fields[0].force_override());
  }
}

INSTANTIATE_TEST_SUITE_P(
    CreditCardFormFillerTest,
    ExpirationDateRefillTest,
    testing::Values(
        // This is the classic case: Autofill filled 04/2999, website overrode
        // 04 / 29, we need to fix this to 04 / 99.
        RefillTestCase{.exp_date_from_js = u"04 / 29",
                       .triggers_refill = true,
                       .refilled_exp_date = u"04 / 99"},
        // Maybe the website replaced the separator and added whitespaces.
        RefillTestCase{.exp_date_from_js = u"04 - 29",
                       .triggers_refill = true,
                       .refilled_exp_date = u"04 - 99"},
        // Maybe the website only replaced the separator.
        RefillTestCase{.exp_date_from_js = u"04-29",
                       .triggers_refill = true,
                       .refilled_exp_date = u"04-99"},
        // Maybe the website was smart and dropped the correct digits.
        RefillTestCase{
            .exp_date_from_js = u"04 / 99",
            .triggers_refill = false,
        },
        // Maybe the website did not modify the values at all.
        RefillTestCase{
            .exp_date_from_js = u"04/2999",
            .triggers_refill = false,
        },
        // Maybe the website did something we don't support.
        RefillTestCase{
            .exp_date_from_js = u"April / 2999",
            .triggers_refill = false,
        },
        // Maybe the website just added some whitespaces.
        RefillTestCase{
            .exp_date_from_js = u"04 / 2999",
            .triggers_refill = false,
        },
        // Don't trigger refill on 3 digit years.
        RefillTestCase{
            .exp_date_from_js = u"04 / 299",
            .triggers_refill = false,
        }));

// Test that, if after an initial form filling, some field is autofilled again,
// Undoing the first filling operation doesn't change that field.
TEST_F(FormFillerTest, UndoSkipsFieldsAutofilledFurther) {
  FormData form = test::GetFormData(
      {.fields = {
           {.role = NAME_FIRST, .autocomplete_attribute = "given-name"},
           {.role = NAME_LAST, .autocomplete_attribute = "family-name"}}});
  FormsSeen({form});
  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);

  // Fill the form with an address profile.
  AutofillProfile profile1 = test::GetFullProfile();
  form = AutofillForm(form, form.fields()[0], &profile1);
  EXPECT_THAT(form.fields()[0], AutofilledWith(u"John"));
  EXPECT_THAT(form.fields()[1], AutofilledWith(u"Doe"));

  // Simulate a field swapping operation on the second field.
  form = FillField(form, form.fields()[1], FillingProduct::kAddress, u"Other");
  EXPECT_THAT(form.fields()[0], AutofilledWith(u"John"));
  EXPECT_THAT(form.fields()[1], AutofilledWith(u"Other"));

  // Now Undo the first filling operation on the first field.
  form = UndoAutofill(form, form.fields()[0]);
  EXPECT_TRUE(form.fields()[0].value().empty());
  EXPECT_FALSE(form.fields()[0].is_autofilled());
  // The second field should not change, because the last operation that
  // modified it isn't the one that is being currently undone.
  EXPECT_THAT(form.fields()[1], AutofilledWith(u"Other"));
}

// Regression test for crbug.com/416019464
TEST_F(FormFillerTest, MultipleUndoOperations) {
  FormData form = test::GetFormData(
      {.fields = {
           {.role = NAME_FIRST, .autocomplete_attribute = "given-name"},
           {.role = NAME_LAST, .autocomplete_attribute = "family-name"}}});
  FormsSeen({form});
  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);

  // Fill the form with an address profile.
  AutofillProfile profile1 = test::GetFullProfile();
  form = AutofillForm(form, form.fields()[0], &profile1);
  EXPECT_THAT(form.fields()[0], AutofilledWith(u"John"));
  EXPECT_THAT(form.fields()[1], AutofilledWith(u"Doe"));

  // Simulate a field swapping operation on the second field.
  form = FillField(form, form.fields()[1], FillingProduct::kAddress, u"Other");
  EXPECT_THAT(form.fields()[0], AutofilledWith(u"John"));
  EXPECT_THAT(form.fields()[1], AutofilledWith(u"Other"));

  // Now Undo the first filling operation on the first field.
  form = UndoAutofill(form, form.fields()[0]);
  // The first field should be cleared, as this was its initial state.
  EXPECT_TRUE(form.fields()[0].value().empty());
  EXPECT_FALSE(form.fields()[0].is_autofilled());
  // The second field should not change.
  EXPECT_THAT(form.fields()[1], AutofilledWith(u"Other"));

  // Now Undo the second filling operation on the second field.
  form = UndoAutofill(form, form.fields()[1]);
  EXPECT_TRUE(form.fields()[0].value().empty());
  EXPECT_FALSE(form.fields()[0].is_autofilled());
  // The second field should restore the value of the first filling operation.
  EXPECT_THAT(form.fields()[1], AutofilledWith(u"Doe"));

  // Now Undo the first filling operation on the second field.
  form = UndoAutofill(form, form.fields()[1]);
  EXPECT_TRUE(form.fields()[0].value().empty());
  EXPECT_FALSE(form.fields()[0].is_autofilled());
  // The second field should be cleared, as this was its initial state.
  EXPECT_TRUE(form.fields()[1].value().empty());
  EXPECT_FALSE(form.fields()[1].is_autofilled());
}

// Tests that Undoing a filling operation on a field discards other fields that
// changed filling product (i.e. were autofilled afterwards using some other
// filling product).
TEST_F(FormFillerTest, UndoDiscardsFieldsThatChangedFillingProduct) {
  FormData form = test::GetFormData(
      {.fields = {
           {.role = NAME_FIRST, .autocomplete_attribute = "given-name"},
           {.role = NAME_LAST, .autocomplete_attribute = "family-name"}}});
  FormsSeen({form});
  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);

  // Fill the form with an address profile.
  AutofillProfile profile1 = test::GetFullProfile();
  form = AutofillForm(form, form.fields()[0], &profile1);
  EXPECT_THAT(form.fields()[0], AutofilledWith(u"John"));
  EXPECT_THAT(form.fields()[1], AutofilledWith(u"Doe"));

  // Simulate a field swapping operation on the second field.
  form = FillField(form, form.fields()[1], FillingProduct::kAutocomplete,
                   u"Other");
  EXPECT_THAT(form.fields()[0], AutofilledWith(u"John"));
  EXPECT_THAT(form.fields()[1], AutofilledWith(u"Other"));

  // Now Undo the first filling operation on the first field.
  form = UndoAutofill(form, form.fields()[0]);
  EXPECT_TRUE(form.fields()[0].value().empty());
  EXPECT_FALSE(form.fields()[0].is_autofilled());
  EXPECT_THAT(form.fields()[1], AutofilledWith(u"Other"));
}

}  // namespace autofill
