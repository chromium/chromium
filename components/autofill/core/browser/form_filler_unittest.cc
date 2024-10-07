// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_form_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/browser_autofill_manager_test_api.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map_test_utils.h"
#include "components/autofill/core/browser/heuristic_source.h"
#include "components/autofill/core/browser/payments/credit_card_cvc_authenticator.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/credit_card_network_identifiers.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace autofill {
namespace {

using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::Each;
using ::testing::Eq;
using ::testing::Field;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::Optional;
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
              DidFillOrPreviewForm,
              (mojom::ActionPersistence action_persistence,
               AutofillTriggerSource trigger_source,
               bool is_refill),
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
               (const base::flat_map<FieldGlobalId, FieldType>&)),
              (override));
  MOCK_METHOD(void,
              ApplyFieldAction,
              (mojom::FieldActionType text_replacement,
               mojom::ActionPersistence action_persistence,
               const FieldGlobalId& field_id,
               const std::u16string& value),
              (override));
};

MATCHER_P(HasValue, value, "") {
  return arg.value() == value;
}

// Takes a FormFieldData argument.
MATCHER_P(AutofilledWith, value, "") {
  return arg.is_autofilled() && arg.value() == value;
}

// Takes an AutofillField argument.
MATCHER_P(AutofilledWithProfile, profile, "") {
  return arg->is_autofilled() && arg->autofill_source_profile_guid() &&
         *arg->autofill_source_profile_guid() == profile.guid();
}

}  // namespace
// The anonymous namespace needs to end here because of `friend`ships between
// the tests and the production code.

class FormFillerTest : public testing::Test {
 public:
  void SetUp() override {
    // Advance the mock clock to a fixed, arbitrary, somewhat recent date.
    // This is needed to fill CC expiry dates.
    base::Time year2020;
    ASSERT_TRUE(base::Time::FromString("01/01/20", &year2020));
    task_environment_.FastForwardBy(year2020 - AutofillClock::Now());

    autofill_client_.GetPaymentsAutofillClient()
        ->set_test_payments_network_interface(
            std::make_unique<payments::TestPaymentsNetworkInterface>(
                autofill_client_.GetURLLoaderFactory(),
                autofill_client_.GetIdentityManager(),
                autofill_client_.GetPersonalDataManager()));
    browser_autofill_manager_ =
        std::make_unique<TestBrowserAutofillManager>(&autofill_driver_);

    // Mandatory re-auth is required for credit card autofill on automotive, so
    // the authenticator response needs to be properly mocked.
#if BUILDFLAG(IS_ANDROID)
    autofill_client_.GetPaymentsAutofillClient()
        ->SetUpDeviceBiometricAuthenticatorSuccessOnAutomotive();
#endif
  }

  void TearDown() override { browser_autofill_manager_.reset(); }

  void FormsSeen(const std::vector<FormData>& forms) {
    browser_autofill_manager_->OnFormsSeen(/*updated_forms=*/forms,
                                           /*removed_forms=*/{});
  }

  FormStructure* GetFormStructure(const FormData& form) {
    return browser_autofill_manager_->FindCachedFormById(form.global_id());
  }

  const AutofillField* GetAutofillField(const FormData& form,
                                        const FormFieldData& field) {
    return browser_autofill_manager_->GetAutofillField(form, field);
  }

  // Lets `BrowserAutofillManager` fill `form` with `profile_or_credit_card` and
  // returns `form` as it would be extracted from the renderer afterwards, i.e.,
  // with the autofilled `FormFieldData::value`s.
  FormData FillAutofillFormData(
      FormData form,
      const FormFieldData& trigger_field,
      absl::variant<const AutofillProfile*, const CreditCard*>
          profile_or_credit_card,
      AutofillTriggerDetails trigger_details = {
          .trigger_source = AutofillTriggerSource::kPopup}) {
    std::vector<FormFieldData> filled_fields;
    std::vector<FieldGlobalId> global_ids;
    for (const FormFieldData& field : form.fields()) {
      global_ids.push_back(field.global_id());
    }
    // After the call, `filled_fields` will only contain the fields that were
    // autofilled in this call of FillOrPreviewFooForm (% fields not filled due
    // to the iframe security policy).
    EXPECT_CALL(autofill_driver_, ApplyFormAction)
        .WillOnce(
            DoAll(SaveArgElementsTo<2>(&filled_fields), Return(global_ids)));
    if (const AutofillProfile** profile =
            absl::get_if<const AutofillProfile*>(&profile_or_credit_card)) {
      browser_autofill_manager_->FillOrPreviewProfileForm(
          mojom::ActionPersistence::kFill, form, trigger_field, **profile,
          trigger_details);
    } else {
      browser_autofill_manager_->FillOrPreviewCreditCardForm(
          mojom::ActionPersistence::kFill, form, trigger_field,
          *absl::get<const CreditCard*>(profile_or_credit_card), /*cvc=*/u"",
          trigger_details);
    }
    // Copy the filled data into the form.
    for (FormFieldData& field : test_api(form).fields()) {
      if (auto it = base::ranges::find(filled_fields, field.global_id(),
                                       &FormFieldData::global_id);
          it != filled_fields.end()) {
        field = *it;
      }
    }
    return form;
  }

