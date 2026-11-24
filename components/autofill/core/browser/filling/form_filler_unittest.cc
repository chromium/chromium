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
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/with_feature_override.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_format_string.h"
#include "components/autofill/core/browser/autofill_trigger_source.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_manager/payments/test_payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_i18n_api.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/autofill_ai/field_filling_entity_util.h"
#include "components/autofill/core/browser/filling/field_filling_skip_reason.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/filling/form_filler_test_api.h"
#include "components/autofill/core/browser/filling/test_form_filler.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/foundations/autofill_manager_test_api.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager_test_api.h"
#include "components/autofill/core/browser/foundations/mock_autofill_manager_observer.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map_test_util.h"
#include "components/autofill/core/browser/heuristic_source.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_suggestion.h"
#include "components/autofill/core/browser/logging/log_receiver.h"
#include "components/autofill/core/browser/logging/log_router.h"
#include "components/autofill/core/browser/metrics/form_events/form_events.h"
#include "components/autofill/core/browser/payments/credit_card_cvc_authenticator.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_util.h"
#include "components/autofill/core/browser/test_utils/autofill_test_util.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_util.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_test_helper.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_test_util.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/credit_card_network_identifiers.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace autofill {
namespace {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Bool;
using ::testing::Contains;
using ::testing::DoAll;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::Matcher;
using ::testing::MockFunction;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::Optional;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::UnorderedElementsAre;
using ::testing::Values;
using ::testing::ValuesIn;
using ::testing::VariantWith;

constexpr char kAppLocale[] = "en-US";

// Action `SaveArgElementsTo<k>(pointer)` saves the value pointed to by the
// `k`th (0-based) argument of the mock function by moving it to `*pointer`.
ACTION_TEMPLATE(SaveArgElementsTo,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(pointer)) {
  auto span = testing::get<k>(args);
  pointer->assign(span.begin(), span.end());
}

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
               const FillId& fill_id,
               bool supports_refill,
               const url::Origin& triggered_origin,
               (const absl::flat_hash_map<FieldGlobalId, FieldType>&)),
              (override));
  MOCK_METHOD(void,
              ApplyFieldAction,
              (mojom::FieldActionType text_replacement,
               mojom::ActionPersistence action_persistence,
               const FieldGlobalId& field_id,
               const std::u16string& value),
              (override));

  using IsSafeToFillCallback =
      base::RepeatingCallback<bool(const FormFieldData&,
                                   FieldType,
                                   const url::Origin&,
                                   const url::Origin&)>;
  void set_is_safe_to_fill_callback(IsSafeToFillCallback callback) {
    is_safe_to_fill_callback_ = std::move(callback);
  }
  bool IsSafeToFill(const FormFieldData& field,
                    FieldType filled_type,
                    const url::Origin& main_origin,
                    const url::Origin& trigger_origin) const override {
    if (is_safe_to_fill_callback_) {
      return is_safe_to_fill_callback_.Run(field, filled_type, main_origin,
                                           trigger_origin);
    }
    return TestAutofillDriver::IsSafeToFill(field, filled_type, main_origin,
                                            trigger_origin);
  }

 private:
  IsSafeToFillCallback is_safe_to_fill_callback_;
};

// Takes a FormFieldData argument.
auto AutofilledWith(std::u16string value) {
  return AllOf(
      Property("FormFieldData::is_autofilled_according_to_renderer",
               &FormFieldData::is_autofilled_according_to_renderer, true),
      Property("FormFieldData::value", &FormFieldData::value,
               std::move(value)));
}

// Takes an AutofillField argument.
MATCHER_P(AutofilledWithProfile, profile, "") {
  return arg->last_modifier() == FieldModifier::kAutofill &&
         arg->autofill_source_profile_guid() &&
         *arg->autofill_source_profile_guid() == profile.guid();
}

Matcher<FormFieldData> HasFieldId(const FieldGlobalId& id) {
  return Property("FormFieldData::global_id", &FormFieldData::global_id, id);
}

// Simulates the renderer filling `form`: Copies the fields in `filled_fields`
// (as passed to `AutofillDriver::ApplyFormAction()`) into the fields of `form`
// with the same IDs.
void CopyFilledFieldsInto(base::span<const FormFieldData> filled_fields,
                          FormData& form) {
  for (FormFieldData& field : test_api(form).fields()) {
    if (auto it = std::ranges::find(filled_fields, field.global_id(),
                                    &FormFieldData::global_id);
        it != filled_fields.end()) {
      field = *it;
    }
  }
}

// Returns an action for `MockAutofillDriver::ApplyFormAction()` that
// simulates the renderer filling `form` (see `CopyFilledFieldsInto()`), then
// runs `after_fill()` and reports all fields as filled. `after_fill()` must not
// modify the form cache because `ApplyFormAction()` never does so
// synchronously. `form` must outlive the action.
template <typename AfterFill = void (*)()>
auto FillInto(FormData& form, AfterFill after_fill = [] {}) {
  return [&form, after_fill = std::move(after_fill)](
             mojom::FormActionType, mojom::ActionPersistence,
             base::span<const FormFieldData> data, const FillId&, bool,
             const url::Origin&,
             const absl::flat_hash_map<FieldGlobalId, FieldType>&) mutable {
    CopyFilledFieldsInto(data, form);
    after_fill();
    return base::ToVector(data, &FormFieldData::global_id);
  };
}

// Collects the chrome://autofill-internals log entries as JSON strings.
class CollectingLogReceiver : public LogReceiver {
 public:
  void LogEntry(const base::DictValue& entry) override {
    entries_.push_back(base::WriteJson(entry).value_or(""));
  }

  const std::vector<std::string>& entries() const { return entries_; }

 private:
  std::vector<std::string> entries_;
};

}  // namespace
// The anonymous namespace needs to end here because of `friend`ships between
// the tests and the production code.

class FormFillerTest
    : public testing::Test,
      public WithTestAutofillClientDriverManager<TestAutofillClient,
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
                                   /*removed_forms=*/{},
                                   AutofillManagerTestApi::pass_key());
  }

  void FormsRemoved(const std::vector<FormGlobalId>& forms) {
    autofill_manager().OnFormsSeen(/*updated_forms=*/{},
                                   /*removed_forms=*/forms,
                                   AutofillManagerTestApi::pass_key());
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
    return test_api(autofill_manager()).FindCachedFormById(form.global_id());
  }

  AutofillField* GetAutofillField(const FormGlobalId& form_id,
                                  const FieldGlobalId& field_id) {
    FormStructure* form =
        test_api(autofill_manager()).FindCachedFormById(form_id);
    if (!form) {
      return nullptr;
    }
    return form->GetFieldById(field_id);
  }

  // Lets `BrowserAutofillManager` fill `form` using `trigger`` and
  // returns `form` as it would be extracted from the renderer afterwards, i.e.,
  // with the autofilled `FormFieldData::value`s. Runs the post-fill bookkeeping
  // before the renderer reports the filled values.
  FormData ApplyFormAction(
      FormData form,
      base::FunctionRef<void(const FormData& form)> trigger) {
    std::vector<FormFieldData> filled_fields;
    // After the call, `filled_fields` will only contain the fields that were
    // autofilled in this call of FillOrPreviewForm (% fields not filled due
    // to the iframe security policy).
    EXPECT_CALL(autofill_driver(), ApplyFormAction)
        .WillOnce(
            [&filled_fields](
                mojom::FormActionType, mojom::ActionPersistence,
                base::span<const FormFieldData> data, const FillId&, bool,
                const url::Origin&,
                const absl::flat_hash_map<FieldGlobalId, FieldType>&) mutable {
              filled_fields = base::ToVector(data);
              return base::ToVector(filled_fields, &FormFieldData::global_id);
            })
        .WillRepeatedly({});
    trigger(form);
    test_api(autofill_manager()).RunPendingPostFillTasks();
    // Copy the filled data into the form.
    for (FormFieldData& field : test_api(form).fields()) {
      if (auto it = std::ranges::find(filled_fields, field.global_id(),
                                      &FormFieldData::global_id);
          it != filled_fields.end()) {
        field = *it;
      }
    }
    test_api(*GetFormStructure(form)).UpdateFormData(form);
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
    FillingProduct filling_product = std::visit(
        absl::Overload{
            [](const AutofillProfile*) { return FillingProduct::kAddress; },
            [](const CreditCard*) { return FillingProduct::kCreditCard; },
            [](const EntityInstance*) { return FillingProduct::kAutofillAi; },
            [](const VerifiedProfile*) {
              return FillingProduct::kIdentityCredential;
            },
            [](const OtpFillData*) {
              return FillingProduct::kOneTimePassword;
            }},
        filling_payload);
    return ApplyFormAction(std::move(form), [&](const FormData& form) {
      form_filler().FillOrPreviewForm(
          mojom::ActionPersistence::kFill, filling_product, filling_payload,
          *GetFormStructure(form),
          *GetAutofillField(form.global_id(), trigger_field.global_id()),
          trigger_source, /*blocked_fields=*/{}, FillId::Create(),
          /*forced_fill_values=*/{}, FormFiller::RefillOptions::NotRefill());
    });
  }

  // Lets `BrowserAutofillManager` undo the last filling operation performed on
  // `trigger_field`, which belongs to `form`, and returns `form` as it would be
  // extracted from the renderer afterwards, i.e., with the autofilled
  // `FormFieldData::value`s.
  FormData UndoAutofill(FormData form, const FormFieldData& trigger_field) {
    return ApplyFormAction(std::move(form), [&](const FormData& form) {
      autofill_manager().UndoAutofill(mojom::ActionPersistence::kFill,
                                      form.global_id(),
                                      trigger_field.global_id());
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
        trigger_field.global_id(),
        GetAutofillField(form.global_id(), trigger_field.global_id()), value,
        filling_product, /*field_type_used=*/std::nullopt);

    FormFieldData& field =
        *std::ranges::find(test_api(form).fields(), trigger_field.global_id(),
                           &FormFieldData::global_id);
    field.set_is_autofilled_according_to_renderer(true);
    field.set_value(value);
    return form;
  }

  std::vector<FormFieldData> PreviewVirtualCardDataAndGetResults(
      const FormData& form,
      const FormFieldData& field,
      const CreditCard& virtual_card) {
    std::vector<FormFieldData> filled_fields;
    EXPECT_CALL(autofill_driver(), ApplyFormAction)
        .WillOnce([&filled_fields](
                      mojom::FormActionType, mojom::ActionPersistence,
                      base::span<const FormFieldData> data, const FillId&, bool,
                      const url::Origin&,
                      const absl::flat_hash_map<FieldGlobalId, FieldType>&) {
          filled_fields = base::ToVector(data);
          return base::ToVector(data, &FormFieldData::global_id);
        });
    form_filler().FillOrPreviewForm(
        mojom::ActionPersistence::kPreview, FillingProduct::kCreditCard,
        &virtual_card, *GetFormStructure(form),
        *GetAutofillField(form.global_id(), field.global_id()),
        AutofillTriggerSource::kPopup, /*blocked_fields=*/{}, FillId::Create(),
        /*forced_fill_values=*/{}, FormFiller::RefillOptions::NotRefill());
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

  ASSERT_TRUE(form_structure);
  EXPECT_NE(form_structure->field(0)->last_modifier(),
            FieldModifier::kAutofill);
  EXPECT_EQ(form_structure->field(1)->last_modifier(),
            FieldModifier::kAutofill);
}

TEST_F(FormFillerTest, SkipPreFilledFields) {
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
            .select_options =
                {{{.value = kSelectedState, .text = kSelectedState},
                  {.value = kToBeFilledState, .text = kToBeFilledState}}}},
           // Value initialized with whitespace-only, expect field to be filled.
           {.role = ADDRESS_HOME_COUNTRY, .value = u" "}}});
  FormsSeen({form});

  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields().front(), &profile).fields();

  EXPECT_THAT(filled_fields[0],
              AutofilledWith(profile.GetInfo(NAME_FIRST, kAppLocale)));
  EXPECT_FALSE(filled_fields[1].is_autofilled_according_to_renderer());
  EXPECT_EQ(filled_fields[1].value(), form.fields()[1].value());
  EXPECT_FALSE(filled_fields[2].is_autofilled_according_to_renderer());
  EXPECT_EQ(filled_fields[2].value(), form.fields()[2].value());
  EXPECT_FALSE(filled_fields[3].is_autofilled_according_to_renderer());
  EXPECT_EQ(filled_fields[3].value(), form.fields()[3].value());
  EXPECT_THAT(filled_fields[4], AutofilledWith(kToBeFilledState));
  EXPECT_THAT(filled_fields[5], AutofilledWith(profile.GetInfo(
                                    ADDRESS_HOME_COUNTRY, kAppLocale)));
}

TEST_F(FormFillerTest, UndoResetsFormFillingData) {
  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FULL, .autocomplete_attribute = "name"}}});
  FormsSeen({form});

  const FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);
  ASSERT_NE(form_structure->field(0)->last_modifier(),
            FieldModifier::kAutofill);
  ASSERT_FALSE(form_structure->field(0)->autofilled_type());
  ASSERT_FALSE(form_structure->field(0)->autofill_source_profile_guid());
  ASSERT_EQ(form_structure->field(0)->filling_product(), FillingProduct::kNone);

  AutofillProfile profile = test::GetFullProfile();
  form = AutofillForm(form, form.fields().front(), &profile);

  ASSERT_EQ(form_structure->field(0)->last_modifier(),
            FieldModifier::kAutofill);
  ASSERT_EQ(form_structure->field(0)->autofilled_type(), NAME_FULL);
  ASSERT_EQ(form_structure->field(0)->autofill_source_profile_guid(),
            profile.guid());
  ASSERT_EQ(form_structure->field(0)->filling_product(),
            FillingProduct::kAddress);

  // Undo early returns if it has no filling history for the trigger field,
  // which is initially empty, therefore calling the driver is proof that data
  // was successfully stored.
  UndoAutofill(form, form.fields().front());

  EXPECT_NE(form_structure->field(0)->last_modifier(),
            FieldModifier::kAutofill);
  EXPECT_FALSE(form_structure->field(0)->autofilled_type());
  EXPECT_FALSE(form_structure->field(0)->autofill_source_profile_guid());
  EXPECT_EQ(form_structure->field(0)->filling_product(), FillingProduct::kNone);
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

  const FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);
  auto field_record_types = [&] {
    return base::ToVector(form_structure->fields(),
                          &AutofillField::filled_entity_record_type);
  };
  ASSERT_THAT(field_record_types(), Each(std::nullopt));

  EntityInstance passport = test::GetPassportEntityInstance(
      {.record_type = EntityInstance::RecordType::kServerWallet});
  form = AutofillForm(form, form.fields().front(), &passport);

  EXPECT_THAT(field_record_types(),
              ElementsAre(EntityInstance::RecordType::kServerWallet,
                          EntityInstance::RecordType::kServerWallet,
                          EntityInstance::RecordType::kServerWallet,
                          std::nullopt, std::nullopt));

  form = UndoAutofill(form, form.fields().front());
  EXPECT_THAT(field_record_types(), Each(std::nullopt));
}

TEST_F(FormFillerTest,
       FilledEntityRecordTypeUpdatedOnOverwriteAndRestoredOnUndo) {
  FormData form = FormSeen(
      {.fields = {{.server_type = PASSPORT_NUMBER},
                  {.server_type = NO_SERVER_DATA, .heuristic_type = NAME_FULL},
                  {.server_type = PASSPORT_ISSUING_COUNTRY,
                   .heuristic_type = ADDRESS_HOME_COUNTRY}}});

  const FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);

  EntityInstance local_passport = test::GetPassportEntityInstance(
      {.record_type = EntityInstance::RecordType::kLocal});
  form = AutofillForm(form, form.fields().front(), &local_passport);
  ASSERT_EQ(form_structure->field(0)->filled_entity_record_type(),
            EntityInstance::RecordType::kLocal);

  // Overwriting the trigger field with an entity of a different record type
  // updates `filled_entity_record_type()`, and Undo restores the prior one.
  EntityInstance personal_context_passport = test::GetPassportEntityInstance(
      {.number = u"999999",
       .record_type = EntityInstance::RecordType::kPersonalContext});
  form = AutofillForm(form, form.fields().front(), &personal_context_passport);
  EXPECT_EQ(form_structure->field(0)->filled_entity_record_type(),
            EntityInstance::RecordType::kPersonalContext);

  form = UndoAutofill(form, form.fields().front());
  EXPECT_EQ(form_structure->field(0)->filled_entity_record_type(),
            EntityInstance::RecordType::kLocal);

  // Overwriting the field with a non-Autofill-AI single-field fill clears
  // `filled_entity_record_type()`, and Undo restores it.
  form = FillField(form, form.fields().front(), FillingProduct::kAddress,
                   u"Some Address");
  EXPECT_EQ(form_structure->field(0)->filled_entity_record_type(),
            std::nullopt);

  form = UndoAutofill(form, form.fields().front());
  EXPECT_EQ(form_structure->field(0)->filled_entity_record_type(),
            EntityInstance::RecordType::kLocal);
}

TEST_F(FormFillerTest, UndoPreviewDoesNotChangeTheCache) {
  FormData form = test::CreateTestAddressFormData();
  FormsSeen({form});
  AutofillField* autofill_field =
      GetAutofillField(form.global_id(), form.fields().front().global_id());
  AutofillProfile profile = test::GetFullProfile();

  form = AutofillForm(form, form.fields().front(), &profile);
  ASSERT_EQ(autofill_field->last_modifier(), FieldModifier::kAutofill);

  EXPECT_CALL(autofill_driver(), ApplyFormAction)
      .Times(AnyNumber())
      .WillRepeatedly([](mojom::FormActionType, mojom::ActionPersistence,
                         base::span<const FormFieldData> fields, const FillId&,
                         bool, const url::Origin&,
                         const absl::flat_hash_map<FieldGlobalId, FieldType>&) {
        return base::flat_set<FieldGlobalId>(
            base::ToVector(fields, &FormFieldData::global_id));
      });

  // A preview of the undo operation won't reset the autofill state.
  autofill_manager().UndoAutofill(mojom::ActionPersistence::kPreview,
                                  form.global_id(),
                                  form.fields().front().global_id());
  EXPECT_EQ(autofill_field->last_modifier(), FieldModifier::kAutofill);

  // An actual undo operation will reset the autofill state.
  autofill_manager().UndoAutofill(mojom::ActionPersistence::kFill,
                                  form.global_id(),
                                  form.fields().front().global_id());
  EXPECT_NE(autofill_field->last_modifier(), FieldModifier::kAutofill);
}

TEST_F(FormFillerTest, UndoSavesFieldByFieldFillingData) {
  FormData form = test::CreateTestAddressFormData();
  FormsSeen({form});

  EXPECT_CALL(autofill_driver(), ApplyFieldAction);
  autofill_manager().FillOrPreviewField(
      mojom::ActionPersistence::kFill, mojom::FieldActionType::kReplaceAll,
      form.global_id(), form.fields().front().global_id(), u"Some Name",
      FillingProduct::kAddress, NAME_FULL);
  // Undo early returns if it has no filling history for the trigger field,
  // which is initially empty, therefore calling the driver is proof that data
  // was successfully stored.
  EXPECT_CALL(autofill_driver(), ApplyFormAction);
  autofill_manager().UndoAutofill(mojom::ActionPersistence::kFill,
                                  form.global_id(),
                                  form.fields().front().global_id());
}

// Tests that single-field filling and previewing for credit cards and IBANs are
// blocked on an insecure context, while non-sensitive fields can still fill.
TEST_F(FormFillerTest,
       FillOrPreviewField_BlocksCreditCardAndIbanOnInsecureContext) {
  const FormData form =
      test::GetFormData({.fields = {{.role = CREDIT_CARD_NUMBER,
                                     .autocomplete_attribute = "cc-number"}}});
  FormsSeen({form});
  AutofillField* const field =
      GetAutofillField(form.global_id(), form.fields().front().global_id());
  ASSERT_NE(field, nullptr);

  autofill_client().set_last_committed_primary_main_frame_url(
      GURL("http://insecure.test"));
  ASSERT_FALSE(autofill_client().IsContextSecure());

  EXPECT_CALL(autofill_driver(), ApplyFieldAction).Times(0);
  form_filler().FillOrPreviewField(
      mojom::ActionPersistence::kFill, mojom::FieldActionType::kReplaceAll,
      field->global_id(), field, u"4111111111111111",
      FillingProduct::kCreditCard, CREDIT_CARD_NUMBER);
  form_filler().FillOrPreviewField(
      mojom::ActionPersistence::kPreview, mojom::FieldActionType::kReplaceAll,
      field->global_id(), field, u"4111111111111111",
      FillingProduct::kCreditCard, CREDIT_CARD_NUMBER);
  form_filler().FillOrPreviewField(
      mojom::ActionPersistence::kFill, mojom::FieldActionType::kReplaceAll,
      field->global_id(), field, u"CH93 0076 2011 6238 5295 7",
      FillingProduct::kIban, IBAN_VALUE);
  form_filler().FillOrPreviewField(
      mojom::ActionPersistence::kPreview, mojom::FieldActionType::kReplaceAll,
      field->global_id(), field, u"CH93 0076 2011 6238 5295 7",
      FillingProduct::kIban, IBAN_VALUE);

  // Non-sensitive address fields can still be filled on an insecure context.
  EXPECT_CALL(autofill_driver(), ApplyFieldAction);
  form_filler().FillOrPreviewField(mojom::ActionPersistence::kFill,
                                   mojom::FieldActionType::kReplaceAll,
                                   field->global_id(), field, u"John Doe",
                                   FillingProduct::kAddress, NAME_FULL);
}

