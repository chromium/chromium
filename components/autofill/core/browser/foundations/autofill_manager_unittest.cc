// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/foundations/autofill_manager.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <tuple>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/to_vector.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/crowdsourcing/mock_autofill_crowdsourcing_manager.h"
#include "components/autofill/core/browser/foundations/autofill_manager_test_api.h"
#include "components/autofill/core/browser/foundations/mock_autofill_manager.h"
#include "components/autofill/core/browser/foundations/mock_autofill_manager_observer.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_autofill_manager_waiter.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/optimization_guide/core/delivery/test_optimization_guide_model_provider.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/translate/core/common/language_detection_details.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "components/autofill/core/browser/ml_model/field_classification_model_handler.h"
#endif

namespace autofill {
namespace {

using ::base::test::RunClosure;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Ref;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::UnorderedElementsAreArray;
using ::testing::VariantWith;
using FieldTypeSource = AutofillManager::Observer::FieldTypeSource;

class MockAutofillDriver : public TestAutofillDriver {
 public:
  using TestAutofillDriver::TestAutofillDriver;
  MockAutofillDriver(const MockAutofillDriver&) = delete;
  MockAutofillDriver& operator=(const MockAutofillDriver&) = delete;
  ~MockAutofillDriver() override = default;

  MOCK_METHOD(bool, CanShowAutofillUi, (), (const));
  MOCK_METHOD(void,
              TriggerFormExtractionInAllFrames,
              (base::OnceCallback<void(bool)>),
              ());
};

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
class MockFieldClassificationModelHandler
    : public FieldClassificationModelHandler {
 public:
  explicit MockFieldClassificationModelHandler(
      optimization_guide::OptimizationGuideModelProvider* provider)
      : FieldClassificationModelHandler(
            provider,
            optimization_guide::proto::OptimizationTarget::
                OPTIMIZATION_TARGET_AUTOFILL_FIELD_CLASSIFICATION) {}
  ~MockFieldClassificationModelHandler() override = default;

  MOCK_METHOD(void,
              GetModelPredictionsForForms,
              (std::vector<FormData>,
               const GeoIpCountryCode& client_country,
               bool,
               base::OnceCallback<void(std::vector<ModelPredictions>)>),
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
  return base::ToVector(forms, &FormData::global_id);
}

// Matches a std::map<FormGlobalId, std::unique_ptr<FormStructure>>::value_type
// whose key is `form.global_id()`.
auto HaveSameFormIdAs(const FormData& form) {
  return Pointee(
      Property("global_id", &FormStructure::global_id, form.global_id()));
}

// Matches a std::map<FormGlobalId, std::unique_ptr<FormStructure>> whose
// keys are the same the FormGlobalIds of the forms in |forms|.
auto HaveSameFormIdsAs(const std::vector<FormData>& forms) {
  return UnorderedElementsAreArray(base::ToVector(forms, &HaveSameFormIdAs));
}

// Expects the calls triggered by OnFormsSeen().
void OnFormsSeenWithExpectations(MockAutofillManager& autofill_manager,
                                 const std::vector<FormData>& updated_forms,
                                 const std::vector<FormGlobalId>& removed_forms,
                                 const std::vector<FormData>& expectation) {
  const size_t num =
      std::min(updated_forms.size(),
               kAutofillManagerMaxFormCacheSize -
                   test_api(autofill_manager).form_structures().size());
  EXPECT_CALL(autofill_manager, ShouldParseForms)
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(autofill_manager, OnBeforeProcessParsedForms()).Times(num > 0);
  EXPECT_CALL(autofill_manager, OnFormProcessed).Times(num);
  TestAutofillManagerWaiter waiter(autofill_manager,
                                   {AutofillManagerEvent::kFormsSeen});
  autofill_manager.OnFormsSeen(updated_forms, removed_forms);
  ASSERT_TRUE(waiter.Wait());
  EXPECT_THAT(test_api(autofill_manager).form_structures(),
              HaveSameFormIdsAs(expectation));
}

class AutofillManagerTest
    : public testing::Test,
      public WithTestAutofillClientDriverManager<TestAutofillClient,
                                                 MockAutofillDriver,
                                                 MockAutofillManager> {
 public:
  void SetUp() override {
    InitAutofillClient();
    autofill_client().SetPrefs(test::PrefServiceForTesting());
    CreateAutofillDriver();
  }

  void TearDown() override { DestroyAutofillClient(); }

  MockAutofillCrowdsourcingManager& crowdsourcing_manager() {
    return static_cast<MockAutofillCrowdsourcingManager&>(
        autofill_client().GetCrowdsourcingManager());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
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
  using QueryResponse = AutofillCrowdsourcingManager::QueryResponse;

  void SetUp() override {
    AutofillManagerTest::SetUp();
    autofill_manager().AddObserver(&observer_);
  }

  void TearDown() override {
    autofill_manager().RemoveObserver(&observer_);
    AutofillManagerTest::TearDown();
  }

  MockAutofillManagerObserver observer_;
};

// Tests that the cache size is bounded by kAutofillManagerMaxFormCacheSize.
TEST_P(AutofillManagerTest_WithIntParam, CacheBoundFormsSeen) {
  size_t num_exp_forms =
      std::min(num_forms(), kAutofillManagerMaxFormCacheSize);
  std::vector<FormData> forms = CreateTestForms(num_forms());
  std::vector<FormData> exp_forms(forms.begin(), forms.begin() + num_exp_forms);
  OnFormsSeenWithExpectations(autofill_manager(), forms, {}, exp_forms);
}

// Tests that removing unseen forms has no effect.
TEST_F(AutofillManagerTest, RemoveUnseenForms) {
  std::vector<FormData> forms = CreateTestForms(9);
  OnFormsSeenWithExpectations(autofill_manager(), {}, GetFormIds(forms), {});
}

// Tests that all forms can be removed at once.
TEST_F(AutofillManagerTest, RemoveAllForms) {
  std::vector<FormData> forms = CreateTestForms(9);
  OnFormsSeenWithExpectations(autofill_manager(), forms, {}, forms);
  OnFormsSeenWithExpectations(autofill_manager(), {}, GetFormIds(forms), {});
}

// Tests that removing some forms leaves the other forms untouched.
TEST_F(AutofillManagerTest, RemoveSomeForms) {
  std::vector<FormData> forms = CreateTestForms(9);
  auto range = [&](size_t begin, size_t end) {
    return std::vector<FormData>(forms.begin() + begin, forms.begin() + end);
  };
  OnFormsSeenWithExpectations(autofill_manager(), range(0, 6), {}, range(0, 6));
  OnFormsSeenWithExpectations(autofill_manager(), range(6, 9),
                              GetFormIds(range(0, 3)), range(3, 9));
}

// Tests that adding and removing the same forms has no effect.
TEST_F(AutofillManagerTest, UpdateAndRemoveSameForms) {
  std::vector<FormData> forms = CreateTestForms(9);
  OnFormsSeenWithExpectations(autofill_manager(), forms, GetFormIds(forms),
                              forms);
  OnFormsSeenWithExpectations(autofill_manager(), forms, GetFormIds(forms),
                              forms);
}

// Tests that events update the form cache. Since there are so many events and
// so many properties of forms that may change, the test only covers a small
// fraction:
// - Events: OnFormsSeen(), OnTextFieldValueChanged(), OnFocusOnFormField()
// - Properties: AutofillField::value()
TEST_F(AutofillManagerTest, FormCacheUpdatesValue) {
  EXPECT_CALL(autofill_manager(), ShouldParseForms)
      .Times(AtLeast(0))
      .WillRepeatedly(Return(true));
  TestAutofillManagerWaiter waiter(autofill_manager());

  FormData form = test::CreateTestAddressFormData();
  FormGlobalId form_id = form.global_id();
  auto current_cached_value = [this,
                               &form_id]() -> std::optional<std::u16string> {
    const FormStructure* cached_form =
        autofill_manager().FindCachedFormById(form_id);
    if (!cached_form || cached_form->fields().empty()) {
      return std::nullopt;
    }
    return cached_form->fields()[0]->value();
  };

  EXPECT_EQ(current_cached_value(), std::nullopt);

  // Triggers a parse.
  test_api(form).field(0).set_value(u"first seen value");
  autofill_manager().OnFormsSeen({form}, {});
  ASSERT_TRUE(waiter.Wait());
  EXPECT_EQ(current_cached_value(), u"first seen value");

  // Triggers a reparse.
  test_api(form).field(0).set_value(u"second seen value");
  autofill_manager().OnFormsSeen({form}, {});
  ASSERT_TRUE(waiter.Wait());
  EXPECT_EQ(current_cached_value(), u"second seen value");

  // Triggers no reparse.
  test_api(form).field(0).set_value(u"first changed value");
  autofill_manager().OnTextFieldValueChanged(form, form.fields()[0].global_id(),
                                             {});
  ASSERT_TRUE(waiter.Wait());
  EXPECT_EQ(current_cached_value(), u"first changed value");

  // Triggers no reparse.
  test_api(form).field(0).set_value(u"second changed value");
  autofill_manager().OnTextFieldValueChanged(form, form.fields()[0].global_id(),
                                             {});
  ASSERT_TRUE(waiter.Wait());
  EXPECT_EQ(current_cached_value(), u"second changed value");

  // Triggers a reparse.
  test_api(form).Remove(-1);
  test_api(form).field(0).set_value(u"first reparse value");
  autofill_manager().OnFocusOnFormField(form, form.fields()[0].global_id());
  ASSERT_TRUE(waiter.Wait());
  EXPECT_EQ(current_cached_value(), u"first reparse value");

  // Triggers a second reparse.
  test_api(form).Remove(-1);
  test_api(form).field(0).set_value(u"second reparse value");
  autofill_manager().OnFocusOnFormField(form, form.fields()[0].global_id());
  ASSERT_TRUE(waiter.Wait());
  EXPECT_EQ(current_cached_value(), u"second reparse value");
}

// Test fixture for AutofillManager::Observer events.
class AutofillManagerTest_ObserverCalls
    : public testing::Test,
      public WithTestAutofillClientDriverManager<
          TestAutofillClient,
          MockAutofillDriver,
          NiceMock<MockAutofillManager>> {
 public:
  void SetUp() override {
    InitAutofillClient();
    autofill_client().SetPrefs(test::PrefServiceForTesting());
    CreateAutofillDriver();
    ON_CALL(autofill_manager(), ShouldParseForms).WillByDefault(Return(true));
    observation_.Observe(&autofill_manager());
  }

  void TearDown() override {
    observation_.Reset();
    DestroyAutofillClient();
  }

  MockAutofillManagerObserver& observer() { return observer_; }

  base::ScopedObservation<AutofillManager, MockAutofillManagerObserver>&
  observation() {
    return observation_;
  }

 private:
  base::test::ScopedFeatureList feature_list{
      features::kAutofillPageLanguageDetection};
  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;

  // Use a strict mock to detect no unexpected events.
  StrictMock<MockAutofillManagerObserver> observer_;
  base::ScopedObservation<AutofillManager, MockAutofillManagerObserver>
      observation_{&observer_};
};

// Tests that AutofillManager::OnFoo() fires
// AutofillManager::Observer::OnBeforeFoo() and
// AutofillManager::Observer::OnAfterFoo().
TEST_F(AutofillManagerTest_ObserverCalls, CallsEvents) {
  std::vector<FormData> forms = CreateTestForms(2);
  FormData form = forms[0];
  FormData other_form = forms[1];
  FormFieldData field = form.fields().front();
  FormGlobalId id_to_remove = test::MakeFormGlobalId();

  // Shorthands for matchers to reduce visual noise.
  using enum AutofillManager::LifecycleState;
  auto m = Ref(autofill_manager());
  auto f = Eq(form.global_id());
  auto g = Eq(other_form.global_id());
  auto ff = Eq(field.global_id());
  auto heuristics = Eq(FieldTypeSource::kHeuristicsOrAutocomplete);
  auto small_forms_parsing = Eq(autofill_client().IsTabInActorMode());

  // Reset the manager and transition from kInactive to kActive. The observers
  // should stick around.
  EXPECT_CALL(observer(),
              OnAutofillManagerStateChanged(m, kInactive, kPendingReset));
  EXPECT_CALL(observer(),
              OnAutofillManagerStateChanged(m, kPendingReset, kInactive));
  ResetAutofillDriver(autofill_driver());
  EXPECT_CALL(observer(), OnAutofillManagerStateChanged(m, kInactive, kActive));
  ActivateAutofillDriver(autofill_driver());

  // Test that not changing the LifecycleState does not fire events.
  ActivateAutofillDriver(autofill_driver());

  {
    // Test that even if the vector of new forms is too large, events are still
    // fired for the removed form.
    base::RunLoop run_loop;
    EXPECT_CALL(observer(),
                OnBeforeFormsSeen(m, IsEmpty(), ElementsAre(id_to_remove)));
    EXPECT_CALL(observer(),
                OnAfterFormsSeen(m, IsEmpty(), ElementsAre(id_to_remove)))
        .WillOnce(RunClosure(run_loop.QuitClosure()));
    autofill_manager().OnFormsSeen(std::vector<FormData>(kMaxListSize + 10),
                                   {id_to_remove});
    std::move(run_loop).Run();
  }

  // Test that seeing a normal amount of new forms triggers all events. Parsing
  // is asynchronous, so the OnAfterFoo() events are asynchronous, too.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer(), OnBeforeFormsSeen(m, ElementsAre(f, g),
                                              ElementsAre(id_to_remove)));
    EXPECT_CALL(observer(), OnBeforeLoadedServerPredictions(m));
    autofill_manager().OnFormsSeen(forms, {id_to_remove});
    EXPECT_CALL(observer(), OnAfterLoadedServerPredictions(m));
    EXPECT_CALL(observer(), OnAfterFormsSeen(m, ElementsAre(f, g),
                                             ElementsAre(id_to_remove)))
        .WillOnce(RunClosure(run_loop.QuitClosure()));
    EXPECT_CALL(observer(),
                OnFieldTypesDetermined(m, f, heuristics, small_forms_parsing));
    EXPECT_CALL(observer(),
                OnFieldTypesDetermined(m, g, heuristics, small_forms_parsing));
    std::move(run_loop).Run();
  }

  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer(), OnBeforeLanguageDetermined(m));
    autofill_manager().OnLanguageDetermined([] {
      translate::LanguageDetectionDetails details;
      details.adopted_language = "en";
      return details;
    }());
    EXPECT_CALL(observer(), OnAfterLanguageDetermined(m))
        .WillOnce(RunClosure(run_loop.QuitClosure()));
    EXPECT_CALL(observer(),
                OnFieldTypesDetermined(m, f, heuristics, small_forms_parsing));
    EXPECT_CALL(observer(),
                OnFieldTypesDetermined(m, g, heuristics, small_forms_parsing));
    std::move(run_loop).Run();
  }

  test_api(form).Append(form.fields().back());
  test_api(form).field(-1).set_renderer_id(test::MakeFieldRendererId());

  {
    // The form was just changed, which causes a reparse. The reparse is
    // asynchronous, so OnAfterTextFieldValueChanged() is asynchronous, too.
    base::RunLoop run_loop;
    EXPECT_CALL(observer(), OnBeforeTextFieldValueChanged(m, f, ff));
    autofill_manager().OnTextFieldValueChanged(form, field.global_id(), {});
    EXPECT_CALL(observer(), OnAfterTextFieldValueChanged(m, f, ff))
        .WillOnce(RunClosure(run_loop.QuitClosure()));
    EXPECT_CALL(observer(),
                OnFieldTypesDetermined(m, f, heuristics, small_forms_parsing));
    std::move(run_loop).Run();
  }

  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer(), OnBeforeTextFieldDidScroll(m, f, ff));
    EXPECT_CALL(observer(), OnAfterTextFieldDidScroll(m, f, ff))
        .WillOnce(RunClosure(run_loop.QuitClosure()));
    autofill_manager().OnTextFieldDidScroll(form, field.global_id());
    std::move(run_loop).Run();
  }

  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer(), OnBeforeSelectControlSelectionChanged(m, f, ff));
    EXPECT_CALL(observer(), OnAfterSelectControlSelectionChanged(m, f, ff))
        .WillOnce(RunClosure(run_loop.QuitClosure()));
    autofill_manager().OnSelectControlSelectionChanged(form, field.global_id());
    std::move(run_loop).Run();
  }

  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer(), OnBeforeSelectFieldOptionsDidChange(m, f));
    EXPECT_CALL(observer(), OnAfterSelectFieldOptionsDidChange(m, f))
        .WillOnce(RunClosure(run_loop.QuitClosure()));
    autofill_manager().OnSelectFieldOptionsDidChange(form, field.global_id());
    std::move(run_loop).Run();
  }

  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer(), OnBeforeDidAutofillForm(m, f));
    EXPECT_CALL(observer(), OnAfterDidAutofillForm(m, f))
        .WillOnce(RunClosure(run_loop.QuitClosure()));
    autofill_manager().OnDidAutofillForm(form);
    std::move(run_loop).Run();
  }

  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer(), OnBeforeAskForValuesToFill(m, f, ff, Ref(form)));
    EXPECT_CALL(observer(), OnAfterAskForValuesToFill(m, f, ff))
        .WillOnce(RunClosure(run_loop.QuitClosure()));
    autofill_manager().OnAskForValuesToFill(
        form, field.global_id(), gfx::Rect(),
        AutofillSuggestionTriggerSource::kUnspecified, std::nullopt);
    std::move(run_loop).Run();
  }

  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer(), OnBeforeFocusOnFormField(m, f, ff));
    EXPECT_CALL(observer(), OnAfterFocusOnFormField(m, f, ff))
        .WillOnce(RunClosure(run_loop.QuitClosure()));
    autofill_manager().OnFocusOnFormField(form, field.global_id());
    std::move(run_loop).Run();
  }

  EXPECT_CALL(observer(), OnBeforeFocusOnNonFormField(m));
  EXPECT_CALL(observer(), OnAfterFocusOnNonFormField(m));
  autofill_manager().OnFocusOnNonFormField();

  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer(), OnBeforeJavaScriptChangedAutofilledValue(m, f, ff));
    EXPECT_CALL(observer(), OnAfterJavaScriptChangedAutofilledValue(m, f, ff))
        .WillOnce(RunClosure(run_loop.QuitClosure()));
    autofill_manager().OnJavaScriptChangedAutofilledValue(
        form, field.global_id(), {});
    std::move(run_loop).Run();
  }

  // TODO(crbug.com/) Test in browser_autofill_manager_unittest.cc that
  // FillOrPreviewForm() triggers OnFillOrPreviewForm().

  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer(), OnBeforeFormSubmitted(m, Ref(form)));
    EXPECT_CALL(observer(), OnAfterFormSubmitted(m, Ref(form)))
        .WillOnce(RunClosure(run_loop.QuitClosure()));
    autofill_manager().OnFormSubmitted(
        form, mojom::SubmissionSource::FORM_SUBMISSION);
    std::move(run_loop).Run();
  }

  // Reset the manager, the observers should stick around.
  EXPECT_CALL(observer(),
              OnAutofillManagerStateChanged(m, kActive, kPendingDeletion))
      .WillOnce([this] { observation().Reset(); });
  DeleteAutofillDriver(autofill_driver());
}

