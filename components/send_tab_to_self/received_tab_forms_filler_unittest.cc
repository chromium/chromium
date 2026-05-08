// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/received_tab_forms_filler.h"

#include <memory>
#include <vector>

#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
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
using ::testing::_;
using ::testing::Eq;
using ::testing::Test;

PageContext::FormField MakeFormField(
    std::u16string id_attribute,
    std::u16string name_attribute,
    std::string form_control_type,
    std::u16string value,
    std::optional<PageContext::FormFieldAutofillSignature> sig = std::nullopt) {
  PageContext::FormField field;
  field.id_attribute = std::move(id_attribute);
  field.name_attribute = std::move(name_attribute);
  field.form_control_type = std::move(form_control_type);
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
    InitAutofillClient();
    CreateAutofillDriver();
  }

  void TearDown() override { DestroyAutofillClient(); }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_;
};

TEST_F(ReceivedTabFormsFillerTest, ShouldFillMatchingFields) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com"));
  PageContext::FormFieldInfo form_field_info;
  form_field_info.fields.push_back(
      MakeFormField(u"id1", u"name1", "text", u"shared_value"));

  const FormData form = autofill::test::GetFormData(
      {.fields = {{.renderer_id = autofill::FieldRendererId(1),
                   .label = u"label1",
                   .name_attribute = u"name1",
                   .id_attribute = u"id1",
                   .origin = kOrigin}},
       .url = "https://example.com"});
  const autofill::FieldGlobalId field_id = form.fields()[0].global_id();

  EXPECT_CALL(autofill_driver(),
              ApplyFieldAction(autofill::mojom::FieldActionType::kReplaceAll,
                               autofill::mojom::ActionPersistence::kFill,
                               Eq(field_id), Eq(u"shared_value")));

  base::RunLoop run_loop;
  ReceivedTabFormsFiller::Start(autofill_client(), kOrigin, form_field_info,
                                run_loop.QuitClosure());

  autofill_manager().OnFormsSeen({form}, {});

  run_loop.Run();
}

// Tests that fallback signature matching works when names/IDs are dynamic
// but the signature is unique.
TEST_F(ReceivedTabFormsFillerTest, ShouldFillFieldsByUniqueSignatureFallback) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com"));

  const FormData form_sender = autofill::test::GetFormData(
      {.fields = {{.renderer_id = autofill::FieldRendererId(1),
                   .name_attribute = u"name_123",
                   .origin = kOrigin}},
       .url = "https://example.com"});
  PageContext::FormFieldInfo form_field_info;
  form_field_info.fields.push_back(MakeFormField(u"id1", u"name_123", "text",
                                                 u"shared_value",
                                                 GetSignature(form_sender, 0)));

  const FormData form_receiver = autofill::test::GetFormData(
      {.fields = {{.renderer_id = autofill::FieldRendererId(2),
                   .label = u"label1",
                   .name_attribute = u"name_124",
                   .id_attribute = u"id2",
                   .origin = kOrigin}},
       .url = "https://example.com"});

  const autofill::FieldGlobalId field_id =
      form_receiver.fields()[0].global_id();

  EXPECT_CALL(autofill_driver(),
              ApplyFieldAction(autofill::mojom::FieldActionType::kReplaceAll,
                               autofill::mojom::ActionPersistence::kFill,
                               Eq(field_id), Eq(u"shared_value")));

  base::RunLoop run_loop;
  ReceivedTabFormsFiller::Start(autofill_client(), kOrigin, form_field_info,
                                run_loop.QuitClosure());

  autofill_manager().OnFormsSeen({form_receiver}, {});

  run_loop.Run();
}