// Tests that credit card form filling and previewing are blocked on an
// insecure context unless triggered via DevTools, while non-sensitive forms
// such as addresses can still fill.
TEST_F(FormFillerTest, FillOrPreviewForm_BlocksCreditCardOnInsecureContext) {
  const CreditCard credit_card = test::GetCreditCard();
  const AutofillProfile profile = test::GetFullProfile();
  const FormData form = test::GetFormData(
      {.fields = {
           {.role = CREDIT_CARD_NAME_FULL, .autocomplete_attribute = "cc-name"},
           {.role = CREDIT_CARD_NUMBER, .autocomplete_attribute = "cc-number"},
           {.role = CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
            .autocomplete_attribute = "cc-exp"}}});
  FormsSeen({form});
  FormStructure* const form_structure = GetFormStructure(form);
  AutofillField* const trigger_field =
      GetAutofillField(form.global_id(), form.fields().front().global_id());
  ASSERT_NE(form_structure, nullptr);
  ASSERT_NE(trigger_field, nullptr);

  autofill_client().set_last_committed_primary_main_frame_url(
      GURL("http://insecure.test"));
  ASSERT_FALSE(autofill_client().IsContextSecure());

  EXPECT_CALL(autofill_driver(), ApplyFormAction).Times(0);
  form_filler().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, FillingProduct::kCreditCard,
      &credit_card, *form_structure, *trigger_field,
      AutofillTriggerSource::kPopup,
      /*blocked_fields=*/{}, FillId::Create(), /*forced_fill_values=*/{},
      FormFiller::RefillOptions::NotRefill());
  form_filler().FillOrPreviewForm(
      mojom::ActionPersistence::kPreview, FillingProduct::kCreditCard,
      &credit_card, *form_structure, *trigger_field,
      AutofillTriggerSource::kPopup, /*blocked_fields=*/{}, FillId::Create(),
      /*forced_fill_values=*/{}, FormFiller::RefillOptions::NotRefill());

  // DevTools and non-sensitive address fills are still allowed.
  EXPECT_CALL(autofill_driver(), ApplyFormAction).Times(2);
  form_filler().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, FillingProduct::kCreditCard,
      &credit_card, *form_structure, *trigger_field,
      AutofillTriggerSource::kDevtools,
      /*blocked_fields=*/{}, FillId::Create(), /*forced_fill_values=*/{},
      FormFiller::RefillOptions::NotRefill());
  test_api(autofill_manager()).RunPendingPostFillTasks();
  form_filler().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, FillingProduct::kAddress, &profile,
      *form_structure, *trigger_field, AutofillTriggerSource::kPopup,
      /*blocked_fields=*/{}, FillId::Create(), /*forced_fill_values=*/{},
      FormFiller::RefillOptions::NotRefill());
}

// Tests that dynamic form refills for credit card forms are blocked if the
// context becomes insecure after the initial fill.
TEST_F(FormFillerTest, Refill_BlocksCreditCardOnInsecureContext) {
  const CreditCard credit_card = test::GetCreditCard();
  FormData form = test::GetFormData(
      {.fields = {
           {.role = CREDIT_CARD_NAME_FULL, .autocomplete_attribute = "cc-name"},
           {.role = CREDIT_CARD_NUMBER, .autocomplete_attribute = "cc-number"},
           {.role = CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
            .autocomplete_attribute = "cc-exp"}}});

  FormFieldData cvc_field = form.fields().back();
  test_api(form).fields().pop_back();
  FormsSeen({form});

  // Initial fill on a secure context succeeds.
  EXPECT_CALL(autofill_driver(), ApplyFormAction)
      .WillOnce(Return(std::vector<FieldGlobalId>{}));
  form_filler().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, FillingProduct::kCreditCard,
      &credit_card, *GetFormStructure(form),
      *GetAutofillField(form.global_id(), form.fields().front().global_id()),
      AutofillTriggerSource::kPopup, /*blocked_fields=*/{}, FillId::Create(),
      /*forced_fill_values=*/{}, FormFiller::RefillOptions::NotRefill());
  test_api(autofill_manager()).RunPendingPostFillTasks();

  // Simulate the context becoming insecure before the dynamic form refill.
  autofill_client().set_last_committed_primary_main_frame_url(
      GURL("http://insecure.test"));
  ASSERT_FALSE(autofill_client().IsContextSecure());

  // Refill should be blocked, so ApplyFormAction is not called again.
  EXPECT_CALL(autofill_driver(), ApplyFormAction).Times(0);
  test_api(form).fields().push_back(std::move(cvc_field));
  FormsSeen({form});
  task_environment_.FastForwardBy(base::Milliseconds(300));
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
  EXPECT_FALSE(filled_fields[1].is_autofilled_according_to_renderer());
  EXPECT_THAT(filled_fields[2],
              AutofilledWith(profile.GetInfo(NAME_LAST, kAppLocale)));

  // It is necessary to clear the cache before filling again. Otherwise Autofill
  // will not fill previously autofilled fields by design.
  FormsRemoved({form.global_id()});
  FormsSeen({form});

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
  EXPECT_FALSE(filled_fields[4].is_autofilled_according_to_renderer());
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
  FormData form1 =
      test::GetFormData({.fields = {{.autocomplete_attribute = "cc-number"}}});
  FormData form2 =
      test::GetFormData({.fields = {{.autocomplete_attribute = "cc-number"}}});
  FormsSeen({form1, form2});

  std::vector<FormFieldData> filled_fields =
      AutofillForm(form1, form1.fields().front(), &credit_card_whitespace)
          .fields();
  EXPECT_THAT(filled_fields[0], AutofilledWith(u"4234567890123456"));

  filled_fields =
      AutofillForm(form2, form2.fields().front(), &credit_card_separator)
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

  FormData filled_form =
      AutofillForm(form, form.fields().front(), &credit_card_full);
  std::vector<FormFieldData> filled_fields = filled_form.fields();

  EXPECT_THAT(filled_fields[0], AutofilledWith(credit_card_full.GetInfo(
                                    CREDIT_CARD_NAME_FULL, kAppLocale)));
  EXPECT_THAT(filled_fields[3], AutofilledWith(credit_card_full.GetInfo(
                                    CREDIT_CARD_EXP_4_DIGIT_YEAR, kAppLocale)));
  EXPECT_TRUE(filled_fields[3].is_autofilled_according_to_renderer());

  std::vector<FormFieldData> updated_fields =
      AutofillForm(filled_form, filled_form.fields().front(),
                   &credit_card_with_empty_data)
          .fields();
  EXPECT_THAT(updated_fields[0],
              AutofilledWith(credit_card_with_empty_data.GetInfo(
                  CREDIT_CARD_NAME_FULL, kAppLocale)));
  EXPECT_EQ(updated_fields[3].value(), u"");
  EXPECT_FALSE(updated_fields[3].is_autofilled_according_to_renderer());
}

// Tests that when payment form fields are autofilled and payment swapping is
// enabled, the cached AutofillField is updated correctly.
TEST_F(FormFillerTest, PaymentsSwappingUpdatesAutofillField) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillPaymentsFieldSwapping);

  FormData form = test::CreateTestCreditCardFormData(/*is_https=*/true,
                                                     /*use_month_type=*/false);
  FormsSeen({form});
  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);

  CreditCard credit_card_full;
  test::SetCreditCardInfo(&credit_card_full, "Elvis Presley",
                          "4234 5678 9012 3456",  // Visa
                          "04", "2999", "1");

  CreditCard credit_card_with_empty_data;
  test::SetCreditCardInfo(&credit_card_with_empty_data, "Elvis Presley New",
                          "4234-5678-9012-3456",  // Visa
                          "04", "", "1");

  FormData filled_form =
      AutofillForm(form, form.fields().front(), &credit_card_full);
  EXPECT_EQ(form_structure->field(0)->last_modifier(),
            FieldModifier::kAutofill);
  EXPECT_EQ(form_structure->field(3)->last_modifier(),
            FieldModifier::kAutofill);

  AutofillForm(filled_form, filled_form.fields().front(),
               &credit_card_with_empty_data);
  EXPECT_EQ(form_structure->field(0)->last_modifier(),
            FieldModifier::kAutofill);
  EXPECT_NE(form_structure->field(3)->last_modifier(),
            FieldModifier::kAutofill);
}

// Tests that undoing a payments swapping fill restores fields that were
// emptied by swapping, while skipping fields that were subsequently edited or
// cleared by the user.
TEST_F(FormFillerTest, PaymentsSwappingWithPartiallyEmptyData_Undo) {
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

  CreditCard credit_card_without_name;
  test::SetCreditCardInfo(&credit_card_without_name, "",
                          "5412 7512 3412 3456",  // Mastercard
                          "06", "2998", "1");

  form = AutofillForm(form, form.fields()[1], &credit_card_full);
  form = AutofillForm(form, form.fields()[1], &credit_card_without_name);
  ASSERT_EQ(form.fields()[0].value(), u"");
  ASSERT_FALSE(form.fields()[0].is_autofilled_according_to_renderer());
  ASSERT_THAT(form.fields()[1], AutofilledWith(u"5412751234123456"));
  ASSERT_THAT(form.fields()[2], AutofilledWith(u"06"));
  ASSERT_THAT(form.fields()[3], AutofilledWith(u"2998"));

  // Simulate the user editing the expiration month and clearing the expiration
  // year after the swap, while leaving the swap-emptied cardholder name
  // untouched.
  test_api(form).field(2).set_value(u"12");
  test_api(form).field(2).set_is_autofilled_according_to_renderer(false);
  test_api(form).field(3).set_value(u"");
  test_api(form).field(3).set_is_autofilled_according_to_renderer(false);
  test_api(*GetFormStructure(form)).UpdateFormData(form);

  form = UndoAutofill(form, form.fields()[1]);

  // The swap-emptied cardholder name and the untouched card number revert to
  // the first card, while user-edited and user-cleared fields are preserved.
  EXPECT_THAT(form.fields()[0], AutofilledWith(u"Elvis Presley"));
  EXPECT_THAT(form.fields()[1], AutofilledWith(u"4234567890123456"));
  EXPECT_EQ(form.fields()[2].value(), u"12");
  EXPECT_FALSE(form.fields()[2].is_autofilled_according_to_renderer());
  EXPECT_EQ(form.fields()[3].value(), u"");
  EXPECT_FALSE(form.fields()[3].is_autofilled_according_to_renderer());
}

// Tests that if a field was emptied by payments swapping and the user then
// manually types a value into it, undoing the swap preserves the user's input.
TEST_F(FormFillerTest,
       PaymentsSwappingWithPartiallyEmptyData_UndoSkipsUserEditedEmptiedField) {
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

  CreditCard credit_card_with_empty_year;
  test::SetCreditCardInfo(&credit_card_with_empty_year, "Elvis Presley New",
                          "5412 7512 3412 3456",  // Mastercard
                          "06", "", "1");

  form = AutofillForm(form, form.fields().front(), &credit_card_full);
  form =
      AutofillForm(form, form.fields().front(), &credit_card_with_empty_year);
  ASSERT_EQ(form.fields()[3].value(), u"");
  ASSERT_FALSE(form.fields()[3].is_autofilled_according_to_renderer());

  // Simulate the user typing into the field that swapping emptied.
  test_api(form).field(3).set_value(u"2030");
  test_api(form).field(3).set_is_autofilled_according_to_renderer(false);
  test_api(*GetFormStructure(form)).UpdateFormData(form);

  form = UndoAutofill(form, form.fields()[0]);

  EXPECT_THAT(form.fields()[0], AutofilledWith(u"Elvis Presley"));
  EXPECT_EQ(form.fields()[3].value(), u"2030");
  EXPECT_FALSE(form.fields()[3].is_autofilled_according_to_renderer());
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
    ValuesIn({PartialCreditCardDateParams{.expected_filled_date = u""},
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
  EXPECT_FALSE(filled_fields[3].is_autofilled_according_to_renderer());
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
  for (size_t i = 0; i < 19; ++i) {
    EXPECT_THAT(filled_fields[i], AutofilledWith(credit_card.GetInfo(
                                      CREDIT_CARD_NUMBER, kAppLocale)))
        << i;
  }
  // Verify that the 20th. credit card number field is not filled.
  EXPECT_FALSE(filled_fields.back().is_autofilled_according_to_renderer());
}

// Test the credit card number is filled correctly into single-digit fields.
TEST_F(FormFillerTest, FillCreditCardNumberIntoSingleDigitFields) {
  // Create a form with 20 credit card number fields.
  FormData form =
      test::GetFormData({.fields = std::vector<test::FieldDescription>(
                             20, {.autocomplete_attribute = "cc-number"})});
  // Set the size limit of the first nineteen fields to 1.
  for (size_t i = 0; i < 19; ++i) {
    test_api(form).field(i).set_max_length(1);
  }
  FormsSeen({form});

  CreditCard credit_card = test::GetCreditCard();
  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields().front(), &credit_card).fields();

  // Verify that the first 19 card number fields are filled.
  std::u16string card_number =
      credit_card.GetInfo(CREDIT_CARD_NUMBER, kAppLocale);
  for (size_t i = 0; i < 19; ++i) {
    EXPECT_THAT(filled_fields[i], AutofilledWith(i < card_number.length()
                                                     ? card_number.substr(i, 1)
                                                     : card_number))
        << i;
  }
  EXPECT_FALSE(filled_fields[19].is_autofilled_according_to_renderer());
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
  EXPECT_FALSE(filled_fields[5].is_autofilled_according_to_renderer());
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
  for (int i = 0; i < 20; ++i) {
    FormFieldData field =
        test::CreateTestSelectField("State", "state", "", "address-level1",
                                    {"AA", "BB", "CA"}, {"AA", "BB", "CA"});
    field.set_origin(form.main_frame_origin());
    test_api(form).Append(std::move(field));
  }
  // Add 10 other a selection box for the country.
  for (int i = 0; i < 10; ++i) {
    FormFieldData field =
        test::CreateTestSelectField("Country", "country", "", "country",
                                    {"DE", "FR", "US"}, {"DE", "FR", "US"});
    field.set_origin(form.main_frame_origin());
    test_api(form).Append(std::move(field));
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
  EXPECT_FALSE(filled_fields[30].is_autofilled_according_to_renderer());
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
  EXPECT_THAT(
      filled_fields,
      ElementsAre(AutofilledWith(profile.GetInfo(NAME_FIRST, kAppLocale)),
                  AutofilledWith(profile.GetInfo(NAME_MIDDLE, kAppLocale)),
                  AutofilledWith(profile.GetInfo(NAME_LAST, kAppLocale))));
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
  EXPECT_THAT(
      filled_fields,
      ElementsAre(AutofilledWith(profile.GetInfo(NAME_FIRST, kAppLocale)),
                  AutofilledWith(profile.GetInfo(NAME_MIDDLE, kAppLocale)),
                  AutofilledWith(profile.GetInfo(NAME_LAST, kAppLocale))));
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
  EXPECT_FALSE(filled_fields[2].is_autofilled_according_to_renderer());
  EXPECT_TRUE(filled_fields[2].value().empty());
  EXPECT_FALSE(filled_fields[3].is_autofilled_according_to_renderer());
  EXPECT_TRUE(filled_fields[3].value().empty());
  EXPECT_FALSE(filled_fields[4].is_autofilled_according_to_renderer());
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
  {
    FormFieldData field = test::CreateTestSelectField(
        "Country", "country", "", "country", {"CA", "US"},
        {"Canada", "United States"}, FormControlType::kSelectOne);
    field.set_origin(form.main_frame_origin());
    test_api(form).Append(std::move(field));
  }
  test_api(form).field(-1).set_is_focusable(false);
  FormsSeen({form});

  AutofillProfile profile = test::GetFullProfile();
  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields()[0], &profile).fields();

  ASSERT_EQ(filled_fields.size(), 3u);
  EXPECT_THAT(filled_fields[0],
              AutofilledWith(profile.GetInfo(NAME_FULL, kAppLocale)));
  EXPECT_FALSE(filled_fields[1].is_autofilled_according_to_renderer());
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
  EXPECT_THAT(filled_fields[0],
              AutofilledWith(profile.GetRawInfo(ADDRESS_HOME_COUNTRY)));
  EXPECT_FALSE(filled_fields[1].is_autofilled_according_to_renderer());
  EXPECT_TRUE(filled_fields[1].value().empty());
  EXPECT_FALSE(filled_fields[2].is_autofilled_according_to_renderer());
  EXPECT_TRUE(filled_fields[2].value().empty());
  EXPECT_FALSE(filled_fields[3].is_autofilled_according_to_renderer());
  EXPECT_TRUE(filled_fields[3].value().empty());
  EXPECT_THAT(filled_fields[4],
              AutofilledWith(profile.GetInfo(EMAIL_ADDRESS, kAppLocale)));

  // Fill the address portion of the billing section.
  filled_fields = AutofillForm(form, form.fields()[1], &profile).fields();
  ASSERT_EQ(filled_fields.size(), 5u);
  EXPECT_FALSE(filled_fields[0].is_autofilled_according_to_renderer());
  EXPECT_TRUE(filled_fields[0].value().empty());
  EXPECT_THAT(filled_fields[1],
              AutofilledWith(profile.GetInfo(ADDRESS_HOME_LINE1, kAppLocale)));
  EXPECT_FALSE(filled_fields[2].is_autofilled_according_to_renderer());
  EXPECT_TRUE(filled_fields[2].value().empty());
  EXPECT_FALSE(filled_fields[3].is_autofilled_according_to_renderer());
  EXPECT_TRUE(filled_fields[3].value().empty());
  EXPECT_FALSE(filled_fields[4].is_autofilled_according_to_renderer());
  EXPECT_TRUE(filled_fields[4].value().empty());

  // Fill the credit card portion of the billing section.
  CreditCard credit_card = test::GetCreditCard();
  filled_fields = AutofillForm(form, form.fields()[2], &credit_card).fields();
  ASSERT_EQ(filled_fields.size(), 5u);
  EXPECT_FALSE(filled_fields[0].is_autofilled_according_to_renderer());
  EXPECT_TRUE(filled_fields[0].value().empty());
  EXPECT_FALSE(filled_fields[1].is_autofilled_according_to_renderer());
  EXPECT_TRUE(filled_fields[1].value().empty());
  EXPECT_THAT(filled_fields[2], AutofilledWith(credit_card.GetInfo(
                                    CREDIT_CARD_NAME_FULL, kAppLocale)));
  EXPECT_THAT(filled_fields[3], AutofilledWith(credit_card.GetInfo(
                                    CREDIT_CARD_NUMBER, kAppLocale)));
  EXPECT_FALSE(filled_fields[4].is_autofilled_according_to_renderer());
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
  EXPECT_THAT(
      filled_fields,
      ElementsAre(AutofilledWith(profile.GetInfo(NAME_FULL, kAppLocale)),
                  AutofilledWith(profile.GetInfo(EMAIL_ADDRESS, kAppLocale)),
                  AutofilledWith(profile.GetInfo(EMAIL_ADDRESS, kAppLocale))));
}

// Test that we correctly fill a previously autofilled address form.
TEST_F(FormFillerTest, FillAutofilledAddressForm) {
  // Set up our form data.
  FormData form =
      test::GetFormData({.fields = {{.role = NAME_FULL,
                                     .value = u"John",
                                     .autocomplete_attribute = "name"},
                                    {.role = EMAIL_ADDRESS,
                                     .value = u"example@email.com",
                                     .autocomplete_attribute = "email"}}});
  FormsSeen({form});

  FormStructure& form_structure =
      *test_api(autofill_manager()).FindCachedFormById(form.global_id());
  form_structure.field(0)->AddFieldModifier(FieldModifier::kAutofill);
  form_structure.field(1)->AddFieldModifier(FieldModifier::kAutofill);

  AutofillProfile profile = test::GetFullProfile();
  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields().front(), &profile).fields();
  ASSERT_EQ(filled_fields.size(), 2u);
  EXPECT_THAT(filled_fields[0], AutofilledWith(profile.GetRawInfo(NAME_FULL)));
  EXPECT_NE(filled_fields[1].value(), profile.GetRawInfo(EMAIL_ADDRESS));
}