// Tests that AutofillManager::OnFoo() fires
// AutofillManager::Observer::OnBeforeFoo() and especially
// AutofillManager::Observer::OnAfterFoo() even if the cache is full.
class AutofillManagerTest_ObserverCalls_FullCache
    : public AutofillManagerTest_ObserverCalls {
 public:
  // Populates the cache with forms so that only one free spot remains.
  void SetUp() override {
    AutofillManagerTest_ObserverCalls::SetUp();
    EXPECT_CALL(observer(), OnFieldTypesDetermined).Times(AtLeast(0));
    EXPECT_CALL(observer(), OnBeforeLoadedServerPredictions).Times(AtLeast(0));
    EXPECT_CALL(observer(), OnAfterLoadedServerPredictions).Times(AtLeast(0));
    base::RunLoop run_loop;
    EXPECT_CALL(observer(), OnBeforeFormsSeen);
    EXPECT_CALL(observer(), OnAfterFormsSeen)
        .WillOnce(RunClosure(run_loop.QuitClosure()));
    autofill_manager().OnFormsSeen(
        CreateTestForms(kAutofillManagerMaxFormCacheSize - 1), {});
    std::move(run_loop).Run();
    EXPECT_EQ(GetCacheSize(), kAutofillManagerMaxFormCacheSize - 1);
  }

  size_t GetCacheSize() {
    return test_api(autofill_manager()).form_structures().size();
  }

  bool CacheContains(const FormGlobalId& form_id) {
    return autofill_manager().FindCachedFormById(form_id);
  }
};