// Tests that fallback matching is skipped if the receiver form has multiple
// fields with the same signature.
TEST_F(ReceivedTabFormsFillerTest,
       ShouldNotFillFieldsByNonUniqueReceiverSignatureFallback) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com"));

  const FormData form_sender = autofill::test::GetFormData(
      {.fields = {{.renderer_id = autofill::FieldRendererId(1),
                   .name_attribute = u"name_123",
                   .origin = kOrigin}},
       .url = "https://example.com"});
  PageContext::FormFieldInfo form_field_info;
  form_field_info.fields.push_back(MakeFormField(u"id1", u"name_123", "text",
                                                 u"shared_value",
                                                 GetSignature(form_sender, 0)));

  // Create a receiver form with TWO fields that have the SAME signature.
  // We use the same name to ensure they generate the same signature in tests.
  const FormData form_receiver = autofill::test::GetFormData(
      {.fields = {{.renderer_id = autofill::FieldRendererId(2),
                   .label = u"label1",
                   .name_attribute = u"name_123",
                   .id_attribute = u"id2",
                   .origin = kOrigin},
                  {.renderer_id = autofill::FieldRendererId(3),
                   .label = u"label2",
                   .name_attribute = u"name_123",
                   .id_attribute = u"id3",
                   .origin = kOrigin}},
       .url = "https://example.com"});

  EXPECT_CALL(autofill_driver(), ApplyFieldAction).Times(0);

  base::RunLoop run_loop;
  ReceivedTabFormsFiller::Start(autofill_client(), kOrigin, form_field_info,
                                run_loop.QuitClosure());

  autofill_manager().OnFormsSeen({form_receiver}, {});

  run_loop.Run();
}

// Tests that fallback matching is skipped if there are multiple pending fields
// with the same signature.
TEST_F(ReceivedTabFormsFillerTest,
       ShouldNotFillFieldsByNonUniquePendingSignatureFallback) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com"));

  const FormData form_sender = autofill::test::GetFormData(
      {.fields = {{.renderer_id = autofill::FieldRendererId(1),
                   .name_attribute = u"name_123",
                   .origin = kOrigin}},
       .url = "https://example.com"});
  const PageContext::FormFieldAutofillSignature sig =
      GetSignature(form_sender, 0);

  PageContext::FormFieldInfo form_field_info;

  // Add TWO fields to pending_fields_ with the SAME signature but different
  // IDs.
  form_field_info.fields.push_back(
      MakeFormField(u"id1", u"name_123", "text", u"value1", sig));

  form_field_info.fields.push_back(
      MakeFormField(u"id2", u"name_124", "text", u"value2", sig));

  const FormData form_receiver = autofill::test::GetFormData(
      {.fields = {{.renderer_id = autofill::FieldRendererId(2),
                   .label = u"label1",
                   .name_attribute = u"name_123",
                   .id_attribute = u"id3",
                   .origin = kOrigin}},
       .url = "https://example.com"});

  EXPECT_CALL(autofill_driver(), ApplyFieldAction).Times(0);

  base::RunLoop run_loop;
  ReceivedTabFormsFiller::Start(autofill_client(), kOrigin, form_field_info,
                                run_loop.QuitClosure());

  autofill_manager().OnFormsSeen({form_receiver}, {});

  run_loop.Run();
}
// Tests that fallback matching via semantic type works when names and IDs do
// not match but there is a unique type match.
TEST_F(ReceivedTabFormsFillerTest, ShouldFillFieldsBySemanticMatchFallback) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com"));

  PageContext::FormFieldInfo form_field_info;
  PageContext::FormField pending_field =
      MakeFormField(u"id1", u"name1", "text", u"shared_value");
  pending_field.autofill_types = {
      sync_pb::FormField_AutofillFieldType_USERNAME};
  form_field_info.fields.push_back(pending_field);

  const FormData form_receiver = autofill::test::GetFormData(
      {.fields = {{.renderer_id = autofill::FieldRendererId(2),
                   .name_attribute = u"name_diff",
                   .id_attribute = u"id_diff",
                   .origin = kOrigin}},
       .url = "https://example.com"});

  ActivateAutofillDriver(autofill_driver());

  auto form_structure =
      std::make_unique<autofill::FormStructure>(form_receiver);
  form_structure->field(0)->SetTypeTo(
      autofill::AutofillType(autofill::FieldType::USERNAME), std::nullopt);
  autofill::test_api(autofill_manager())
      .AddSeenFormStructure(std::move(form_structure));

  const autofill::FieldGlobalId field_id =
      form_receiver.fields()[0].global_id();
  EXPECT_CALL(autofill_driver(),
              ApplyFieldAction(autofill::mojom::FieldActionType::kReplaceAll,
                               autofill::mojom::ActionPersistence::kFill,
                               Eq(field_id), Eq(u"shared_value")));

  base::RunLoop run_loop;
  ReceivedTabFormsFiller::Start(autofill_client(), kOrigin, form_field_info,
                                run_loop.QuitClosure());

  run_loop.Run();
}