// Test that we correctly fill a previously autofilled credit card form.
TEST_F(FormFillerTest, FillAutofilledCreditCardForm) {
  // Set up our form data.
  FormData form =
      test::GetFormData({.fields = {{.role = CREDIT_CARD_NAME_FULL,
                                     .value = u"John",
                                     .autocomplete_attribute = "cc-name"},
                                    {.role = CREDIT_CARD_NUMBER,
                                     .value = u"378282246310005",
                                     .autocomplete_attribute = "cc-number"}}});
  FormsSeen({form});

  FormStructure& form_structure =
      *test_api(autofill_manager()).FindCachedFormById(form.global_id());
  form_structure.field(0)->AddFieldModifier(FieldModifier::kAutofill);
  form_structure.field(1)->AddFieldModifier(FieldModifier::kAutofill);

  CreditCard credit_card = test::GetCreditCard();
  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields().front(), &credit_card).fields();
  ASSERT_EQ(filled_fields.size(), 2u);
  EXPECT_THAT(filled_fields[0],
              AutofilledWith(credit_card.GetRawInfo(CREDIT_CARD_NAME_FULL)));
  EXPECT_NE(filled_fields[1].value(), credit_card.number());
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
  EXPECT_FALSE(filled_fields[2].is_autofilled_according_to_renderer());
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
  EXPECT_FALSE(filled_fields[1].is_autofilled_according_to_renderer());
  EXPECT_THAT(filled_fields[2], AutofilledWith(credit_card.GetInfo(
                                    CREDIT_CARD_NUMBER, kAppLocale)));
}

// Test that we correctly fill a phone number split across multiple fields.
TEST_F(FormFillerTest, FillPhoneNumber) {
  // In one form, rely on the max length attribute to imply US phone number
  // parts. In the other form, rely on the autocomplete type attribute.
  FormData form_with_us_number_max_length = test::GetFormData(
      {.fields = {{.role = PHONE_HOME_COUNTRY_CODE, .max_length = 1},
                  {.role = PHONE_HOME_CITY_CODE, .max_length = 3},
                  {.role = PHONE_HOME_NUMBER_PREFIX, .max_length = 3},
                  {.role = PHONE_HOME_NUMBER_SUFFIX, .max_length = 4}}});

  FormData form_with_autocomplete = test::GetFormData(
      {.fields = {{.role = PHONE_HOME_COUNTRY_CODE,
                   .autocomplete_attribute = "tel-country-code"},
                  {.role = PHONE_HOME_CITY_CODE,
                   .autocomplete_attribute = "tel-area-code"},
                  {.role = PHONE_HOME_NUMBER_PREFIX,
                   .autocomplete_attribute = "tel-local-prefix"},
                  {.role = PHONE_HOME_NUMBER_SUFFIX,
                   .autocomplete_attribute = "tel-local-suffix"}}});

  FormsSeen({form_with_us_number_max_length, form_with_autocomplete});

  // We should be able to fill prefix and suffix fields for US numbers.
  AutofillProfile profile = test::GetFullProfile();
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"16505554567");
  FormData filled_form1 =
      AutofillForm(form_with_us_number_max_length,
                   form_with_us_number_max_length.fields().front(), &profile);
  ASSERT_EQ(filled_form1.fields().size(), 4u);
  EXPECT_EQ(filled_form1.fields()[0].value(), u"1");
  EXPECT_EQ(filled_form1.fields()[1].value(), u"650");
  EXPECT_EQ(filled_form1.fields()[2].value(), u"555");
  EXPECT_EQ(filled_form1.fields()[3].value(), u"4567");

  FormData filled_form2 =
      AutofillForm(form_with_autocomplete,
                   form_with_autocomplete.fields().front(), &profile);
  ASSERT_EQ(filled_form2.fields().size(), 4u);
  EXPECT_EQ(filled_form2.fields()[0].value(), u"1");
  EXPECT_EQ(filled_form2.fields()[1].value(), u"650");
  EXPECT_EQ(filled_form2.fields()[2].value(), u"555");
  EXPECT_EQ(filled_form2.fields()[3].value(), u"4567");

  // It is necessary to clear the cache before filling again. Otherwise Autofill
  // will not fill previously autofilled fields by design.
  FormsRemoved({form_with_us_number_max_length.global_id(),
                form_with_autocomplete.global_id()});
  FormsSeen({form_with_us_number_max_length, form_with_autocomplete});

  // For other countries, fill prefix and suffix fields with best effort.
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"GB");
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"447700954321");
  FormData filled_form3 =
      AutofillForm(form_with_us_number_max_length,
                   form_with_us_number_max_length.fields().front(), &profile);
  ASSERT_EQ(filled_form3.fields().size(), 4u);
  EXPECT_EQ(filled_form3.fields()[0].value(), u"4");
  EXPECT_EQ(filled_form3.fields()[1].value(), u"700");
  EXPECT_EQ(filled_form3.fields()[2].value(), u"95");
  EXPECT_EQ(filled_form3.fields()[3].value(), u"4321");

  FormData filled_form4 =
      AutofillForm(form_with_autocomplete,
                   form_with_autocomplete.fields().front(), &profile);
  ASSERT_EQ(filled_form4.fields().size(), 4u);
  EXPECT_EQ(filled_form4.fields()[0].value(), u"44");
  EXPECT_EQ(filled_form4.fields()[1].value(), u"7700");
  EXPECT_EQ(filled_form4.fields()[2].value(), u"95");
  EXPECT_EQ(filled_form4.fields()[3].value(), u"4321");
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

  ASSERT_EQ(filled_fields.size(), 4U);
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

  ASSERT_EQ(filled_fields.size(), 1u);
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
  ASSERT_EQ(filled_fields.size(), 7u);
  EXPECT_EQ(filled_fields[0].value(), u"John H. Doe");
  EXPECT_EQ(filled_fields[1].value(), u"1");
  EXPECT_EQ(filled_fields[2].value(), u"650");
  EXPECT_EQ(filled_fields[3].value(), u"5554567");
  EXPECT_EQ(filled_fields[4].value(), std::u16string());
  EXPECT_EQ(filled_fields[5].value(), std::u16string());
  EXPECT_EQ(filled_fields[6].value(), std::u16string());
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
  ASSERT_EQ(filled_fields.size(), 3u);
  EXPECT_EQ(filled_fields[0].value(), u"John H. Doe");
  EXPECT_EQ(filled_fields[1].value(), u"6505554567");
  EXPECT_EQ(filled_fields[2].value(), std::u16string());
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
  ASSERT_EQ(filled_fields.size(), 4u);
  EXPECT_EQ(filled_fields[0].value(), u"John H. Doe");
  EXPECT_EQ(filled_fields[1].value(), u"1");
  EXPECT_EQ(filled_fields[2].value(), std::u16string());
  EXPECT_EQ(filled_fields[3].value(), u"6505554567");
}

// Verify when extension is misclassified, and there is a complete
// phone field, we do not fill anything to extension field.
TEST_F(FormFillerTest, FillFirstPhoneNumber_NotFillMisclassifiedExtension) {
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
  ASSERT_EQ(filled_fields.size(), 3u);
  EXPECT_EQ(filled_fields[0].value(), u"John H. Doe");
  EXPECT_EQ(filled_fields[1].value(), u"5554567");
  EXPECT_EQ(filled_fields[2].value(), std::u16string());
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
  ASSERT_EQ(filled_fields.size(), 2U);
  EXPECT_EQ(filled_fields[0].value(), u"John H. Doe");
  EXPECT_EQ(filled_fields[1].value(), u"650");
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
  ASSERT_EQ(filled_fields.size(), 3u);
  EXPECT_EQ(filled_fields[0].value(), u"John H. Doe");
  EXPECT_EQ(filled_fields[1].value(), u"6505554567");
  EXPECT_EQ(filled_fields[2].value(), u"6505554567");
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
  ASSERT_EQ(filled_fields.size(), 3u);
  EXPECT_EQ(filled_fields[0].value(), u"John H. Doe");
  EXPECT_EQ(filled_fields[1].value(), std::u16string());
  EXPECT_EQ(filled_fields[2].value(), u"6505554567");
}

// Tests that non-focusable fields are not filled unless they are select
// elements.
TEST_F(FormFillerTest, FillNonFocusableFields) {
  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FULL, .autocomplete_attribute = "name"}}});

  // <input role="presentation"> were considered unfillable in the past but are
  // now fillable.
  {
    FormFieldData field =
        test::CreateTestSelectField("Country", "country", "", "country",
                                    {"CA", "US"}, {"Canada", "United States"});
    field.set_origin(form.main_frame_origin());
    test_api(form).Append(std::move(field));
  }
  test_api(form).field(-1).set_is_focusable(false);
  {
    FormFieldData field =
        test::CreateTestSelectField("State", "state", "", "address-level1",
                                    {"NY", "CA"}, {"New York", "California"});
    field.set_origin(form.main_frame_origin());
    test_api(form).Append(std::move(field));
  }
  test_api(form).field(-1).set_role(
      FormFieldData::RoleAttribute::kPresentation);

  {
    FormFieldData field = test::CreateTestFormField(
        "City", "city", "", FormControlType::kInputText);
    field.set_origin(form.main_frame_origin());
    test_api(form).Append(std::move(field));
  }
  test_api(form).field(-1).set_is_focusable(false);
  {
    FormFieldData field =
        test::CreateTestFormField("Street Address", "address", "",
                                  FormControlType::kInputText, "address");
    field.set_origin(form.main_frame_origin());
    test_api(form).Append(std::move(field));
  }
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
  EXPECT_FALSE(filled_fields[3].is_autofilled_according_to_renderer());
  EXPECT_TRUE(filled_fields[4].is_autofilled_according_to_renderer());
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
  ASSERT_EQ(filled_fields.size(), 6u);
  EXPECT_EQ(filled_fields[0].value(), u"John H. Doe");
  EXPECT_EQ(filled_fields[1].value(), u"6505554567");
  EXPECT_EQ(filled_fields[2].value(), std::u16string());
  EXPECT_EQ(filled_fields[3].value(), std::u16string());
  EXPECT_EQ(filled_fields[4].value(), std::u16string());
  EXPECT_EQ(filled_fields[5].value(), std::u16string());

  // Fill the second section.
  filled_fields = AutofillForm(form, form.fields()[3], &profile).fields();
  // Verify second section is filled with rationalization.
  ASSERT_EQ(filled_fields.size(), 6u);
  EXPECT_EQ(filled_fields[0].value(), std::u16string());
  EXPECT_EQ(filled_fields[1].value(), std::u16string());
  EXPECT_EQ(filled_fields[2].value(), std::u16string());
  EXPECT_EQ(filled_fields[3].value(), u"John H. Doe");
  EXPECT_EQ(filled_fields[4].value(), u"6505554567");
  EXPECT_EQ(filled_fields[5].value(), std::u16string());
}

// Tests that a prefilled country calling code does not prevent an Autofill.
TEST_F(FormFillerTest, FillPhoneNumber_OverwriteCountryCallingCode) {
  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FULL, .autocomplete_attribute = "name"},
                  {.role = PHONE_HOME_WHOLE_NUMBER,
                   .value = u"+49",
                   .autocomplete_attribute = "tel"}}});
  FormsSeen({form});

  AutofillProfile profile = test::GetFullProfile();
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"16505554567");
  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields().front(), &profile).fields();

  ASSERT_EQ(filled_fields.size(), 2U);
  EXPECT_EQ(filled_fields[0].value(), u"John H. Doe");
  EXPECT_THAT(filled_fields[1], AutofilledWith(u"16505554567"));
}

// Tests that a overwriting of country calling codes is limited to *valid* ones.
TEST_F(FormFillerTest,
       FillPhoneNumber_DoNotOverwriteInvalidCountryCallingCode) {
  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FULL, .autocomplete_attribute = "name"},
                  {.role = PHONE_HOME_WHOLE_NUMBER,
                   .value = u"+12",
                   .autocomplete_attribute = "tel"}}});
  FormsSeen({form});

  AutofillProfile profile = test::GetFullProfile();
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"16505554567");
  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields().front(), &profile).fields();

  ASSERT_EQ(filled_fields.size(), 2U);
  EXPECT_EQ(filled_fields[0].value(), u"John H. Doe");
  EXPECT_EQ(filled_fields[1].value(), u"+12");
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
          return ToSafeFieldType(p.type()).value_or(NO_SERVER_DATA);
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
  EXPECT_THAT(
      filled_fields,
      ElementsAre(AutofilledWith(profile.GetInfo(NAME_FULL, kAppLocale)),
                  AutofilledWith(profile.GetInfo(EMAIL_ADDRESS, kAppLocale))));
}

// Test that we can still fill a form when a field has been added to it.
TEST_F(FormFillerTest, FormChangesAddField) {
  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FULL, .autocomplete_attribute = "name"}}});
  FormsSeen({form});
  {
    FormFieldData field = test::CreateTestFormField(
        "email", "email", "", FormControlType::kInputText, "email");
    field.set_origin(form.main_frame_origin());
    test_api(form).Append(std::move(field));
  }
  FormsSeen({form});

  AutofillProfile profile = test::GetFullProfile();
  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields()[0], &profile).fields();
  EXPECT_THAT(
      filled_fields,
      ElementsAre(AutofilledWith(profile.GetInfo(NAME_FULL, kAppLocale)),
                  AutofilledWith(profile.GetInfo(EMAIL_ADDRESS, kAppLocale))));
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

  ASSERT_EQ(filled_form.fields().size(), 4u);
  EXPECT_THAT(filled_form.fields()[0],
              AutofilledWith(profile.GetInfo(NAME_FULL, kAppLocale)));
  EXPECT_THAT(filled_form.fields()[1],
              AutofilledWith(profile.GetInfo(ADDRESS_HOME_LINE1, kAppLocale)));
  EXPECT_FALSE(filled_form.fields()[2].is_autofilled_according_to_renderer());
  EXPECT_FALSE(filled_form.fields()[3].is_autofilled_according_to_renderer());

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
  EXPECT_THAT(
      later_filled_fields,
      ElementsAre(
          AutofilledWith(profile.GetInfo(NAME_FULL, kAppLocale)),
          AutofilledWith(profile.GetInfo(ADDRESS_HOME_LINE1, kAppLocale)),
          AutofilledWith(profile2.GetInfo(ADDRESS_HOME_ZIP, kAppLocale)),
          AutofilledWith(profile2.GetRawInfo(ADDRESS_HOME_COUNTRY))));
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
      filled_form, filled_form.fields()[0].global_id(), base::TimeTicks::Now(),
      AutofillManagerTestApi::pass_key());

  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);
  ASSERT_TRUE(form_structure->field(0)->all_modifiers().contains(
      FieldModifier::kAutofill));
  EXPECT_NE(form_structure->field(0)->last_modifier(),
            FieldModifier::kAutofill);
  EXPECT_THAT(form_structure->field(0)->autofill_source_profile_guid(),
              Optional(profile.guid()));
  EXPECT_THAT(form_structure->field(1), AutofilledWithProfile(profile));
}

// Tests that when a blocked field set is provided, fields in that set are
// skipped.
TEST_F(FormFillerTest, FillOrPreviewForm_WithBlockedFields) {
  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FIRST, .autocomplete_attribute = "given-name"},
                  {.role = NAME_LAST, .autocomplete_attribute = "family-name"},
                  {.role = EMAIL_ADDRESS, .autocomplete_attribute = "email"}}});
  FormsSeen({form});
  FormStructure* form_structure = GetFormStructure(form);

  AutofillProfile profile = test::GetFullProfile();
  base::flat_set<FieldGlobalId> blocked_fields = {form.fields()[2].global_id()};

  // Only the NAME_FIRST and NAME_LAST fields should reach the driver, as
  // EMAIL_ADDRESS should be blocked.
  EXPECT_CALL(
      autofill_driver(),
      ApplyFormAction(_, _,
                      ElementsAre(HasFieldId(form.fields()[0].global_id()),
                                  HasFieldId(form.fields()[1].global_id())),
                      _, _, _, _))
      .WillOnce(Return(std::vector<FieldGlobalId>{
          form.fields()[0].global_id(), form.fields()[1].global_id()}));

  form_filler().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, FillingProduct::kAddress, &profile,
      *form_structure, *form_structure->field(0), AutofillTriggerSource::kPopup,
      blocked_fields, FillId::Create(), /*forced_fill_values=*/{},
      FormFiller::RefillOptions::NotRefill());
  test_api(autofill_manager()).RunPendingPostFillTasks();

  // Verify that the skip reasons explicitly included being blocked.
  base::flat_map<FieldGlobalId, DenseSet<FieldFillingSkipReason>> skip_reasons =
      FormFiller::GetFieldFillingSkipReasons(
          *form_structure, *form_structure->field(0),
          FormFiller::RefillOptions::NotRefill(), FillingProduct::kAddress,
          AutofillTriggerSource::kPopup, autofill_client(), blocked_fields);

  EXPECT_TRUE(skip_reasons[form.fields()[0].global_id()].empty());
  EXPECT_TRUE(skip_reasons[form.fields()[1].global_id()].empty());
  EXPECT_THAT(
      skip_reasons[form.fields()[2].global_id()],
      Contains(
          FieldFillingSkipReason::kBlockedByOtherFillingOperationOrProduct));
}

// Tests that triggering an address fill on a field that is also fillable by
// Autofill AI does not block the trigger field itself, while still blocking
// other Autofill AI fields and respecting caller-supplied `blocked_fields`.
TEST_F(FormFillerTest,
       FillOrPreviewForm_AddressTriggerFieldNotBlockedByAutofillAi) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAiWithDataSchema};
  AutofillWebDataServiceTestHelper webdata_helper{
      std::make_unique<EntityTable>()};
  autofill_client().set_entity_data_manager(std::make_unique<EntityDataManager>(
      autofill_client().GetPrefs(), autofill_client().GetIdentityManager(),
      autofill_client().GetSyncService(),
      webdata_helper.autofill_webdata_service(),
      /*history_service=*/nullptr,
      /*pcontext_manager=*/nullptr,
      /*strike_database=*/nullptr,
      /*variation_country_code=*/GeoIpCountryCode("US")));
  autofill_client().GetEntityDataManager()->AddOrUpdateEntityInstance(
      test::GetPassportEntityInstance());
  webdata_helper.WaitUntilIdle();
  autofill_client().SetUpPrefsAndIdentityForAutofillAi();

  FormData form = FormSeen(
      {.fields = {{.server_type = NO_SERVER_DATA, .heuristic_type = NAME_FULL},
                  {.server_type = PASSPORT_NUMBER},
                  {.server_type = PASSPORT_ISSUING_COUNTRY,
                   .heuristic_type = ADDRESS_HOME_COUNTRY},
                  {.server_type = NO_SERVER_DATA,
                   .heuristic_type = ADDRESS_HOME_LINE1}}});
  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);
  ASSERT_THAT(GetFieldsFillableByAutofillAi(*form_structure, autofill_client()),
              UnorderedElementsAre(form.fields()[0].global_id(),
                                   form.fields()[1].global_id(),
                                   form.fields()[2].global_id()));

  // Caller-supplied `blocked_fields` must still block the trigger field if
  // explicitly requested.
  base::flat_map<FieldGlobalId, DenseSet<FieldFillingSkipReason>>
      caller_blocked_skip_reasons = FormFiller::GetFieldFillingSkipReasons(
          *form_structure, *form_structure->field(0),
          FormFiller::RefillOptions::NotRefill(), FillingProduct::kAddress,
          AutofillTriggerSource::kPopup, autofill_client(),
          /*blocked_fields=*/{form.fields()[0].global_id()});
  EXPECT_THAT(
      caller_blocked_skip_reasons[form.fields()[0].global_id()],
      Contains(
          FieldFillingSkipReason::kBlockedByOtherFillingOperationOrProduct));

  // Without caller-supplied `blocked_fields`, the trigger field (`NAME_FULL`)
  // and non-AI address field (`ADDRESS_HOME_LINE1`) are filled, while other AI
  // fields (`PASSPORT_NUMBER`, `PASSPORT_ISSUING_COUNTRY`) remain blocked.
  AutofillProfile profile = test::GetFullProfile();
  std::vector<FormFieldData> filled_fields =
      AutofillForm(form, form.fields()[0], &profile).fields();

  EXPECT_THAT(filled_fields[0],
              AutofilledWith(profile.GetInfo(NAME_FULL, kAppLocale)));
  EXPECT_FALSE(filled_fields[1].is_autofilled_according_to_renderer());
  EXPECT_FALSE(filled_fields[2].is_autofilled_according_to_renderer());
  EXPECT_THAT(filled_fields[3],
              AutofilledWith(profile.GetInfo(ADDRESS_HOME_LINE1, kAppLocale)));
}