// Tests that AutofillManager::ParseFormsAsync() (plural) calls OnAfterFoo()
// even if the cache is full.
TEST_F(AutofillManagerTest_ObserverCalls_FullCache,
       CallsOnAfterFoo_IfCacheIsFull_ParseFormsAsync) {
  base::RunLoop run_loop;

  std::vector<FormData> updated_forms = CreateTestForms(10);
  std::vector<FormGlobalId> updated_form_ids =
      base::ToVector(updated_forms, &FormData::global_id);
  std::vector<FormGlobalId> removed_form_ids = {test::MakeFormGlobalId()};

  // The cache is too full to accommodate `updated_forms`.
  // Nonetheless, OnBeforeFormsSeen() and OnAfterFormsSeen() must both be
  // called with the same arguments.
  testing::InSequence s;
  EXPECT_CALL(observer(),
              OnBeforeFormsSeen(_, ElementsAreArray(updated_form_ids),
                                ElementsAreArray(removed_form_ids)));
  EXPECT_CALL(observer(),
              OnAfterFormsSeen(_, ElementsAreArray(updated_form_ids),
                               ElementsAreArray(removed_form_ids)))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  EXPECT_GT(GetCacheSize() + updated_form_ids.size() - removed_form_ids.size(),
            kAutofillManagerMaxFormCacheSize);
  autofill_manager().OnFormsSeen(updated_forms, removed_form_ids);
  std::move(run_loop).Run();
  EXPECT_EQ(GetCacheSize(), kAutofillManagerMaxFormCacheSize);
}

