// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_manager.h"

#include <iterator>
#include <memory>
#include <tuple>
#include <vector>

#include "base/ranges/algorithm.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_tick_clock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Field;
using testing::NiceMock;
using testing::Return;
using testing::UnorderedElementsAreArray;

namespace autofill {

namespace {

class MockAutofillDownloadManager : public AutofillDownloadManager {
 public:
  explicit MockAutofillDownloadManager(AutofillClient* client)
      : AutofillDownloadManager(client,
                                /*api_key=*/"",
                                /*is_raw_metadata_uploading_enabled=*/false,
                                /*log_manager=*/nullptr) {}

  MockAutofillDownloadManager(const MockAutofillDownloadManager&) = delete;
  MockAutofillDownloadManager& operator=(const MockAutofillDownloadManager&) =
      delete;

  MOCK_METHOD(bool,
              StartQueryRequest,
              (const std::vector<FormStructure*>&,
               net::IsolationInfo,
               base::WeakPtr<Observer>),
              (override));
};

class MockAutofillClient : public TestAutofillClient {
 public:
  MockAutofillClient() = default;
  MockAutofillClient(const MockAutofillClient&) = delete;
  MockAutofillClient& operator=(const MockAutofillClient&) = delete;
  ~MockAutofillClient() override = default;
};

class MockAutofillDriver : public TestAutofillDriver {
 public:
  MockAutofillDriver() = default;
  MockAutofillDriver(const MockAutofillDriver&) = delete;
  MockAutofillDriver& operator=(const MockAutofillDriver&) = delete;
  ~MockAutofillDriver() override = default;

  MOCK_METHOD(void, SetShouldSuppressKeyboard, (bool), ());
  MOCK_METHOD(bool, CanShowAutofillUi, (), (const));
  MOCK_METHOD(void,
              TriggerReparseInAllFrames,
              (base::OnceCallback<void(bool)>),
              ());
};

class MockAutofillManager : public AutofillManager {
 public:
  MockAutofillManager(AutofillDriver* driver, AutofillClient* client)
      : AutofillManager(driver, client) {}

  base::WeakPtr<AutofillManager> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