// Tests that refills respect the blocked field set from the initial fill.
TEST_F(FormFillerTest, Refill_UsesBlockedFields) {
  FormData form = test::GetFormData(
      {.fields = {
           {.role = NAME_FIRST, .autocomplete_attribute = "given-name"},
           {.role = NAME_LAST, .autocomplete_attribute = "family-name"}}});
  FormsSeen({form});
  FormStructure* form_structure = GetFormStructure(form);

  AutofillProfile profile = test::GetFullProfile();
  base::flat_set<FieldGlobalId> blocked_fields = {form.fields()[1].global_id()};

  // Initial fill. This will block the 'family-name' field.
  EXPECT_CALL(autofill_driver(), ApplyFormAction)
      .WillOnce(
          Return(std::vector<FieldGlobalId>{form.fields()[0].global_id()}));
  form_filler().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, FillingProduct::kAddress, &profile,
      *form_structure, *form_structure->field(0), AutofillTriggerSource::kPopup,
      blocked_fields, FillId::Create(), /*forced_fill_values=*/{},
      FormFiller::RefillOptions::NotRefill());
  test_api(autofill_manager()).RunPendingPostFillTasks();

  // Append a new field to the form, which will trigger a refill when the form
  // is re-parsed.
  {
    FormFieldData field = test::CreateTestFormField(
        "Full Name", "full name", "", FormControlType::kInputText, "name");
    field.set_origin(form.main_frame_origin());
    test_api(form).Append(std::move(field));
  }

  // TODO(crbug.com/489280538): Currently the refill will always fill the new
  // field, even if it "shouldn't" (i.e., if the caller that originally set the
  // blocked fields list would also have blocked the new field).
  EXPECT_CALL(
      autofill_driver(),
      ApplyFormAction(_, _,
                      ElementsAre(HasFieldId(form.fields()[0].global_id()),
                                  HasFieldId(form.fields()[2].global_id())),
                      _, _, _, _))
      .WillOnce(Return(std::vector<FieldGlobalId>{
          form.fields()[0].global_id(), form.fields()[2].global_id()}));

  // Trigger the refill. This happens synchronously due to the use of
  // TestFormFiller.
  FormsSeen({form});
}

// Regression test that a field with an unrelated type doesn't cause a crash
// (crbug.com/324811625).
TEST_F(FormFillerTest, PreFilledCCFieldInAddressFormDoesNotCauseCrash) {
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

// Tests that two initial fills are assigned distinct IDs.
TEST_F(FormFillerTest, InitialFillsHaveDistinctIds) {
  FillId fill_id1;
  FillId fill_id2;
  base::RunLoop run_loop;
  {
    InSequence s;
    EXPECT_CALL(autofill_driver(), ApplyFormAction)
        .WillOnce(
            DoAll(SaveArg<3>(&fill_id1), Return(std::vector<FieldGlobalId>{})));
    EXPECT_CALL(autofill_driver(), ApplyFormAction)
        .WillOnce(DoAll(SaveArg<3>(&fill_id2),
                        base::test::RunOnceClosure(run_loop.QuitClosure()),
                        Return(std::vector<FieldGlobalId>{})));
  }

  CreditCard credit_card = test::GetCreditCard();
  FormData form1 = test::GetFormData(
      {.fields = {
           {.role = CREDIT_CARD_NAME_FULL, .autocomplete_attribute = "cc-name"},
           {.role = CREDIT_CARD_NUMBER, .autocomplete_attribute = "cc-number"},
           {.role = CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
            .autocomplete_attribute = "cc-exp"}}});
  FormData form2 = test::GetFormData(
      {.fields = {
           {.role = CREDIT_CARD_NAME_FULL, .autocomplete_attribute = "cc-name"},
           {.role = CREDIT_CARD_NUMBER, .autocomplete_attribute = "cc-number"},
           {.role = CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
            .autocomplete_attribute = "cc-exp"}}});

  FormsSeen({form1, form2});

  form_filler().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, FillingProduct::kCreditCard,
      &credit_card, *GetFormStructure(form1),
      *GetAutofillField(form1.global_id(), form1.fields().front().global_id()),
      AutofillTriggerSource::kPopup, /*blocked_fields=*/{}, FillId::Create(),
      /*forced_fill_values=*/{}, FormFiller::RefillOptions::NotRefill());
  test_api(autofill_manager()).RunPendingPostFillTasks();
  form_filler().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, FillingProduct::kCreditCard,
      &credit_card, *GetFormStructure(form2),
      *GetAutofillField(form2.global_id(), form2.fields().front().global_id()),
      AutofillTriggerSource::kPopup, /*blocked_fields=*/{}, FillId::Create(),
      /*forced_fill_values=*/{}, FormFiller::RefillOptions::NotRefill());

  std::move(run_loop).Run();
  EXPECT_FALSE(fill_id1->is_empty());
  EXPECT_FALSE(fill_id2->is_empty());
  EXPECT_NE(fill_id1, fill_id2);
}

// Tests that the initial fill and a refill are assigned the same IDs.
TEST_F(FormFillerTest, FillAndRefillHaveSameFillId) {
  FillId initial_fill_id;
  FillId refill_fill_id;
  MockFunction<void(std::string_view)> check;
  base::RunLoop run_loop;
  {
    InSequence s;
    EXPECT_CALL(autofill_driver(), ApplyFormAction)
        .WillOnce(DoAll(SaveArg<3>(&initial_fill_id),
                        Return(std::vector<FieldGlobalId>{})));
    EXPECT_CALL(check, Call("initial fill complete"));
    EXPECT_CALL(autofill_driver(), ApplyFormAction)
        .WillOnce(DoAll(SaveArg<3>(&refill_fill_id),
                        base::test::RunOnceClosure(run_loop.QuitClosure()),
                        Return(std::vector<FieldGlobalId>{})));
  }

  CreditCard credit_card = test::GetCreditCard();
  FormData form = test::GetFormData(
      {.fields = {
           {.role = CREDIT_CARD_NAME_FULL, .autocomplete_attribute = "cc-name"},
           {.role = CREDIT_CARD_NUMBER, .autocomplete_attribute = "cc-number"},
           {.role = CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
            .autocomplete_attribute = "cc-exp"}}});

  FormFieldData cvc_field = form.fields().back();
  test_api(form).fields().pop_back();
  FormsSeen({form});

  form_filler().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, FillingProduct::kCreditCard,
      &credit_card, *GetFormStructure(form),
      *GetAutofillField(form.global_id(), form.fields().front().global_id()),
      AutofillTriggerSource::kPopup, /*blocked_fields=*/{}, FillId::Create(),
      /*forced_fill_values=*/{}, FormFiller::RefillOptions::NotRefill());
  test_api(autofill_manager()).RunPendingPostFillTasks();
  check.Call("initial fill complete");

  test_api(form).fields().push_back(std::move(cvc_field));
  FormsSeen({form});

  std::move(run_loop).Run();
  EXPECT_FALSE(initial_fill_id->is_empty());
  EXPECT_FALSE(refill_fill_id->is_empty());
  EXPECT_EQ(initial_fill_id, refill_fill_id);
}

// Tests that refills skip sensitive fields if the initial fill did not include
// them.
TEST_F(FormFillerTest, RefillSkipsSensitiveFieldsIfNotFilledInitially) {
  CreditCard credit_card = test::GetCreditCard();
  // Create form with a credit card name by making one with a name and number,
  // then removing the number field. It will be readded after initial fill.
  FormData form =
      test::GetFormData({.fields = {{.role = CREDIT_CARD_NAME_FULL,
                                     .autocomplete_attribute = "cc-name"},
                                    {.role = CREDIT_CARD_NUMBER,
                                     .autocomplete_attribute = "cc-number"}}});
  FormFieldData number_field = form.fields().back();
  test_api(form).fields().pop_back();
  FormsSeen({form});

  // Initial fill.
  form = AutofillForm(form, form.fields().front(), &credit_card);

  // Now add the number field back to trigger refill.
  test_api(form).fields().push_back(std::move(number_field));

  // Expect refill, but expect NO new fields to be filled because number field
  // is sensitive and was not filled initially.
  EXPECT_CALL(autofill_driver(), ApplyFormAction)
      .WillOnce([&](mojom::FormActionType action_type,
                    mojom::ActionPersistence action_persistence,
                    base::span<const FormFieldData> data, const FillId& fill_id,
                    bool supports_refill, const url::Origin& triggered_origin,
                    const absl::flat_hash_map<FieldGlobalId, FieldType>&) {
        EXPECT_TRUE(data.empty());
        return std::vector<FieldGlobalId>{};
      });

  FormsSeen({form});
}

// Tests that a programmatic refill can be triggered within the timeout.
// Also tests that a second refill within the timeout is a no-op.
TEST_F(FormFillerTest, ProgrammaticRefillBeforeTimeout) {
  FillId fill_id;
  FillId refill_id;
  base::RunLoop run_loop;
  MockFunction<void(std::string_view)> check;
  {
    InSequence s;
    EXPECT_CALL(autofill_driver(), ApplyFormAction)
        .WillOnce(DoAll(SaveArg<3>(&fill_id),
                        base::test::RunOnceClosure(run_loop.QuitClosure()),
                        Return(std::vector<FieldGlobalId>{})));
    EXPECT_CALL(check, Call("initial fill complete"));
    EXPECT_CALL(autofill_driver(), ApplyFormAction)
        .WillOnce(DoAll(SaveArg<3>(&refill_id),
                        Return(std::vector<FieldGlobalId>{})));
    EXPECT_CALL(check, Call("refill complete"));
    EXPECT_CALL(autofill_driver(), ApplyFormAction).Times(0);
    EXPECT_CALL(check, Call("second refill ignored"));
  }

  CreditCard credit_card = test::GetCreditCard();
  FormData form = test::GetFormData(
      {.fields = {
           {.role = CREDIT_CARD_NAME_FULL, .autocomplete_attribute = "cc-name"},
           {.role = CREDIT_CARD_NUMBER, .autocomplete_attribute = "cc-number"},
           {.role = CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
            .autocomplete_attribute = "cc-exp"}}});

  FormsSeen({form});

  // The original fill.
  form_filler().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, FillingProduct::kCreditCard,
      &credit_card, *GetFormStructure(form),
      *GetAutofillField(form.global_id(), form.fields().front().global_id()),
      AutofillTriggerSource::kPopup, /*blocked_fields=*/{}, FillId::Create(),
      /*forced_fill_values=*/{}, FormFiller::RefillOptions::NotRefill());
  test_api(autofill_manager()).RunPendingPostFillTasks();
  std::move(run_loop).Run();
  check.Call("initial fill complete");

  task_environment_.FastForwardBy(kLimitBeforeProgrammaticRefill / 2);

  // The first refill. `TestFormFiller::StartRefillTimer()` triggers the refill
  // synchronously instead of after `kWaitTimeForDynamicForms`.
  form_filler().MaybeScheduleProgrammaticRefill(fill_id);
  test_api(autofill_manager()).RunPendingPostFillTasks();
  check.Call("refill complete");

  // The second refill.
  form_filler().MaybeScheduleProgrammaticRefill(fill_id);
  check.Call("second refill ignored");

  EXPECT_FALSE(fill_id->is_empty());
  EXPECT_EQ(fill_id, refill_id);
}

// Tests that a programmatic refill cannot be triggered outside of the timeout.
TEST_F(FormFillerTest, NoProgrammaticRefillAfterTimeout) {
  FillId fill_id;
  base::RunLoop run_loop;
  MockFunction<void(std::string_view)> check;
  {
    InSequence s;
    EXPECT_CALL(autofill_driver(), ApplyFormAction)
        .WillOnce(DoAll(SaveArg<3>(&fill_id),
                        base::test::RunOnceClosure(run_loop.QuitClosure()),
                        Return(std::vector<FieldGlobalId>{})));
    EXPECT_CALL(check, Call("initial fill complete"));
    EXPECT_CALL(autofill_driver(), ApplyFormAction).Times(0);
    EXPECT_CALL(check, Call("refill ignored"));
  }

  CreditCard credit_card = test::GetCreditCard();
  FormData form = test::GetFormData(
      {.fields = {
           {.role = CREDIT_CARD_NAME_FULL, .autocomplete_attribute = "cc-name"},
           {.role = CREDIT_CARD_NUMBER, .autocomplete_attribute = "cc-number"},
           {.role = CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
            .autocomplete_attribute = "cc-exp"}}});

  FormsSeen({form});

  // The original fill.
  form_filler().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, FillingProduct::kCreditCard,
      &credit_card, *GetFormStructure(form),
      *GetAutofillField(form.global_id(), form.fields().front().global_id()),
      AutofillTriggerSource::kPopup, /*blocked_fields=*/{}, FillId::Create(),
      /*forced_fill_values=*/{}, FormFiller::RefillOptions::NotRefill());
  test_api(autofill_manager()).RunPendingPostFillTasks();
  std::move(run_loop).Run();
  check.Call("initial fill complete");

  task_environment_.FastForwardBy(kLimitBeforeProgrammaticRefill +
                                  base::Seconds(1));

  // The first refill.
  form_filler().MaybeScheduleProgrammaticRefill(fill_id);
  check.Call("refill ignored");
}

class MockFormFiller : public TestFormFiller {
 public:
  explicit MockFormFiller(BrowserAutofillManager& manager)
      : TestFormFiller(manager) {}
  MOCK_METHOD(void,
              StartRefillTimer,
              (const FormGlobalId& form_id,
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

TEST_F(RefillTest, FormChanged_IrrelevantFieldChanges) {
  AutofillProfile profile = test::GetFullProfile();
  auto change_field_css_classes = [](FormData& form) {
    auto fields = form.ExtractFields();
    fields.front().set_css_classes(u"field-css-classes");
    form.set_fields(std::move(fields));
  };

  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FULL, .autocomplete_attribute = "name"}}});
  FormsSeen({form});
  AutofillForm(form, form.fields().front(), &profile);
  EXPECT_CALL(mock_form_filler(), StartRefillTimer).Times(0);
  change_field_css_classes(form);
  FormsSeen({form});
}

TEST_F(RefillTest, SelectOptionsChanged_IrrelevantSelectField) {
  AutofillProfile profile = test::GetFullProfile();

  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FULL, .autocomplete_attribute = "name"},
                  {.form_control_type = mojom::FormControlType::kSelectOne}}});
  FormsSeen({form});
  AutofillForm(form, form.fields().front(), &profile);
  EXPECT_CALL(mock_form_filler(), StartRefillTimer).Times(0);
  autofill_manager().OnSelectFieldOptionsDidChange(
      form, form.fields().back().global_id(),
      AutofillManagerTestApi::pass_key());
}

// Test fixture for FormFiller::SuppressAutomaticRefills().
class RefillTest_SuppressAutomaticRefills
    : public RefillTest,
      public testing::WithParamInterface<bool> {
 public:
  bool should_suppress_automatic_refills() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(, RefillTest_SuppressAutomaticRefills, Bool());

// Tests that SuppressAutomaticRefills() prevents automatic refills for the
// given FillId.
//
// If `should_suppress_automatic_refills()` is false, the test calls
// SuppressAutomaticRefills() with a random FillId. Since no such fill exists,
// a refill is expected.
TEST_P(RefillTest_SuppressAutomaticRefills, SuppressAutomaticRefills) {
  MockFunction<void(std::string_view)> check;
  {
    InSequence s;
    EXPECT_CALL(autofill_driver(), ApplyFormAction)
        .WillOnce([&](mojom::FormActionType action_type,
                      mojom::ActionPersistence action_persistence,
                      base::span<const FormFieldData> data,
                      const FillId& fill_id, bool supports_refill,
                      const url::Origin& triggered_origin,
                      const absl::flat_hash_map<FieldGlobalId, FieldType>&
                          field_type_map) {
          mock_form_filler().SuppressAutomaticRefills(
              should_suppress_automatic_refills() ? fill_id : FillId::Create());
          return std::vector<FieldGlobalId>{};
        });
    EXPECT_CALL(check, Call("initial fill complete"));
    EXPECT_CALL(mock_form_filler(), StartRefillTimer)
        .Times(should_suppress_automatic_refills() ? 0 : 1);
  }

  CreditCard credit_card = test::GetCreditCard();
  FormData form = test::GetFormData(
      {.fields = {
           {.role = CREDIT_CARD_NAME_FULL, .autocomplete_attribute = "cc-name"},
           {.role = CREDIT_CARD_NUMBER, .autocomplete_attribute = "cc-number"},
           {.role = CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
            .autocomplete_attribute = "cc-exp"}}});

  FormFieldData cvc_field = form.fields().back();
  test_api(form).fields().pop_back();
  FormsSeen({form});

  form_filler().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, FillingProduct::kCreditCard,
      &credit_card, *GetFormStructure(form),
      *GetAutofillField(form.global_id(), form.fields().front().global_id()),
      AutofillTriggerSource::kPopup, /*blocked_fields=*/{}, FillId::Create(),
      /*forced_fill_values=*/{}, FormFiller::RefillOptions::NotRefill());
  test_api(autofill_manager()).RunPendingPostFillTasks();
  check.Call("initial fill complete");

  test_api(form).fields().push_back(std::move(cvc_field));
  FormsSeen({form});
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
  EXPECT_THAT(first_fill_data.fields(),
              ElementsAre(AutofilledWith(u"Elvis Presley"),
                          AutofilledWith(u"4234567890123456"),
                          AutofilledWith(u"04/2999")));

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
      form_after_js_modification.fields()[2].global_id(), u"04/2999",
      AutofillManagerTestApi::pass_key());
  test_api(autofill_manager()).RunPendingPostFillTasks();

  testing::Mock::VerifyAndClearExpectations(&autofill_driver());

  if (test_case.triggers_refill) {
    ASSERT_EQ(refilled_fields.size(), 1u);
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
    Values(
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
  EXPECT_FALSE(form.fields()[0].is_autofilled_according_to_renderer());
  // The second field should not change, because the last operation that
  // modified it isn't the one that is being currently undone.
  EXPECT_THAT(form.fields()[1], AutofilledWith(u"Other"));
}

// Tests that undoing a form fill skips fields that were subsequently edited or
// cleared by the user, including fields that had a pre-filled value prior to
// autofilling.
TEST_F(FormFillerTest, UndoSkipsUserModifiedFields) {
  FormData form = test::GetFormData(
      {.fields = {
           {.role = NAME_FIRST, .autocomplete_attribute = "given-name"},
           {.role = NAME_LAST, .autocomplete_attribute = "family-name"},
           {.role = ADDRESS_HOME_COUNTRY,
            .value = u"DE",
            .autocomplete_attribute = "country",
            .form_control_type = FormControlType::kSelectOne,
            .select_options = {{{.value = u"DE", .text = u"Germany"},
                                {.value = u"US", .text = u"United States"},
                                {.value = u"CA", .text = u"Canada"}}}}}});
  FormsSeen({form});

  AutofillProfile profile = test::GetFullProfile();
  form = AutofillForm(form, form.fields()[0], &profile);
  ASSERT_THAT(form.fields()[0], AutofilledWith(u"John"));
  ASSERT_THAT(form.fields()[1], AutofilledWith(u"Doe"));
  ASSERT_THAT(form.fields()[2], AutofilledWith(u"US"));

  // Simulate the user clearing `NAME_FIRST` and changing the `<select>` value,
  // while leaving `NAME_LAST` autofilled.
  test_api(form).field(0).set_value(u"");
  test_api(form).field(0).set_is_autofilled_according_to_renderer(false);
  test_api(form).field(2).set_value(u"CA");
  test_api(form).field(2).set_is_autofilled_according_to_renderer(false);
  test_api(*GetFormStructure(form)).UpdateFormData(form);

  form = UndoAutofill(form, form.fields()[1]);

  EXPECT_EQ(form.fields()[0].value(), u"");
  EXPECT_FALSE(form.fields()[0].is_autofilled_according_to_renderer());
  EXPECT_EQ(form.fields()[1].value(), u"");
  EXPECT_FALSE(form.fields()[1].is_autofilled_according_to_renderer());
  EXPECT_EQ(form.fields()[2].value(), u"CA");
  EXPECT_FALSE(form.fields()[2].is_autofilled_according_to_renderer());
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
  EXPECT_FALSE(form.fields()[0].is_autofilled_according_to_renderer());
  // The second field should not change.
  EXPECT_THAT(form.fields()[1], AutofilledWith(u"Other"));

  // Now Undo the second filling operation on the second field.
  form = UndoAutofill(form, form.fields()[1]);
  EXPECT_TRUE(form.fields()[0].value().empty());
  EXPECT_FALSE(form.fields()[0].is_autofilled_according_to_renderer());
  // The second field should restore the value of the first filling operation.
  EXPECT_THAT(form.fields()[1], AutofilledWith(u"Doe"));

  // Now Undo the first filling operation on the second field.
  form = UndoAutofill(form, form.fields()[1]);
  EXPECT_TRUE(form.fields()[0].value().empty());
  EXPECT_FALSE(form.fields()[0].is_autofilled_according_to_renderer());
  // The second field should be cleared, as this was its initial state.
  EXPECT_TRUE(form.fields()[1].value().empty());
  EXPECT_FALSE(form.fields()[1].is_autofilled_according_to_renderer());
}

// Tests that undoing a filling operation on a field discards other fields that
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
  EXPECT_FALSE(form.fields()[0].is_autofilled_according_to_renderer());
  EXPECT_THAT(form.fields()[1], AutofilledWith(u"Other"));
}