// Tests that matching is skipped if multiple pending fields share the same
// semantic type.
TEST_F(ReceivedTabFormsFillerTest,
       ShouldNotFillFieldsByAmbiguousSemanticMatchFallback) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com"));

  PageContext::FormFieldInfo form_field_info;
  PageContext::FormField pending_field1 =
      MakeFormField(u"id1", u"name1", "text", u"val1");
  pending_field1.autofill_types = {
      sync_pb::FormField_AutofillFieldType_USERNAME};
  form_field_info.fields.push_back(pending_field1);

  PageContext::FormField pending_field2 =
      MakeFormField(u"id2", u"name2", "text", u"val2");
  pending_field2.autofill_types = {
      sync_pb::FormField_AutofillFieldType_USERNAME};
  form_field_info.fields.push_back(pending_field2);

  const FormData form_receiver = autofill::test::GetFormData(
      {.fields = {{.renderer_id = autofill::FieldRendererId(2),
                   .name_attribute = u"name_diff",
                   .id_attribute = u"id_diff",
                   .origin = kOrigin}},
       .url = "https://example.com"});

  ActivateAutofillDriver(autofill_driver());

  auto form_structure =
      std::make_unique<autofill::FormStructure>(form_receiver);
  form_structure->field(0)->SetTypeTo(
      autofill::AutofillType(autofill::FieldType::USERNAME), std::nullopt);
  autofill::test_api(autofill_manager())
      .AddSeenFormStructure(std::move(form_structure));

  EXPECT_CALL(autofill_driver(), ApplyFieldAction).Times(0);

  base::RunLoop run_loop;
  ReceivedTabFormsFiller::Start(autofill_client(), kOrigin, form_field_info,
                                run_loop.QuitClosure());

  run_loop.Run();
}

// Tests that matching is skipped if the semantic type is not unique within
// the receiver form.
TEST_F(ReceivedTabFormsFillerTest,
       ShouldNotFillFieldsByDuplicateTypesInReceiverForm) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com"));

  PageContext::FormFieldInfo form_field_info;
  PageContext::FormField pending_field =
      MakeFormField(u"id1", u"name1", "text", u"shared_value");
  pending_field.autofill_types = {
      sync_pb::FormField_AutofillFieldType_USERNAME};
  form_field_info.fields.push_back(pending_field);

  const FormData form_receiver = autofill::test::GetFormData(
      {.fields = {{.renderer_id = autofill::FieldRendererId(2),
                   .name_attribute = u"name_diff1",
                   .id_attribute = u"id_diff1",
                   .origin = kOrigin},
                  {.renderer_id = autofill::FieldRendererId(3),
                   .name_attribute = u"name_diff2",
                   .id_attribute = u"id_diff2",
                   .origin = kOrigin}},
       .url = "https://example.com"});

  ActivateAutofillDriver(autofill_driver());

  auto form_structure =
      std::make_unique<autofill::FormStructure>(form_receiver);
  form_structure->field(0)->SetTypeTo(
      autofill::AutofillType(autofill::FieldType::USERNAME), std::nullopt);
  form_structure->field(1)->SetTypeTo(
      autofill::AutofillType(autofill::FieldType::USERNAME), std::nullopt);
  autofill::test_api(autofill_manager())
      .AddSeenFormStructure(std::move(form_structure));

  EXPECT_CALL(autofill_driver(), ApplyFieldAction).Times(0);

  base::RunLoop run_loop;
  ReceivedTabFormsFiller::Start(autofill_client(), kOrigin, form_field_info,
                                run_loop.QuitClosure());

  run_loop.Run();
}