  MOCK_METHOD(bool, ShouldClearPreviewedForm, (), (override));
  MOCK_METHOD(CreditCardAccessManager*,
              GetCreditCardAccessManager,
              (),
              (override));
  MOCK_METHOD(void,
              FillCreditCardFormImpl,
              (const FormData& form,
               const FormFieldData& field,
               const CreditCard& credit_card,
               const std::u16string& cvc,
               const AutofillTriggerSource trigger_source),
              (override));
  MOCK_METHOD(void,
              FillProfileFormImpl,
              (const FormData& form,
               const FormFieldData& field,
               const AutofillProfile& profile,
               const AutofillTriggerSource trigger_source),
              (override));
  MOCK_METHOD(void,
              OnFocusNoLongerOnFormImpl,
              (bool had_interacted_form),
              (override));
  MOCK_METHOD(void,
              OnDidFillAutofillFormDataImpl,
              (const FormData& form, const base::TimeTicks timestamp),
              (override));
  MOCK_METHOD(void, OnDidPreviewAutofillFormDataImpl, (), (override));
  MOCK_METHOD(void, OnDidEndTextFieldEditingImpl, (), (override));
  MOCK_METHOD(void, OnHidePopupImpl, (), (override));
  MOCK_METHOD(void,
              OnSelectFieldOptionsDidChangeImpl,
              (const FormData& form),
              (override));
  MOCK_METHOD(void,
              OnJavaScriptChangedAutofilledValueImpl,
              (const FormData& form,
               const FormFieldData& field,
               const std::u16string& old_value),
              (override));
  MOCK_METHOD(void,
              PropagateAutofillPredictions,
              (const std::vector<FormStructure*>& forms),
              (override));
  MOCK_METHOD(void,
              OnFormSubmittedImpl,
              (const FormData& form,
               bool known_success,
               mojom::SubmissionSource source),
              (override));
  MOCK_METHOD(void,
              OnTextFieldDidChangeImpl,
              (const FormData& form,
               const FormFieldData& field,
               const gfx::RectF& bounding_box,
               const base::TimeTicks timestamp),
              (override));
  MOCK_METHOD(void,
              OnTextFieldDidScrollImpl,
              (const FormData& form,
               const FormFieldData& field,
               const gfx::RectF& bounding_box),
              (override));
  MOCK_METHOD(void,
              OnAskForValuesToFillImpl,
              (const FormData& form,
               const FormFieldData& field,
               const gfx::RectF& bounding_box,
               AutoselectFirstSuggestion autoselect_first_suggestion,
               FormElementWasClicked form_element_was_clicked),
              (override));
  MOCK_METHOD(void,
              OnFocusOnFormFieldImpl,
              (const FormData& form,
               const FormFieldData& field,
               const gfx::RectF& bounding_box),
              (override));
  MOCK_METHOD(void,
              OnSelectControlDidChangeImpl,
              (const FormData& form,
               const FormFieldData& field,
               const gfx::RectF& bounding_box),
              (override));
  MOCK_METHOD(bool, ShouldParseForms, (), (override));
  MOCK_METHOD(void, OnBeforeProcessParsedForms, (), (override));
  MOCK_METHOD(void,
              OnFormProcessed,
              (const FormData& form_data, const FormStructure& form_structure),
              (override));
  MOCK_METHOD(void,
              OnAfterProcessParsedForms,
              (const DenseSet<FormType>& form_types),
              (override));
  MOCK_METHOD(void,
              ReportAutofillWebOTPMetrics,
              (bool used_web_otp),
              (override));
  MOCK_METHOD(void,
              OnContextMenuShownInField,
              (const FormGlobalId& form_global_id,
               const FieldGlobalId& field_global_id),
              (override));

 private:
  base::WeakPtrFactory<MockAutofillManager> weak_ptr_factory_{this};
};

class MockAutofillObserver : public AutofillManager::Observer {
 public:
  MockAutofillObserver() = default;
  MockAutofillObserver(const MockAutofillObserver&) = delete;
  MockAutofillObserver& operator=(const MockAutofillObserver&) = delete;
  ~MockAutofillObserver() override = default;

  MOCK_METHOD(void, OnFormParsed, (AutofillManager&), (override));

  MOCK_METHOD(void, OnTextFieldDidChange, (AutofillManager&), (override));

  MOCK_METHOD(void, OnTextFieldDidScroll, (AutofillManager&), (override));

  MOCK_METHOD(void, OnSelectControlDidChange, (AutofillManager&), (override));

  MOCK_METHOD(void, OnFormSubmitted, (AutofillManager&), (override));

  MOCK_METHOD(void,
              OnBeforeLoadedServerPredictions,
              (AutofillManager&),
              (override));