  std::vector<FormFieldData> PreviewVirtualCardDataAndGetResults(
      const FormData& input_form,
      const FormFieldData& input_field,
      const CreditCard& virtual_card) {
    std::vector<FormFieldData> filled_fields;
    EXPECT_CALL(autofill_driver_, ApplyFormAction)
        .WillOnce((DoAll(SaveArgElementsTo<2>(&filled_fields),
                         Return(std::vector<FieldGlobalId>{}))));
    browser_autofill_manager_->FillOrPreviewCreditCardForm(
        mojom::ActionPersistence::kPreview, input_form, input_field,
        virtual_card, std::u16string(),
        {.trigger_source = AutofillTriggerSource::kPopup});
    return filled_fields;
  }

  void PrepareForRealPanResponse() {
    // This line silences the warning from PaymentsNetworkInterface about
    // matching sync and Payments server types.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        "sync-url", "https://google.com");

    FormData form = test::CreateTestCreditCardFormData(true, false);
    FormsSeen({form});
    CreditCard card =
        CreditCard(CreditCard::RecordType::kMaskedServerCard, "a123");
    test::SetCreditCardInfo(&card, "John Dillinger", "1881" /* Visa */, "01",
                            "2017", "1");
    card.SetNetworkForMaskedCard(kVisaCard);

    EXPECT_CALL(autofill_driver_, ApplyFormAction).Times(AtLeast(1));
    browser_autofill_manager_->AuthenticateThenFillCreditCardForm(
        form, form.fields().front(), card,
        {.trigger_source = AutofillTriggerSource::kPopup});
  }

  void OnDidGetRealPan(
      payments::PaymentsAutofillClient::PaymentsRpcResult result,
      const std::string& real_pan,
      bool is_virtual_card = false) {
    payments::FullCardRequest* full_card_request =
        browser_autofill_manager_->client()
            .GetPaymentsAutofillClient()
            ->GetCvcAuthenticator()
            .full_card_request_.get();
    DCHECK(full_card_request);

    // Mock user response.
    payments::FullCardRequest::UserProvidedUnmaskDetails details;
    details.cvc = u"123";
    full_card_request->OnUnmaskPromptAccepted(details);

    // Mock payments response.
    payments::PaymentsNetworkInterface::UnmaskResponseDetails response;
    response.card_type = is_virtual_card ? payments::PaymentsAutofillClient::
                                               PaymentsRpcCardType::kVirtualCard
                                         : payments::PaymentsAutofillClient::
                                               PaymentsRpcCardType::kServerCard;
    full_card_request->OnDidGetRealPan(result,
                                       response.with_real_pan(real_pan));
  }