// Tests that fallback semantic matching works when both the pending field and
// receiver field have the same multiple semantic types (exact match).
TEST_F(ReceivedTabFormsFillerTest,
       ShouldFillFieldsBySemanticMatchWithMultipleTypes) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com"));

  PageContext::FormFieldInfo form_field_info;
  PageContext::FormField pending_field =
      MakeFormField(u"id1", u"name1", "text", u"shared_value");
  pending_field.autofill_types = {
      sync_pb::FormField_AutofillFieldType_EMAIL_ADDRESS,
      sync_pb::FormField_AutofillFieldType_USERNAME};
  form_field_info.fields.push_back(pending_field);

  const FormData form_receiver = autofill::test::GetFormData(
      {.fields = {{.renderer_id = autofill::FieldRendererId(2),
                   .name_attribute = u"name_diff",
                   .id_attribute = u"id_diff",
                   .origin = kOrigin}},
       .url = "https://example.com"});

  ActivateAutofillDriver(autofill_driver());

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

  base::RunLoop run_loop;
  ReceivedTabFormsFiller::Start(autofill_client(), kOrigin, form_field_info,
                                run_loop.QuitClosure());

  run_loop.Run();
}

// Tests that matching is skipped if a semantic type is not unique within
// the incoming fields, even if they match separate fields in the receiver form.
TEST_F(ReceivedTabFormsFillerTest,
       ShouldNotFillFieldsByDuplicateTypesInIncomingFields) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com"));

  PageContext::FormFieldInfo form_field_info;
  // Field 1 and Field 2 in the incoming fields share the same type (USERNAME).
  PageContext::FormField pending_field1 =
      MakeFormField(u"id1", u"name1", "text", u"val1");
  pending_field1.autofill_types = {
      sync_pb::FormField_AutofillFieldType_USERNAME};
  form_field_info.fields.push_back(pending_field1);

  PageContext::FormField pending_field2 =
      MakeFormField(u"id2", u"name2", "text", u"val2");
  pending_field2.autofill_types = {
      sync_pb::FormField_AutofillFieldType_USERNAME};
  form_field_info.fields.push_back(pending_field2);

  // The receiver form has two distinct fields, both matching the USERNAME type.
  const FormData form_receiver = autofill::test::GetFormData(
      {.fields = {{.renderer_id = autofill::FieldRendererId(2),
                   .name_attribute = u"name_diff1",
                   .id_attribute = u"id_diff1",
                   .origin = kOrigin},
                  {.renderer_id = autofill::FieldRendererId(3),
                   .name_attribute = u"name_diff2",
                   .id_attribute = u"id_diff2",
                   .origin = kOrigin}},
       .url = "https://example.com"});

  ActivateAutofillDriver(autofill_driver());

  auto form_structure =
      std::make_unique<autofill::FormStructure>(form_receiver);
  form_structure->field(0)->SetTypeTo(
      autofill::AutofillType(autofill::FieldType::USERNAME), std::nullopt);
  form_structure->field(1)->SetTypeTo(
      autofill::AutofillType(autofill::FieldType::USERNAME), std::nullopt);
  autofill::test_api(autofill_manager())
      .AddSeenFormStructure(std::move(form_structure));

  // Since the type is not unique in incoming fields, no autofill action should
  // be applied.
  EXPECT_CALL(autofill_driver(), ApplyFieldAction).Times(0);

  base::RunLoop run_loop;
  ReceivedTabFormsFiller::Start(autofill_client(), kOrigin, form_field_info,
                                run_loop.QuitClosure());

  run_loop.Run();
}