// Tests that AutofillManager::ParseFormAsync() (singular) calls OnAfterFoo()
// even if the cache is full.
//
// OnTextFieldValueChanged() calls ParseFormAsync(), not ParseFormsAsync()
// (singular vs plural).
TEST_F(AutofillManagerTest_ObserverCalls_FullCache,
       CallsOnAfterFoo_IfCacheIsFull_ParseFormAsync) {
  const FormData form1 = test::CreateTestAddressFormData();
  const FormData form2 = test::CreateTestAddressFormData();
  const FormGlobalId form_id1 = form1.global_id();
  const FormGlobalId form_id2 = form2.global_id();
  ASSERT_NE(form1.global_id(), form2.global_id());
  base::RunLoop run_loop1;
  base::RunLoop run_loop2;

  // Adding `form1` makes the cache full.
  // So the cache is full at the time of OnTextFieldValueChanged() for `form2`.
  // Nonetheless, OnAfterTextFieldValueChanged() is fired for `form2`.
  testing::InSequence s;
  EXPECT_CALL(observer(), OnBeforeTextFieldValueChanged(_, form_id1, _));
  EXPECT_CALL(observer(), OnAfterTextFieldValueChanged(_, form_id1, _))
      .WillOnce(RunClosure(run_loop1.QuitClosure()));
  EXPECT_CALL(observer(), OnBeforeTextFieldValueChanged(_, form_id2, _));
  EXPECT_CALL(observer(), OnAfterTextFieldValueChanged(_, form_id2, _))
      .WillOnce(RunClosure(run_loop2.QuitClosure()));

  autofill_manager().OnTextFieldValueChanged(
      form1, form1.fields().front().global_id(), base::TimeTicks::Now());
  std::move(run_loop1).Run();
  EXPECT_TRUE(CacheContains(form_id1));
  EXPECT_EQ(GetCacheSize(), kAutofillManagerMaxFormCacheSize);

  autofill_manager().OnTextFieldValueChanged(
      form2, form2.fields().front().global_id(), base::TimeTicks::Now());
  std::move(run_loop2).Run();
  EXPECT_FALSE(CacheContains(form_id2));
  EXPECT_EQ(GetCacheSize(), kAutofillManagerMaxFormCacheSize);
}