  // Convenience method to cast the FullCardRequest into a CardUnmaskDelegate.
  CardUnmaskDelegate* full_card_unmask_delegate() {
    payments::FullCardRequest* full_card_request =
        browser_autofill_manager_->client()
            .GetPaymentsAutofillClient()
            ->GetCvcAuthenticator()
            .full_card_request_.get();
    DCHECK(full_card_request);
    return static_cast<CardUnmaskDelegate*>(full_card_request);
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  NiceMock<MockAutofillClient> autofill_client_;
  NiceMock<MockAutofillDriver> autofill_driver_{&autofill_client_};
  // TODO(crbug.com/41490871): Replace with FormFiller.
  std::unique_ptr<TestBrowserAutofillManager> browser_autofill_manager_;
};

// Tests that only fields from `field_types_to_fill` are filled.
TEST_F(FormFillerTest, FillingDetails_FieldTypesToFill_FillOnlySpecificFields) {
  base::test::ScopedFeatureList enabled_features(
      features::kAutofillGranularFillingAvailable);
  FormData form = test::GetFormData(
      {.fields = {
           {.role = NAME_FIRST, .autocomplete_attribute = "given-name"},
           {.role = NAME_LAST, .autocomplete_attribute = "family-name"}}});
  FormsSeen({form});
  AutofillProfile profile = test::GetFullProfile();

  // Only `NAME_FIRST` fields should be filled.
  FieldTypeSet target_fields = FieldTypeSet({NAME_FIRST});
  std::vector<FormFieldData> filled_fields =
      FillAutofillFormData(form, form.fields()[0], &profile,
                           {.trigger_source = AutofillTriggerSource::kPopup,
                            .field_types_to_fill = target_fields})
          .fields();

  ASSERT_EQ(filled_fields.size(), 2u);
  EXPECT_THAT(filled_fields[0],
              AutofilledWith(profile.GetInfo(NAME_FIRST, kAppLocale)));
  EXPECT_FALSE(filled_fields[1].is_autofilled());
  EXPECT_TRUE(filled_fields[1].value().empty());
}

// Test that the correct section is filled.
TEST_F(FormFillerTest, FillTriggeredSection) {
  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FULL, .autocomplete_attribute = "name"},
                  {.role = NAME_FULL, .autocomplete_attribute = "name"}}});
  FormsSeen({form});
  FormStructure* form_structure = GetFormStructure(form);

  // Assign different sections to the fields.
  base::flat_map<LocalFrameToken, size_t> frame_token_ids;
  for (const std::unique_ptr<AutofillField>& field : form_structure->fields()) {
    field->set_section(Section::FromFieldIdentifier(*field, frame_token_ids));
  }

  AutofillProfile profile = test::GetFullProfile();
  FillAutofillFormData(form, form.fields()[1], &profile);

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
      FillAutofillFormData(form, form.fields().front(), &profile).fields();

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

  EXPECT_CALL(autofill_driver_, ApplyFormAction).Times(0);
  browser_autofill_manager_->FillOrPreviewProfileForm(
      mojom::ActionPersistence::kFill, form, form.fields().front(),
      test::GetFullProfile(), /*trigger_details=*/{});
}

TEST_F(FormFillerTest, SkipFillIfFieldIsMeaningfullyPreFilled) {
  base::test::ScopedFeatureList placeholders_feature;
  placeholders_feature.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillOverwritePlaceholdersOnly},
      /*disabled_features=*/{features::kAutofillSkipPreFilledFields});

  const FieldType kSkippedType = ADDRESS_HOME_LINE1;
  FormData form = test::GetFormData(
      {.fields = {
           {.role = NAME_FIRST, .value = u"Triggering field (filled)"},
           {.role = NAME_LAST, .value = u"Placeholder (filled)"},
           {.role = EMAIL_ADDRESS, .value = u"No data (filled)"},
           {.role = kSkippedType,
            .value = u"Meaningfully pre-filled (skipped)"},
           // Value initialized with whitespace-only, expect field to be filled.
           {.role = ADDRESS_HOME_COUNTRY, .value = u" "}}});
  FormsSeen({form});

  FormStructure* form_structure = GetFormStructure(form);
  form_structure->fields()[0]->set_may_use_prefilled_placeholder(false);
  form_structure->fields()[1]->set_may_use_prefilled_placeholder(true);
  form_structure->fields()[3]->set_may_use_prefilled_placeholder(false);

  AutofillProfile profile = test::GetFullProfile();
  std::vector<FormFieldData> filled_fields =
      FillAutofillFormData(form, form.fields().front(), &profile).fields();

  auto expect_hash = [&](const FormFieldData& field,
                         std::optional<size_t> expected_hash) {
    AutofillField* autofill_field = nullptr;
    FormStructure* form_structure = nullptr;
    ASSERT_TRUE(browser_autofill_manager_->GetCachedFormAndField(
        form, field, &form_structure, &autofill_field));
    ASSERT_TRUE(autofill_field);
    EXPECT_THAT(
        autofill_field->field_log_events(),
        Contains(VariantWith<FillFieldLogEvent>(Field(
            "value_that_would_have_been_filled_in_a_prefilled_field_hash",
            &FillFieldLogEvent::
                value_that_would_have_been_filled_in_a_prefilled_field_hash,
            testing::Conditional(expected_hash.has_value(),
                                 testing::Optional(expected_hash),
                                 Eq(std::nullopt))))));
  };

  EXPECT_THAT(filled_fields[0],
              AutofilledWith(profile.GetInfo(NAME_FIRST, kAppLocale)));
  expect_hash(filled_fields[0], std::nullopt);
  EXPECT_THAT(filled_fields[1],
              AutofilledWith(profile.GetInfo(NAME_LAST, kAppLocale)));
  expect_hash(filled_fields[1], std::nullopt);
  EXPECT_THAT(filled_fields[2],
              AutofilledWith(profile.GetInfo(EMAIL_ADDRESS, kAppLocale)));
  expect_hash(filled_fields[2], std::nullopt);
  EXPECT_FALSE(filled_fields[3].is_autofilled());
  EXPECT_EQ(filled_fields[3].value(), form.fields()[3].value());
  expect_hash(filled_fields[3],
              base::FastHash(base::UTF16ToUTF8(
                  profile.GetInfo(kSkippedType, kAppLocale))));
  EXPECT_THAT(filled_fields[4], AutofilledWith(profile.GetInfo(
                                    ADDRESS_HOME_COUNTRY, kAppLocale)));
}

