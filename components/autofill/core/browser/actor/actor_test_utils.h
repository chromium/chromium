// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ACTOR_ACTOR_TEST_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ACTOR_ACTOR_TEST_UTILS_H_

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/types/expected.h"
#include "components/actor/core/aggregated_journal.h"
#include "components/autofill/core/browser/actor/actor_form_filling_service_impl.h"
#include "components/autofill/core/browser/actor/actor_key_metrics_recorder.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/integrators/actor/actor_form_filling_types.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

// A test Autofill driver subclass that supports mock expectations on form
// actions. Used for verifying renderer-side fill actions triggered by the
// Autofill Actor service.
class TestActorAutofillDriver : public TestAutofillDriver {
 public:
  explicit TestActorAutofillDriver(TestAutofillClient* client);
  ~TestActorAutofillDriver() override;

  MOCK_METHOD(void, RendererShouldClearPreviewedForm, (), (override));
  MOCK_METHOD(void, ScrollFieldIntoView, (FieldGlobalId), (override));

  MOCK_METHOD(
      base::flat_set<FieldGlobalId>,
      ApplyFormAction,
      (mojom::FormActionType action_type,
       mojom::ActionPersistence action_persistence,
       base::span<const FormFieldData> fields,
       const FillId& fill_id,
       bool supports_refill,
       const url::Origin& triggered_origin,
       (const absl::flat_hash_map<FieldGlobalId, FieldType>& field_type_map)),
      (override));

  base::flat_set<FieldGlobalId> BaseApplyFormAction(
      mojom::FormActionType action_type,
      mojom::ActionPersistence action_persistence,
      base::span<const FormFieldData> fields,
      const FillId& fill_id,
      bool supports_refill,
      const url::Origin& triggered_origin,
      const absl::flat_hash_map<FieldGlobalId, FieldType>& field_type_map);
};

// A test credit card access manager that allows tests to intercept and
// manually complete credit card fetching operations.
class TestCreditCardAccessManager : public CreditCardAccessManager {
 public:
  explicit TestCreditCardAccessManager(BrowserAutofillManager* manager);
  ~TestCreditCardAccessManager() override;

  void PrepareToFetchCreditCard() override {}

  void FetchCreditCard(const CreditCard*,
                       OnCreditCardFetchedCallback callback) override;

  [[nodiscard]] bool RunCreditCardFetchedCallback(const CreditCard& card);

 private:
  OnCreditCardFetchedCallback callback_;
};

// A test BrowserAutofillManager subclass that instantiates and manages the
// TestCreditCardAccessManager, and tracks the last field ID that triggered form
// filling or preview.
class TestBrowserAutofillManagerWithTestCCAM
    : public TestBrowserAutofillManager {
 public:
  explicit TestBrowserAutofillManagerWithTestCCAM(AutofillDriver* driver);
  ~TestBrowserAutofillManagerWithTestCCAM() override;

  void Reset() override;

  void FillOrPreviewForm(
      mojom::ActionPersistence action_persistence,
      const FormGlobalId& form_id,
      const FieldGlobalId& field_id,
      const FillingPayload& filling_payload,
      AutofillTriggerSource trigger_source,
      const base::flat_set<FieldGlobalId>& blocked_fields) override;

  FieldGlobalId last_trigger_field_id() const { return last_trigger_field_id_; }

 private:
  FieldGlobalId last_trigger_field_id_;
};

// A test Autofill client subclass that manages the ActorKeyMetricsRecorder and
// resolves the primary main frame's AutofillManager.
class TestActorAutofillClient : public TestAutofillClient {
 public:
  TestActorAutofillClient();
  ~TestActorAutofillClient() override;

  AutofillManager* GetAutofillManagerForPrimaryMainFrame() override;

  ActorKeyMetricsRecorder* GetActorKeyMetricsRecorder() override;

 private:
  std::unique_ptr<ActorKeyMetricsRecorder> recorder_;
};

// Base test fixture for core Autofill Actor component unit tests. Manages the
// lifetime of core-only test Autofill client, driver, and manager instances
// without content or Blink dependencies.
class ActorTestBase : public testing::Test,
                      public WithTestAutofillClientDriverManager<
                          TestActorAutofillClient,
                          TestActorAutofillDriver,
                          TestBrowserAutofillManagerWithTestCCAM> {
 public:
  ActorTestBase();
  ~ActorTestBase() override;

  void SetUp() override;
  void TearDown() override;

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }

  FormData SeeForm(test::FormDescription form_description);

  const absl::flat_hash_map<FieldGlobalId, std::u16string>& last_filled_values()
      const {
    return last_filled_values_;
  }

 protected:
  TestActorAutofillClient& client() { return autofill_client(); }
  TestCreditCardAccessManager& credit_card_access_manager();
  PaymentsDataManager& payments_data_manager();
  TestActorAutofillDriver& driver() { return autofill_driver(); }
  TestBrowserAutofillManagerWithTestCCAM& manager() {
    return autofill_manager();
  }
  ActorFormFillingServiceImpl& service() { return *service_; }
  ::actor::AggregatedJournal& journal() { return journal_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  ::actor::AggregatedJournal journal_;
  std::unique_ptr<ActorFormFillingServiceImpl> service_;
  absl::flat_hash_map<FieldGlobalId, std::u16string> last_filled_values_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ACTOR_ACTOR_TEST_UTILS_H_