// Tests that AutofillManager::ParseFormAsync() calls OnAfterFoo() even if the
// cache is full when the asynchronous parsing has finished.
//
// OnTextFieldValueChanged() calls ParseFormAsync(), not ParseFormsAsync()
// (singular vs plural), and that is what this test is testing.
TEST_F(AutofillManagerTest_ObserverCalls_FullCache,
       CallsOnAfterFoo_IfCacheIsFullAtTheEndOfParsing) {
  const FormData form1 = test::CreateTestAddressFormData();
  const FormData form2 = test::CreateTestAddressFormData();
  const FormGlobalId form_id1 = form1.global_id();
  const FormGlobalId form_id2 = form2.global_id();
  ASSERT_NE(form1.global_id(), form2.global_id());
  base::RunLoop run_loop;

  // Both `form1` and `form2` are parsed asynchronously.
  // Parsing of `form1` finishes before parsing of `form2`.
  // So the cache is full at the time parsing of `form2` finishes.
  // Nonetheless, OnAfterTextFieldValueChanged() is fired for `form2`.
  testing::InSequence s;
  EXPECT_CALL(observer(), OnBeforeTextFieldValueChanged(_, form_id1, _));
  EXPECT_CALL(observer(), OnBeforeTextFieldValueChanged(_, form_id2, _));
  EXPECT_CALL(observer(), OnAfterTextFieldValueChanged(_, form_id1, _));
  EXPECT_CALL(observer(), OnAfterTextFieldValueChanged(_, form_id2, _))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  autofill_manager().OnTextFieldValueChanged(
      form1, form1.fields().front().global_id(), base::TimeTicks::Now());
  autofill_manager().OnTextFieldValueChanged(
      form2, form2.fields().front().global_id(), base::TimeTicks::Now());
  EXPECT_FALSE(CacheContains(form_id1));
  EXPECT_FALSE(CacheContains(form_id2));
  std::move(run_loop).Run();
  EXPECT_TRUE(CacheContains(form_id1));
  EXPECT_FALSE(CacheContains(form_id2));
  EXPECT_EQ(GetCacheSize(), kAutofillManagerMaxFormCacheSize);
}

TEST_F(AutofillManagerTest, CanShowAutofillUi) {
  EXPECT_CALL(autofill_driver(), CanShowAutofillUi).WillOnce(Return(true));
  EXPECT_TRUE(autofill_manager().CanShowAutofillUi());
}

TEST_F(AutofillManagerTest, TriggerFormExtractionInAllFrames) {
  EXPECT_CALL(autofill_driver(), TriggerFormExtractionInAllFrames);
  autofill_manager().TriggerFormExtractionInAllFrames(base::DoNothing());
}

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
// Ensure that `FieldClassificationModelHandler`s are called when parsing the
// form in `ParseFormsAsync()`. These tests intentionally don't associate
// predictions to the `FormStructure`, they only verify that
// `GetModelPredictionsForForms` gets called.
class AutofillManagerTestForModelPredictions : public AutofillManagerTest {
 public:
  void SetUp() override {
    AutofillManagerTest::SetUp();
    observation_.Observe(&autofill_manager());
  }

  void TearDown() override {
    // The handler owns the model executor, which operates on a background
    // thread and is destroyed asynchronously. Resetting handlers owned by the
    // client triggers their destruction. Wait for it to complete. This needs to
    // happen before the `provider` is destroyed.
    observation_.Reset();
    autofill_client().set_autofill_ml_prediction_model_handler(nullptr);
    autofill_client().set_password_ml_prediction_model_handler(nullptr);
    task_environment_.RunUntilIdle();
    AutofillManagerTest::TearDown();
  }

  // Creates a model handler that is just runs the final callback upon receiving
  // forms that need to be parsed.
  std::unique_ptr<MockFieldClassificationModelHandler>
  CreateMockModelHandler() {
    auto handler =
        std::make_unique<MockFieldClassificationModelHandler>(&model_provider_);
    ON_CALL(*handler, GetModelPredictionsForForms)
        .WillByDefault(
            [](std::vector<FormData> forms,
               const GeoIpCountryCode& client_country, bool ignore_small_forms,
               base::OnceCallback<void(std::vector<ModelPredictions>)>
                   callback) {
              const ModelPredictions kEmptyPredictions = ModelPredictions(
                  HeuristicSource::kAutofillMachineLearning, {}, {});
              std::move(callback).Run(std::vector<ModelPredictions>(
                  forms.size(), kEmptyPredictions));
            });
    return handler;
  }

  MockAutofillManagerObserver& observer() { return observer_; }

 private:
  optimization_guide::TestOptimizationGuideModelProvider model_provider_;
  MockAutofillManagerObserver observer_;
  base::ScopedObservation<AutofillManager, MockAutofillManagerObserver>
      observation_{&observer_};
};

TEST_F(AutofillManagerTestForModelPredictions,
       GetMlModelPredictionsForForm_AutofillOnly) {
  autofill_client().set_autofill_ml_prediction_model_handler(
      CreateMockModelHandler());

  EXPECT_CALL(
      static_cast<MockFieldClassificationModelHandler&>(
          *autofill_client().GetAutofillFieldClassificationModelHandler()),
      GetModelPredictionsForForms);
  // Check that AutofillManager's observer is notified after parsing using
  // models is done.
  EXPECT_CALL(observer(), OnFieldTypesDetermined);
  FormData form = test::CreateTestAddressFormData();
  OnFormsSeenWithExpectations(autofill_manager(), /*updated_forms=*/{form},
                              /*removed_forms=*/{}, /*expectation=*/{form});
}