TEST_F(FormFillerTest, SkipAllPreFilledFieldsExceptIfFieldIsAPlaceholder) {
  base::test::ScopedFeatureList placeholders_features;
  placeholders_features.InitWithFeatures(
      {features::kAutofillOverwritePlaceholdersOnly,
       features::kAutofillSkipPreFilledFields},
      {});

  AutofillProfile profile = test::GetFullProfile();
  const std::u16string kToBeFilledState =
      profile.GetInfo(ADDRESS_HOME_STATE, kAppLocale);
  const std::u16string kSelectedState = u"NC (filled)";
  FormData form = test::GetFormData(
      {.fields = {
           {.role = NAME_FIRST, .value = u"Triggering field (filled)"},
           {.role = NAME_LAST, .value = u"Placeholder (filled)"},
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

  FormStructure* form_structure = GetFormStructure(form);
  form_structure->fields()[0]->set_may_use_prefilled_placeholder(true);
  form_structure->fields()[1]->set_may_use_prefilled_placeholder(true);
  form_structure->fields()[3]->set_may_use_prefilled_placeholder(false);
  form_structure->fields()[4]->set_may_use_prefilled_placeholder(std::nullopt);

  std::vector<FormFieldData> filled_fields =
      FillAutofillFormData(form, form.fields().front(), &profile).fields();

  EXPECT_THAT(filled_fields[0],
              AutofilledWith(profile.GetInfo(NAME_FIRST, kAppLocale)));
  EXPECT_THAT(filled_fields[1],
              AutofilledWith(profile.GetInfo(NAME_LAST, kAppLocale)));
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
  EXPECT_CALL(autofill_driver_, ApplyFormAction)
      .Times(2)
      .WillRepeatedly(Return(safe_fields));

  browser_autofill_manager_->FillOrPreviewProfileForm(
      mojom::ActionPersistence::kFill, form, form.fields().front(),
      test::GetFullProfile(), /*trigger_details=*/{});
  // Undo early returns if it has no filling history for the trigger field,
  // which is initially empty, therefore calling the driver is proof that data
  // was successfully stored.
  browser_autofill_manager_->UndoAutofill(mojom::ActionPersistence::kFill, form,
                                          form.fields().front());
}

TEST_F(FormFillerTest, UndoSavesFieldByFieldFillingData) {
  FormData form = test::CreateTestAddressFormData();
  FormsSeen({form});

  EXPECT_CALL(autofill_driver_, ApplyFieldAction);
  browser_autofill_manager_->FillOrPreviewField(
      mojom::ActionPersistence::kFill, mojom::FieldActionType::kReplaceAll,
      form, form.fields().front(), u"Some Name",
      SuggestionType::kAddressFieldByFieldFilling, NAME_FULL);
  // Undo early returns if it has no filling history for the trigger field,
  // which is initially empty, therefore calling the driver is proof that data
  // was successfully stored.
  EXPECT_CALL(autofill_driver_, ApplyFormAction);
  browser_autofill_manager_->UndoAutofill(mojom::ActionPersistence::kFill, form,
                                          form.fields().front());
}

TEST_F(FormFillerTest, UndoResetsCachedAutofillState) {
  FormData form = test::CreateTestAddressFormData();

  AutofillField filled_autofill_field(form.fields().front());
  test_api(form).field(0).set_is_autofilled(false);
  test_api(test_api(*browser_autofill_manager_).form_filler())
      .AddFormFillEntry(
          std::to_array<const FormFieldData*>({&form.fields().front()}),
          std::to_array<const AutofillField*>({&filled_autofill_field}),
          FillingProduct::kAddress, /*is_refill=*/false);

  test_api(form).field(0).set_is_autofilled(true);
  FormsSeen({form});

  const AutofillField* autofill_field =
      GetAutofillField(form, form.fields().front());
  ASSERT_TRUE(autofill_field->is_autofilled());
  browser_autofill_manager_->UndoAutofill(mojom::ActionPersistence::kFill, form,
                                          form.fields().front());
  EXPECT_FALSE(autofill_field->is_autofilled());
}

TEST_F(FormFillerTest, FillOrPreviewDataModelFormCallsDidFillOrPreviewForm) {
  FormData form = test::CreateTestAddressFormData();
  FormsSeen({form});

  AutofillProfile profile = test::GetFullProfile();
  EXPECT_CALL(autofill_client_, DidFillOrPreviewForm);
  FillAutofillFormData(form, form.fields().front(), &profile);
}

// Tests that for autocomplete=unrecognized fields:
// - Are not filled by default.
// - Are filled through manual fallbacks.
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
  form_structure->field(1)->SetTypeTo(AutofillType(NAME_MIDDLE));
  ASSERT_EQ(form_structure->field(1)->html_type(),
            HtmlFieldType::kUnrecognized);

  // Fill the `form` regularly and expect that everything but the middle name
  // gets filled.
  AutofillProfile profile = test::GetFullProfile();
  std::vector<FormFieldData> filled_fields =
      FillAutofillFormData(form, form.fields()[0], &profile).fields();
  EXPECT_THAT(filled_fields[0],
              AutofilledWith(profile.GetInfo(NAME_FIRST, kAppLocale)));
  EXPECT_FALSE(filled_fields[1].is_autofilled());
  EXPECT_THAT(filled_fields[2],
              AutofilledWith(profile.GetInfo(NAME_LAST, kAppLocale)));

  // Fill the `form` as-if through manual fallbacks. Expect that every field
  // gets filled.
  EXPECT_CALL(autofill_driver_, ApplyFormAction)
      .WillOnce(DoAll(SaveArgElementsTo<2>(&filled_fields),
                      Return(base::flat_set<FieldGlobalId>{})));
  browser_autofill_manager_->FillOrPreviewProfileForm(
      mojom::ActionPersistence::kFill, form, form.fields()[0], profile,
      {.trigger_source = AutofillTriggerSource::kManualFallback});

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
      FillAutofillFormData(form, form.fields().front(), &credit_card).fields();
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
      FillAutofillFormData(form, form.fields().front(), &credit_card_whitespace)
          .fields();
  EXPECT_THAT(filled_fields[0], AutofilledWith(u"4234567890123456"));

  filled_fields =
      FillAutofillFormData(form, form.fields().front(), &credit_card_separator)
          .fields();
  EXPECT_THAT(filled_fields[0], AutofilledWith(u"4234567890123456"));
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
      FillAutofillFormData(form, form.fields().front(), &credit_card).fields();
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
      FillAutofillFormData(form, form.fields().front(), &credit_card).fields();

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
      FillAutofillFormData(form, form.fields().front(), &credit_card).fields();

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
      FillAutofillFormData(form, form.fields().front(), &credit_card).fields();
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
      FillAutofillFormData(form, form.fields().front(), &profile).fields();

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
  ASSERT_EQ(form_structure->field(1)->Type().GetStorableType(), NAME_MIDDLE);

  AutofillProfile profile = test::GetFullProfile();
  std::vector<FormFieldData> filled_fields =
      FillAutofillFormData(form, form.fields()[0], &profile).fields();
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
      FillAutofillFormData(form, form.fields()[0], &profile).fields();
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
      FillAutofillFormData(form, form.fields().front(), &credit_card).fields();
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
      FillAutofillFormData(form, form.fields().front(), &credit_card).fields();
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
      FillAutofillFormData(form, *form.fields().begin(), &expired_card)
          .fields();
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
      FillAutofillFormData(form, form.fields()[0], &profile).fields();

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
      FillAutofillFormData(form, form.fields()[0], &profile).fields();
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
  filled_fields =
      FillAutofillFormData(form, form.fields()[1], &profile).fields();
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
  filled_fields =
      FillAutofillFormData(form, form.fields()[2], &credit_card).fields();
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
      FillAutofillFormData(form, form.fields()[0], &profile).fields();
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
      FillAutofillFormData(form, form.fields().front(), &profile).fields();
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
      FillAutofillFormData(form, form.fields().front(), &credit_card).fields();
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
      FillAutofillFormData(form, form.fields().front(), &profile).fields();
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
      FillAutofillFormData(form, form.fields().front(), &credit_card).fields();
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
  FormData filled_form1 = FillAutofillFormData(
      form_with_us_number_max_length,
      form_with_us_number_max_length.fields().front(), &profile);
  ASSERT_EQ(4u, filled_form1.fields().size());
  EXPECT_EQ(u"1", filled_form1.fields()[0].value());
  EXPECT_EQ(u"650", filled_form1.fields()[1].value());
  EXPECT_EQ(u"555", filled_form1.fields()[2].value());
  EXPECT_EQ(u"4567", filled_form1.fields()[3].value());

  FormData filled_form2 = FillAutofillFormData(
      form_with_autocompletetype, form_with_autocompletetype.fields().front(),
      &profile);
  ASSERT_EQ(4u, filled_form2.fields().size());
  EXPECT_EQ(u"1", filled_form2.fields()[0].value());
  EXPECT_EQ(u"650", filled_form2.fields()[1].value());
  EXPECT_EQ(u"555", filled_form2.fields()[2].value());
  EXPECT_EQ(u"4567", filled_form2.fields()[3].value());

  // For other countries, fill prefix and suffix fields with best effort.
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"GB");
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"447700954321");
  FormData filled_form3 = FillAutofillFormData(
      form_with_us_number_max_length,
      form_with_us_number_max_length.fields().front(), &profile);
  ASSERT_EQ(4u, filled_form3.fields().size());
  EXPECT_EQ(u"4", filled_form3.fields()[0].value());
  EXPECT_EQ(u"700", filled_form3.fields()[1].value());
  EXPECT_EQ(u"95", filled_form3.fields()[2].value());
  EXPECT_EQ(u"4321", filled_form3.fields()[3].value());

  FormData filled_form4 = FillAutofillFormData(
      form_with_autocompletetype, form_with_autocompletetype.fields().front(),
      &profile);
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
      FillAutofillFormData(form, form.fields().front(), &profile).fields();

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
      FillAutofillFormData(form, form.fields().front(), &profile).fields();

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
      FillAutofillFormData(form, form.fields().front(), &profile).fields();
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
      FillAutofillFormData(form, form.fields().front(), &profile).fields();
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
      FillAutofillFormData(form, form.fields().front(), &profile).fields();
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
      FillAutofillFormData(form, form.fields().front(), &profile).fields();
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
      FillAutofillFormData(form, form.fields().front(), &profile).fields();
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
      FillAutofillFormData(form, form.fields()[2], &profile).fields();
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
      FillAutofillFormData(form, form.fields().front(), &profile).fields();
  // Verify hidden/non-focusable phone field is set to only_fill_when_focused.
  ASSERT_EQ(3u, filled_fields.size());
  EXPECT_EQ(u"John H. Doe", filled_fields[0].value());
  EXPECT_EQ(std::u16string(), filled_fields[1].value());
  EXPECT_EQ(u"6505554567", filled_fields[2].value());
}

