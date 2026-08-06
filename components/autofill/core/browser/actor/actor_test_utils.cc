// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/actor/actor_test_utils.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager_test_api.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TestActorAutofillDriver::TestActorAutofillDriver(TestAutofillClient* client)
    : TestAutofillDriver(client) {}

TestActorAutofillDriver::~TestActorAutofillDriver() = default;

base::flat_set<FieldGlobalId> TestActorAutofillDriver::BaseApplyFormAction(
    mojom::FormActionType action_type,
    mojom::ActionPersistence action_persistence,
    base::span<const FormFieldData> fields,
    const FillId& fill_id,
    bool supports_refill,
    const url::Origin& triggered_origin,
    const absl::flat_hash_map<FieldGlobalId, FieldType>& field_type_map) {
  return TestAutofillDriver::ApplyFormAction(action_type, action_persistence,
                                             fields, fill_id, supports_refill,
                                             triggered_origin, field_type_map);
}

TestCreditCardAccessManager::TestCreditCardAccessManager(
    BrowserAutofillManager* manager)
    : CreditCardAccessManager(manager) {}

TestCreditCardAccessManager::~TestCreditCardAccessManager() = default;

void TestCreditCardAccessManager::FetchCreditCard(
    const CreditCard*,
    OnCreditCardFetchedCallback callback) {
  callback_ = std::move(callback);
}

bool TestCreditCardAccessManager::RunCreditCardFetchedCallback(
    const CreditCard& card) {
  if (!callback_) {
    return false;
  }
  std::move(callback_).Run(card);
  return true;
}

TestBrowserAutofillManagerWithTestCCAM::TestBrowserAutofillManagerWithTestCCAM(
    AutofillDriver* driver)
    : TestBrowserAutofillManager(driver) {
  test_api(*this).set_credit_card_access_manager(
      std::make_unique<TestCreditCardAccessManager>(this));
}

TestBrowserAutofillManagerWithTestCCAM::
    ~TestBrowserAutofillManagerWithTestCCAM() = default;

void TestBrowserAutofillManagerWithTestCCAM::Reset() {
  test_api(*this).ResetBrowserAutofillManagerWithoutDynamicDispatch();
  test_api(*this).set_credit_card_access_manager(
      std::make_unique<TestCreditCardAccessManager>(this));
}

void TestBrowserAutofillManagerWithTestCCAM::FillOrPreviewForm(
    mojom::ActionPersistence action_persistence,
    const FormGlobalId& form_id,
    const FieldGlobalId& field_id,
    const FillingPayload& filling_payload,
    AutofillTriggerSource trigger_source,
    const base::flat_set<FieldGlobalId>& blocked_fields) {
  last_trigger_field_id_ = field_id;
  TestBrowserAutofillManager::FillOrPreviewForm(action_persistence, form_id,
                                                field_id, filling_payload,
                                                trigger_source, blocked_fields);
}

TestActorAutofillClient::TestActorAutofillClient() {
  recorder_ = std::make_unique<ActorKeyMetricsRecorder>(this);
}

TestActorAutofillClient::~TestActorAutofillClient() = default;

AutofillManager*
TestActorAutofillClient::GetAutofillManagerForPrimaryMainFrame() {
  if (GetAutofillDriverFactory().GetExistingDrivers().empty()) {
    return nullptr;
  }
  return &GetAutofillDriverFactory().driver(0)->GetAutofillManager();
}

ActorKeyMetricsRecorder* TestActorAutofillClient::GetActorKeyMetricsRecorder() {
  return recorder_.get();
}

ActorTestBase::ActorTestBase()
    : service_(
          std::make_unique<ActorFormFillingServiceImpl>(journal_.GetSafeRef(),
                                                        ::actor::TaskId(1))) {}

ActorTestBase::~ActorTestBase() = default;

void ActorTestBase::SetUp() {
  InitAutofillClient();
  CreateAutofillDriver();
  client().GetPersonalDataManager().address_data_manager().AddProfile(
      test::GetFullProfile());

  ON_CALL(driver(), ApplyFormAction)
      .WillByDefault([&](mojom::FormActionType action_type,
                         mojom::ActionPersistence action_persistence,
                         base::span<const FormFieldData> fields,
                         const FillId& fill_id, bool supports_refill,
                         const url::Origin& triggered_origin,
                         const absl::flat_hash_map<FieldGlobalId, FieldType>&
                             field_type_map) {
        base::flat_set<FieldGlobalId> filled_fields =
            driver().BaseApplyFormAction(action_type, action_persistence,
                                         fields, fill_id, supports_refill,
                                         triggered_origin, field_type_map);
        for (const FormFieldData& field : fields) {
          if (filled_fields.contains(field.global_id())) {
            last_filled_values_[field.global_id()] = field.value();
          }
        }
        return filled_fields;
      });
}

void ActorTestBase::TearDown() {
  DestroyAutofillClient();
}

FormData ActorTestBase::SeeForm(test::FormDescription form_description) {
  FormData form = test::GetFormData(form_description);
  manager().AddSeenForm(form, test::GetHeuristicTypes(form_description),
                        test::GetServerTypes(form_description));
  return form;
}

TestCreditCardAccessManager& ActorTestBase::credit_card_access_manager() {
  return CHECK_DEREF(static_cast<TestCreditCardAccessManager*>(
      manager().GetCreditCardAccessManager()));
}

PaymentsDataManager& ActorTestBase::payments_data_manager() {
  return client().GetPersonalDataManager().payments_data_manager();
}

}  // namespace autofill