// Tests that when Glic triggers a filling operation, some fields that are
// usually skipped by regular operations are not skipped.
TEST_F(FormFillerTest, GlicFillingDoeNotSkipSomeUsuallySkippableFields) {
  FormData form = test::GetFormData(
      {.fields = {
           {.role = ADDRESS_HOME_COUNTRY, .autocomplete_attribute = "country"},
           {.role = NAME_FIRST, .autocomplete_attribute = "given-name"},
           {.role = NAME_MIDDLE, .autocomplete_attribute = "additional-name"},
           // Simulate that field [3] has an invalid autocomplete attribute.
           {.role = NAME_LAST, .autocomplete_attribute = "nope"}}});
  // Simulate that field [2] is prefilled on page load.
  test_api(form).field(2).set_value(u"G");

  FormsSeen({form});
  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);
  ASSERT_EQ(form_structure->fields().size(), 4u);

  // Simulate that the user typed into field [1].
  test_api(form).field(1).set_value(u"Ji");
  form_structure->field(1)->set_value(u"Ji");
  test_api(form).field(1).set_properties_mask(kUserTyped);
  form_structure->field(1)->set_properties_mask(kUserTyped);

  base::flat_map<FieldGlobalId, DenseSet<FieldFillingSkipReason>> skip_reasons =
      FormFiller::GetFieldFillingSkipReasons(
          *form_structure, *form_structure->field(0),
          FormFiller::RefillOptions::NotRefill(), FillingProduct::kAddress,
          AutofillTriggerSource::kPopup, autofill_client(),
          /*blocked_fields=*/{});

  ASSERT_EQ(skip_reasons[form.fields()[1].global_id()],
            DenseSet<FieldFillingSkipReason>{
                FieldFillingSkipReason::kUserFilledFields});
  ASSERT_EQ(skip_reasons[form.fields()[2].global_id()],
            DenseSet<FieldFillingSkipReason>{
                FieldFillingSkipReason::kValuePrefilled});
  ASSERT_EQ(skip_reasons[form.fields()[3].global_id()],
            DenseSet<FieldFillingSkipReason>{
                FieldFillingSkipReason::kUnrecognizedAutocompleteAttribute});

  skip_reasons = FormFiller::GetFieldFillingSkipReasons(
      *form_structure, *form_structure->field(0),
      FormFiller::RefillOptions::NotRefill(), FillingProduct::kAddress,
      AutofillTriggerSource::kGlic, autofill_client(), /*blocked_fields=*/{});

  EXPECT_TRUE(skip_reasons[form.fields()[1].global_id()].empty());
  EXPECT_TRUE(skip_reasons[form.fields()[2].global_id()].empty());
  EXPECT_TRUE(skip_reasons[form.fields()[3].global_id()].empty());
}

// Tests that when Glic triggers a filling operation, fields that are filled are
// not marked as autofilled in the renderer.
TEST_F(FormFillerTest, GlicFillingDoeNotSetIsAutofilled) {
  FormData standard_filled_form = test::GetFormData(
      {.fields = {{.role = NAME_FULL, .autocomplete_attribute = "name"}}});

  FormData glic_filled_form = test::GetFormData(
      {.fields = {{.role = NAME_FULL, .autocomplete_attribute = "name"}}});

  FormsSeen({standard_filled_form, glic_filled_form});

  AutofillProfile profile = test::GetFullProfile();
  standard_filled_form = AutofillForm(
      standard_filled_form, standard_filled_form.fields().front(), &profile);
  glic_filled_form =
      AutofillForm(glic_filled_form, glic_filled_form.fields().front(),
                   &profile, AutofillTriggerSource::kGlic);

  ASSERT_THAT(standard_filled_form.fields()[0], AutofilledWith(u"John H. Doe"));
  EXPECT_EQ(glic_filled_form.fields()[0].value(),
            standard_filled_form.fields()[0].value());
  EXPECT_FALSE(
      glic_filled_form.fields()[0].is_autofilled_according_to_renderer());
}

// Tests that when autofilling a select field, we correctly set the
// `selected_option_text` to send to the renderer, even if the field contains
// multiple options with the same value.
TEST_F(FormFillerTest, SelectElementWithDuplicateValuesAndDistinctTexts) {
  FormData us_filled_form = test::GetFormData(
      {.fields = {{.role = NAME_FULL},
                  {.role = PHONE_HOME_COUNTRY_CODE,
                   .form_control_type = FormControlType::kSelectOne,
                   .select_options = {{{.value = u"1", .text = u"Canada (+1)"},
                                       {.value = u"1",
                                        .text = u"United States (+1)"}}}},
                  {.role = PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX}}});

  FormData ca_filled_form = test::GetFormData(
      {.fields = {{.role = NAME_FULL},
                  {.role = PHONE_HOME_COUNTRY_CODE,
                   .form_control_type = FormControlType::kSelectOne,
                   .select_options = {{{.value = u"1", .text = u"Canada (+1)"},
                                       {.value = u"1",
                                        .text = u"United States (+1)"}}}},
                  {.role = PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX}}});

  FormsSeen({us_filled_form, ca_filled_form});

  AutofillProfile us_profile = test::GetFullProfile(AddressCountryCode("US"));
  AutofillProfile ca_profile = test::GetFullProfile(AddressCountryCode("CA"));

  us_filled_form = AutofillForm(us_filled_form, us_filled_form.fields().front(),
                                &us_profile);
  ca_filled_form = AutofillForm(ca_filled_form, ca_filled_form.fields().front(),
                                &ca_profile);

  ASSERT_THAT(us_filled_form.fields()[1], AutofilledWith(u"1"));
  ASSERT_THAT(ca_filled_form.fields()[1], AutofilledWith(u"1"));

  EXPECT_EQ(us_filled_form.fields()[1].selected_option_text(),
            u"United States (+1)");
  EXPECT_EQ(ca_filled_form.fields()[1].selected_option_text(), u"Canada (+1)");
}

// Tests filling a single-field OTP form. Ensures the OTP field is autofilled
// with the complete token value and the browser cache reflects
// `FillingProduct::kOneTimePassword` and `ONE_TIME_CODE`, while non-OTP fields
// remain untouched.
TEST_F(FormFillerTest, FillOtp_SingleField) {
  FormData form = FormSeen(
      {.fields = {
           {.role = ONE_TIME_CODE, .autocomplete_attribute = "one-time-code"},
           {.role = USERNAME, .autocomplete_attribute = "username"}}});
  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);
  ASSERT_EQ(form.fields().size(), 2u);
  ASSERT_EQ(form_structure->fields().size(), 2u);

  const OtpFillData otp_fill_data = {{form.fields()[0].global_id(), u"123456"}};
  FormData filled_form = AutofillForm(form, form.fields()[0], &otp_fill_data);

  ASSERT_EQ(filled_form.fields().size(), 2u);

  // The OTP field should be autofilled.
  EXPECT_THAT(filled_form.fields()[0], AutofilledWith(u"123456"));
  EXPECT_EQ(form_structure->field(0)->last_modifier(),
            FieldModifier::kAutofill);
  EXPECT_EQ(form_structure->field(0)->filling_product(),
            FillingProduct::kOneTimePassword);
  EXPECT_EQ(form_structure->field(0)->autofilled_type(), ONE_TIME_CODE);

  // The username field should not be filled.
  EXPECT_FALSE(filled_form.fields()[1].is_autofilled_according_to_renderer());
  EXPECT_TRUE(filled_form.fields()[1].value().empty());
  EXPECT_NE(form_structure->field(1)->last_modifier(),
            FieldModifier::kAutofill);
  EXPECT_EQ(form_structure->field(1)->filling_product(), FillingProduct::kNone);
  EXPECT_FALSE(form_structure->field(1)->autofilled_type());
}

// Tests filling a multi-field OTP form where each digit is distributed into a
// separate input field.
TEST_F(FormFillerTest, FillOtp_MultiField) {
  FormData form = FormSeen(
      {.fields = {
           {.role = ONE_TIME_CODE, .autocomplete_attribute = "one-time-code"},
           {.role = ONE_TIME_CODE, .autocomplete_attribute = "one-time-code"},
           {.role = ONE_TIME_CODE, .autocomplete_attribute = "one-time-code"},
           {.role = ONE_TIME_CODE,
            .autocomplete_attribute = "one-time-code"}}});
  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);
  ASSERT_EQ(form.fields().size(), 4u);
  ASSERT_EQ(form_structure->fields().size(), 4u);

  OtpFillData otp_fill_data = CreateFillDataForOtpSuggestion(
      *form_structure, *form_structure->field(0), u"1234");
  ASSERT_EQ(otp_fill_data.size(), 4u);

  FormData filled_form = AutofillForm(form, form.fields()[0], &otp_fill_data);

  EXPECT_THAT(filled_form.fields(),
              ElementsAre(AutofilledWith(u"1"), AutofilledWith(u"2"),
                          AutofilledWith(u"3"), AutofilledWith(u"4")));

  for (size_t i = 0; i < 4; ++i) {
    EXPECT_EQ(form_structure->field(i)->last_modifier(),
              FieldModifier::kAutofill);
    EXPECT_EQ(form_structure->field(i)->filling_product(),
              FillingProduct::kOneTimePassword);
    EXPECT_EQ(form_structure->field(i)->autofilled_type(), ONE_TIME_CODE);
  }
}

// Tests previewing an OTP value on a single-field form. The renderer receives
// the previewed value, but the browser cache is not modified.
TEST_F(FormFillerTest, PreviewOtp_SingleField) {
  FormData form =
      FormSeen({.fields = {{.role = ONE_TIME_CODE,
                            .autocomplete_attribute = "one-time-code"}}});
  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);
  ASSERT_EQ(form.fields().size(), 1u);
  ASSERT_EQ(form_structure->fields().size(), 1u);

  const OtpFillData otp_fill_data = {{form.fields()[0].global_id(), u"123456"}};

  std::vector<FormFieldData> previewed_fields;
  EXPECT_CALL(
      autofill_driver(),
      ApplyFormAction(mojom::FormActionType::kFill,
                      mojom::ActionPersistence::kPreview, _, _, _, _, _))
      .WillOnce([&previewed_fields](
                    mojom::FormActionType, mojom::ActionPersistence,
                    base::span<const FormFieldData> data, const FillId&, bool,
                    const url::Origin&,
                    const absl::flat_hash_map<FieldGlobalId, FieldType>&) {
        previewed_fields = base::ToVector(data);
        return base::ToVector(previewed_fields, &FormFieldData::global_id);
      });

  form_filler().FillOrPreviewForm(
      mojom::ActionPersistence::kPreview, FillingProduct::kOneTimePassword,
      &otp_fill_data, *form_structure, *form_structure->field(0),
      AutofillTriggerSource::kPopup, /*blocked_fields=*/{}, FillId::Create(),
      /*forced_fill_values=*/{}, FormFiller::RefillOptions::NotRefill());

  ASSERT_EQ(previewed_fields.size(), 1u);
  EXPECT_THAT(previewed_fields[0], AutofilledWith(u"123456"));

  // Browser cache must remain untouched on preview.
  EXPECT_NE(form_structure->field(0)->last_modifier(),
            FieldModifier::kAutofill);
  EXPECT_EQ(form_structure->field(0)->filling_product(), FillingProduct::kNone);
  EXPECT_FALSE(form_structure->field(0)->autofilled_type());
}

// Tests previewing OTP values on a multi-field form.
TEST_F(FormFillerTest, PreviewOtp_MultiField) {
  FormData form = FormSeen(
      {.fields = {
           {.role = ONE_TIME_CODE, .autocomplete_attribute = "one-time-code"},
           {.role = ONE_TIME_CODE, .autocomplete_attribute = "one-time-code"},
           {.role = ONE_TIME_CODE, .autocomplete_attribute = "one-time-code"},
           {.role = ONE_TIME_CODE,
            .autocomplete_attribute = "one-time-code"}}});
  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);
  ASSERT_EQ(form.fields().size(), 4u);
  ASSERT_EQ(form_structure->fields().size(), 4u);

  OtpFillData otp_fill_data = CreateFillDataForOtpSuggestion(
      *form_structure, *form_structure->field(0), u"4321");

  std::vector<FormFieldData> previewed_fields;
  EXPECT_CALL(
      autofill_driver(),
      ApplyFormAction(mojom::FormActionType::kFill,
                      mojom::ActionPersistence::kPreview, _, _, _, _, _))
      .WillOnce([&previewed_fields](
                    mojom::FormActionType, mojom::ActionPersistence,
                    base::span<const FormFieldData> data, const FillId&, bool,
                    const url::Origin&,
                    const absl::flat_hash_map<FieldGlobalId, FieldType>&) {
        previewed_fields = base::ToVector(data);
        return base::ToVector(previewed_fields, &FormFieldData::global_id);
      });

  form_filler().FillOrPreviewForm(
      mojom::ActionPersistence::kPreview, FillingProduct::kOneTimePassword,
      &otp_fill_data, *form_structure, *form_structure->field(0),
      AutofillTriggerSource::kPopup, /*blocked_fields=*/{}, FillId::Create(),
      /*forced_fill_values=*/{}, FormFiller::RefillOptions::NotRefill());

  EXPECT_THAT(previewed_fields,
              ElementsAre(AutofilledWith(u"4"), AutofilledWith(u"3"),
                          AutofilledWith(u"2"), AutofilledWith(u"1")));

  for (size_t i = 0; i < 4; ++i) {
    EXPECT_NE(form_structure->field(i)->last_modifier(),
              FieldModifier::kAutofill);
    EXPECT_EQ(form_structure->field(i)->filling_product(),
              FillingProduct::kNone);
    EXPECT_FALSE(form_structure->field(i)->autofilled_type());
  }
}

// Tests skip reasons determined for OTP filling using
// `FormFiller::GetFieldFillingSkipReasons()`.
TEST_F(FormFillerTest, OtpFilling_SkipReasons) {
  // Field 2 is prefilled on page load.
  FormData form = FormSeen({.fields = {{.role = ONE_TIME_CODE},
                                       {.role = ONE_TIME_CODE},
                                       {.role = ONE_TIME_CODE, .value = u"1"},
                                       {.role = ONE_TIME_CODE},
                                       {.role = ONE_TIME_CODE},
                                       {.role = ONE_TIME_CODE}}});

  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);
  ASSERT_EQ(form.fields().size(), 6u);
  ASSERT_EQ(form_structure->fields().size(), 6u);

  // Field 1 was typed into by the user.
  test_api(form).field(1).set_value(u"2");
  form_structure->field(1)->set_value(u"2");
  test_api(form).field(1).set_properties_mask(kUserTyped);
  form_structure->field(1)->set_properties_mask(kUserTyped);

  // Field 3 was already autofilled.
  form_structure->field(3)->set_value(u"3");
  form_structure->field(3)->AddFieldModifier(FieldModifier::kAutofill);

  // Field 4 is in a different section.
  base::flat_map<LocalFrameToken, size_t> frame_token_ids;
  form_structure->field(4)->set_section(
      Section::FromFieldIdentifier(*form_structure->field(4), frame_token_ids));

  // Field 5 is blocked.
  base::flat_set<FieldGlobalId> blocked_fields = {form.fields()[5].global_id()};

  base::flat_map<FieldGlobalId, DenseSet<FieldFillingSkipReason>> skip_reasons =
      FormFiller::GetFieldFillingSkipReasons(
          *form_structure, *form_structure->field(0),
          FormFiller::RefillOptions::NotRefill(),
          FillingProduct::kOneTimePassword, AutofillTriggerSource::kPopup,
          autofill_client(), blocked_fields);

  EXPECT_TRUE(skip_reasons[form.fields()[0].global_id()].empty());
  EXPECT_THAT(skip_reasons[form.fields()[1].global_id()],
              Contains(FieldFillingSkipReason::kUserFilledFields));
  EXPECT_THAT(skip_reasons[form.fields()[2].global_id()],
              Contains(FieldFillingSkipReason::kValuePrefilled));
  EXPECT_THAT(skip_reasons[form.fields()[3].global_id()],
              Contains(FieldFillingSkipReason::kAlreadyAutofilled));
  EXPECT_THAT(skip_reasons[form.fields()[4].global_id()],
              Contains(FieldFillingSkipReason::kNotInFilledSection));
  EXPECT_THAT(
      skip_reasons[form.fields()[5].global_id()],
      Contains(
          FieldFillingSkipReason::kBlockedByOtherFillingOperationOrProduct));
}

// Tests that skip reasons (such as user-edited fields, blocked fields, or
// missing fill values) are respected during OTP form filling and prevent those
// fields from being filled.
TEST_F(FormFillerTest, FillOtp_SkipReasonsAppliedDuringFill) {
  FormData form = FormSeen({.fields = {{.role = ONE_TIME_CODE},
                                       {.role = ONE_TIME_CODE},
                                       {.role = ONE_TIME_CODE},
                                       {.role = NAME_FULL}}});
  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);
  ASSERT_EQ(form.fields().size(), 4u);
  ASSERT_EQ(form_structure->fields().size(), 4u);

  // User typed into field 1.
  test_api(form).field(1).set_value(u"9");
  form_structure->field(1)->set_value(u"9");
  test_api(form).field(1).set_properties_mask(kUserTyped);
  form_structure->field(1)->set_properties_mask(kUserTyped);

  // Field 2 is blocked.
  base::flat_set<FieldGlobalId> blocked_fields = {form.fields()[2].global_id()};

  // Field 3 has no value in `OtpFillData`.
  const OtpFillData otp_fill_data = {{form.fields()[0].global_id(), u"1"},
                                     {form.fields()[1].global_id(), u"2"},
                                     {form.fields()[2].global_id(), u"3"}};

  std::vector<FormFieldData> filled_fields;
  EXPECT_CALL(autofill_driver(), ApplyFormAction)
      .WillOnce([&filled_fields](
                    mojom::FormActionType, mojom::ActionPersistence,
                    base::span<const FormFieldData> data, const FillId&, bool,
                    const url::Origin&,
                    const absl::flat_hash_map<FieldGlobalId, FieldType>&) {
        filled_fields = base::ToVector(data);
        return base::ToVector(filled_fields, &FormFieldData::global_id);
      });

  form_filler().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, FillingProduct::kOneTimePassword,
      &otp_fill_data, *form_structure, *form_structure->field(0),
      AutofillTriggerSource::kPopup, blocked_fields, FillId::Create(),
      /*forced_fill_values=*/{}, FormFiller::RefillOptions::NotRefill());

  // Only field 0 should be sent to the renderer to be filled.
  ASSERT_EQ(filled_fields.size(), 1u);
  EXPECT_EQ(filled_fields[0].global_id(), form.fields()[0].global_id());
  EXPECT_EQ(filled_fields[0].value(), u"1");
}

// Tests undoing a single-field OTP filling operation.
TEST_F(FormFillerTest, UndoOtp_SingleField) {
  FormData form =
      FormSeen({.fields = {{.role = ONE_TIME_CODE,
                            .autocomplete_attribute = "one-time-code"}}});
  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);
  ASSERT_EQ(form.fields().size(), 1u);
  ASSERT_EQ(form_structure->fields().size(), 1u);

  const OtpFillData otp_fill_data = {{form.fields()[0].global_id(), u"123456"}};
  FormData filled_form = AutofillForm(form, form.fields()[0], &otp_fill_data);

  ASSERT_EQ(filled_form.fields().size(), 1u);
  ASSERT_THAT(filled_form.fields()[0], AutofilledWith(u"123456"));
  ASSERT_EQ(form_structure->field(0)->last_modifier(),
            FieldModifier::kAutofill);
  ASSERT_EQ(form_structure->field(0)->filling_product(),
            FillingProduct::kOneTimePassword);
  ASSERT_EQ(form_structure->field(0)->autofilled_type(), ONE_TIME_CODE);

  FormData reverted_form = UndoAutofill(filled_form, filled_form.fields()[0]);

  ASSERT_EQ(reverted_form.fields().size(), 1u);
  EXPECT_FALSE(reverted_form.fields()[0].is_autofilled_according_to_renderer());
  EXPECT_TRUE(reverted_form.fields()[0].value().empty());

  EXPECT_NE(form_structure->field(0)->last_modifier(),
            FieldModifier::kAutofill);
  EXPECT_EQ(form_structure->field(0)->filling_product(), FillingProduct::kNone);
  EXPECT_FALSE(form_structure->field(0)->autofilled_type());
}