// Tests that only hidden/presentational select fields are filled.
TEST_F(FormFillerTest, FormWithHiddenOrPresentationalFields) {
  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FULL, .autocomplete_attribute = "name"}}});

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

  base::HistogramTester histogram_tester;
  AutofillProfile profile = test::GetFullProfile();
  std::vector<FormFieldData> filled_fields =
      FillAutofillFormData(form, form.fields()[0], &profile).fields();

  ASSERT_EQ(filled_fields.size(), 5u);
  EXPECT_THAT(filled_fields[0],
              AutofilledWith(profile.GetInfo(NAME_FULL, kAppLocale)));
  EXPECT_THAT(filled_fields[1], AutofilledWith(u"US"));
  EXPECT_THAT(filled_fields[2], AutofilledWith(u"CA"));
  EXPECT_FALSE(filled_fields[3].is_autofilled());
  EXPECT_FALSE(filled_fields[4].is_autofilled());
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
      FillAutofillFormData(form, form.fields()[0], &profile).fields();
  // Verify first section is filled with rationalization.
  ASSERT_EQ(6u, filled_fields.size());
  EXPECT_EQ(u"John H. Doe", filled_fields[0].value());
  EXPECT_EQ(u"6505554567", filled_fields[1].value());
  EXPECT_EQ(std::u16string(), filled_fields[2].value());
  EXPECT_EQ(std::u16string(), filled_fields[3].value());
  EXPECT_EQ(std::u16string(), filled_fields[4].value());
  EXPECT_EQ(std::u16string(), filled_fields[5].value());

  // Fill the second section.
  filled_fields =
      FillAutofillFormData(form, form.fields()[3], &profile).fields();
  // Verify second section is filled with rationalization.
  ASSERT_EQ(6u, filled_fields.size());
  EXPECT_EQ(std::u16string(), filled_fields[0].value());
  EXPECT_EQ(std::u16string(), filled_fields[1].value());
  EXPECT_EQ(std::u16string(), filled_fields[2].value());
  EXPECT_EQ(u"John H. Doe", filled_fields[3].value());
  EXPECT_EQ(u"6505554567", filled_fields[4].value());
  EXPECT_EQ(std::u16string(), filled_fields[5].value());
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
      FillAutofillFormData(form, form.fields()[0], &profile).fields();
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
      FillAutofillFormData(form, form.fields()[0], &profile).fields();
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
  FormData filled_form = FillAutofillFormData(form, form.fields()[0], &profile);

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
      FillAutofillFormData(filled_form, filled_form.fields()[2], &profile2)
          .fields();
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