  MOCK_METHOD(void,
              OnAfterLoadedServerPredictions,
              (AutofillManager&),
              (override));
};

// Creates a vector of test forms which differ in their FormGlobalIds
// and FieldGlobalIds.
std::vector<FormData> CreateTestForms(size_t num_forms) {
  std::vector<FormData> forms(num_forms);
  for (FormData& form : forms)
    test::CreateTestAddressFormData(&form);
  return forms;
}

// Returns the FormGlobalIds of the specified |forms|.
std::vector<FormGlobalId> GetFormIds(const std::vector<FormData>& forms) {
  std::vector<FormGlobalId> ids;
  ids.reserve(forms.size());
  base::ranges::transform(forms, std::back_inserter(ids), &FormData::global_id);
  return ids;
}

// Matches a std::map<FormGlobalId, std::unique_ptr<FormStructure>>::value_type
// whose key is `form.global_id()`.
auto HaveSameFormIdAs(const FormData& form) {
  return Field(
      "global_id",
      &std::pair<const FormGlobalId, std::unique_ptr<FormStructure>>::first,
      form.global_id());
}

// Matches a std::map<FormGlobalId, std::unique_ptr<FormStructure>> whose
// keys are the same the FormGlobalIds of the forms in |forms|.
auto HaveSameFormIdsAs(const std::vector<FormData>& forms) {
  std::vector<decltype(HaveSameFormIdAs(forms.front()))> matchers;
  matchers.reserve(forms.size());
  base::ranges::transform(forms, std::back_inserter(matchers),
                          &HaveSameFormIdAs);
  return UnorderedElementsAreArray(matchers);
}

// Expects the calls triggered by OnFormsSeen().
void OnFormsSeenWithExpectations(MockAutofillManager& manager,
                                 const std::vector<FormData>& updated_forms,
                                 const std::vector<FormGlobalId>& removed_forms,
                                 const std::vector<FormData>& expectation) {
  size_t num =
      std::min(updated_forms.size(), kAutofillManagerMaxFormCacheSize -
                                         manager.form_structures().size());
  EXPECT_CALL(manager, ShouldParseForms).Times(1).WillOnce(Return(true));
  EXPECT_CALL(manager, OnBeforeProcessParsedForms()).Times(num > 0);
  EXPECT_CALL(manager, OnFormProcessed).Times(num);
  EXPECT_CALL(manager, OnAfterProcessParsedForms).Times(num > 0);
  TestAutofillManagerWaiter waiter(manager, {AutofillManagerEvent::kFormsSeen});
  manager.OnFormsSeen(updated_forms, removed_forms);
  ASSERT_TRUE(waiter.Wait());
  EXPECT_THAT(manager.form_structures(), HaveSameFormIdsAs(expectation));
}

}  // namespace

class AutofillManagerTest : public testing::Test {
 public:
  AutofillManagerTest() {
    scoped_feature_list_async_parse_form_.InitWithFeatureState(
        features::kAutofillParseAsync, true);
  }

  void SetUp() override {
    client_.SetPrefs(test::PrefServiceForTesting());
    driver_ = std::make_unique<NiceMock<MockAutofillDriver>>();
    manager_ = std::make_unique<MockAutofillManager>(driver_.get(), &client_);
  }

  void TearDown() override {
    manager_.reset();
    driver_.reset();
  }