// Tests undoing a multi-field OTP filling operation.
TEST_F(FormFillerTest, UndoOtp_MultiField) {
  FormData form = FormSeen(
      {.fields = {
           {.role = ONE_TIME_CODE, .autocomplete_attribute = "one-time-code"},
           {.role = ONE_TIME_CODE, .autocomplete_attribute = "one-time-code"},
           {.role = ONE_TIME_CODE, .autocomplete_attribute = "one-time-code"},
           {.role = ONE_TIME_CODE,
            .autocomplete_attribute = "one-time-code"}}});
  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);
  ASSERT_EQ(form.fields().size(), 4u);
  ASSERT_EQ(form_structure->fields().size(), 4u);

  OtpFillData otp_fill_data = CreateFillDataForOtpSuggestion(
      *form_structure, *form_structure->field(0), u"5678");
  FormData filled_form = AutofillForm(form, form.fields()[0], &otp_fill_data);

  EXPECT_THAT(filled_form.fields(),
              ElementsAre(AutofilledWith(u"5"), AutofilledWith(u"6"),
                          AutofilledWith(u"7"), AutofilledWith(u"8")));

  for (size_t i = 0; i < 4; ++i) {
    ASSERT_EQ(form_structure->field(i)->last_modifier(),
              FieldModifier::kAutofill);
    ASSERT_EQ(form_structure->field(i)->filling_product(),
              FillingProduct::kOneTimePassword);
    ASSERT_EQ(form_structure->field(i)->autofilled_type(), ONE_TIME_CODE);
  }

  // Trigger Undo from an arbitrary filled field (e.g. the 3rd field).
  FormData reverted_form = UndoAutofill(filled_form, filled_form.fields()[2]);

  ASSERT_EQ(reverted_form.fields().size(), 4u);

  for (size_t i = 0; i < 4; ++i) {
    EXPECT_FALSE(
        reverted_form.fields()[i].is_autofilled_according_to_renderer());
    EXPECT_TRUE(reverted_form.fields()[i].value().empty());

    EXPECT_NE(form_structure->field(i)->last_modifier(),
              FieldModifier::kAutofill);
    EXPECT_EQ(form_structure->field(i)->filling_product(),
              FillingProduct::kNone);
    EXPECT_FALSE(form_structure->field(i)->autofilled_type());
  }
}

// Tests previewing an Undo operation on an OTP filled field.
TEST_F(FormFillerTest, UndoOtp_Preview) {
  FormData form =
      FormSeen({.fields = {{.role = ONE_TIME_CODE,
                            .autocomplete_attribute = "one-time-code"}}});
  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);
  ASSERT_EQ(form.fields().size(), 1u);
  ASSERT_EQ(form_structure->fields().size(), 1u);

  const OtpFillData otp_fill_data = {{form.fields()[0].global_id(), u"123456"}};
  AutofillForm(form, form.fields()[0], &otp_fill_data);

  ASSERT_EQ(form_structure->field(0)->last_modifier(),
            FieldModifier::kAutofill);
  ASSERT_EQ(form_structure->field(0)->filling_product(),
            FillingProduct::kOneTimePassword);
  ASSERT_EQ(form_structure->field(0)->autofilled_type(), ONE_TIME_CODE);

  // Preview Undo: driver receives undo form action with preview persistence.
  std::vector<FormFieldData> undo_previewed_fields;
  EXPECT_CALL(
      autofill_driver(),
      ApplyFormAction(mojom::FormActionType::kUndo,
                      mojom::ActionPersistence::kPreview, _, _, _, _, _))
      .WillOnce([&undo_previewed_fields](
                    mojom::FormActionType, mojom::ActionPersistence,
                    base::span<const FormFieldData> data, const FillId&, bool,
                    const url::Origin&,
                    const absl::flat_hash_map<FieldGlobalId, FieldType>&) {
        undo_previewed_fields = base::ToVector(data);
        return base::ToVector(undo_previewed_fields, &FormFieldData::global_id);
      });

  autofill_manager().UndoAutofill(mojom::ActionPersistence::kPreview,
                                  form.global_id(),
                                  form.fields()[0].global_id());

  ASSERT_EQ(undo_previewed_fields.size(), 1u);
  EXPECT_TRUE(undo_previewed_fields[0].value().empty());
  EXPECT_FALSE(undo_previewed_fields[0].is_autofilled_according_to_renderer());

  // Cache must remain marked as autofilled during preview.
  EXPECT_EQ(form_structure->field(0)->last_modifier(),
            FieldModifier::kAutofill);
  EXPECT_EQ(form_structure->field(0)->filling_product(),
            FillingProduct::kOneTimePassword);
  EXPECT_EQ(form_structure->field(0)->autofilled_type(), ONE_TIME_CODE);

  // An actual Undo will revert the cache.
  EXPECT_CALL(autofill_driver(),
              ApplyFormAction(mojom::FormActionType::kUndo,
                              mojom::ActionPersistence::kFill, _, _, _, _, _))
      .WillOnce(
          Return(base::flat_set<FieldGlobalId>{form.fields()[0].global_id()}));

  autofill_manager().UndoAutofill(mojom::ActionPersistence::kFill,
                                  form.global_id(),
                                  form.fields()[0].global_id());

  EXPECT_NE(form_structure->field(0)->last_modifier(),
            FieldModifier::kAutofill);
  EXPECT_EQ(form_structure->field(0)->filling_product(), FillingProduct::kNone);
  EXPECT_FALSE(form_structure->field(0)->autofilled_type());
}

// Tests credit card swapping across cardholder name transitions when
// `kAutofillPaymentsFieldSwapping` is enabled:
// a) card with name -> card with name (overwrites name and card details)
// b) card with name -> card without name (clears name and updates card details)
// c) card without name -> card with name (fills name and updates card details)
TEST_F(FormFillerTest, PaymentsSwappingCardNameTransitions) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillPaymentsFieldSwapping);

  CreditCard card_with_name_1;
  test::SetCreditCardInfo(&card_with_name_1, "Elvis Presley",
                          "4234 5678 9012 3456", "04", "2999", "1");

  CreditCard card_with_name_2;
  test::SetCreditCardInfo(&card_with_name_2, "Jane Smith",
                          "5412 7512 3412 3456", "06", "2998", "1");

  CreditCard nameless_card;
  test::SetCreditCardInfo(&nameless_card, "", "3782 822463 10005", "08", "2997",
                          "1");

  FormData form = test::CreateTestCreditCardFormData(/*is_https=*/true,
                                                     /*use_month_type=*/false);
  FormsSeen({form});
  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);

  // Initial fill with `card_with_name_1`.
  form = AutofillForm(form, form.fields()[1], &card_with_name_1);
  EXPECT_THAT(form.fields()[0], AutofilledWith(u"Elvis Presley"));
  EXPECT_THAT(form.fields()[1], AutofilledWith(u"4234567890123456"));
  EXPECT_THAT(form.fields()[2], AutofilledWith(u"04"));
  EXPECT_THAT(form.fields()[3], AutofilledWith(u"2999"));

  // a) Card with name -> card with name.
  form = AutofillForm(form, form.fields()[1], &card_with_name_2);
  EXPECT_THAT(form.fields()[0], AutofilledWith(u"Jane Smith"));
  EXPECT_THAT(form.fields()[1], AutofilledWith(u"5412751234123456"));
  EXPECT_THAT(form.fields()[2], AutofilledWith(u"06"));
  EXPECT_THAT(form.fields()[3], AutofilledWith(u"2998"));
  EXPECT_EQ(form_structure->field(0)->last_modifier(),
            FieldModifier::kAutofill);
  EXPECT_EQ(form_structure->field(0)->filling_product(),
            FillingProduct::kCreditCard);

  // b) Card with name -> card without name.
  form = AutofillForm(form, form.fields()[1], &nameless_card);
  EXPECT_EQ(form.fields()[0].value(), u"");
  EXPECT_FALSE(form.fields()[0].is_autofilled_according_to_renderer());
  EXPECT_THAT(form.fields()[1], AutofilledWith(u"378282246310005"));
  EXPECT_THAT(form.fields()[2], AutofilledWith(u"08"));
  EXPECT_THAT(form.fields()[3], AutofilledWith(u"2997"));
  EXPECT_NE(form_structure->field(0)->last_modifier(),
            FieldModifier::kAutofill);

  // c) Card without name -> card with name.
  form = AutofillForm(form, form.fields()[1], &card_with_name_1);
  EXPECT_THAT(form.fields()[0], AutofilledWith(u"Elvis Presley"));
  EXPECT_THAT(form.fields()[1], AutofilledWith(u"4234567890123456"));
  EXPECT_THAT(form.fields()[2], AutofilledWith(u"04"));
  EXPECT_THAT(form.fields()[3], AutofilledWith(u"2999"));
  EXPECT_EQ(form_structure->field(0)->last_modifier(),
            FieldModifier::kAutofill);
  EXPECT_EQ(form_structure->field(0)->filling_product(),
            FillingProduct::kCreditCard);
}

// Tests that payment swapping does not overwrite or clear fields that were
// previously autofilled by `FillingProduct::kAddress` or
// `FillingProduct::kAutocomplete` rather than `FillingProduct::kCreditCard`
// across all cardholder name transitions:
// a) card with name -> card with name
// b) card with name -> card without name
// c) card without name -> card with name
TEST_F(FormFillerTest,
       PaymentsSwappingDoesNotOverwriteAddressOrAutocompleteFilledField) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillPaymentsFieldSwapping);

  CreditCard card_with_name_1;
  test::SetCreditCardInfo(&card_with_name_1, "Elvis Presley",
                          "4234 5678 9012 3456", "04", "2999", "1");

  CreditCard card_with_name_2;
  test::SetCreditCardInfo(&card_with_name_2, "Jane Smith",
                          "5412 7512 3412 3456", "06", "2998", "1");

  CreditCard nameless_card;
  test::SetCreditCardInfo(&nameless_card, "", "3782 822463 10005", "08", "2997",
                          "1");

  for (FillingProduct prior_product :
       {FillingProduct::kAddress, FillingProduct::kAutocomplete}) {
    FormData form = test::CreateTestCreditCardFormData(
        /*is_https=*/true, /*use_month_type=*/false);
    test_api(form).field(0).set_value(u"John Doe");
    test_api(form).field(0).set_is_autofilled_according_to_renderer(true);
    FormsSeen({form});

    FormStructure* form_structure = GetFormStructure(form);
    ASSERT_TRUE(form_structure);
    form_structure->field(0)->AddFieldModifier(FieldModifier::kAutofill);
    form_structure->field(0)->set_filling_product(prior_product);

    // Initial credit card fill with `card_with_name_1` must not overwrite the
    // `prior_product` name field.
    form = AutofillForm(form, form.fields()[1], &card_with_name_1);
    EXPECT_THAT(form.fields()[0], AutofilledWith(u"John Doe"));
    EXPECT_THAT(form.fields()[1], AutofilledWith(u"4234567890123456"));
    EXPECT_EQ(form_structure->field(0)->filling_product(), prior_product);

    // a) Card with name -> card with name: `field(0)` stays "John Doe", while
    // card details swap to `card_with_name_2`.
    form = AutofillForm(form, form.fields()[1], &card_with_name_2);
    EXPECT_THAT(form.fields()[0], AutofilledWith(u"John Doe"));
    EXPECT_THAT(form.fields()[1], AutofilledWith(u"5412751234123456"));
    EXPECT_EQ(form_structure->field(0)->filling_product(), prior_product);

    // b) Card with name -> card without name: `field(0)` is not cleared, while
    // card details swap to `nameless_card`.
    form = AutofillForm(form, form.fields()[1], &nameless_card);
    EXPECT_THAT(form.fields()[0], AutofilledWith(u"John Doe"));
    EXPECT_THAT(form.fields()[1], AutofilledWith(u"378282246310005"));
    EXPECT_EQ(form_structure->field(0)->filling_product(), prior_product);

    // c) Card without name -> card with name: `field(0)` is not overwritten,
    // while card details swap back to `card_with_name_1`.
    form = AutofillForm(form, form.fields()[1], &card_with_name_1);
    EXPECT_THAT(form.fields()[0], AutofilledWith(u"John Doe"));
    EXPECT_THAT(form.fields()[1], AutofilledWith(u"4234567890123456"));
    EXPECT_EQ(form_structure->field(0)->filling_product(), prior_product);
  }
}

// Tests that a non-credit-card fill operation (e.g. `FillingProduct::kAddress`)
// on fields with union `AutofillType`s (holding both a `NAME_*` type and a
// `CREDIT_CARD_NAME_*` type) does not trigger payment swapping on non-trigger
// fields that were previously filled by `FillingProduct::kCreditCard`.
TEST_F(FormFillerTest,
       PaymentsSwappingDoesNotTriggerForNonCreditCardFillOnUnionTypeFields) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillPaymentsFieldSwapping);

  FormData form = test::GetFormData(
      {.fields = {{.role = CREDIT_CARD_NAME_FIRST,
                   .autocomplete_attribute = "cc-given-name"},
                  {.role = CREDIT_CARD_NAME_LAST,
                   .autocomplete_attribute = "cc-family-name"}}});
  FormsSeen({form});

  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);
  form_structure->field(0)->SetTypeTo(
      AutofillType(FieldTypeSet{NAME_FIRST, CREDIT_CARD_NAME_FIRST}),
      AutofillPredictionSource::kHeuristics);
  form_structure->field(1)->SetTypeTo(
      AutofillType(FieldTypeSet{NAME_LAST, CREDIT_CARD_NAME_LAST}),
      AutofillPredictionSource::kHeuristics);

  CreditCard credit_card = test::GetCreditCard();
  form = AutofillForm(form, form.fields()[0], &credit_card);
  ASSERT_THAT(form.fields()[0], AutofilledWith(credit_card.GetInfo(
                                    CREDIT_CARD_NAME_FIRST, kAppLocale)));
  ASSERT_THAT(form.fields()[1], AutofilledWith(credit_card.GetInfo(
                                    CREDIT_CARD_NAME_LAST, kAppLocale)));
  ASSERT_EQ(form_structure->field(1)->filling_product(),
            FillingProduct::kCreditCard);

  // Trigger an address fill on `field(0)` with a full profile. `field(0)` (the
  // trigger field) is overwritten, but `field(1)` must not be swapped or
  // overwritten by the address fill even though both fields have
  // `CREDIT_CARD_NAME_*` types and `field(1)` was filled by
  // `FillingProduct::kCreditCard`.
  AutofillProfile profile = test::GetFullProfile();
  form = AutofillForm(form, form.fields()[0], &profile);
  EXPECT_THAT(form.fields()[0],
              AutofilledWith(profile.GetInfo(NAME_FIRST, kAppLocale)));
  EXPECT_THAT(form.fields()[1], AutofilledWith(credit_card.GetInfo(
                                    CREDIT_CARD_NAME_LAST, kAppLocale)));
  EXPECT_EQ(form_structure->field(1)->filling_product(),
            FillingProduct::kCreditCard);

  // Also verify that an address profile with an empty last name does not clear
  // `field(1)` via payment swapping.
  AutofillProfile profile_without_last_name = test::GetFullProfile();
  profile_without_last_name.ClearFields({NAME_LAST});
  form = AutofillForm(form, form.fields()[0], &profile_without_last_name);
  EXPECT_THAT(form.fields()[0],
              AutofilledWith(
                  profile_without_last_name.GetInfo(NAME_FIRST, kAppLocale)));
  EXPECT_THAT(form.fields()[1], AutofilledWith(credit_card.GetInfo(
                                    CREDIT_CARD_NAME_LAST, kAppLocale)));
  EXPECT_EQ(form_structure->field(1)->filling_product(),
            FillingProduct::kCreditCard);
}

// Tests payments field swapping on a checkout page where `cc-name` is in the
// main frame and the other card fields are in a cross-origin payment iframe.
// Filling a card from `cc-name` only fills `cc-name` because the iframe's
// fields cannot be filled from the main frame. When the user then selects a
// card from the still-empty `cc-number` field in the iframe, the iframe can
// fill the non-sensitive `cc-name` in the main frame, so `cc-name` is swapped
// to the newly selected card.
TEST_F(FormFillerTest, PaymentsSwappingFromEmptyFieldInCrossOriginIframe) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillPaymentsFieldSwapping};

  const url::Origin main_origin =
      autofill_client().GetLastCommittedPrimaryMainFrameOrigin();
  const url::Origin iframe_origin =
      url::Origin::Create(GURL("https://payments.example"));
  ASSERT_NE(main_origin, iframe_origin);

  // Mirrors `AutofillDriverRouter::IsSafeToFill()` for a cross-origin payment
  // iframe without `allow="autofill"`: fields can always be filled from their
  // own origin, and the iframe may additionally fill the non-sensitive
  // cardholder name in the main frame.
  autofill_driver().set_is_safe_to_fill_callback(base::BindRepeating(
      [](const FormFieldData& field, FieldType filled_type,
         const url::Origin& main_origin, const url::Origin& trigger_origin) {
        return field.origin() == trigger_origin ||
               (field.origin() == main_origin &&
                filled_type == CREDIT_CARD_NAME_FULL);
      }));

  // Returns a form whose first field (`cc-name`) is in the main frame and whose
  // other fields are in the iframe. `cc-name` is typed as both a name and a
  // cardholder name, so it can be filled from an address or a card.
  auto create_checkout_form = [&] {
    FormData form = test::CreateTestCreditCardFormData(
        /*is_https=*/true, /*use_month_type=*/false);
    form.set_main_frame_origin(main_origin);
    test_api(form).field(0).set_origin(main_origin);
    for (size_t i = 1; i < form.fields().size(); ++i) {
      test_api(form).field(i).set_origin(iframe_origin);
    }
    FormsSeen({form});
    CHECK_DEREF(GetFormStructure(form))
        .field(0)
        ->SetTypeTo(
            AutofillType(FieldTypeSet{NAME_FULL, CREDIT_CARD_NAME_FULL}),
            AutofillPredictionSource::kHeuristics);
    return form;
  };

  CreditCard card_with_name_1;
  test::SetCreditCardInfo(&card_with_name_1, "Elvis Presley",
                          "4234 5678 9012 3456", "04", "2999", "1");
  CreditCard card_with_name_2;
  test::SetCreditCardInfo(&card_with_name_2, "Jane Smith",
                          "5412 7512 3412 3456", "06", "2998", "1");
  CreditCard nameless_card;
  test::SetCreditCardInfo(&nameless_card, "", "3782 822463 10005", "08", "2997",
                          "1");

  // Filling from `cc-name` only fills `cc-name`.
  {
    FormData form = create_checkout_form();
    form = AutofillForm(form, form.fields()[0], &card_with_name_1);
    EXPECT_THAT(form.fields()[0], AutofilledWith(u"Elvis Presley"));
    EXPECT_EQ(form.fields()[1].value(), u"");
  }

  // Selecting a card with a name from the empty `cc-number` overwrites the
  // name of the first card.
  {
    FormData form = create_checkout_form();
    form = AutofillForm(form, form.fields()[0], &card_with_name_1);
    form = AutofillForm(form, form.fields()[1], &card_with_name_2);
    EXPECT_THAT(form.fields()[0], AutofilledWith(u"Jane Smith"));
    EXPECT_THAT(form.fields()[1], AutofilledWith(u"5412751234123456"));
    EXPECT_THAT(form.fields()[2], AutofilledWith(u"06"));
    EXPECT_THAT(form.fields()[3], AutofilledWith(u"2998"));
  }

  // Selecting a card without a name from the empty `cc-number` clears the name
  // of the first card.
  {
    FormData form = create_checkout_form();
    form = AutofillForm(form, form.fields()[0], &card_with_name_1);
    form = AutofillForm(form, form.fields()[1], &nameless_card);
    EXPECT_EQ(form.fields()[0].value(), u"");
    EXPECT_FALSE(form.fields()[0].is_autofilled_according_to_renderer());
    EXPECT_THAT(form.fields()[1], AutofilledWith(u"378282246310005"));
    EXPECT_THAT(form.fields()[2], AutofilledWith(u"08"));
    EXPECT_THAT(form.fields()[3], AutofilledWith(u"2997"));
  }

  // A name that was filled from an address is never swapped.
  {
    FormData form = create_checkout_form();
    AutofillProfile profile = test::GetFullProfile();
    form = AutofillForm(form, form.fields()[0], &profile);
    form = AutofillForm(form, form.fields()[1], &card_with_name_2);
    EXPECT_THAT(form.fields()[0],
                AutofilledWith(profile.GetInfo(NAME_FULL, kAppLocale)));
    EXPECT_THAT(form.fields()[1], AutofilledWith(u"5412751234123456"));
  }
}