TEST_F(FormFillerTest, FillInUpdatedExpirationDate) {
  PrepareForRealPanResponse();

  CardUnmaskDelegate::UserProvidedUnmaskDetails details;
  details.cvc = u"123";
  details.exp_month = u"02";
  details.exp_year = u"2018";
  full_card_unmask_delegate()->OnUnmaskPromptAccepted(details);
  OnDidGetRealPan(payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
                  "4012888888881881");
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
  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);

  AutofillProfile profile = test::GetFullProfile();
  FillAutofillFormData(form, form.fields()[0], &profile);
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
  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);

  // Fill the form with a profile without email
  AutofillProfile profile1 = test::GetFullProfile();
  profile1.ClearFields({EMAIL_ADDRESS});
  FormData filled_form =
      FillAutofillFormData(form, form.fields()[0], &profile1);

  // Check that the email field has no filling source.
  ASSERT_EQ(form.fields()[2].label(), u"E-mail address");
  EXPECT_EQ(form_structure->field(2)->autofill_source_profile_guid(),
            std::nullopt);

  // Then fill the email field using the second profile
  AutofillProfile profile2 = test::GetFullProfile2();
  FillAutofillFormData(filled_form, form.fields()[2], &profile2);

  // Check that the first three fields have the first profile as filling source
  // and the last field has the second profile.
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
  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);

  AutofillProfile profile = test::GetFullProfile();
  FormData filled_form = FillAutofillFormData(form, form.fields()[0], &profile);

  // Simulate editing the first field.
  test_api(filled_form).field(0).set_value(u"");
  browser_autofill_manager_->OnTextFieldDidChange(
      filled_form, filled_form.fields()[0].global_id(), base::TimeTicks::Now());

  ASSERT_TRUE(form_structure->field(0)->previously_autofilled());
  EXPECT_FALSE(form_structure->field(0)->is_autofilled());
  EXPECT_THAT(form_structure->field(0)->autofill_source_profile_guid(),
              Optional(profile.guid()));
  EXPECT_THAT(form_structure->field(1), AutofilledWithProfile(profile));
}