TEST_F(AutofillManagerTestForModelPredictions,
       GetMlModelPredictionsForForm_PasswordManagerOnly) {
  autofill_client().set_password_ml_prediction_model_handler(
      CreateMockModelHandler());

  EXPECT_CALL(static_cast<MockFieldClassificationModelHandler&>(
                  *autofill_client()
                       .GetPasswordManagerFieldClassificationModelHandler()),
              GetModelPredictionsForForms);
  // Check that AutofillManager's observer is notified after parsing using
  // models is done.
  EXPECT_CALL(observer(), OnFieldTypesDetermined);
  FormData form = test::CreateTestAddressFormData();
  OnFormsSeenWithExpectations(autofill_manager(), /*updated_forms=*/{form},
                              /*removed_forms=*/{}, /*expectation=*/{form});
}

TEST_F(AutofillManagerTestForModelPredictions,
       GetMlModelPredictionsForForm_BothAutofillAndPasswordManager) {
  autofill_client().set_autofill_ml_prediction_model_handler(
      CreateMockModelHandler());
  autofill_client().set_password_ml_prediction_model_handler(
      CreateMockModelHandler());

  EXPECT_CALL(
      static_cast<MockFieldClassificationModelHandler&>(
          *autofill_client().GetAutofillFieldClassificationModelHandler()),
      GetModelPredictionsForForms);
  EXPECT_CALL(static_cast<MockFieldClassificationModelHandler&>(
                  *autofill_client()
                       .GetPasswordManagerFieldClassificationModelHandler()),
              GetModelPredictionsForForms);
  // Check that AutofillManager's observer is notified after parsing using
  // models is done.
  EXPECT_CALL(observer(), OnFieldTypesDetermined);
  FormData form = test::CreateTestAddressFormData();
  OnFormsSeenWithExpectations(autofill_manager(), /*updated_forms=*/{form},
                              /*removed_forms=*/{}, /*expectation=*/{form});
}
#endif

TEST_F(
    AutofillManagerTest_OnLoadedServerPredictionsObserver,
    OnFormsSeen_FailedQueryRequest_NotifiesBothLoadedServerPredictionsObservers) {
  std::vector<FormData> forms = CreateTestForms(1);
  base::RunLoop run_loop;
  EXPECT_CALL(observer_,
              OnBeforeLoadedServerPredictions(Ref(autofill_manager())));
  EXPECT_CALL(observer_, OnFieldTypesDetermined).Times(0);
  EXPECT_CALL(observer_, OnFieldTypesDetermined(
                             Ref(autofill_manager()), forms[0].global_id(),
                             FieldTypeSource::kHeuristicsOrAutocomplete,
                             /*small_forms_were_parsed=*/
                             Eq(autofill_client().IsTabInActorMode())));
  EXPECT_CALL(crowdsourcing_manager(), StartQueryRequest)
      .WillOnce(
          [](const auto&, const auto&,
             base::OnceCallback<void(std::optional<QueryResponse>)> callback) {
            std::move(callback).Run(std::nullopt);
            return true;
          });
  EXPECT_CALL(observer_,
              OnAfterLoadedServerPredictions(Ref(autofill_manager())))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  OnFormsSeenWithExpectations(autofill_manager(), forms, {}, forms);
  std::move(run_loop).Run();
}

TEST_F(AutofillManagerTest_OnLoadedServerPredictionsObserver, TabInActorMode) {
  std::vector<FormData> forms = CreateTestForms(1);
  autofill_client().set_is_tab_in_actor_mode(true);

  base::RunLoop run_loop;
  EXPECT_CALL(observer_, OnFieldTypesDetermined(
                             Ref(autofill_manager()), forms[0].global_id(),
                             FieldTypeSource::kHeuristicsOrAutocomplete,
                             /*small_forms_were_parsed=*/true));
  EXPECT_CALL(observer_,
              OnAfterLoadedServerPredictions(Ref(autofill_manager())))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  OnFormsSeenWithExpectations(autofill_manager(), forms, {}, forms);
  std::move(run_loop).Run();
}

TEST_F(
    AutofillManagerTest_OnLoadedServerPredictionsObserver,
    OnFormsSeen_NonEmptyQueriedFormSignatures_NotifiesAfterLoadedServerPredictionsObserver) {
  std::vector<FormData> forms = CreateTestForms(1);
  base::RunLoop run_loop;
  EXPECT_CALL(observer_,
              OnBeforeLoadedServerPredictions(Ref(autofill_manager())));
  EXPECT_CALL(observer_, OnFieldTypesDetermined).Times(0);
  EXPECT_CALL(observer_, OnFieldTypesDetermined(
                             Ref(autofill_manager()), forms[0].global_id(),
                             FieldTypeSource::kHeuristicsOrAutocomplete,
                             /*small_forms_were_parsed=*/
                             Eq(autofill_client().IsTabInActorMode())));
  EXPECT_CALL(observer_,
              OnFieldTypesDetermined(
                  Ref(autofill_manager()), forms[0].global_id(),
                  FieldTypeSource::kAutofillServer, /*small_forms_were_parsed=*/
                  Eq(autofill_client().IsTabInActorMode())));
  EXPECT_CALL(crowdsourcing_manager(), StartQueryRequest)
      .WillOnce(
          [&](const auto&, const auto&,
              base::OnceCallback<void(std::optional<QueryResponse>)> callback) {
            std::move(callback).Run(
                // Server responses always have non-empty form signatures.
                QueryResponse("", {autofill_manager()
                                       .FindCachedFormById(forms[0].global_id())
                                       ->form_signature()}));
            return true;
          });
  EXPECT_CALL(observer_,
              OnAfterLoadedServerPredictions(Ref(autofill_manager())))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  OnFormsSeenWithExpectations(autofill_manager(), forms, {}, forms);
  std::move(run_loop).Run();
}

TEST_F(AutofillManagerTest, GetHeuristicPredictionForForm) {
  FormData seen_form = test::CreateTestAddressFormData();
  OnFormsSeenWithExpectations(autofill_manager(), {seen_form}, {}, {seen_form});

  FormData unseen_form = test::CreateTestAddressFormData();
  ASSERT_NE(seen_form.global_id(), unseen_form.global_id());

  // Check that predictions are returned for the form that was seen.
  EXPECT_EQ(autofill_manager()
                .GetHeuristicPredictionForForm(
                    autofill::HeuristicSource::kPasswordManagerMachineLearning,
                    seen_form.global_id(),
                    base::ToVector(seen_form.fields(),
                                   &autofill::FormFieldData::global_id))
                .size(),
            seen_form.fields().size());

  // Check that no predictions are returned for the unseen form.
  EXPECT_TRUE(
      autofill_manager()
          .GetHeuristicPredictionForForm(
              autofill::HeuristicSource::kPasswordManagerMachineLearning,
              unseen_form.global_id(),
              base::ToVector(unseen_form.fields(),
                             &autofill::FormFieldData::global_id))
          .empty());
}