// Tests that selecting a card from a field that does not hold an autofilled
// card value still swaps the previously filled card completely: values that
// the new card lacks (here: the expiration date) are cleared rather than left
// next to the new card's number.
TEST_F(FormFillerTest, PaymentsSwappingFromEmptyFieldClearsMissingCardValues) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillPaymentsFieldSwapping};

  CreditCard nameless_card;
  test::SetCreditCardInfo(&nameless_card, "", "3782 822463 10005", "08", "2997",
                          "1");
  CreditCard card_without_expiration;
  test::SetCreditCardInfo(&card_without_expiration, "Elvis Presley",
                          "4234 5678 9012 3456", "", "", "1");

  FormData form = test::CreateTestCreditCardFormData(/*is_https=*/true,
                                                     /*use_month_type=*/false);
  FormsSeen({form});

  // The nameless card leaves `cc-name` empty.
  form = AutofillForm(form, form.fields()[1], &nameless_card);
  ASSERT_EQ(form.fields()[0].value(), u"");
  ASSERT_THAT(form.fields()[1], AutofilledWith(u"378282246310005"));
  ASSERT_THAT(form.fields()[2], AutofilledWith(u"08"));
  ASSERT_THAT(form.fields()[3], AutofilledWith(u"2997"));

  // Select the card without expiration date from the empty `cc-name`.
  form = AutofillForm(form, form.fields()[0], &card_without_expiration);
  EXPECT_THAT(form.fields()[0], AutofilledWith(u"Elvis Presley"));
  EXPECT_THAT(form.fields()[1], AutofilledWith(u"4234567890123456"));
  EXPECT_EQ(form.fields()[2].value(), u"");
  EXPECT_FALSE(form.fields()[2].is_autofilled_according_to_renderer());
  EXPECT_EQ(form.fields()[3].value(), u"");
  EXPECT_FALSE(form.fields()[3].is_autofilled_according_to_renderer());
}

// Tests that the chrome://autofill-internals log of `FillOrPreviewForm` reports
// the state of the fields before filling, i.e., before the post-fill
// bookkeeping updates the cache.
TEST_F(FormFillerTest, FillOrPreviewForm_LogsPreFillState) {
  CollectingLogReceiver log_receiver;
  autofill_client().log_router().RegisterReceiver(&log_receiver);

  FormData form =
      FormSeen({.fields = {{.role = NAME_FIRST, .value = u"OldFirst"},
                           {.role = NAME_LAST, .value = u""}}});
  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);

  AutofillProfile profile = test::GetFullProfile();
  EXPECT_CALL(autofill_driver(), ApplyFormAction).WillOnce(FillInto(form));

  form_filler().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, FillingProduct::kAddress, &profile,
      *form_structure, *form_structure->field(0), AutofillTriggerSource::kPopup,
      /*blocked_fields=*/{}, FillId::Create(), /*forced_fill_values=*/{},
      FormFiller::RefillOptions::NotRefill());
  test_api(autofill_manager()).RunPendingPostFillTasks();
  autofill_client().log_router().UnregisterReceiver(&log_receiver);

  // Neither field was autofilled before filling and `field(1)` was empty.
  EXPECT_THAT(log_receiver.entries(),
              Contains(HasSubstr("Field 0 Fillable - has value: 1->1; "
                                 "autofilled: 0->1.")));
  EXPECT_THAT(log_receiver.entries(),
              Contains(HasSubstr("Field 1 Fillable - has value: 0->1; "
                                 "autofilled: 0->1.")));
}

// Runs the post-fill bookkeeping of fills synchronously or, if the parameter
// is true, in a separate task.
// TODO(crbug.com/582269824) Merge with FormFillerTest.
class FormFillerPostFillTest : public base::test::WithFeatureOverride,
                               public FormFillerTest {
 public:
  FormFillerPostFillTest()
      : base::test::WithFeatureOverride(
            features::kAutofillPostTaskAfterFillOrPreviewForm) {}

  // Returns true if the post-fill bookkeeping of fills runs in a separate task.
  bool IsPostFillAsync() const { return IsParamFeatureEnabled(); }
};

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(FormFillerPostFillTest);

// Tests that previews neither record undo history nor update the cache, but
// notify observers synchronously.
TEST_P(FormFillerPostFillTest, FillOrPreviewForm_PreviewNotifiesObserversOnly) {
  FormData form =
      FormSeen({.fields = {{.role = NAME_FIRST}, {.role = NAME_LAST}}});
  const FieldGlobalId first_name_id = form.fields()[0].global_id();
  const FieldGlobalId last_name_id = form.fields()[1].global_id();
  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);

  AutofillProfile profile = test::GetFullProfile();

  NiceMock<MockAutofillManagerObserver> observer;
  base::ScopedObservation<AutofillManager, AutofillManager::Observer>
      observation(&observer);
  observation.Observe(&autofill_manager());
  MockFunction<void()> check;
  {
    InSequence s;
    EXPECT_CALL(observer,
                OnFillOrPreviewForm(
                    _, form.global_id(), first_name_id,
                    mojom::ActionPersistence::kPreview,
                    ElementsAre(first_name_id, last_name_id), _,
                    VariantWith<const AutofillProfile*>(Pointee(
                        Property(&AutofillProfile::guid, profile.guid())))));
    EXPECT_CALL(check, Call);
  }

  EXPECT_CALL(autofill_driver(), ApplyFormAction)
      .WillOnce([](mojom::FormActionType, mojom::ActionPersistence,
                   base::span<const FormFieldData> data, const FillId&, bool,
                   const url::Origin&,
                   const absl::flat_hash_map<FieldGlobalId, FieldType>&) {
        return base::ToVector(data, &FormFieldData::global_id);
      });

  form_filler().FillOrPreviewForm(
      mojom::ActionPersistence::kPreview, FillingProduct::kAddress, &profile,
      *form_structure, *form_structure->field(0), AutofillTriggerSource::kPopup,
      /*blocked_fields=*/{}, FillId::Create(), /*forced_fill_values=*/{},
      FormFiller::RefillOptions::NotRefill());
  check.Call();
  EXPECT_FALSE(test_api(form_filler()).has_pending_post_fill_tasks());

  EXPECT_EQ(form_structure->field(0)->last_modifier(), std::nullopt);
  EXPECT_EQ(form_structure->field(1)->last_modifier(), std::nullopt);
  EXPECT_FALSE(form_structure->last_filling_timestamp().has_value());

  // There is no undo history.
  EXPECT_CALL(autofill_driver(), ApplyFormAction).Times(0);
  autofill_manager().UndoAutofill(mojom::ActionPersistence::kFill,
                                  form.global_id(), first_name_id);
}

// Tests that the post-fill bookkeeping of a fill runs synchronously if
// `features::kAutofillPostTaskAfterFillOrPreviewForm` is disabled and in a
// separate task otherwise, and that it has the same result in both cases.
// In the async case, cache updates, undo history, and
// `Observer::OnFillOrPreviewForm` are deferred until the bookkeeping task runs,
// while `ApplyFormAction` and setting `last_filling_timestamp` happen
// immediately.
// TODO(crbug.com/582269824) Update test description when launching
// `features::kAutofillPostTaskAfterFillOrPreviewForm`.
TEST_P(FormFillerPostFillTest, FillOrPreviewForm_PostFillBookkeeping) {
  FormData form =
      FormSeen({.fields = {{.role = NAME_FIRST, .value = u"OldFirst"},
                           {.role = NAME_LAST, .value = u""}}});
  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);

  NiceMock<MockAutofillManagerObserver> observer;
  base::ScopedObservation<AutofillManager, AutofillManager::Observer>
      observation(&observer);
  observation.Observe(&autofill_manager());
  MockFunction<void()> check;
  {
    InSequence s;
    if (IsPostFillAsync()) {
      EXPECT_CALL(check, Call);
      EXPECT_CALL(observer, OnFillOrPreviewForm);
    } else {
      EXPECT_CALL(observer, OnFillOrPreviewForm);
      EXPECT_CALL(check, Call);
    }
  }

  AutofillProfile profile = test::GetFullProfile();
  EXPECT_CALL(autofill_driver(), ApplyFormAction).WillOnce(FillInto(form));
  form_filler().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, FillingProduct::kAddress, &profile,
      *form_structure, *form_structure->field(0), AutofillTriggerSource::kPopup,
      /*blocked_fields=*/{}, FillId::Create(), /*forced_fill_values=*/{},
      FormFiller::RefillOptions::NotRefill());

  // `ApplyFormAction` has executed, so `form` received the filled values.
  ASSERT_THAT(form.fields()[0],
              AutofilledWith(profile.GetInfo(NAME_FIRST, kAppLocale)));
  ASSERT_THAT(form.fields()[1],
              AutofilledWith(profile.GetInfo(NAME_LAST, kAppLocale)));

  EXPECT_EQ(test_api(form_filler()).has_pending_post_fill_tasks(),
            IsPostFillAsync());
  if (IsPostFillAsync()) {
    // Before the posted task runs, the cache, undo history and observers have
    // not been updated yet, but `last_filling_timestamp` has been set.
    EXPECT_EQ(form_structure->field(0)->last_modifier(), std::nullopt);
    EXPECT_EQ(form_structure->field(1)->last_modifier(), std::nullopt);
    EXPECT_TRUE(form_structure->last_filling_timestamp().has_value());

    EXPECT_CALL(autofill_driver(), ApplyFormAction).Times(0);
    form_filler().UndoAutofill(mojom::ActionPersistence::kFill, *form_structure,
                               form.fields()[0].global_id(),
                               FillingProduct::kAddress);
    testing::Mock::VerifyAndClearExpectations(&autofill_driver());
  }

  // Simulate the renderer reporting the filled values and run the pending
  // task, if any.
  test_api(*form_structure).UpdateFormData(form);
  check.Call();
  test_api(form_filler()).RunPendingPostFillTasks();
  EXPECT_FALSE(test_api(form_filler()).has_pending_post_fill_tasks());
  EXPECT_THAT(form_structure->field(0), AutofilledWithProfile(profile));
  EXPECT_THAT(form_structure->field(1), AutofilledWithProfile(profile));

  form = UndoAutofill(form, form.fields()[0]);
  EXPECT_EQ(form.fields()[0].value(), u"OldFirst");
  EXPECT_EQ(form.fields()[1].value(), u"");
}

// The following tests run the post-fill bookkeeping in a separate task and
// simulate events that happen after `FillOrPreviewForm` has returned but before
// the bookkeeping task runs.
// TODO(crbug.com/582269824) Merge with FormFillerTest when
// kAutofillPostTaskAfterFillOrPreviewForm is enabled by default.
class FormFillerAsyncTest : public FormFillerTest {
 protected:
  // Runs the tasks posted to the current default `SequencedTaskRunner` so far,
  // including cancelled ones (which are dropped): The runner is FIFO, so they
  // are dispatched before the quit task posted here.
  void FlushPostedTasks() {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  base::test::ScopedFeatureList feature_list_{
      features::kAutofillPostTaskAfterFillOrPreviewForm};
};

// Tests that `FormFiller::Reset()` cancels a pending `OnAfterFillForm()` task.
// In production, `FormFiller::Reset()` is called by
// `BrowserAutofillManager::Reset()` (e.g., on navigation), which also clears
// the form cache. This test calls only `FormFiller::Reset()` and keeps the
// form cached so that the task, if it wrongly ran, would observably update the
// cache and notify the observer. Later fills still run their bookkeeping.
TEST_F(FormFillerAsyncTest, FillOrPreviewForm_AsyncCancelledOnReset) {
  FormData form =
      FormSeen({.fields = {{.role = NAME_FIRST, .value = u"OldFirst"},
                           {.role = NAME_LAST, .value = u""}}});
  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);

  NiceMock<MockAutofillManagerObserver> observer;
  base::ScopedObservation<AutofillManager, AutofillManager::Observer>
      observation(&observer);
  observation.Observe(&autofill_manager());
  EXPECT_CALL(observer, OnFillOrPreviewForm).Times(0);

  AutofillProfile profile = test::GetFullProfile();
  EXPECT_CALL(autofill_driver(), ApplyFormAction).WillOnce(FillInto(form));

  form_filler().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, FillingProduct::kAddress, &profile,
      *form_structure, *form_structure->field(0), AutofillTriggerSource::kPopup,
      /*blocked_fields=*/{}, FillId::Create(), /*forced_fill_values=*/{},
      FormFiller::RefillOptions::NotRefill());

  ASSERT_TRUE(test_api(form_filler()).has_pending_post_fill_tasks());
  form_filler().Reset();
  EXPECT_FALSE(test_api(form_filler()).has_pending_post_fill_tasks());
  FlushPostedTasks();

  EXPECT_EQ(form_structure->field(0)->last_modifier(), std::nullopt);
  EXPECT_EQ(form_structure->field(1)->last_modifier(), std::nullopt);

  EXPECT_CALL(autofill_driver(), ApplyFormAction).Times(0);
  form_filler().UndoAutofill(mojom::ActionPersistence::kFill, *form_structure,
                             form.fields()[0].global_id(),
                             FillingProduct::kAddress);
  testing::Mock::VerifyAndClearExpectations(&autofill_driver());
  testing::Mock::VerifyAndClearExpectations(&observer);

  // `Reset()` does not prevent the bookkeeping of later fills.
  EXPECT_CALL(observer, OnFillOrPreviewForm);
  EXPECT_CALL(autofill_driver(), ApplyFormAction).WillOnce(FillInto(form));
  form_filler().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, FillingProduct::kAddress, &profile,
      *form_structure, *form_structure->field(0), AutofillTriggerSource::kPopup,
      /*blocked_fields=*/{}, FillId::Create(), /*forced_fill_values=*/{},
      FormFiller::RefillOptions::NotRefill());
  test_api(form_filler()).RunPendingPostFillTasks();
  EXPECT_THAT(form_structure->field(0), AutofilledWithProfile(profile));
}

// Tests that destroying the caller-owned payload and mutating the cached form
// (updating post-fill values, removing a filled field, and adding a new field)
// after `FillOrPreviewForm` has returned but before the `OnAfterFillForm` task
// runs completes cleanly and preserves the pre-fill undo history.
TEST_F(FormFillerAsyncTest,
       FillOrPreviewForm_AsyncPayloadDestroyedAndFormMutatedWhilePending) {
  FormData form =
      FormSeen({.fields = {{.role = NAME_FIRST, .value = u"OldFirst"},
                           {.role = NAME_LAST, .value = u""},
                           {.role = EMAIL_ADDRESS, .value = u""}}});
  const FieldGlobalId first_name_id = form.fields()[0].global_id();
  const FieldGlobalId last_name_id = form.fields()[1].global_id();

  NiceMock<MockAutofillManagerObserver> observer;
  base::ScopedObservation<AutofillManager, AutofillManager::Observer>
      observation(&observer);
  observation.Observe(&autofill_manager());
  EXPECT_CALL(observer, OnFillOrPreviewForm(
                            _, form.global_id(), first_name_id,
                            mojom::ActionPersistence::kFill,
                            ElementsAre(first_name_id, last_name_id), _, _));

  EXPECT_CALL(autofill_driver(), ApplyFormAction).WillOnce(FillInto(form));

  const AutofillProfile expected_profile = test::GetFullProfile();
  {
    AutofillProfile stack_local_profile = expected_profile;
    FormStructure* form_structure = GetFormStructure(form);
    ASSERT_TRUE(form_structure);
    form_filler().FillOrPreviewForm(
        mojom::ActionPersistence::kFill, FillingProduct::kAddress,
        &stack_local_profile, *form_structure, *form_structure->field(0),
        AutofillTriggerSource::kPopup, /*blocked_fields=*/{}, FillId::Create(),
        /*forced_fill_values=*/{}, FormFiller::RefillOptions::NotRefill());
  }

  // While the task is still pending (and `stack_local_profile` is out of
  // scope), update the cached `FormStructure` with post-fill values, a removed
  // filled field, and a newly added field.
  test_api(form).Remove(2);
  test_api(form).Append(test::CreateTestFormField("Added", "added", "",
                                                  FormControlType::kInputText));
  test_api(*GetFormStructure(form)).UpdateFormData(form);

  test_api(form_filler()).RunPendingPostFillTasks();

  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);
  ASSERT_EQ(form_structure->field_count(), 3u);
  EXPECT_THAT(form_structure->field(0),
              AutofilledWithProfile(expected_profile));
  EXPECT_THAT(form_structure->field(1),
              AutofilledWithProfile(expected_profile));
  EXPECT_TRUE(form_structure->field(2)->field_log_events().empty());

  form = UndoAutofill(form, form.fields()[0]);
  EXPECT_EQ(form.fields()[0].value(), u"OldFirst");
  EXPECT_EQ(form.fields()[1].value(), u"");
}

// Tests that the bookkeeping returns early without notifying observers if the
// form is removed from the cache before the bookkeeping task runs.
TEST_F(FormFillerAsyncTest, FillOrPreviewForm_AsyncFormRemovedWhilePending) {
  FormData form =
      FormSeen({.fields = {{.role = NAME_FIRST}, {.role = NAME_LAST}}});
  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);

  NiceMock<MockAutofillManagerObserver> observer;
  base::ScopedObservation<AutofillManager, AutofillManager::Observer>
      observation(&observer);
  observation.Observe(&autofill_manager());
  EXPECT_CALL(observer, OnFillOrPreviewForm).Times(0);

  AutofillProfile profile = test::GetFullProfile();
  EXPECT_CALL(autofill_driver(), ApplyFormAction).WillOnce(FillInto(form));
  form_filler().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, FillingProduct::kAddress, &profile,
      *form_structure, *form_structure->field(0), AutofillTriggerSource::kPopup,
      /*blocked_fields=*/{}, FillId::Create(), /*forced_fill_values=*/{},
      FormFiller::RefillOptions::NotRefill());

  FormsRemoved({form.global_id()});
  test_api(form_filler()).RunPendingPostFillTasks();
}

// Tests that if the trigger field is removed from the cached `FormStructure`
// before the bookkeeping task runs, the bookkeeping still logs fill events,
// records undo history and updates the cache for the remaining filled fields,
// but skips the notifications that require the trigger field.
TEST_F(FormFillerAsyncTest,
       FillOrPreviewForm_AsyncTriggerFieldRemovedWhilePending) {
  FormData form =
      FormSeen({.fields = {{.role = NAME_FIRST}, {.role = NAME_LAST}}});
  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);

  NiceMock<MockAutofillManagerObserver> observer;
  base::ScopedObservation<AutofillManager, AutofillManager::Observer>
      observation(&observer);
  observation.Observe(&autofill_manager());
  EXPECT_CALL(observer, OnFillOrPreviewForm).Times(0);

  AutofillProfile profile = test::GetFullProfile();
  EXPECT_CALL(autofill_driver(), ApplyFormAction).WillOnce(FillInto(form));
  form_filler().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, FillingProduct::kAddress, &profile,
      *form_structure, *form_structure->field(0), AutofillTriggerSource::kPopup,
      /*blocked_fields=*/{}, FillId::Create(), /*forced_fill_values=*/{},
      FormFiller::RefillOptions::NotRefill());

  test_api(form).Remove(0);
  test_api(*form_structure).UpdateFormData(form);
  test_api(form_filler()).RunPendingPostFillTasks();

  ASSERT_EQ(form_structure->field_count(), 1u);
  EXPECT_THAT(form_structure->field(0), AutofilledWithProfile(profile));
  EXPECT_TRUE(std::ranges::any_of(
      form_structure->field(0)->field_log_events(), [](const auto& event) {
        return std::holds_alternative<FillFieldLogEvent>(event);
      }));
  EXPECT_TRUE(form_structure->last_filling_timestamp().has_value());

  form = UndoAutofill(form, form.fields()[0]);
  EXPECT_EQ(form.fields()[0].value(), u"");
}

// Tests that if the user edits a field before the bookkeeping task runs, the
// bookkeeping neither overwrites that field's `kUser` modifier and filling
// metadata nor records it in the undo history or logs a fill event for it, so
// a subsequent Undo leaves the user's edit intact.
TEST_F(FormFillerAsyncTest,
       FillOrPreviewForm_AsyncUserEditWhilePendingPreserved) {
  FormData form =
      FormSeen({.fields = {{.role = NAME_FIRST, .value = u"OldFirst"},
                           {.role = NAME_LAST, .value = u""}}});
  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);

  AutofillProfile profile = test::GetFullProfile();
  EXPECT_CALL(autofill_driver(), ApplyFormAction).WillOnce(FillInto(form));
  form_filler().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, FillingProduct::kAddress, &profile,
      *form_structure, *form_structure->field(0), AutofillTriggerSource::kPopup,
      /*blocked_fields=*/{}, FillId::Create(), /*forced_fill_values=*/{},
      FormFiller::RefillOptions::NotRefill());

  test_api(form).field(0).set_value(u"UserEditedFirst");
  test_api(form).field(0).set_is_autofilled_according_to_renderer(false);
  autofill_manager().OnTextFieldValueChanged(
      form, form.fields()[0].global_id(), base::TimeTicks::Now(),
      AutofillManagerTestApi::pass_key());
  test_api(form_filler()).RunPendingPostFillTasks();

  EXPECT_EQ(form_structure->field(0)->last_modifier(), FieldModifier::kUser);
  EXPECT_EQ(form_structure->field(0)->filling_product(), FillingProduct::kNone);
  EXPECT_EQ(form_structure->field(0)->autofill_source_profile_guid(),
            std::nullopt);
  EXPECT_FALSE(std::ranges::any_of(
      form_structure->field(0)->field_log_events(), [](const auto& event) {
        return std::holds_alternative<FillFieldLogEvent>(event);
      }));
  EXPECT_THAT(form_structure->field(1), AutofilledWithProfile(profile));

  // Undoing Autofill on `field(1)` reverts `field(1)` while leaving the user's
  // edit on `field(0)` untouched.
  form = UndoAutofill(form, form.fields()[1]);
  EXPECT_EQ(form.fields()[0].value(), u"UserEditedFirst");
  EXPECT_EQ(form.fields()[1].value(), u"");
  EXPECT_EQ(form_structure->field(0)->last_modifier(), FieldModifier::kUser);
}

