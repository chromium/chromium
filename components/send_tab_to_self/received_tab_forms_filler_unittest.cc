// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/received_tab_forms_filler.h"

#include <memory>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_driver.h"
#include "components/autofill/core/browser/foundations/autofill_driver_test_api.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/autofill_manager_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_autofill_manager_waiter.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/send_tab_to_self/page_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace send_tab_to_self {

namespace {

using autofill::FormData;
using autofill::TestBrowserAutofillManager;
using base::test::TestFuture;
using ::testing::_;
using ::testing::Eq;
using ::testing::Mock;
using ::testing::Test;
using ::testing::TestParamInfo;
using ::testing::Values;
using ::testing::WithParamInterface;

PageContext::FormField MakeFormField(
    std::u16string id_attribute,
    std::u16string name_attribute,
    autofill::FormControlType form_control_type,
    std::u16string value,
    std::optional<PageContext::FormFieldAutofillSignature> sig = std::nullopt) {
  PageContext::FormField field;
  field.id_attribute = std::move(id_attribute);
  field.name_attribute = std::move(name_attribute);
  field.form_control_type = form_control_type;
  field.value = std::move(value);
  if (sig) {
    field.autofill_signature = *sig;
  }
  return field;
}

PageContext::FormFieldAutofillSignature GetSignature(const FormData& form_data,
                                                     size_t field_index) {
  autofill::FormStructure form(form_data);
  return {form.form_signature(), form.field(field_index)->GetFieldSignature()};
}

class MockAutofillDriver : public autofill::TestAutofillDriver {
 public:
  explicit MockAutofillDriver(autofill::TestAutofillClient* client)
      : autofill::TestAutofillDriver(client) {}
  MOCK_METHOD(void,
              ApplyFieldAction,
              (autofill::mojom::FieldActionType action_type,
               autofill::mojom::ActionPersistence action_persistence,
               const autofill::FieldGlobalId& field_id,
               const std::u16string& value),
              (override));
};

class ReceivedTabFormsFillerTest
    : public Test,
      public autofill::WithTestAutofillClientDriverManager<
          autofill::TestAutofillClient,
          MockAutofillDriver,
          TestBrowserAutofillManager> {
 public:
  ReceivedTabFormsFillerTest() = default;

  void SetUp() override {
    // Initialize the test Autofill client to manage autofill state.
    InitAutofillClient();
    // Create the test driver associated with the client.
    CreateAutofillDriver();
    // Activate the driver to allow it to receive and process page actions.
    ActivateAutofillDriver(autofill_driver());
  }

  void TearDown() override { DestroyAutofillClient(); }

 protected:
  // Helper to start the filler, simulate seeing a form, and wait for
  // completion.
  [[nodiscard]] bool TryStartFillerAndSeeForm(
      const url::Origin& origin,
      const PageContext::FormFieldInfo& field_info,
      const FormData& form) {
    TestFuture<void> future;
    ReceivedTabFormsFiller::Start(autofill_client(), origin, field_info,
                                  future.GetCallback());
    autofill_manager().OnFormsSeen(
        {form}, {}, autofill::AutofillManagerTestApi::pass_key());
    return future.Wait();
  }

  // Helper to assert a unique match outcome sample.
  void ExpectUniqueMatchOutcome(FormFieldMatchOutcome outcome, int count) {
    histogram_tester_.ExpectUniqueSample(
        "Sharing.SendTabToSelf.ReceivedTabFormFieldMatchOutcome", outcome,
        count);
  }

  // Helper to assert no match outcome was recorded.
  void ExpectNoMatchOutcome() {
    histogram_tester_.ExpectTotalCount(
        "Sharing.SendTabToSelf.ReceivedTabFormFieldMatchOutcome", 0);
  }

  // Helper to reduce FormFieldInfo boilerplate.
  PageContext::FormFieldInfo CreateFormFieldInfo(
      std::vector<PageContext::FormField> fields) {
    PageContext::FormFieldInfo info;
    info.fields = std::move(fields);
    return info;
  }

  // Helper to create a FormData with multiple fields, reducing boilerplate.
  FormData CreateTestForm(
      std::vector<autofill::test::FieldDescription> fields,
      std::optional<autofill::FormRendererId> form_renderer_id = std::nullopt,
      std::optional<autofill::LocalFrameToken> host_frame = std::nullopt) {
    uint32_t renderer_id = 0;
    for (autofill::test::FieldDescription& field : fields) {
      if (!field.renderer_id) {
        field.renderer_id = autofill::FieldRendererId(++renderer_id);
      }
      if (!field.label) {
        field.label = u"label";
      }
      if (!field.origin) {
        field.origin = origin_;
      }
    }

    autofill::test::FormDescription form_desc{.url = origin_.Serialize()};
    form_desc.fields = std::move(fields);
    if (form_renderer_id) {
      form_desc.renderer_id = *form_renderer_id;
    }
    if (host_frame) {
      form_desc.host_frame = *host_frame;
    }

    return autofill::test::GetFormData(form_desc);
  }

  // Helper to create a FormData with a single field, reducing boilerplate.
  FormData CreateSingleFieldForm(
      autofill::test::FieldDescription field,
      std::optional<autofill::FormRendererId> form_renderer_id = std::nullopt,
      std::optional<autofill::LocalFrameToken> host_frame = std::nullopt) {
    return CreateTestForm({std::move(field)}, form_renderer_id, host_frame);
  }

  const url::Origin origin_ = url::Origin::Create(GURL("https://example.com"));
  const url::Origin other_origin_ =
      url::Origin::Create(GURL("https://other.com"));

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;

 private:
  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_;
};

struct FillTriggerTestCase {
  std::string test_name;
  std::u16string receiver_field_value;
  autofill::FieldPropertiesMask receiver_field_properties_mask;
  bool expect_fill;
};

class ReceivedTabFormsFillerFillTriggerTest
    : public ReceivedTabFormsFillerTest,
      public WithParamInterface<FillTriggerTestCase> {};

TEST_P(ReceivedTabFormsFillerFillTriggerTest, ShouldConditionallyFill) {
  const FillTriggerTestCase& test_case = GetParam();
  PageContext::FormFieldInfo form_field_info = CreateFormFieldInfo(
      {MakeFormField(u"id1", u"name1", autofill::FormControlType::kInputText,
                     u"shared_value")});

  const FormData form = CreateSingleFieldForm(
      {.name_attribute = u"name1",
       .id_attribute = u"id1",
       .value = test_case.receiver_field_value,
       .properties_mask = test_case.receiver_field_properties_mask});
  const autofill::FieldGlobalId field_id = form.fields()[0].global_id();

  if (test_case.expect_fill) {
    EXPECT_CALL(autofill_driver(),
                ApplyFieldAction(autofill::mojom::FieldActionType::kReplaceAll,
                                 autofill::mojom::ActionPersistence::kFill,
                                 Eq(field_id), Eq(u"shared_value")));
  } else {
    EXPECT_CALL(autofill_driver(), ApplyFieldAction).Times(0);
  }

  EXPECT_TRUE(TryStartFillerAndSeeForm(origin_, form_field_info, form));

  ExpectUniqueMatchOutcome(FormFieldMatchOutcome::kMatchedByIdNameAndType, 1);
}

INSTANTIATE_TEST_SUITE_P(
    ReceivedTabFormsFillerFillTriggerTests,
    ReceivedTabFormsFillerFillTriggerTest,
    Values(
        FillTriggerTestCase{
            .test_name = "NormalFieldShouldFill",
            .receiver_field_value = u"",
            .receiver_field_properties_mask = 0,
            .expect_fill = true,
        },
        FillTriggerTestCase{
            .test_name = "UserEditedNotEmptyFieldShouldNotFill",
            .receiver_field_value = u"user_value",
            .receiver_field_properties_mask = autofill::kUserTyped,
            .expect_fill = false,
        },
        FillTriggerTestCase{
            .test_name = "UserEditedButEmptyFieldShouldFill",
            .receiver_field_value = u"",
            .receiver_field_properties_mask = autofill::kUserTyped,
            .expect_fill = true,
        }),
    [](const TestParamInfo<ReceivedTabFormsFillerFillTriggerTest::ParamType>&
           info) { return info.param.test_name; });

TEST_F(ReceivedTabFormsFillerTest, ShouldNotFillUserClearedPrefilledField) {
  PageContext::FormFieldInfo form_field_info = CreateFormFieldInfo(
      {MakeFormField(u"id1", u"name1", autofill::FormControlType::kInputText,
                     u"shared_value")});

  const autofill::LocalFrameToken frame_token =
      autofill_driver().GetFrameToken();
  const autofill::FormRendererId form_renderer_id = autofill::FormRendererId(1);

  // 1. Simulate a form that was previously seen with a pre-filled value.
  const FormData initial_form =
      CreateSingleFieldForm({.name_attribute = u"name1",
                             .id_attribute = u"id1",
                             .value = u"prefilled_value"},
                            form_renderer_id, frame_token);
  autofill_manager().OnFormsSeen({initial_form}, {},
                                 autofill::AutofillManagerTestApi::pass_key());

  // 2. Simulate user clearing the field (value is empty, properties_mask has
  // autofill::kUserTyped).
  const FormData form =
      CreateSingleFieldForm({.name_attribute = u"name1",
                             .id_attribute = u"id1",
                             .properties_mask = autofill::kUserTyped},
                            form_renderer_id, frame_token);
  autofill_manager().OnFormsSeen({form}, {},
                                 autofill::AutofillManagerTestApi::pass_key());

  // Expect ApplyFieldAction to NOT be called because the field is
  // user-cleared.
  EXPECT_CALL(autofill_driver(), ApplyFieldAction).Times(0);

  // 3. Now start the filler.
  TestFuture<void> future;
  ReceivedTabFormsFiller::Start(autofill_client(), origin_, form_field_info,
                                future.GetCallback());

  EXPECT_TRUE(future.Wait());

  ExpectUniqueMatchOutcome(FormFieldMatchOutcome::kMatchedByIdNameAndType, 1);
}

TEST_F(ReceivedTabFormsFillerTest, ShouldNotFillIncomingSensitiveField) {
  PageContext::FormFieldInfo form_field_info =
      CreateFormFieldInfo({MakeFormField(
          u"id1", u"name1", autofill::FormControlType::kInputPassword,
          u"shared_value")});

  const FormData form = CreateSingleFieldForm(
      {.name_attribute = u"name1",
       .id_attribute = u"id1",
       .form_control_type = autofill::FormControlType::kInputPassword});

  EXPECT_CALL(autofill_driver(), ApplyFieldAction).Times(0);

  EXPECT_TRUE(TryStartFillerAndSeeForm(origin_, form_field_info, form));

  ExpectNoMatchOutcome();
}

// Tests that fallback signature matching is skipped if the control types
// of the sender and receiver fields differ.
TEST_F(ReceivedTabFormsFillerTest,
       ShouldNotMatchSignatureFallbackWithDifferentControlTypes) {
  // Create a sender form (control type "text").
  const FormData form_sender =
      CreateSingleFieldForm({.name_attribute = u"name_123"});
  PageContext::FormFieldInfo form_field_info;
  form_field_info.fields.push_back(
      MakeFormField(u"id1", u"name_123", autofill::FormControlType::kInputText,
                    u"shared_value", GetSignature(form_sender, 0)));

  // Create a receiver form with same signature but different control type
  // ("password").
  const FormData form_receiver = CreateSingleFieldForm(
      {.name_attribute = u"name_124",
       .id_attribute = u"id2",
       .form_control_type = autofill::FormControlType::kInputPassword});

  EXPECT_CALL(autofill_driver(), ApplyFieldAction).Times(0);

  base::HistogramTester histogram_tester;
  TestFuture<void> future;
  ReceivedTabFormsFiller::Start(autofill_client(), origin_, form_field_info,
                                future.GetCallback());

  autofill_manager().OnFormsSeen({form_receiver}, {},
                                 autofill::AutofillManagerTestApi::pass_key());

  EXPECT_TRUE(future.Wait());

  histogram_tester.ExpectUniqueSample(
      "Sharing.SendTabToSelf.ReceivedTabFormFieldMatchOutcome",
      FormFieldMatchOutcome::kNoMatch, 1);
}

// Tests that fallback semantic matching is skipped if the control types
// of the sender and receiver fields differ.
TEST_F(ReceivedTabFormsFillerTest,
       ShouldNotMatchSemanticFallbackWithDifferentControlTypes) {
  // Sender field is "text" with USERNAME type.
  PageContext::FormField pending_field = MakeFormField(
      u"id1", u"name1", autofill::FormControlType::kInputText, u"shared_value");
  pending_field.autofill_types = {
      sync_pb::FormField_AutofillFieldType_USERNAME};
  PageContext::FormFieldInfo form_field_info =
      CreateFormFieldInfo({pending_field});

  // Receiver field is "password" with USERNAME type.
  const FormData form_receiver = CreateSingleFieldForm(
      {.name_attribute = u"name_diff",
       .id_attribute = u"id_diff",
       .form_control_type = autofill::FormControlType::kInputPassword});

  // The *autofill* type of the local field is USERNAME. Even though it matches
  // the pending field's type, they should not match because the control types
  // differ.
  auto form_structure =
      std::make_unique<autofill::FormStructure>(form_receiver);
  form_structure->field(0)->SetTypeTo(
      autofill::AutofillType(autofill::FieldType::USERNAME), std::nullopt);
  autofill::test_api(autofill_manager())
      .AddSeenFormStructure(std::move(form_structure));

  EXPECT_CALL(autofill_driver(), ApplyFieldAction).Times(0);

  TestFuture<void> future;
  ReceivedTabFormsFiller::Start(autofill_client(), origin_, form_field_info,
                                future.GetCallback());

  EXPECT_TRUE(future.Wait());

  // Since the control types differ, it should not have been filled.
  ExpectUniqueMatchOutcome(FormFieldMatchOutcome::kNoMatch, 1);
}
// Tests that fallback signature matching works when names/IDs are dynamic
// but the signature is unique.
TEST_F(ReceivedTabFormsFillerTest, ShouldFillFieldsByUniqueSignatureFallback) {
  // Create a sender form to generate a signature for the pending field.
  const FormData form_sender =
      CreateSingleFieldForm({.name_attribute = u"name_123"});
  PageContext::FormFieldInfo form_field_info = CreateFormFieldInfo(
      {MakeFormField(u"id1", u"name_123", autofill::FormControlType::kInputText,
                     u"shared_value", GetSignature(form_sender, 0))});

  // Create a receiver form with a different name/ID but same signature.
  const FormData form_receiver = CreateSingleFieldForm(
      {.name_attribute = u"name_124", .id_attribute = u"id2"});

  const autofill::FieldGlobalId field_id =
      form_receiver.fields()[0].global_id();

  EXPECT_CALL(autofill_driver(),
              ApplyFieldAction(autofill::mojom::FieldActionType::kReplaceAll,
                               autofill::mojom::ActionPersistence::kFill,
                               Eq(field_id), Eq(u"shared_value")));

  EXPECT_TRUE(
      TryStartFillerAndSeeForm(origin_, form_field_info, form_receiver));

  ExpectUniqueMatchOutcome(FormFieldMatchOutcome::kMatchedBySignature, 1);
}

// Tests that fallback matching is skipped if the receiver form has multiple
// fields with the same signature.
TEST_F(ReceivedTabFormsFillerTest,
       ShouldNotFillFieldsByNonUniqueReceiverSignatureFallback) {
  // Create a sender form to generate a signature for the pending field.
  const FormData form_sender =
      CreateSingleFieldForm({.name_attribute = u"name_123"});
  PageContext::FormFieldInfo form_field_info = CreateFormFieldInfo(
      {MakeFormField(u"id1", u"name_123", autofill::FormControlType::kInputText,
                     u"shared_value", GetSignature(form_sender, 0))});

  // Create a receiver form with TWO fields that have the SAME signature.
  // Using the same name ensures they generate the same signature in tests.
  const FormData form_receiver =
      CreateTestForm({{.name_attribute = u"name_123", .id_attribute = u"id2"},
                      {.name_attribute = u"name_123", .id_attribute = u"id3"}});

  EXPECT_CALL(autofill_driver(), ApplyFieldAction).Times(0);

  EXPECT_TRUE(
      TryStartFillerAndSeeForm(origin_, form_field_info, form_receiver));

  ExpectUniqueMatchOutcome(FormFieldMatchOutcome::kNoMatch, 1);
}

// Tests that fallback signature matching works and ignores cross-origin fields
// when determining signature uniqueness in the receiver form.
TEST_F(ReceivedTabFormsFillerTest,
       ShouldFillFieldsByUniqueSignatureFallbackIgnoringCrossOriginFields) {
  // Create a sender form to generate a signature for the pending field.
  // The sender form mirrors the receiver form's structure (one same-origin
  // and one cross-origin field) to ensure the computed FormSignature matches.
  const FormData form_sender = CreateTestForm(
      {{.name_attribute = u"name_123", .origin = origin_},
       {.name_attribute = u"name_123", .origin = other_origin_}});
  PageContext::FormFieldInfo form_field_info = CreateFormFieldInfo(
      {MakeFormField(u"id1", u"name_123", autofill::FormControlType::kInputText,
                     u"shared_value", GetSignature(form_sender, 0))});

  // Create a receiver form with TWO fields that have the SAME signature.
  // But one is same-origin (origin_) and the other is cross-origin
  // (other_origin_).
  const FormData form_receiver = CreateTestForm({{.name_attribute = u"name_123",
                                                  .id_attribute = u"id2",
                                                  .origin = origin_},
                                                 {.name_attribute = u"name_123",
                                                  .id_attribute = u"id3",
                                                  .origin = other_origin_}});

  const autofill::FieldGlobalId field_id =
      form_receiver.fields()[0].global_id();

  // Since the signature is unique among same-origin fields, it should be
  // filled.
  EXPECT_CALL(autofill_driver(),
              ApplyFieldAction(autofill::mojom::FieldActionType::kReplaceAll,
                               autofill::mojom::ActionPersistence::kFill,
                               Eq(field_id), Eq(u"shared_value")));

  EXPECT_TRUE(
      TryStartFillerAndSeeForm(origin_, form_field_info, form_receiver));

  ExpectUniqueMatchOutcome(FormFieldMatchOutcome::kMatchedBySignature, 1);
}

// Tests that fallback matching is skipped if there are multiple pending fields
// with the same signature.
TEST_F(ReceivedTabFormsFillerTest,
       ShouldNotFillFieldsByNonUniquePendingSignatureFallback) {
  // Create a sender form to generate a signature for the pending field.
  const FormData form_sender =
      CreateSingleFieldForm({.name_attribute = u"name_123"});
  const PageContext::FormFieldAutofillSignature sig =
      GetSignature(form_sender, 0);

  // Add TWO fields to pending_fields_ with the SAME signature but different
  // IDs.
  PageContext::FormFieldInfo form_field_info = CreateFormFieldInfo(
      {MakeFormField(u"id1", u"name_123", autofill::FormControlType::kInputText,
                     u"value1", sig),
       MakeFormField(u"id2", u"name_124", autofill::FormControlType::kInputText,
                     u"value2", sig)});

  // Create a receiver form with a field that has the same signature.
  const FormData form_receiver = CreateSingleFieldForm(
      {.name_attribute = u"name_123", .id_attribute = u"id3"});

  EXPECT_CALL(autofill_driver(), ApplyFieldAction).Times(0);

  EXPECT_TRUE(
      TryStartFillerAndSeeForm(origin_, form_field_info, form_receiver));

  ExpectUniqueMatchOutcome(FormFieldMatchOutcome::kNoMatch, 2);
}
// Tests that fallback matching via semantic type works when names and IDs do
// not match but there is a unique type match.
TEST_F(ReceivedTabFormsFillerTest, ShouldFillFieldsBySemanticMatchFallback) {
  PageContext::FormField pending_field = MakeFormField(
      u"id1", u"name1", autofill::FormControlType::kInputText, u"shared_value");
  pending_field.autofill_types = {
      sync_pb::FormField_AutofillFieldType_EMAIL_ADDRESS};
  PageContext::FormFieldInfo form_field_info =
      CreateFormFieldInfo({pending_field});

  const FormData form_receiver =
      CreateSingleFieldForm({.role = autofill::FieldType::EMAIL_ADDRESS,
                             .name_attribute = u"name_diff",
                             .id_attribute = u"id_diff"});

  const autofill::FieldGlobalId field_id =
      form_receiver.fields()[0].global_id();
  EXPECT_CALL(autofill_driver(),
              ApplyFieldAction(autofill::mojom::FieldActionType::kReplaceAll,
                               autofill::mojom::ActionPersistence::kFill,
                               Eq(field_id), Eq(u"shared_value")));

  EXPECT_TRUE(
      TryStartFillerAndSeeForm(origin_, form_field_info, form_receiver));

  ExpectUniqueMatchOutcome(FormFieldMatchOutcome::kMatchedByExactTypeSet, 1);
}

// Tests that matching is skipped if multiple pending fields share the same
// semantic type.
TEST_F(ReceivedTabFormsFillerTest,
       ShouldNotFillFieldsByAmbiguousSemanticMatchFallback) {
  PageContext::FormField pending_field1 = MakeFormField(
      u"id1", u"name1", autofill::FormControlType::kInputText, u"val1");
  pending_field1.autofill_types = {
      sync_pb::FormField_AutofillFieldType_EMAIL_ADDRESS};
  PageContext::FormField pending_field2 = MakeFormField(
      u"id2", u"name2", autofill::FormControlType::kInputText, u"val2");
  pending_field2.autofill_types = {
      sync_pb::FormField_AutofillFieldType_EMAIL_ADDRESS};
  PageContext::FormFieldInfo form_field_info =
      CreateFormFieldInfo({pending_field1, pending_field2});

  const FormData form_receiver =
      CreateSingleFieldForm({.role = autofill::FieldType::EMAIL_ADDRESS,
                             .name_attribute = u"name_diff",
                             .id_attribute = u"id_diff"});

  EXPECT_CALL(autofill_driver(), ApplyFieldAction).Times(0);

  EXPECT_TRUE(
      TryStartFillerAndSeeForm(origin_, form_field_info, form_receiver));

  ExpectUniqueMatchOutcome(FormFieldMatchOutcome::kNoMatch, 2);
}

// Tests that matching is skipped if the semantic type is not unique within
// the receiver form.
TEST_F(ReceivedTabFormsFillerTest,
       ShouldNotFillFieldsByDuplicateTypesInReceiverForm) {
  PageContext::FormField pending_field = MakeFormField(
      u"id1", u"name1", autofill::FormControlType::kInputText, u"shared_value");
  pending_field.autofill_types = {
      sync_pb::FormField_AutofillFieldType_EMAIL_ADDRESS};
  PageContext::FormFieldInfo form_field_info =
      CreateFormFieldInfo({pending_field});

  const FormData form_receiver =
      CreateTestForm({{.role = autofill::FieldType::EMAIL_ADDRESS,
                       .name_attribute = u"name_diff1",
                       .id_attribute = u"id_diff1"},
                      {.role = autofill::FieldType::EMAIL_ADDRESS,
                       .name_attribute = u"name_diff2",
                       .id_attribute = u"id_diff2"}});

  EXPECT_CALL(autofill_driver(), ApplyFieldAction).Times(0);

  EXPECT_TRUE(
      TryStartFillerAndSeeForm(origin_, form_field_info, form_receiver));

  ExpectUniqueMatchOutcome(FormFieldMatchOutcome::kNoMatch, 1);
}

// Tests that fallback semantic matching works when both the pending field and
// receiver field have the same multiple semantic types (exact match).
TEST_F(ReceivedTabFormsFillerTest,
       ShouldFillFieldsBySemanticMatchWithMultipleTypes) {
  PageContext::FormField pending_field = MakeFormField(
      u"id1", u"name1", autofill::FormControlType::kInputText, u"shared_value");
  pending_field.autofill_types = {
      sync_pb::FormField_AutofillFieldType_EMAIL_ADDRESS,
      sync_pb::FormField_AutofillFieldType_USERNAME};
  PageContext::FormFieldInfo form_field_info =
      CreateFormFieldInfo({pending_field});

  const FormData form_receiver = CreateSingleFieldForm(
      {.name_attribute = u"name_diff", .id_attribute = u"id_diff"});

  auto form_structure =
      std::make_unique<autofill::FormStructure>(form_receiver);
  autofill::FieldTypeSet types = {autofill::FieldType::USERNAME,
                                  autofill::FieldType::EMAIL_ADDRESS};
  form_structure->field(0)->SetTypeTo(autofill::AutofillType(types),
                                      std::nullopt);
  autofill::test_api(autofill_manager())
      .AddSeenFormStructure(std::move(form_structure));

  const autofill::FieldGlobalId field_id =
      form_receiver.fields()[0].global_id();
  EXPECT_CALL(autofill_driver(),
              ApplyFieldAction(autofill::mojom::FieldActionType::kReplaceAll,
                               autofill::mojom::ActionPersistence::kFill,
                               Eq(field_id), Eq(u"shared_value")));

  TestFuture<void> future;
  ReceivedTabFormsFiller::Start(autofill_client(), origin_, form_field_info,
                                future.GetCallback());

  EXPECT_TRUE(future.Wait());

  ExpectUniqueMatchOutcome(FormFieldMatchOutcome::kMatchedByExactTypeSet, 1);
}

// Tests that matching is skipped if a semantic type is not unique within
// the incoming fields, even if they match separate fields in the receiver form.
TEST_F(ReceivedTabFormsFillerTest,
       ShouldNotFillFieldsByDuplicateTypesInIncomingFields) {
  // Field 1 and Field 2 in the incoming fields share the same type
  // (EMAIL_ADDRESS).
  PageContext::FormField pending_field1 = MakeFormField(
      u"id1", u"name1", autofill::FormControlType::kInputText, u"val1");
  pending_field1.autofill_types = {
      sync_pb::FormField_AutofillFieldType_EMAIL_ADDRESS};
  PageContext::FormField pending_field2 = MakeFormField(
      u"id2", u"name2", autofill::FormControlType::kInputText, u"val2");
  pending_field2.autofill_types = {
      sync_pb::FormField_AutofillFieldType_EMAIL_ADDRESS};
  PageContext::FormFieldInfo form_field_info =
      CreateFormFieldInfo({pending_field1, pending_field2});

  // The receiver form has two distinct fields, both matching the EMAIL_ADDRESS
  // type.
  const FormData form_receiver =
      CreateTestForm({{.role = autofill::FieldType::EMAIL_ADDRESS,
                       .name_attribute = u"name_diff1",
                       .id_attribute = u"id_diff1"},
                      {.role = autofill::FieldType::EMAIL_ADDRESS,
                       .name_attribute = u"name_diff2",
                       .id_attribute = u"id_diff2"}});

  // Since the type is not unique in incoming fields, no autofill action should
  // be applied.
  EXPECT_CALL(autofill_driver(), ApplyFieldAction).Times(0);

  EXPECT_TRUE(
      TryStartFillerAndSeeForm(origin_, form_field_info, form_receiver));

  ExpectUniqueMatchOutcome(FormFieldMatchOutcome::kNoMatch, 2);
}

// Tests that a single pending field does not match multiple fields in the
// receiver form due to deferred erasure.
TEST_F(ReceivedTabFormsFillerTest,
       ShouldNotFillSameFieldMultipleTimesDueToDeferredErasure) {
  PageContext::FormFieldInfo form_field_info = CreateFormFieldInfo(
      {MakeFormField(u"id1", u"name1", autofill::FormControlType::kInputText,
                     u"shared_value")});

  // Create a receiver form with TWO identical fields.
  const FormData form_receiver =
      CreateTestForm({{.name_attribute = u"name1", .id_attribute = u"id1"},
                      {.name_attribute = u"name1", .id_attribute = u"id1"}});

  const autofill::FieldGlobalId first_field_id =
      form_receiver.fields()[0].global_id();

  // Should only apply action for the first matching field.
  EXPECT_CALL(autofill_driver(),
              ApplyFieldAction(autofill::mojom::FieldActionType::kReplaceAll,
                               autofill::mojom::ActionPersistence::kFill,
                               Eq(first_field_id), Eq(u"shared_value")))
      .Times(1);

  EXPECT_TRUE(
      TryStartFillerAndSeeForm(origin_, form_field_info, form_receiver));

  ExpectUniqueMatchOutcome(FormFieldMatchOutcome::kMatchedByIdNameAndType, 1);
}

TEST_F(ReceivedTabFormsFillerTest, ShouldStopOnManagerDestruction) {
  PageContext::FormFieldInfo form_field_info =
      CreateFormFieldInfo({MakeFormField(
          u"id1", u"", autofill::FormControlType::kInputText, u"val")});

  EXPECT_CALL(autofill_driver(), ApplyFieldAction).Times(0);

  TestFuture<void> future;
  ReceivedTabFormsFiller::Start(autofill_client(), origin_, form_field_info,
                                future.GetCallback());

  // Simulate destruction by notifying observers.
  autofill_manager().NotifyObservers(
      &autofill::AutofillManager::Observer::OnAutofillManagerStateChanged,
      autofill::AutofillManager::LifecycleState::kActive,
      autofill::AutofillManager::LifecycleState::kPendingDeletion);

  // Verifies that the completion callback gets invoked upon manager
  // destruction.
  EXPECT_TRUE(future.Wait());

  ExpectUniqueMatchOutcome(FormFieldMatchOutcome::kNoMatch, 1);
}

TEST_F(ReceivedTabFormsFillerTest, ShouldStopOnTimeout) {
  PageContext::FormFieldInfo form_field_info =
      CreateFormFieldInfo({MakeFormField(
          u"id1", u"", autofill::FormControlType::kInputText, u"val")});

  base::MockCallback<base::OnceClosure> completion_callback;
  ReceivedTabFormsFiller::Start(autofill_client(), origin_, form_field_info,
                                completion_callback.Get());

  // Should not stop after 9 seconds.
  EXPECT_CALL(completion_callback, Run).Times(0);
  task_environment_.FastForwardBy(base::Seconds(9));
  Mock::VerifyAndClearExpectations(&completion_callback);

  // Should stop after 10 seconds.
  EXPECT_CALL(completion_callback, Run);
  task_environment_.FastForwardBy(base::Seconds(1));

  ExpectUniqueMatchOutcome(FormFieldMatchOutcome::kNoMatch, 1);
}

TEST_F(ReceivedTabFormsFillerTest, ShouldNotFillFieldsWithDifferentOrigin) {
  PageContext::FormFieldInfo form_field_info = CreateFormFieldInfo(
      {MakeFormField(u"id1", u"name1", autofill::FormControlType::kInputText,
                     u"shared_value")});

  const FormData form = CreateSingleFieldForm({.name_attribute = u"name1",
                                               .id_attribute = u"id1",
                                               .origin = other_origin_});

  EXPECT_CALL(autofill_driver(), ApplyFieldAction).Times(0);

  TestFuture<void> future;
  ReceivedTabFormsFiller::Start(autofill_client(), origin_, form_field_info,
                                future.GetCallback());

  autofill_manager().OnFormsSeen({form}, {},
                                 autofill::AutofillManagerTestApi::pass_key());

  // Force self-destruction by notifying about manager deletion.
  autofill_manager().NotifyObservers(
      &autofill::AutofillManager::Observer::OnAutofillManagerStateChanged,
      autofill::AutofillManager::LifecycleState::kActive,
      autofill::AutofillManager::LifecycleState::kPendingDeletion);

  EXPECT_TRUE(future.Wait());

  ExpectUniqueMatchOutcome(FormFieldMatchOutcome::kNoMatch, 1);
}

}  // namespace

}  // namespace send_tab_to_self