// Regression test that a field with an unrelated type doesn't cause a crash
// (crbug.com/324811625).
TEST_F(FormFillerTest, PreFilledCCFieldInAddressFormDoesNotCauseCrash) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({features::kAutofillSkipPreFilledFields,
                                 features::kAutofillOverwritePlaceholdersOnly},
                                {});
  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FULL,
                   .value = u"pre-filled",
                   .autocomplete_attribute = "section-billing name"},
                  {.role = CREDIT_CARD_NUMBER,
                   .value = u"pre-filled",
                   .autocomplete_attribute = "section-billing cc-number"}}});
  FormsSeen({form});

  AutofillProfile profile = test::GetFullProfile();
  FillAutofillFormData(form, form.fields().front(), &profile);
  // Expect that this test doesn't cause a crash.
}

TEST_F(FormFillerTest, FillOrPreviewFormExperimental) {
  test::FormDescription form_description = {
      .fields = {{.role = NAME_FIRST, .heuristic_type = NAME_FIRST},
                 {.role = NAME_LAST, .heuristic_type = NAME_LAST},
                 {.role = EMAIL_ADDRESS, .heuristic_type = EMAIL_ADDRESS},
                 {.role = UNKNOWN_TYPE, .heuristic_type = UNKNOWN_TYPE}}};
  FormData form = test::GetFormData(form_description);
  FormsSeen({form});
  base::flat_map<FieldGlobalId, std::u16string> values_to_fill = {
      // Not filled because the value to fill is empty.
      {form.fields()[0].global_id(), u""},
      // Filled.
      {form.fields()[1].global_id(), u"Doe"},
      // Not filled because `field_types_to_fill` doesn't contain
      // `EMAIL_ADDRESS`.
      {form.fields()[2].global_id(), u"johndoe@example.com"},
      // Filled (because `field_types_to_fill` include `UNKNOWN_TYPE` and
      // `ignorable_skip_reasons` include `kNoFillableGroup`).
      {form.fields()[3].global_id(), u"100 John Doe Rd"}};
  std::vector<FormFieldData> filled_fields;
  EXPECT_CALL(autofill_driver_, ApplyFormAction)
      .WillOnce(DoAll(SaveArgElementsTo<2>(&filled_fields),
                      Return(std::vector<FieldGlobalId>())));
  browser_autofill_manager_->FillOrPreviewFormExperimental(
      mojom::ActionPersistence::kFill, FillingProduct::kAddress,
      /*field_types_to_fill=*/{UNKNOWN_TYPE, NAME_FIRST, NAME_LAST},
      /*ignorable_skip_reasons=*/{FieldFillingSkipReason::kNoFillableGroup},
      form, form.fields().front(), values_to_fill);
  ASSERT_EQ(filled_fields.size(), 2UL);
  EXPECT_EQ(filled_fields[0].value(), u"Doe");
  EXPECT_EQ(filled_fields[1].value(), u"100 John Doe Rd");
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
      FillAutofillFormData(form, form.fields().front(), &credit_card);
  ASSERT_EQ(3u, first_fill_data.fields().size());
  EXPECT_THAT(first_fill_data.fields()[0], AutofilledWith(u"Elvis Presley"));
  EXPECT_THAT(first_fill_data.fields()[1], AutofilledWith(u"4234567890123456"));
  EXPECT_THAT(first_fill_data.fields()[2], AutofilledWith(u"04/2999"));

  std::vector<FormFieldData> refilled_fields;
  if (test_case.triggers_refill) {
    // Prepare intercepting the filling operation to the driver and capture
    // the re-filled form data.
    EXPECT_CALL(autofill_driver_, ApplyFormAction)
        .WillOnce(DoAll(SaveArgElementsTo<2>(&refilled_fields),
                        Return(std::vector<FieldGlobalId>{})));
  } else {
    EXPECT_CALL(autofill_driver_, ApplyFormAction).Times(0);
  }

  // Simulate that JavaScript modifies the expiration date field.
  FormData form_after_js_modification = first_fill_data;
  test_api(form_after_js_modification)
      .field(2)
      .set_value(test_case.exp_date_from_js);
  browser_autofill_manager_->OnJavaScriptChangedAutofilledValue(
      form_after_js_modification,
      form_after_js_modification.fields()[2].global_id(), u"04/2999",
      /*formatting_only=*/false);

  testing::Mock::VerifyAndClearExpectations(&autofill_driver_);

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

}  // namespace autofill