// Like `FillOrPreviewForm_AsyncUserEditWhilePendingPreserved`, but the user had
// already edited the field before filling, so the edit before the bookkeeping
// does not change the field's sequence of modifiers.
TEST_F(FormFillerAsyncTest,
       FillOrPreviewForm_AsyncRepeatedUserEditWhilePendingPreserved) {
  FormData form = FormSeen({.fields = {{.role = NAME_FIRST, .value = u""},
                                       {.role = NAME_LAST, .value = u""}}});
  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);

  // The user types into `field(0)` and then fills the form from it.
  test_api(form).field(0).set_value(u"Typed");
  autofill_manager().OnTextFieldValueChanged(
      form, form.fields()[0].global_id(), base::TimeTicks::Now(),
      AutofillManagerTestApi::pass_key());
  ASSERT_EQ(form_structure->field(0)->last_modifier(), FieldModifier::kUser);

  AutofillProfile profile = test::GetFullProfile();
  EXPECT_CALL(autofill_driver(), ApplyFormAction).WillOnce(FillInto(form));
  form_filler().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, FillingProduct::kAddress, &profile,
      *form_structure, *form_structure->field(0), AutofillTriggerSource::kPopup,
      /*blocked_fields=*/{}, FillId::Create(), /*forced_fill_values=*/{},
      FormFiller::RefillOptions::NotRefill());

  test_api(form).field(0).set_value(u"TypedAgain");
  test_api(form).field(0).set_is_autofilled_according_to_renderer(false);
  autofill_manager().OnTextFieldValueChanged(
      form, form.fields()[0].global_id(), base::TimeTicks::Now(),
      AutofillManagerTestApi::pass_key());
  test_api(form_filler()).RunPendingPostFillTasks();

  EXPECT_EQ(form_structure->field(0)->last_modifier(), FieldModifier::kUser);
  EXPECT_EQ(form_structure->field(0)->filling_product(), FillingProduct::kNone);
  EXPECT_THAT(form_structure->field(1), AutofilledWithProfile(profile));

  form = UndoAutofill(form, form.fields()[1]);
  EXPECT_EQ(form.fields()[0].value(), u"TypedAgain");
  EXPECT_EQ(form.fields()[1].value(), u"");
}

// Tests that a refill that is triggered before the initial fill's bookkeeping
// task has run is deferred until the bookkeeping is complete. (Once the
// bookkeeping is complete, `TestFormFiller::StartRefillTimer()` triggers the
// refill synchronously instead of after `kWaitTimeForDynamicForms`.)
TEST_F(FormFillerAsyncTest,
       FillOrPreviewForm_AsyncRefillDeferredUntilInitialFillDone) {
  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FIRST, .autocomplete_attribute = "given-name"},
                  {.role = ADDRESS_HOME_LINE1,
                   .autocomplete_attribute = "address-line1"},
                  {.role = ADDRESS_HOME_LINE2,
                   .autocomplete_attribute = "address-line2"}}});
  FormFieldData line2_field = form.fields()[2];
  test_api(form).Resize(2);
  FormsSeen({form});
  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);

  AutofillProfile profile = test::GetFullProfile();
  EXPECT_CALL(autofill_driver(), ApplyFormAction)
      .WillOnce(FillInto(form))
      .WillOnce(FillInto(form, [&] {
        // The refill only runs after the initial fill's bookkeeping.
        EXPECT_THAT(form_structure->field(0), AutofilledWithProfile(profile));
      }));
  form_filler().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, FillingProduct::kAddress, &profile,
      *form_structure, *form_structure->field(0), AutofillTriggerSource::kPopup,
      /*blocked_fields=*/{}, FillId::Create(), /*forced_fill_values=*/{},
      FormFiller::RefillOptions::NotRefill());

  // An address field is added dynamically, which triggers a refill.
  test_api(form).Append(line2_field);
  test_api(*form_structure).UpdateFormData(form);
  test_api(*form_structure).AssignSections();
  form_structure->field(2)->SetTypeTo(AutofillType(ADDRESS_HOME_LINE2),
                                      AutofillPredictionSource::kHeuristics);
  form_filler().MaybeScheduleAutomaticRefill(*form_structure,
                                             RefillTriggerReason::kFormChanged,
                                             AutofillTriggerSource::kFormsSeen);
  // The refill is deferred.
  EXPECT_FALSE(form.fields()[2].is_autofilled_according_to_renderer());

  test_api(form_filler()).RunPendingPostFillTasks();

  EXPECT_THAT(form.fields()[2],
              AutofilledWith(profile.GetInfo(ADDRESS_HOME_LINE2, kAppLocale)));
  EXPECT_THAT(form_structure->field(2), AutofilledWithProfile(profile));
}

// Tests that if several refills are triggered before the initial fill's
// bookkeeping task has run, only the latest trigger is deferred and replayed
// once the bookkeeping is complete: the latest trigger wins. The latest trigger
// is a programmatic refill, which is deferred like automatic refills.
TEST_F(FormFillerAsyncTest,
       FillOrPreviewForm_AsyncLatestDeferredRefillTriggerWins) {
  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FIRST, .autocomplete_attribute = "given-name"},
                  {.role = ADDRESS_HOME_LINE1,
                   .autocomplete_attribute = "address-line1"},
                  {.role = ADDRESS_HOME_LINE2,
                   .autocomplete_attribute = "address-line2"}}});
  FormFieldData line2_field = form.fields()[2];
  test_api(form).Resize(2);
  FormsSeen({form});
  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);

  AutofillProfile profile = test::GetFullProfile();
  const FillId fill_id = FillId::Create();
  MockFunction<void()> check;
  {
    InSequence s;
    EXPECT_CALL(autofill_driver(), ApplyFormAction).WillOnce(FillInto(form));
    EXPECT_CALL(check, Call);
    EXPECT_CALL(autofill_driver(), ApplyFormAction).WillOnce(FillInto(form));
  }
  base::HistogramTester histogram_tester;
  form_filler().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, FillingProduct::kAddress, &profile,
      *form_structure, *form_structure->field(0), AutofillTriggerSource::kPopup,
      /*blocked_fields=*/{}, fill_id, /*forced_fill_values=*/{},
      FormFiller::RefillOptions::NotRefill());

  // An address field is added dynamically, which triggers a refill. The
  // refill is deferred because the initial fill's bookkeeping is pending.
  test_api(form).Append(line2_field);
  test_api(*form_structure).UpdateFormData(form);
  test_api(*form_structure).AssignSections();
  form_structure->field(2)->SetTypeTo(AutofillType(ADDRESS_HOME_LINE2),
                                      AutofillPredictionSource::kHeuristics);
  form_filler().MaybeScheduleAutomaticRefill(*form_structure,
                                             RefillTriggerReason::kFormChanged,
                                             AutofillTriggerSource::kFormsSeen);

  // A newer programmatic trigger replaces the deferred trigger.
  form_filler().MaybeScheduleProgrammaticRefill(fill_id);
  check.Call();

  // Completing the bookkeeping replays only the newer trigger.
  // (`TestFormFiller::StartRefillTimer()` triggers the refill synchronously.)
  test_api(form_filler()).RunPendingPostFillTasks();
  histogram_tester.ExpectUniqueSample("Autofill.RefillTriggerReason",
                                      RefillTriggerReason::kProgrammaticRefill,
                                      1);
  EXPECT_THAT(form_structure->field(2), AutofilledWithProfile(profile));
}

// Tests that refills are not blocked forever if the form is removed from the
// cache before the initial fill's bookkeeping task has run.
// (`TestFormFiller::StartRefillTimer()` triggers the refill synchronously.)
TEST_F(FormFillerAsyncTest,
       FillOrPreviewForm_AsyncRefillNotBlockedAfterFormRemovedWhilePending) {
  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FIRST, .autocomplete_attribute = "given-name"},
                  {.role = ADDRESS_HOME_LINE1,
                   .autocomplete_attribute = "address-line1"},
                  {.role = ADDRESS_HOME_LINE2,
                   .autocomplete_attribute = "address-line2"}}});
  FormFieldData line2_field = form.fields()[2];
  test_api(form).Resize(2);
  FormsSeen({form});
  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);

  AutofillProfile profile = test::GetFullProfile();
  EXPECT_CALL(autofill_driver(), ApplyFormAction).WillOnce(FillInto(form));
  form_filler().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, FillingProduct::kAddress, &profile,
      *form_structure, *form_structure->field(0), AutofillTriggerSource::kPopup,
      /*blocked_fields=*/{}, FillId::Create(), /*forced_fill_values=*/{},
      FormFiller::RefillOptions::NotRefill());
  FormsRemoved({form.global_id()});
  test_api(form_filler()).RunPendingPostFillTasks();
  testing::Mock::VerifyAndClearExpectations(&autofill_driver());
  ASSERT_FALSE(GetFormStructure(form));

  // The form reappears with an additional field, which triggers a refill.
  test_api(form).Append(line2_field);
  EXPECT_CALL(autofill_driver(), ApplyFormAction).WillOnce(FillInto(form));
  FormsSeen({form});
  test_api(form_filler()).RunPendingPostFillTasks();

  EXPECT_THAT(form.fields()[2],
              AutofilledWith(profile.GetInfo(ADDRESS_HOME_LINE2, kAppLocale)));
}

// Tests that if `SuppressAutomaticRefills` is called before the initial fill's
// bookkeeping task has run, `SuppressAutomaticRefills` drops a deferred refill
// and remains respected afterwards. (`TestFormFiller::StartRefillTimer()`
// triggers the refill synchronously.)
TEST_F(FormFillerAsyncTest,
       FillOrPreviewForm_AsyncSuppressAutomaticRefillsWhilePending) {
  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FIRST, .autocomplete_attribute = "given-name"},
                  {.role = ADDRESS_HOME_LINE1,
                   .autocomplete_attribute = "address-line1"},
                  {.role = ADDRESS_HOME_LINE2,
                   .autocomplete_attribute = "address-line2"},
                  {.role = ADDRESS_HOME_CITY,
                   .autocomplete_attribute = "address-level2"}}});
  FormFieldData city_field = form.fields()[3];
  FormFieldData line2_field = form.fields()[2];
  test_api(form).Resize(2);
  FormsSeen({form});
  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);

  // `WillOnce()` makes sure that there is no refill.
  AutofillProfile profile = test::GetFullProfile();
  const FillId fill_id = FillId::Create();
  EXPECT_CALL(autofill_driver(), ApplyFormAction).WillOnce(FillInto(form));
  form_filler().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, FillingProduct::kAddress, &profile,
      *form_structure, *form_structure->field(0), AutofillTriggerSource::kPopup,
      /*blocked_fields=*/{}, fill_id, /*forced_fill_values=*/{},
      FormFiller::RefillOptions::NotRefill());

  // An address field is added dynamically, which triggers a refill, but then
  // automatic refills are suppressed.
  test_api(form).Append(line2_field);
  test_api(*form_structure).UpdateFormData(form);
  test_api(*form_structure).AssignSections();
  form_structure->field(2)->SetTypeTo(AutofillType(ADDRESS_HOME_LINE2),
                                      AutofillPredictionSource::kHeuristics);
  form_filler().MaybeScheduleAutomaticRefill(*form_structure,
                                             RefillTriggerReason::kFormChanged,
                                             AutofillTriggerSource::kFormsSeen);
  form_filler().SuppressAutomaticRefills(fill_id);
  test_api(form_filler()).RunPendingPostFillTasks();
  testing::Mock::VerifyAndClearExpectations(&autofill_driver());

  test_api(form).Append(city_field);
  EXPECT_CALL(autofill_driver(), ApplyFormAction).Times(0);
  FormsSeen({form});
  FlushPostedTasks();
}

// Tests that the bookkeeping of an asynchronous refill uses its own copy of the
// payload, which is owned by the `RefillContext` that `TriggerRefill()`
// destroys right after starting the refill.
// (`TestFormFiller::StartRefillTimer()` triggers the refill synchronously.)
TEST_F(FormFillerAsyncTest,
       FillOrPreviewForm_AsyncRefillOutlivesRefillContext) {
  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FIRST, .autocomplete_attribute = "given-name"},
                  {.role = ADDRESS_HOME_LINE1,
                   .autocomplete_attribute = "address-line1"},
                  {.role = ADDRESS_HOME_LINE2,
                   .autocomplete_attribute = "address-line2"}}});
  FormFieldData line2_field = form.fields()[2];
  test_api(form).Resize(2);
  FormsSeen({form});
  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);

  AutofillProfile profile = test::GetFullProfile();
  EXPECT_CALL(autofill_driver(), ApplyFormAction).WillOnce(FillInto(form));
  form_filler().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, FillingProduct::kAddress, &profile,
      *form_structure, *form_structure->field(0), AutofillTriggerSource::kPopup,
      /*blocked_fields=*/{}, FillId::Create(), /*forced_fill_values=*/{},
      FormFiller::RefillOptions::NotRefill());
  test_api(form_filler()).RunPendingPostFillTasks();
  testing::Mock::VerifyAndClearExpectations(&autofill_driver());

  // A dynamic form change triggers a refill.
  test_api(form).Append(line2_field);
  EXPECT_CALL(autofill_driver(), ApplyFormAction).WillOnce(FillInto(form));
  FormsSeen({form});
  test_api(form_filler()).RunPendingPostFillTasks();

  form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);
  ASSERT_EQ(form_structure->field_count(), 3u);
  EXPECT_THAT(form_structure->field(2), AutofilledWithProfile(profile));
}

// Documents a known limitation (see the TODO in
// `FormFiller::FillOrPreviewForm()`): If the form is filled again before the
// post-fill bookkeeping of the first fill has run, the second fill does not
// skip the fields filled by the first fill as `kAlreadyAutofilled`, and its
// bookkeeping skips the fields that the first fill's bookkeeping updated. As a
// result, the cache attributes the fields to the first profile, although the
// renderer shows the second profile's values.
TEST_F(FormFillerAsyncTest, FillOrPreviewForm_AsyncSecondFillWhilePending) {
  FormData form =
      FormSeen({.fields = {{.role = NAME_FIRST}, {.role = NAME_LAST}}});
  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);

  AutofillProfile profile1 = test::GetFullProfile();
  AutofillProfile profile2 = test::GetFullProfile2();
  ASSERT_NE(profile1.GetInfo(NAME_FIRST, kAppLocale),
            profile2.GetInfo(NAME_FIRST, kAppLocale));
  EXPECT_CALL(autofill_driver(), ApplyFormAction)
      .WillOnce(FillInto(form))
      .WillOnce(FillInto(form));
  form_filler().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, FillingProduct::kAddress, &profile1,
      *form_structure, *form_structure->field(0), AutofillTriggerSource::kPopup,
      /*blocked_fields=*/{}, FillId::Create(), /*forced_fill_values=*/{},
      FormFiller::RefillOptions::NotRefill());
  form_filler().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, FillingProduct::kAddress, &profile2,
      *form_structure, *form_structure->field(1), AutofillTriggerSource::kPopup,
      /*blocked_fields=*/{}, FillId::Create(), /*forced_fill_values=*/{},
      FormFiller::RefillOptions::NotRefill());

  // The second fill overwrote `field(0)` (instead of skipping it as
  // `kAlreadyAutofilled`).
  EXPECT_THAT(form.fields()[0],
              AutofilledWith(profile2.GetInfo(NAME_FIRST, kAppLocale)));
  EXPECT_THAT(form.fields()[1],
              AutofilledWith(profile2.GetInfo(NAME_LAST, kAppLocale)));

  test_api(*form_structure).UpdateFormData(form);
  test_api(form_filler()).RunPendingPostFillTasks();

  EXPECT_THAT(form_structure->field(0), AutofilledWithProfile(profile1));
  EXPECT_THAT(form_structure->field(1), AutofilledWithProfile(profile1));
}

// Tests that `Observer::OnFillOrPreviewFormSentToRenderer()` is notified
// synchronously when the values of a fill are sent to the renderer, before the
// post-fill bookkeeping, including `Observer::OnFillOrPreviewForm()`, which
// may run in a separate task.
TEST_P(FormFillerPostFillTest,
       FillOrPreviewForm_SentToRendererNotifiedBeforeFillBookkeeping) {
  FormData form =
      FormSeen({.fields = {{.role = NAME_FIRST}, {.role = NAME_LAST}}});
  const FieldGlobalId first_name_id = form.fields()[0].global_id();
  const FieldGlobalId last_name_id = form.fields()[1].global_id();
  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);

  NiceMock<MockAutofillManagerObserver> observer;
  base::ScopedObservation<AutofillManager, AutofillManager::Observer>
      observation(&observer);
  observation.Observe(&autofill_manager());
  MockFunction<void()> check;
  {
    InSequence s;
    EXPECT_CALL(
        observer,
        OnFillOrPreviewFormSentToRenderer(
            _, form.global_id(), first_name_id, mojom::ActionPersistence::kFill,
            UnorderedElementsAre(first_name_id, last_name_id)));
    if (!IsPostFillAsync()) {
      EXPECT_CALL(observer, OnFillOrPreviewForm);
    }
    EXPECT_CALL(check, Call);
    if (IsPostFillAsync()) {
      EXPECT_CALL(observer, OnFillOrPreviewForm);
    }
  }

  AutofillProfile profile = test::GetFullProfile();
  EXPECT_CALL(autofill_driver(), ApplyFormAction).WillOnce(FillInto(form));
  form_filler().FillOrPreviewForm(
      mojom::ActionPersistence::kFill, FillingProduct::kAddress, &profile,
      *form_structure, *form_structure->field(0), AutofillTriggerSource::kPopup,
      /*blocked_fields=*/{}, FillId::Create(), /*forced_fill_values=*/{},
      FormFiller::RefillOptions::NotRefill());
  check.Call();
  test_api(form_filler()).RunPendingPostFillTasks();
}

// Tests that for previews, `Observer::OnFillOrPreviewFormSentToRenderer()` is
// notified synchronously, right before `Observer::OnFillOrPreviewForm()`.
TEST_P(FormFillerPostFillTest,
       FillOrPreviewForm_SentToRendererNotifiedBeforePreviewNotification) {
  FormData form =
      FormSeen({.fields = {{.role = NAME_FIRST}, {.role = NAME_LAST}}});
  const FieldGlobalId first_name_id = form.fields()[0].global_id();
  const FieldGlobalId last_name_id = form.fields()[1].global_id();
  FormStructure* form_structure = GetFormStructure(form);
  ASSERT_TRUE(form_structure);

  NiceMock<MockAutofillManagerObserver> observer;
  base::ScopedObservation<AutofillManager, AutofillManager::Observer>
      observation(&observer);
  observation.Observe(&autofill_manager());
  MockFunction<void()> check;
  {
    InSequence s;
    EXPECT_CALL(observer,
                OnFillOrPreviewFormSentToRenderer(
                    _, form.global_id(), first_name_id,
                    mojom::ActionPersistence::kPreview,
                    UnorderedElementsAre(first_name_id, last_name_id)));
    EXPECT_CALL(observer, OnFillOrPreviewForm(
                              _, form.global_id(), first_name_id,
                              mojom::ActionPersistence::kPreview, _, _, _));
    EXPECT_CALL(check, Call);
  }

  AutofillProfile profile = test::GetFullProfile();
  EXPECT_CALL(autofill_driver(), ApplyFormAction).WillOnce(FillInto(form));
  form_filler().FillOrPreviewForm(
      mojom::ActionPersistence::kPreview, FillingProduct::kAddress, &profile,
      *form_structure, *form_structure->field(0), AutofillTriggerSource::kPopup,
      /*blocked_fields=*/{}, FillId::Create(), /*forced_fill_values=*/{},
      FormFiller::RefillOptions::NotRefill());
  check.Call();
}

}  // namespace autofill