  void SetUpObserverAndDownloadManager(bool successful_request) {
    auto download_manager =
        std::make_unique<MockAutofillDownloadManager>(&client_);
    ON_CALL(*download_manager, StartQueryRequest)
        .WillByDefault(Return(successful_request));
    client_.set_download_manager(std::move(download_manager));
    manager_->AddObserver(&observer_);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_async_parse_form_;
  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  NiceMock<MockAutofillClient> client_;
  std::unique_ptr<MockAutofillDriver> driver_;
  std::unique_ptr<MockAutofillManager> manager_;
  MockAutofillObserver observer_;
};

// The test parameter sets the number of forms to be generated.
class AutofillManagerTest_WithIntParam
    : public AutofillManagerTest,
      public ::testing::WithParamInterface<size_t> {
 public:
  size_t num_forms() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(AutofillManagerTest,
                         AutofillManagerTest_WithIntParam,
                         testing::Values(0, 1, 5, 10, 100, 110));

class AutofillManagerTest_OnLoadedServerPredictionsObserver
    : public AutofillManagerTest {
 public:
  void SetUpObserverAndDownloadManager(bool successful_request) {
    auto download_manager =
        std::make_unique<MockAutofillDownloadManager>(&client_);
    ON_CALL(*download_manager, StartQueryRequest)
        .WillByDefault(Return(successful_request));
    client_.set_download_manager(std::move(download_manager));
    manager_->AddObserver(&observer_);
  }
};

// Tests that the cache size is bounded by kAutofillManagerMaxFormCacheSize.
TEST_P(AutofillManagerTest_WithIntParam, CacheBoundFormsSeen) {
  size_t num_exp_forms =
      std::min(num_forms(), kAutofillManagerMaxFormCacheSize);
  std::vector<FormData> forms = CreateTestForms(num_forms());
  std::vector<FormData> exp_forms(forms.begin(), forms.begin() + num_exp_forms);
  OnFormsSeenWithExpectations(*manager_, forms, {}, exp_forms);
}

// Tests that removing unseen forms has no effect.
TEST_F(AutofillManagerTest, RemoveUnseenForms) {
  std::vector<FormData> forms = CreateTestForms(9);
  OnFormsSeenWithExpectations(*manager_, {}, GetFormIds(forms), {});
}

// Tests that all forms can be removed at once.
TEST_F(AutofillManagerTest, RemoveAllForms) {
  std::vector<FormData> forms = CreateTestForms(9);
  OnFormsSeenWithExpectations(*manager_, forms, {}, forms);
  OnFormsSeenWithExpectations(*manager_, {}, GetFormIds(forms), {});
}

// Tests that removing some forms leaves the other forms untouched.
TEST_F(AutofillManagerTest, RemoveSomeForms) {
  std::vector<FormData> forms = CreateTestForms(9);
  auto range = [&](size_t begin, size_t end) {
    return std::vector<FormData>(forms.begin() + begin, forms.begin() + end);
  };
  OnFormsSeenWithExpectations(*manager_, range(0, 6), {}, range(0, 6));
  OnFormsSeenWithExpectations(*manager_, range(6, 9), GetFormIds(range(0, 3)),
                              range(3, 9));
}

// Tests that adding and removing the same forms has no effect.
TEST_F(AutofillManagerTest, UpdateAndRemoveSameForms) {
  std::vector<FormData> forms = CreateTestForms(9);
  OnFormsSeenWithExpectations(*manager_, forms, GetFormIds(forms), forms);
  OnFormsSeenWithExpectations(*manager_, forms, GetFormIds(forms), forms);
}

TEST_F(AutofillManagerTest, ObserverReceiveCalls) {
  FormData form = CreateTestForms(1).front();
  FormFieldData field = form.fields.front();
  gfx::RectF bounds;
  base::TimeTicks time = AutofillTickClock::NowTicks();

  MockAutofillObserver observer;
  manager_->AddObserver(&observer);
  // Reset the manager, the observers should stick around.
  manager_->Reset();

  EXPECT_CALL(observer, OnTextFieldDidChange(testing::Address(manager_.get())));
  manager_->OnTextFieldDidChange(form, field, bounds, time);
  EXPECT_CALL(observer, OnTextFieldDidChange).Times(0);

  EXPECT_CALL(observer, OnTextFieldDidScroll(testing::Address(manager_.get())));
  manager_->OnTextFieldDidScroll(form, field, bounds);
  EXPECT_CALL(observer, OnTextFieldDidScroll).Times(0);

  EXPECT_CALL(observer,
              OnSelectControlDidChange(testing::Address(manager_.get())));
  manager_->OnSelectControlDidChange(form, field, bounds);
  EXPECT_CALL(observer, OnSelectControlDidChange).Times(0);

  EXPECT_CALL(observer, OnFormSubmitted(testing::Address(manager_.get())));
  manager_->OnFormSubmitted(form, true,
                            mojom::SubmissionSource::FORM_SUBMISSION);
  EXPECT_CALL(observer, OnFormSubmitted).Times(0);

  // Remove observer from manager, the observer should no longer receive pings.
  manager_->RemoveObserver(&observer);
  EXPECT_CALL(observer, OnTextFieldDidChange).Times(0);
  manager_->OnTextFieldDidChange(form, field, bounds, time);
}

TEST_F(AutofillManagerTest, SetShouldSuppressKeyboard) {
  EXPECT_CALL(*driver_, SetShouldSuppressKeyboard(true));
  manager_->SetShouldSuppressKeyboard(true);
}

TEST_F(AutofillManagerTest, CanShowAutofillUi) {
  EXPECT_CALL(*driver_, CanShowAutofillUi).WillOnce(Return(true));
  EXPECT_TRUE(manager_->CanShowAutofillUi());
}

TEST_F(AutofillManagerTest, TriggerReparseInAllFrames) {
  EXPECT_CALL(*driver_, TriggerReparseInAllFrames);
  manager_->TriggerReparseInAllFrames(base::DoNothing());
}

TEST_F(
    AutofillManagerTest_OnLoadedServerPredictionsObserver,
    OnFormsSeen_SuccessfulQueryRequest_NotifiesBeforeLoadedServerPredictionsObserver) {
  SetUpObserverAndDownloadManager(/*successful_request=*/true);

  std::vector<FormData> forms = CreateTestForms(1);
  EXPECT_CALL(observer_, OnBeforeLoadedServerPredictions(
                             testing::Address(manager_.get())));
  EXPECT_CALL(observer_, OnAfterLoadedServerPredictions).Times(0);
  OnFormsSeenWithExpectations(*manager_, forms, {}, forms);
  task_environment_.RunUntilIdle();

  manager_->RemoveObserver(&observer_);
}

TEST_F(
    AutofillManagerTest_OnLoadedServerPredictionsObserver,
    OnFormsSeen_FailedQueryRequest_NotifiesBothLoadedServerPredictionsObservers) {
  SetUpObserverAndDownloadManager(/*successful_request=*/false);

  std::vector<FormData> forms = CreateTestForms(1);
  EXPECT_CALL(observer_, OnBeforeLoadedServerPredictions(
                             testing::Address(manager_.get())));
  EXPECT_CALL(observer_,
              OnAfterLoadedServerPredictions(testing::Address(manager_.get())));
  OnFormsSeenWithExpectations(*manager_, forms, {}, forms);
  task_environment_.RunUntilIdle();

  manager_->RemoveObserver(&observer_);
}

TEST_F(
    AutofillManagerTest_OnLoadedServerPredictionsObserver,
    OnLoadedServerPredictions_EmptyQueriedFormSignatures_NotifiesAfterLoadedServerPredictionsObserver) {
  SetUpObserverAndDownloadManager(/*successful_request=*/true);

  std::vector<FormData> forms = CreateTestForms(1);
  EXPECT_CALL(observer_, OnBeforeLoadedServerPredictions(
                             testing::Address(manager_.get())));
  OnFormsSeenWithExpectations(*manager_, forms, {}, forms);
  task_environment_.RunUntilIdle();

  EXPECT_CALL(observer_,
              OnAfterLoadedServerPredictions(testing::Address(manager_.get())));
  manager_->OnLoadedServerPredictionsForTest("", {});

  manager_->RemoveObserver(&observer_);
}

TEST_F(
    AutofillManagerTest_OnLoadedServerPredictionsObserver,
    OnLoadedServerPredictions_NonEmptyQueriedFormSignatures_NotifiesAfterLoadedServerPredictionsObserver) {
  SetUpObserverAndDownloadManager(/*successful_request=*/true);

  std::vector<FormData> forms = CreateTestForms(1);
  EXPECT_CALL(observer_, OnBeforeLoadedServerPredictions(
                             testing::Address(manager_.get())));
  OnFormsSeenWithExpectations(*manager_, forms, {}, forms);
  task_environment_.RunUntilIdle();

  EXPECT_CALL(observer_,
              OnAfterLoadedServerPredictions(testing::Address(manager_.get())));
  manager_->OnLoadedServerPredictionsForTest(
      "",
      {manager_->FindCachedFormById(forms[0].global_id())->form_signature()});

  manager_->RemoveObserver(&observer_);
}

}  // namespace autofill