// Tests that QueryServerPredictions() starts a query request for the forms
// that should be queried and that the resulting call to
// OnLoadedServerPredictions() adds a new form structure to the cache.
TEST_F(AutofillManagerTest,
       EarlyServerPredictions_QueryServerPredictions_StartQueryRequestCalled) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillServerQueryPredictionsEarly};

  std::vector<FormData> forms = CreateTestForms(1);
  std::vector<FormSignature> form_signatures = {
      CalculateFormSignature(forms[0])};

  EXPECT_CALL(crowdsourcing_manager(), StartQueryRequest)
      .WillOnce(
          [form_signatures](
              const std::vector<FormData>&, std::optional<net::IsolationInfo>,
              base::OnceCallback<void(
                  std::optional<AutofillCrowdsourcingManager::QueryResponse>)>
                  callback) {
            std::move(callback).Run(AutofillCrowdsourcingManager::QueryResponse(
                "", form_signatures));
            return true;
          });

  EXPECT_CALL(autofill_manager(), OnFormProcessed);

  test_api(autofill_manager()).QueryServerPredictions(forms);
  EXPECT_EQ(test_api(autofill_manager()).form_structures().size(), 1u);
}

// Tests that QueryServerPredictions() starts a query request for the forms
// that should be queried and that the resulting call to
// OnLoadedServerPredictions() updates the existing form structure with the
// same form ID but different form signature and lower version.
TEST_F(AutofillManagerTest,
       EarlyServerPredictions_QueryServerPredictions_UpdatesExistingFormId) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillServerQueryPredictionsEarly};

  // Create a form and add it to the form cache via OnFormsSeen().
  EXPECT_CALL(autofill_manager(), ShouldParseForms).WillOnce(Return(true));
  TestAutofillManagerWaiter waiter(autofill_manager());
  FormData initial_form = test::CreateTestAddressFormData();
  autofill_manager().OnFormsSeen({initial_form}, /*removed_forms=*/{});
  ASSERT_TRUE(waiter.Wait());
  ASSERT_EQ(test_api(autofill_manager()).form_structures().size(), 1u);

  // Create a form with the same form ID but different form signature and higher
  // version for server predictions.
  FormData updated_form = initial_form;
  updated_form.set_action(GURL("https://example.test/"));
  updated_form.set_version(autofill::FormVersion::FromUnsafeValue(
      initial_form.version().value() + 1));
  std::vector<FormSignature> form_signatures = {
      CalculateFormSignature(updated_form)};
  ASSERT_NE(test_api(autofill_manager()).form_structures()[0]->form_signature(),
            form_signatures[0]);
  ASSERT_EQ(test_api(autofill_manager()).form_structures()[0]->global_id(),
            updated_form.global_id());

  EXPECT_CALL(crowdsourcing_manager(), StartQueryRequest)
      .WillOnce(
          [form_signatures](
              const std::vector<FormData>&, std::optional<net::IsolationInfo>,
              base::OnceCallback<void(
                  std::optional<AutofillCrowdsourcingManager::QueryResponse>)>
                  callback) {
            std::move(callback).Run(AutofillCrowdsourcingManager::QueryResponse(
                "", form_signatures));
            return true;
          });

  // Expect updated form to be stored in the form cache.
  test_api(autofill_manager()).QueryServerPredictions({updated_form});
  ASSERT_EQ(test_api(autofill_manager()).form_structures().size(), 1u);
  EXPECT_EQ(test_api(autofill_manager()).form_structures()[0]->form_signature(),
            form_signatures[0]);
  EXPECT_EQ(test_api(autofill_manager()).form_structures()[0]->version(),
            updated_form.version());
}

// Tests that QueryServerPredictions() starts a query request for the forms
// that should be queried and that the resulting call to
// OnLoadedServerPredictions() updates the existing form structure
// with the same form ID but same form signature and lower version.
TEST_F(
    AutofillManagerTest,
    EarlyServerPredictions_QueryServerPredictions_UpdatesExistingFormIdWithSameSignature) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillServerQueryPredictionsEarly};

  // Create a form and add it to the form cache via OnFormsSeen().
  EXPECT_CALL(autofill_manager(), ShouldParseForms).WillOnce(Return(true));
  TestAutofillManagerWaiter waiter(autofill_manager());
  FormData initial_form = test::CreateTestAddressFormData();
  autofill_manager().OnFormsSeen({initial_form}, /*removed_forms=*/{});
  ASSERT_TRUE(waiter.Wait());
  ASSERT_EQ(test_api(autofill_manager()).form_structures().size(), 1u);
  const FormStructure& form_structure = CHECK_DEREF(
      autofill_manager().FindCachedFormById(initial_form.global_id()));
  ASSERT_EQ(form_structure.field_count(), 11u);
  ASSERT_EQ(form_structure.fields()[0]->heuristic_type(), NAME_FIRST);
  ASSERT_EQ(form_structure.fields()[1]->heuristic_type(), NAME_MIDDLE);

  // Create a form with the same form ID but same form signature and higher
  // version for server predictions.
  FormData updated_form = initial_form;
  updated_form.set_version(autofill::FormVersion::FromUnsafeValue(
      initial_form.version().value() + 1));
  std::vector<FormSignature> form_signatures = {
      CalculateFormSignature(updated_form)};
  ASSERT_EQ(test_api(autofill_manager()).form_structures()[0]->form_signature(),
            form_signatures[0]);
  ASSERT_EQ(test_api(autofill_manager()).form_structures()[0]->global_id(),
            updated_form.global_id());

  EXPECT_CALL(crowdsourcing_manager(), StartQueryRequest)
      .WillOnce(
          [form_signatures](
              const std::vector<FormData>&, std::optional<net::IsolationInfo>,
              base::OnceCallback<void(
                  std::optional<AutofillCrowdsourcingManager::QueryResponse>)>
                  callback) {
            std::move(callback).Run(AutofillCrowdsourcingManager::QueryResponse(
                "", form_signatures));
            return true;
          });

  // The cached form should still be in the cache, predictions should be applied
  // and other attributes (like heuristic predictions) should not be overridden.
  test_api(autofill_manager()).QueryServerPredictions({updated_form});
  ASSERT_EQ(test_api(autofill_manager()).form_structures().size(), 1u);
  EXPECT_EQ(form_structure.form_signature(), form_signatures[0]);
  EXPECT_EQ(form_structure.version(), updated_form.version());
  ASSERT_EQ(form_structure.field_count(), 11u);
  EXPECT_EQ(form_structure.fields()[0]->heuristic_type(), NAME_FIRST);
  EXPECT_EQ(form_structure.fields()[1]->heuristic_type(), NAME_MIDDLE);
}