// Tests that a single pending field does not match multiple fields in the
// receiver form due to deferred erasure.
TEST_F(ReceivedTabFormsFillerTest,
       ShouldNotFillSameFieldMultipleTimesDueToDeferredErasure) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com"));

  PageContext::FormFieldInfo form_field_info;
  form_field_info.fields.push_back(
      MakeFormField(u"id1", u"name1", "text", u"shared_value"));

  // Create a receiver form with TWO identical fields.
  const FormData form_receiver = autofill::test::GetFormData(
      {.fields = {{.renderer_id = autofill::FieldRendererId(2),
                   .name_attribute = u"name1",
                   .id_attribute = u"id1",
                   .origin = kOrigin},
                  {.renderer_id = autofill::FieldRendererId(3),
                   .name_attribute = u"name1",
                   .id_attribute = u"id1",
                   .origin = kOrigin}},
       .url = "https://example.com"});

  ActivateAutofillDriver(autofill_driver());

  const autofill::FieldGlobalId first_field_id =
      form_receiver.fields()[0].global_id();

  // Should only apply action for the first matching field.
  EXPECT_CALL(autofill_driver(),
              ApplyFieldAction(autofill::mojom::FieldActionType::kReplaceAll,
                               autofill::mojom::ActionPersistence::kFill,
                               Eq(first_field_id), Eq(u"shared_value")))
      .Times(1);

  base::RunLoop run_loop;
  ReceivedTabFormsFiller::Start(autofill_client(), kOrigin, form_field_info,
                                run_loop.QuitClosure());

  autofill_manager().OnFormsSeen({form_receiver}, {});

  run_loop.Run();
}

TEST_F(ReceivedTabFormsFillerTest, ShouldStopOnManagerDestruction) {
  PageContext::FormFieldInfo form_field_info;
  form_field_info.fields.push_back(MakeFormField(u"id1", u"", "text", u"val"));

  EXPECT_CALL(autofill_driver(), ApplyFieldAction).Times(0);

  base::RunLoop run_loop;
  ReceivedTabFormsFiller::Start(
      autofill_client(), url::Origin::Create(GURL("https://example.com")),
      form_field_info, run_loop.QuitClosure());

  // Simulate destruction by notifying observers.
  autofill_manager().NotifyObservers(
      &autofill::AutofillManager::Observer::OnAutofillManagerStateChanged,
      autofill::AutofillManager::LifecycleState::kActive,
      autofill::AutofillManager::LifecycleState::kPendingDeletion);

  // Verifies that the completion callback gets invoked upon manager
  // destruction.
  run_loop.Run();
}

TEST_F(ReceivedTabFormsFillerTest, ShouldStopOnTimeout) {
  PageContext::FormFieldInfo form_field_info;
  form_field_info.fields.push_back(MakeFormField(u"id1", u"", "text", u"val"));

  base::MockCallback<base::OnceClosure> completion_callback;
  ReceivedTabFormsFiller::Start(
      autofill_client(), url::Origin::Create(GURL("https://example.com")),
      form_field_info, completion_callback.Get());

  // Should not stop after 9 seconds.
  EXPECT_CALL(completion_callback, Run).Times(0);
  task_environment_.FastForwardBy(base::Seconds(9));
  testing::Mock::VerifyAndClearExpectations(&completion_callback);

  // Should stop after 10 seconds.
  EXPECT_CALL(completion_callback, Run);
  task_environment_.FastForwardBy(base::Seconds(1));
}

TEST_F(ReceivedTabFormsFillerTest, ShouldNotFillFieldsWithDifferentOrigin) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com"));
  const url::Origin kOtherOrigin =
      url::Origin::Create(GURL("https://other.com"));
  PageContext::FormFieldInfo form_field_info;
  form_field_info.fields.push_back(
      MakeFormField(u"id1", u"name1", "text", u"shared_value"));

  const FormData form = autofill::test::GetFormData(
      {.fields = {{.renderer_id = autofill::FieldRendererId(1),
                   .label = u"label1",
                   .name_attribute = u"name1",
                   .id_attribute = u"id1",
                   .origin = kOtherOrigin}},
       .url = "https://example.com"});

  EXPECT_CALL(autofill_driver(), ApplyFieldAction).Times(0);

  base::RunLoop run_loop;
  ReceivedTabFormsFiller::Start(autofill_client(), kOrigin, form_field_info,
                                run_loop.QuitClosure());

  autofill_manager().OnFormsSeen({form}, {});

  // Force self-destruction by notifying about manager deletion.
  autofill_manager().NotifyObservers(
      &autofill::AutofillManager::Observer::OnAutofillManagerStateChanged,
      autofill::AutofillManager::LifecycleState::kActive,
      autofill::AutofillManager::LifecycleState::kPendingDeletion);

  run_loop.Run();
}

}  // namespace

}  // namespace send_tab_to_self
