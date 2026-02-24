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
using autofill::FormFieldData;
using autofill::FormStructure;
using autofill::TestBrowserAutofillManager;
using ::testing::_;
using ::testing::Eq;
using ::testing::Test;

PageContext::FormField MakeFormField(std::u16string id_attribute,
                                     std::u16string name_attribute,
                                     std::string form_control_type,
                                     std::u16string value) {
  PageContext::FormField field;
  field.id_attribute = std::move(id_attribute);
  field.name_attribute = std::move(name_attribute);
  field.form_control_type = std::move(form_control_type);
  field.value = std::move(value);
  return field;
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