// Tests that QueryServerPredictions() starts a query request for the forms
// that should be queried and that the resulting call to
// OnLoadedServerPredictions() does not override the existing form structure
// with the same form ID but different form signature and higher version.
TEST_F(
    AutofillManagerTest,
    EarlyServerPredictions_QueryServerPredictions_DoesNotOverridesExistingFormIdWithHigherVersion) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillServerQueryPredictionsEarly};

  // Create a form and add it to the form cache via OnFormsSeen().
  EXPECT_CALL(autofill_manager(), ShouldParseForms).WillOnce(Return(true));
  TestAutofillManagerWaiter waiter(autofill_manager());
  FormData initial_form = test::CreateTestAddressFormData();
  initial_form.set_version(autofill::FormVersion::FromUnsafeValue(1));
  autofill_manager().OnFormsSeen({initial_form}, /*removed_forms=*/{});
  ASSERT_TRUE(waiter.Wait());
  ASSERT_EQ(test_api(autofill_manager()).form_structures().size(), 1u);

  // Create a form with the same form ID but different form signature and lower
  // version for server predictions.
  FormData previous_form = initial_form;
  previous_form.set_action(GURL("https://example.test/"));
  previous_form.set_version(autofill::FormVersion::FromUnsafeValue(
      initial_form.version().value() - 1));
  std::vector<FormSignature> form_signatures = {
      CalculateFormSignature(previous_form)};
  ASSERT_NE(test_api(autofill_manager()).form_structures()[0]->form_signature(),
            form_signatures[0]);
  ASSERT_EQ(test_api(autofill_manager()).form_structures()[0]->global_id(),
            previous_form.global_id());

  EXPECT_CALL(crowdsourcing_manager(), StartQueryRequest)
      .WillOnce(
          [form_signatures](
              const std::vector<FormData>&, std::optional<net::IsolationInfo>,
              base::OnceCallback<void(
                  std::optional<AutofillCrowdsourcingManager::QueryResponse>)>
                  callback) {
            std::move(callback).Run(AutofillCrowdsourcingManager::QueryResponse(
                "", form_signatures));
            return true;
          });

  // Expect server predictions to still be processed for the previous form.
  EXPECT_CALL(autofill_manager(), OnFormProcessed);
  test_api(autofill_manager()).QueryServerPredictions({previous_form});
  ASSERT_EQ(test_api(autofill_manager()).form_structures().size(), 1u);
  EXPECT_EQ(test_api(autofill_manager()).form_structures()[0]->form_signature(),
            CalculateFormSignature(initial_form));
}

// Tests that OnLoadedServerPredictions() does not add forms to the cache if
// their signature is not part of the query response.
TEST_F(AutofillManagerTest,
       EarlyServerPredictions_QueryServerPredictions_MissingSignature) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillServerQueryPredictionsEarly};

  std::vector<FormData> forms = {
      test::CreateTestAddressFormData(/*unique_id=*/"0"),
      test::CreateTestAddressFormData(/*unique_id=*/"1")};
  std::vector<FormSignature> form_signatures = {
      CalculateFormSignature(forms[0])};

  EXPECT_CALL(crowdsourcing_manager(), StartQueryRequest)
      .WillOnce(
          [form_signatures](
              const std::vector<FormData>&, std::optional<net::IsolationInfo>,
              base::OnceCallback<void(
                  std::optional<AutofillCrowdsourcingManager::QueryResponse>)>
                  callback) {
            std::move(callback).Run(AutofillCrowdsourcingManager::QueryResponse(
                "", form_signatures));
            return true;
          });

  EXPECT_CALL(autofill_manager(), OnFormProcessed);

  test_api(autofill_manager()).QueryServerPredictions(forms);
  EXPECT_EQ(test_api(autofill_manager()).form_structures().size(), 1u);
}

// Tests that OnLoadedServerPredictions() respects the form cache size limit
// when adding new forms to the cache.
TEST_F(
    AutofillManagerTest,
    EarlyServerPredictions_OnLoadedServerPredictions_QueryServerPredictions_RespectsCacheSize) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillServerQueryPredictionsEarly};

  std::vector<FormData> forms;
  for (size_t i = 0; i < kAutofillManagerMaxFormCacheSize + 1; ++i) {
    forms.push_back(test::CreateTestAddressFormData(
        base::NumberToString(static_cast<int>(i))));
  }
  std::vector<FormSignature> form_signatures =
      base::ToVector(forms, &CalculateFormSignature);

  EXPECT_CALL(crowdsourcing_manager(), StartQueryRequest)
      .WillOnce(
          [form_signatures](
              const std::vector<FormData>&, std::optional<net::IsolationInfo>,
              base::OnceCallback<void(
                  std::optional<AutofillCrowdsourcingManager::QueryResponse>)>
                  callback) {
            std::move(callback).Run(AutofillCrowdsourcingManager::QueryResponse(
                "", form_signatures));
            return true;
          });

  EXPECT_CALL(autofill_manager(), OnFormProcessed)
      .Times(kAutofillManagerMaxFormCacheSize);

  test_api(autofill_manager()).QueryServerPredictions(forms);
  EXPECT_EQ(test_api(autofill_manager()).form_structures().size(),
            kAutofillManagerMaxFormCacheSize);
}

}  // namespace
}  // namespace autofill
