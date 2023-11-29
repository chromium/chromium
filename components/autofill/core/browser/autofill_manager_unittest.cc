// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iterator>
#include <memory>
#include <tuple>
#include <vector>

#include "base/ranges/algorithm.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_manager_test_api.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/crowdsourcing/mock_autofill_crowdsourcing_manager.h"
#include "components/autofill/core/browser/mock_autofill_manager_observer.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_tick_clock.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/translate/core/common/language_detection_details.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "components/autofill/core/browser/ml_model/autofill_ml_prediction_model_handler.h"
#endif

namespace autofill {

namespace {

using ::testing::_;
using ::testing::AtLeast;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Pair;
using ::testing::Property;
using ::testing::Ref;
using ::testing::Return;
using ::testing::UnorderedElementsAreArray;
using ::testing::VariantWith;
using FieldTypeSource = AutofillManager::Observer::FieldTypeSource;

class MockAutofillDriver : public TestAutofillDriver {
 public:
  MockAutofillDriver() = default;
  MockAutofillDriver(const MockAutofillDriver&) = delete;
  MockAutofillDriver& operator=(const MockAutofillDriver&) = delete;
  ~MockAutofillDriver() override = default;

  MOCK_METHOD(bool, CanShowAutofillUi, (), (const));
  MOCK_METHOD(void,
              TriggerFormExtractionInAllFrames,
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
  MOCK_METHOD(void,
              FillOrPreviewField,
              (mojom::ActionPersistence action_persistence,
               mojom::TextReplacement text_replacement,
               const FormData& form,
               const FormFieldData& field,
               const std::u16string& value,
               PopupItemId popup_item_id),
              (override));
  MOCK_METHOD(void,
              OnFocusNoLongerOnFormImpl,
              (bool had_interacted_form),
              (override));
  MOCK_METHOD(void,
              OnDidFillAutofillFormDataImpl,
              (const FormData& form, const base::TimeTicks timestamp),
              (override));
  MOCK_METHOD(void, OnDidEndTextFieldEditingImpl, (), (override));
  MOCK_METHOD(void, OnHidePopupImpl, (), (override));
  MOCK_METHOD(void,
              OnSelectOrSelectListFieldOptionsDidChangeImpl,
              (const FormData& form),
              (override));
  MOCK_METHOD(void,
              OnJavaScriptChangedAutofilledValueImpl,
              (const FormData& form,
               const FormFieldData& field,
               const std::u16string& old_value),
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
               AutofillSuggestionTriggerSource trigger_source),
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

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
class MockAutofillMlPredictionModelHandler
    : public AutofillMlPredictionModelHandler {
 public:
  explicit MockAutofillMlPredictionModelHandler(
      optimization_guide::OptimizationGuideModelProvider* provider)
      : AutofillMlPredictionModelHandler(provider) {}
  ~MockAutofillMlPredictionModelHandler() override = default;

  MOCK_METHOD(
      void,
      GetModelPredictionsForForms,
      (std::vector<std::unique_ptr<FormStructure>>,
       base::OnceCallback<void(std::vector<std::unique_ptr<FormStructure>>)>),
      (override));
};
#endif

// Creates a vector of test forms which differ in their FormGlobalIds
// and FieldGlobalIds.
std::vector<FormData> CreateTestForms(size_t num_forms) {
  std::vector<FormData> forms;
  for (size_t i = 0; i < num_forms; ++i) {
    forms.push_back(test::CreateTestAddressFormData());
  }
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

  void SetUpObserverAndCrowdsourcingManager(bool successful_request) {
    auto crowdsourcing_manager =
        std::make_unique<MockAutofillCrowdsourcingManager>(&client_);
    ON_CALL(*crowdsourcing_manager, StartQueryRequest)
        .WillByDefault(Return(successful_request));
    client_.set_crowdsourcing_manager(std::move(crowdsourcing_manager));
    manager_->AddObserver(&observer_);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_async_parse_form_;
  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  TestAutofillClient client_;
  std::unique_ptr<MockAutofillDriver> driver_;
  std::unique_ptr<MockAutofillManager> manager_;
  MockAutofillManagerObserver observer_;
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
  void SetUpObserverAndCrowdsourcingManager(bool successful_request) {
    auto crowdsourcing_manager =
        std::make_unique<MockAutofillCrowdsourcingManager>(&client_);
    ON_CALL(*crowdsourcing_manager, StartQueryRequest)
        .WillByDefault(Return(successful_request));
    client_.set_crowdsourcing_manager(std::move(crowdsourcing_manager));
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
  base::test::ScopedFeatureList feature_list{
      features::kAutofillPageLanguageDetection};

  std::vector<FormData> forms = CreateTestForms(2);
  FormData form = forms[0];
  FormData other_form = forms[1];
  FormFieldData field = form.fields.front();

  // Shorthands for matchers to reduce visual noise.
  auto m = Ref(*manager_);
  auto f = Eq(form.global_id());
  auto g = Eq(other_form.global_id());
  auto ff = Eq(field.global_id());
  auto ff_property = Property(&FormFieldData::global_id, field.global_id());
  auto heuristics = Eq(FieldTypeSource::kHeuristicsOrAutocomplete);

  MockAutofillManagerObserver observer;
  base::ScopedObservation<AutofillManager, MockAutofillManagerObserver>
      observation{&observer};
  observation.Observe(manager_.get());

  // This test should have no unexpected calls of observer events.
  EXPECT_CALL(observer, OnAutofillManagerDestroyed).Times(0);
  EXPECT_CALL(observer, OnAutofillManagerReset).Times(0);
  EXPECT_CALL(observer, OnBeforeLanguageDetermined).Times(0);
  EXPECT_CALL(observer, OnAfterLanguageDetermined).Times(0);
  EXPECT_CALL(observer, OnBeforeFormsSeen).Times(0);
  EXPECT_CALL(observer, OnAfterFormsSeen).Times(0);
  EXPECT_CALL(observer, OnBeforeTextFieldDidChange).Times(0);
  EXPECT_CALL(observer, OnAfterTextFieldDidChange).Times(0);
  EXPECT_CALL(observer, OnBeforeTextFieldDidScroll).Times(0);
  EXPECT_CALL(observer, OnAfterTextFieldDidScroll).Times(0);
  EXPECT_CALL(observer, OnBeforeSelectControlDidChange).Times(0);
  EXPECT_CALL(observer, OnAfterSelectControlDidChange).Times(0);
  EXPECT_CALL(observer, OnBeforeDidFillAutofillFormData).Times(0);
  EXPECT_CALL(observer, OnAfterDidFillAutofillFormData).Times(0);
  EXPECT_CALL(observer, OnBeforeAskForValuesToFill).Times(0);
  EXPECT_CALL(observer, OnAfterAskForValuesToFill).Times(0);
  EXPECT_CALL(observer, OnBeforeJavaScriptChangedAutofilledValue).Times(0);
  EXPECT_CALL(observer, OnAfterJavaScriptChangedAutofilledValue).Times(0);
  EXPECT_CALL(observer, OnBeforeLoadedServerPredictions).Times(0);
  EXPECT_CALL(observer, OnAfterLoadedServerPredictions).Times(0);
  EXPECT_CALL(observer, OnFieldTypesDetermined).Times(0);
  EXPECT_CALL(observer, OnFillOrPreviewDataModelForm).Times(0);
  EXPECT_CALL(observer, OnFormSubmitted).Times(0);

  EXPECT_CALL(*manager_, ShouldParseForms)
      .Times(AtLeast(0))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*manager_, OnFocusNoLongerOnFormImpl).Times(AtLeast(0));
  EXPECT_CALL(*manager_, OnDidFillAutofillFormDataImpl).Times(AtLeast(0));
  EXPECT_CALL(*manager_, OnDidEndTextFieldEditingImpl).Times(AtLeast(0));
  EXPECT_CALL(*manager_, OnSelectOrSelectListFieldOptionsDidChangeImpl)
      .Times(AtLeast(0));
  EXPECT_CALL(*manager_, OnJavaScriptChangedAutofilledValueImpl)
      .Times(AtLeast(0));
  EXPECT_CALL(*manager_, OnFormSubmittedImpl).Times(AtLeast(0));
  EXPECT_CALL(*manager_, OnTextFieldDidChangeImpl).Times(AtLeast(0));
  EXPECT_CALL(*manager_, OnTextFieldDidScrollImpl).Times(AtLeast(0));
  EXPECT_CALL(*manager_, OnAskForValuesToFillImpl).Times(AtLeast(0));
  EXPECT_CALL(*manager_, OnFocusOnFormFieldImpl).Times(AtLeast(0));
  EXPECT_CALL(*manager_, OnSelectControlDidChangeImpl).Times(AtLeast(0));
  EXPECT_CALL(*manager_, OnBeforeProcessParsedForms).Times(AtLeast(0));
  EXPECT_CALL(*manager_, OnFormProcessed).Times(AtLeast(0));
  EXPECT_CALL(*manager_, OnAfterProcessParsedForms).Times(AtLeast(0));
  EXPECT_CALL(*manager_, OnContextMenuShownInField).Times(AtLeast(0));

  // Reset the manager, the observers should stick around.
  EXPECT_CALL(observer, OnAutofillManagerReset(m));
  manager_->Reset();

  EXPECT_CALL(observer, OnBeforeFormsSeen(m, ElementsAre(f, g)));
  manager_->OnFormsSeen(forms, /*removed_forms=*/{test::MakeFormGlobalId()});
  EXPECT_CALL(observer, OnAfterFormsSeen(m, ElementsAre(f, g)));
  EXPECT_CALL(observer, OnFieldTypesDetermined(m, f, heuristics));
  EXPECT_CALL(observer, OnFieldTypesDetermined(m, g, heuristics));
  task_environment_.RunUntilIdle();

  EXPECT_CALL(observer, OnBeforeLanguageDetermined(m));
  manager_->OnLanguageDetermined([] {
    translate::LanguageDetectionDetails details;
    details.adopted_language = "en";
    return details;
  }());
  EXPECT_CALL(observer, OnAfterLanguageDetermined(m));
  EXPECT_CALL(observer, OnFieldTypesDetermined(m, f, heuristics));
  EXPECT_CALL(observer, OnFieldTypesDetermined(m, g, heuristics));
  task_environment_.RunUntilIdle();

  form.fields.push_back(form.fields.back());
  form.fields.back().unique_renderer_id = test::MakeFieldRendererId();

  // The form was just changed, which causes a reparse. The reparse is
  // asynchronous, so OnAfterTextFieldDidChange() is asynchronous, too.
  EXPECT_CALL(observer, OnBeforeTextFieldDidChange(m, f, ff));
  manager_->OnTextFieldDidChange(form, field, {}, {});
  EXPECT_CALL(observer, OnAfterTextFieldDidChange(m, f, ff, std::u16string()));
  EXPECT_CALL(observer, OnFieldTypesDetermined(m, f, heuristics));
  task_environment_.RunUntilIdle();

  EXPECT_CALL(observer, OnBeforeTextFieldDidScroll(m, f, ff));
  EXPECT_CALL(observer, OnAfterTextFieldDidScroll(m, f, ff));
  manager_->OnTextFieldDidScroll(form, field, {});

  EXPECT_CALL(observer, OnBeforeDidFillAutofillFormData(m, f));
  EXPECT_CALL(observer, OnAfterDidFillAutofillFormData(m, f));
  manager_->OnDidFillAutofillFormData(form, {});

  EXPECT_CALL(observer, OnBeforeAskForValuesToFill(m, f, ff, Ref(form)));
  EXPECT_CALL(observer, OnAfterAskForValuesToFill(m, f, ff));
  manager_->OnAskForValuesToFill(form, field, {}, {});

  EXPECT_CALL(observer, OnBeforeJavaScriptChangedAutofilledValue(m, f, ff));
  EXPECT_CALL(observer, OnAfterJavaScriptChangedAutofilledValue(m, f, ff));
  manager_->OnJavaScriptChangedAutofilledValue(form, field, {});

  // TODO(crbug.com/) Test in browser_autofill_manager_unittest.cc that
  // FillOrPreviewForm() triggers OnFillOrPreviewDataModelForm().

  EXPECT_CALL(observer, OnFormSubmitted(m, f));
  manager_->OnFormSubmitted(form, true,
                            mojom::SubmissionSource::FORM_SUBMISSION);

  // OnBeforeLoadedServerPredictions(), OnAfterLoadedServerPredictions() are
  // tested in AutofillManagerTest_OnLoadedServerPredictionsObserver.

  EXPECT_CALL(observer, OnAutofillManagerDestroyed(m)).WillOnce(Invoke([&] {
    observation.Reset();
  }));
  manager_.reset();
}

TEST_F(AutofillManagerTest, CanShowAutofillUi) {
  EXPECT_CALL(*driver_, CanShowAutofillUi).WillOnce(Return(true));
  EXPECT_TRUE(manager_->CanShowAutofillUi());
}

TEST_F(AutofillManagerTest, TriggerFormExtractionInAllFrames) {
  EXPECT_CALL(*driver_, TriggerFormExtractionInAllFrames);
  manager_->TriggerFormExtractionInAllFrames(base::DoNothing());
}

// Ensure that `AutofillMlPredictionModelHandler` is called when parsing the
// form in `ParseFormsAsync()`
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
TEST_F(AutofillManagerTest, GetMlModelPredictionsForForm) {
  base::test::ScopedFeatureList features(features::kAutofillModelPredictions);

  auto provider = std::make_unique<
      optimization_guide::TestOptimizationGuideModelProvider>();
  auto mock_handler =
      std::make_unique<MockAutofillMlPredictionModelHandler>(provider.get());
  // This test intentionally doesn't associate predictions to the
  // `FormStructure`, it only expects that `GetModelPredictionsForForms` gets
  // called.
  ON_CALL(*mock_handler, GetModelPredictionsForForms)
      .WillByDefault(
          [](std::vector<std::unique_ptr<FormStructure>> forms,
             base::OnceCallback<void(
                 std::vector<std::unique_ptr<FormStructure>>)> callback) {
            std::move(callback).Run(std::move(forms));
          });
  EXPECT_CALL(*mock_handler, GetModelPredictionsForForms);
  client_.set_ml_prediction_model_handler(std::move(mock_handler));

  FormData form = test::CreateTestAddressFormData();
  OnFormsSeenWithExpectations(*manager_, /*updated_forms=*/{form},
                              /*removed_forms=*/{}, /*expectation=*/{form});

  // The handler own the model executor, which operates on a background thread
  // and is destroyed asynchronously. Resetting the `client_`'s handler triggers
  // the deletion. Wait for it to complete. This needs to happen before the
  // `provider` goes out of scope.
  client_.set_ml_prediction_model_handler(nullptr);
  task_environment_.RunUntilIdle();
}
#endif

TEST_F(
    AutofillManagerTest_OnLoadedServerPredictionsObserver,
    OnFormsSeen_SuccessfulQueryRequest_NotifiesBeforeLoadedServerPredictionsObserver) {
  SetUpObserverAndCrowdsourcingManager(/*successful_request=*/true);

  std::vector<FormData> forms = CreateTestForms(1);
  EXPECT_CALL(observer_, OnBeforeLoadedServerPredictions(Ref(*manager_)));
  EXPECT_CALL(observer_, OnAfterLoadedServerPredictions).Times(0);
  EXPECT_CALL(observer_, OnFieldTypesDetermined).Times(0);
  EXPECT_CALL(observer_, OnFieldTypesDetermined(
                             Ref(*manager_), forms[0].global_id(),
                             FieldTypeSource::kHeuristicsOrAutocomplete));
  OnFormsSeenWithExpectations(*manager_, forms, {}, forms);
  task_environment_.RunUntilIdle();

  manager_->RemoveObserver(&observer_);
}

TEST_F(
    AutofillManagerTest_OnLoadedServerPredictionsObserver,
    OnFormsSeen_FailedQueryRequest_NotifiesBothLoadedServerPredictionsObservers) {
  SetUpObserverAndCrowdsourcingManager(/*successful_request=*/false);

  std::vector<FormData> forms = CreateTestForms(1);
  EXPECT_CALL(observer_, OnBeforeLoadedServerPredictions(Ref(*manager_)));
  EXPECT_CALL(observer_, OnFieldTypesDetermined).Times(0);
  EXPECT_CALL(observer_, OnFieldTypesDetermined(
                             Ref(*manager_), forms[0].global_id(),
                             FieldTypeSource::kHeuristicsOrAutocomplete));
  EXPECT_CALL(observer_, OnAfterLoadedServerPredictions(Ref(*manager_)));
  OnFormsSeenWithExpectations(*manager_, forms, {}, forms);
  task_environment_.RunUntilIdle();

  manager_->RemoveObserver(&observer_);
}

TEST_F(
    AutofillManagerTest_OnLoadedServerPredictionsObserver,
    OnLoadedServerPredictions_EmptyQueriedFormSignatures_NotifiesAfterLoadedServerPredictionsObserver) {
  SetUpObserverAndCrowdsourcingManager(/*successful_request=*/true);

  std::vector<FormData> forms = CreateTestForms(1);
  EXPECT_CALL(observer_, OnBeforeLoadedServerPredictions(Ref(*manager_)));
  EXPECT_CALL(observer_, OnFieldTypesDetermined).Times(0);
  EXPECT_CALL(observer_, OnFieldTypesDetermined(
                             Ref(*manager_), forms[0].global_id(),
                             FieldTypeSource::kHeuristicsOrAutocomplete));
  OnFormsSeenWithExpectations(*manager_, forms, {}, forms);
  task_environment_.RunUntilIdle();

  EXPECT_CALL(observer_, OnAfterLoadedServerPredictions(Ref(*manager_)));
  test_api(*manager_).OnLoadedServerPredictions("", {});

  manager_->RemoveObserver(&observer_);
}

TEST_F(
    AutofillManagerTest_OnLoadedServerPredictionsObserver,
    OnLoadedServerPredictions_NonEmptyQueriedFormSignatures_NotifiesAfterLoadedServerPredictionsObserver) {
  SetUpObserverAndCrowdsourcingManager(/*successful_request=*/true);

  std::vector<FormData> forms = CreateTestForms(1);
  EXPECT_CALL(observer_, OnBeforeLoadedServerPredictions(Ref(*manager_)));
  EXPECT_CALL(observer_, OnFieldTypesDetermined).Times(0);
  EXPECT_CALL(observer_, OnFieldTypesDetermined(
                             Ref(*manager_), forms[0].global_id(),
                             FieldTypeSource::kHeuristicsOrAutocomplete));
  EXPECT_CALL(observer_,
              OnFieldTypesDetermined(Ref(*manager_), forms[0].global_id(),
                                     FieldTypeSource::kAutofillServer));
  OnFormsSeenWithExpectations(*manager_, forms, {}, forms);
  task_environment_.RunUntilIdle();

  EXPECT_CALL(observer_, OnAfterLoadedServerPredictions(Ref(*manager_)));
  test_api(*manager_).OnLoadedServerPredictions(
      "",
      {manager_->FindCachedFormById(forms[0].global_id())->form_signature()});

  manager_->RemoveObserver(&observer_);
}

}  // namespace autofill
