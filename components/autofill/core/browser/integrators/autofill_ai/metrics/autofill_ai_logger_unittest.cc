// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/autofill_ai/metrics/autofill_ai_logger.h"

#include <memory>
#include <optional>
#include <tuple>

#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_manager.h"
#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_manager_test_api.h"
#include "components/autofill/core/browser/integrators/autofill_ai/metrics/autofill_ai_ukm_logger.h"
#include "components/autofill/core/browser/strike_databases/payments/test_strike_database.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_test_helper.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_quality/test_model_quality_logs_uploader_service.h"
#include "components/optimization_guide/proto/features/forms_classifications.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace autofill {

namespace {

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

constexpr auto kVehicle = EntityType(EntityTypeName::kVehicle);
constexpr auto kDriversLicense = EntityType(EntityTypeName::kDriversLicense);
constexpr auto kPassport = EntityType(EntityTypeName::kPassport);
constexpr auto kNationalIdCard = EntityType(EntityTypeName::kNationalIdCard);
constexpr char kDefaultUrl[] = "https://example.com";

class MockAutofillClient : public TestAutofillClient {
 public:
  MockAutofillClient() = default;
  MockAutofillClient(const MockAutofillClient&) = delete;
  MockAutofillClient& operator=(const MockAutofillClient&) = delete;
  ~MockAutofillClient() override = default;

  MOCK_METHOD(optimization_guide::ModelQualityLogsUploaderService*,
              GetMqlsUploadService,
              (),
              (override));
};

class BaseAutofillAiTest : public testing::Test {
 public:
  BaseAutofillAiTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kAutofillAiWithDataSchema,
                              features::kAutofillAiNationalIdCard,
                              features::kAutofillAiKnownTravelerNumber,
                              features::kAutofillAiRedressNumber},
        /*disabled_features=*/{});
    autofill_client().set_entity_data_manager(
        std::make_unique<EntityDataManager>(
            webdata_helper_.autofill_webdata_service(),
            /*history_service=*/nullptr,
            /*strike_database=*/nullptr));
    manager_ = std::make_unique<AutofillAiManager>(&autofill_client_,
                                                   &strike_database_);
    autofill_client().SetUpPrefsAndIdentityForAutofillAi();
  }

  AutofillAiManager& manager() { return *manager_; }
  std::unique_ptr<AutofillAiManager>& manager_ptr() { return manager_; }

  void AddOrUpdateEntityInstance(EntityInstance entity) {
    autofill_client().GetEntityDataManager()->AddOrUpdateEntityInstance(
        std::move(entity));
    webdata_helper_.WaitUntilIdle();
  }

  [[nodiscard]] std::unique_ptr<FormStructure> CreateFormStructure(
      const std::vector<FieldType>& field_types_predictions,
      std::string url = std::string(kDefaultUrl)) {
    test::FormDescription form_description{.url = std::move(url)};
    for (FieldType field_type : field_types_predictions) {
      form_description.fields.emplace_back(
          test::FieldDescription({.role = field_type}));
    }
    FormData form_data = test::GetFormData(form_description);
    form_data.set_main_frame_origin(
        url::Origin::Create(GURL("https://myform_root.com/form.html")));
    auto form_structure = std::make_unique<FormStructure>(form_data);
    for (size_t i = 0; i < form_structure->field_count(); i++) {
      AutofillQueryResponse::FormSuggestion::FieldSuggestion::FieldPrediction
          prediction;
      prediction.set_type(form_description.fields[i].role);
      form_structure->field(i)->set_server_predictions({prediction});
    }
    return form_structure;
  }

  [[nodiscard]] std::unique_ptr<FormStructure> CreatePassportForm(
      std::string url = std::string(kDefaultUrl)) {
    std::unique_ptr<FormStructure> form = CreateFormStructure(
        {PASSPORT_NAME_TAG, PASSPORT_NUMBER, PHONE_HOME_WHOLE_NUMBER},
        std::move(url));
    return form;
  }

  [[nodiscard]] std::unique_ptr<FormStructure> CreateVehicleForm(
      std::string url = std::string(kDefaultUrl)) {
    std::unique_ptr<FormStructure> form = CreateFormStructure(
        {VEHICLE_OWNER_TAG, VEHICLE_LICENSE_PLATE}, std::move(url));
    return form;
  }

  [[nodiscard]] std::unique_ptr<FormStructure> CreateDriversLicenseForm(
      std::string url = std::string(kDefaultUrl)) {
    std::unique_ptr<FormStructure> form =
        CreateFormStructure({DRIVERS_LICENSE_NAME_TAG, DRIVERS_LICENSE_NUMBER,
                             DRIVERS_LICENSE_REGION, DRIVERS_LICENSE_ISSUE_DATE,
                             DRIVERS_LICENSE_EXPIRATION_DATE},
                            std::move(url));
    return form;
  }

  [[nodiscard]] std::unique_ptr<FormStructure> CreateKnownTravelerNumberForm(
      std::string url = std::string(kDefaultUrl)) {
    std::unique_ptr<FormStructure> form = CreateFormStructure(
        {KNOWN_TRAVELER_NUMBER, KNOWN_TRAVELER_NUMBER_EXPIRATION_DATE},
        std::move(url));
    return form;
  }

  [[nodiscard]] std::unique_ptr<FormStructure> CreateRedressNumberForm(
      std::string url = std::string(kDefaultUrl)) {
    std::unique_ptr<FormStructure> form =
        CreateFormStructure({REDRESS_NUMBER}, std::move(url));
    return form;
  }

  [[nodiscard]] std::unique_ptr<FormStructure> CreateNationalIdCardForm(
      std::string url = std::string(kDefaultUrl)) {
    std::unique_ptr<FormStructure> form = CreateFormStructure(
        {NATIONAL_ID_CARD_NUMBER, NATIONAL_ID_CARD_ISSUING_COUNTRY,
         NATIONAL_ID_CARD_ISSUE_DATE, NATIONAL_ID_CARD_EXPIRATION_DATE},
        std::move(url));
    return form;
  }

  std::unique_ptr<FormStructure> CreateIneligibleForm() {
    FormData form_data;
    auto form = std::make_unique<FormStructure>(form_data);
    AutofillField& prediction_improvement_field = test_api(*form).PushField();
    prediction_improvement_field.SetTypeTo(
        AutofillType(CREDIT_CARD_NUMBER),
        AutofillPredictionSource::kHeuristics);
    return form;
  }

  MockAutofillClient& autofill_client() { return autofill_client_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  test::AutofillUnitTestEnvironment autofill_test_env_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  NiceMock<MockAutofillClient> autofill_client_;
  std::unique_ptr<AutofillAiManager> manager_;
  TestStrikeDatabase strike_database_;
  AutofillWebDataServiceTestHelper webdata_helper_{
      std::make_unique<EntityTable>()};
};

// Tests the recording of the number of filled fields at form submission.
TEST_F(BaseAutofillAiTest, NumberOfFilledFields) {
  std::unique_ptr<FormStructure> form = CreatePassportForm();

  form->field(0)->set_is_autofilled(true);
  form->field(0)->set_filling_product(FillingProduct::kAddress);
  form->field(1)->set_is_autofilled(true);
  form->field(1)->set_filling_product(FillingProduct::kAutocomplete);
  {
    manager().OnFormSeen(*form);
    base::HistogramTester histogram_tester;
    manager().OnFormSubmitted(*form, /*ukm_source_id=*/{});

    // Only one field should be recorded, since Autocomplete is excluded from
    // the counts.
    histogram_tester.ExpectUniqueSample(
        "Autofill.Ai.NumberOfFilledFields.Total.OptedIn", 1, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.Ai.NumberOfFilledFields.Total.NoDataToFill", 1, 1);
  }
  {
    AddOrUpdateEntityInstance(test::GetPassportEntityInstance());
    manager().OnFormSeen(*form);
    form->field(2)->set_is_autofilled(true);
    form->field(2)->set_filling_product(FillingProduct::kAutofillAi);
    base::HistogramTester histogram_tester;
    manager().OnFormSubmitted(*form, /*ukm_source_id=*/{});
    histogram_tester.ExpectUniqueSample(
        "Autofill.Ai.NumberOfFilledFields.Total.OptedIn", 2, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.Ai.NumberOfFilledFields.Total.HasDataToFill", 2, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.Ai.NumberOfFilledFields.AutofillAi.OptedIn", 1, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.Ai.NumberOfFilledFields.AutofillAi.HasDataToFill", 1, 1);
  }
}

// Test that the funnel metrics are logged correctly given different scenarios.
// This test is parameterized by a boolean representing whether the form was
// submitted or abandoned, an `EntityType` representing the type of funnel we're
// testing, and an integer representing the last stage of the funnel that was
// reached:
//
// 0) A form was loaded
// 1) The form was detected eligible for AutofillAi.
// 2) The user had data stored to fill the loaded form.
// 3) The user saw filling suggestions.
// 4) The user accepted a filling suggestion.
// 5) The user corrected the filled suggestion.
class AutofillAiFunnelMetricsTest
    : public BaseAutofillAiTest,
      public testing::WithParamInterface<std::tuple<bool, EntityType, int>> {
  static constexpr char kFunnelUmaMask[] = "Autofill.Ai.Funnel.%s.%s%s";

 public:
  AutofillAiFunnelMetricsTest() = default;

  bool submitted() { return std::get<0>(GetParam()); }
  EntityType entity_type() { return std::get<1>(GetParam()); }
  bool is_form_eligible() { return std::get<2>(GetParam()) > 0; }
  bool user_has_data() { return std::get<2>(GetParam()) > 1; }
  bool user_saw_suggestions() { return std::get<2>(GetParam()) > 2; }
  bool user_filled_suggestion() { return std::get<2>(GetParam()) > 3; }
  bool user_corrected_filling() { return std::get<2>(GetParam()) > 4; }

  std::unique_ptr<FormStructure> CreateForm() {
    switch (entity_type().name()) {
      case EntityTypeName::kPassport:
        return CreatePassportForm();
      case EntityTypeName::kDriversLicense:
        return CreateDriversLicenseForm();
      case EntityTypeName::kKnownTravelerNumber:
        return CreateKnownTravelerNumberForm();
      case EntityTypeName::kRedressNumber:
        return CreateRedressNumberForm();
      case EntityTypeName::kVehicle:
        return CreateVehicleForm();
      case EntityTypeName::kNationalIdCard:
        return CreateNationalIdCardForm();
    }
    NOTREACHED();
  }

  EntityInstance CreateEntity() {
    switch (entity_type().name()) {
      case EntityTypeName::kPassport:
        return test::GetPassportEntityInstance();
      case EntityTypeName::kDriversLicense:
        return test::GetDriversLicenseEntityInstance();
      case EntityTypeName::kKnownTravelerNumber:
        return test::GetKnownTravelerNumberInstance();
      case EntityTypeName::kRedressNumber:
        return test::GetRedressNumberEntityInstance();
      case EntityTypeName::kVehicle:
        return test::GetVehicleEntityInstance();
      case EntityTypeName::kNationalIdCard:
        return test::GetNationalIdCardEntityInstance();
    }
    NOTREACHED();
  }

  void ExpectCorrectFunnelRecording(
      const base::HistogramTester& histogram_tester) {
    // Expect that we do not record any sample for the submission-specific
    // histograms that are not applicable.
    histogram_tester.ExpectTotalCount(GetEligibilityHistogram(!submitted()), 0);
    histogram_tester.ExpectTotalCount(
        GetReadinessAfterEligibilityHistogram(!submitted()), 0);
    histogram_tester.ExpectTotalCount(
        GetFillAfterSuggestionHistogram(!submitted()), 0);
    histogram_tester.ExpectTotalCount(
        GetCorrectionAfterFillHistogram(!submitted()), 0);

    // Expect that the aggregate and appropriate submission-specific histograms
    // record the correct values.
    if (is_form_eligible()) {
      histogram_tester.ExpectUniqueSample(GetEligibilityHistogram(),
                                          entity_type().name(), 1);
      histogram_tester.ExpectUniqueSample(GetEligibilityHistogram(submitted()),
                                          entity_type().name(), 1);
    } else {
      histogram_tester.ExpectTotalCount(GetEligibilityHistogram(), 0);
      histogram_tester.ExpectTotalCount(GetEligibilityHistogram(submitted()),
                                        0);
    }

    if (is_form_eligible()) {
      histogram_tester.ExpectUniqueSample(
          GetReadinessAfterEligibilityHistogram(), user_has_data(), 1);
      histogram_tester.ExpectUniqueSample(
          GetReadinessAfterEligibilityHistogram(submitted()), user_has_data(),
          1);
    } else {
      histogram_tester.ExpectTotalCount(GetReadinessAfterEligibilityHistogram(),
                                        0);
      histogram_tester.ExpectTotalCount(
          GetReadinessAfterEligibilityHistogram(submitted()), 0);
    }

    if (user_has_data()) {
      histogram_tester.ExpectUniqueSample(
          GetSuggestionAfterReadinessHistogram(), user_saw_suggestions(), 1);
      histogram_tester.ExpectUniqueSample(
          GetSuggestionAfterReadinessHistogram(submitted()),
          user_saw_suggestions(), 1);
    } else {
      histogram_tester.ExpectTotalCount(GetSuggestionAfterReadinessHistogram(),
                                        0);
      histogram_tester.ExpectTotalCount(
          GetSuggestionAfterReadinessHistogram(submitted()), 0);
    }

    if (user_saw_suggestions()) {
      histogram_tester.ExpectUniqueSample(GetFillAfterSuggestionHistogram(),
                                          user_filled_suggestion(), 1);
      histogram_tester.ExpectUniqueSample(
          GetFillAfterSuggestionHistogram(submitted()),
          user_filled_suggestion(), 1);
    } else {
      histogram_tester.ExpectTotalCount(GetFillAfterSuggestionHistogram(), 0);
      histogram_tester.ExpectTotalCount(
          GetFillAfterSuggestionHistogram(submitted()), 0);
    }

    if (user_filled_suggestion()) {
      histogram_tester.ExpectUniqueSample(GetCorrectionAfterFillHistogram(),
                                          user_corrected_filling(), 1);
      histogram_tester.ExpectUniqueSample(
          GetCorrectionAfterFillHistogram(submitted()),
          user_corrected_filling(), 1);
    } else {
      histogram_tester.ExpectTotalCount(GetCorrectionAfterFillHistogram(), 0);
      histogram_tester.ExpectTotalCount(
          GetCorrectionAfterFillHistogram(submitted()), 0);
    }
  }

 private:
  std::string_view GetEntityTypeAsString() {
    switch (entity_type().name()) {
      case EntityTypeName::kPassport:
        return "Passport";
      case EntityTypeName::kDriversLicense:
        return "DriversLicense";
      case EntityTypeName::kKnownTravelerNumber:
        return "KnownTravelerNumber";
      case EntityTypeName::kRedressNumber:
        return "RedressNumber";
      case EntityTypeName::kVehicle:
        return "Vehicle";
      case EntityTypeName::kNationalIdCard:
        return "NationalIdCard";
    }
    NOTREACHED();
  }

  std::string GetFunnelHistogram(std::string_view funnel_state,
                                 std::optional<bool> submitted,
                                 std::optional<std::string_view> entity_type) {
    std::string_view submission_state = "Aggregate";
    if (submitted) {
      submission_state = *submitted ? "Submitted" : "Abandoned";
    }
    return base::StringPrintf(
        kFunnelUmaMask, submission_state, funnel_state,
        entity_type ? std::string(".") + std::string(*entity_type) : "");
  }

  std::string GetEligibilityHistogram(
      std::optional<bool> submitted = std::nullopt) {
    return GetFunnelHistogram("Eligibility2", submitted,
                              /*entity_type=*/std::nullopt);
  }

  std::string GetReadinessAfterEligibilityHistogram(
      std::optional<bool> submitted = std::nullopt) {
    return GetFunnelHistogram("ReadinessAfterEligibility", submitted,
                              GetEntityTypeAsString());
  }

  std::string GetSuggestionAfterReadinessHistogram(
      std::optional<bool> submitted = std::nullopt) {
    return GetFunnelHistogram("SuggestionAfterReadiness", submitted,
                              GetEntityTypeAsString());
  }

  std::string GetFillAfterSuggestionHistogram(
      std::optional<bool> submitted = std::nullopt) {
    return GetFunnelHistogram("FillAfterSuggestion", submitted,
                              GetEntityTypeAsString());
  }

  std::string GetCorrectionAfterFillHistogram(
      std::optional<bool> submitted = std::nullopt) {
    return GetFunnelHistogram("CorrectionAfterFill", submitted,
                              GetEntityTypeAsString());
  }
};

INSTANTIATE_TEST_SUITE_P(AutofillAiTest,
                         AutofillAiFunnelMetricsTest,
                         testing::Combine(testing::Bool(),
                                          testing::Values(kPassport,
                                                          kDriversLicense,
                                                          kVehicle,
                                                          kNationalIdCard),
                                          testing::Values(0, 1, 2, 3, 4, 5)));

// Tests that appropriate calls in `AutofillAiManager`
// result in correct metric logging.
TEST_P(AutofillAiFunnelMetricsTest, Manager) {
  // This will dictate whether the form will be eligible for filling or not.
  std::unique_ptr<FormStructure> form =
      is_form_eligible() ? CreateForm() : CreateIneligibleForm();
  // This will dictate whether we consider the form ready to be filled or not.
  EntityInstance entity = CreateEntity();
  if (user_has_data()) {
    AddOrUpdateEntityInstance(entity);
  }
  manager().OnFormSeen(*form);

  if (user_saw_suggestions()) {
    manager().OnSuggestionsShown(*form, *form->field(0), {entity_type()},
                                 /*ukm_source_id=*/{});
  }
  if (user_filled_suggestion()) {
    manager().OnDidFillSuggestion(entity, *form, *form->field(0),
                                  {form->field(0)},
                                  /*ukm_source_id=*/{});
  }
  if (user_corrected_filling()) {
    manager().OnEditedAutofilledField(*form, *form->field(0),
                                      /*ukm_source_id=*/{});
  }

  base::HistogramTester histogram_tester;
  if (submitted()) {
    manager().OnFormSubmitted(*form, /*ukm_source_id=*/{});
  } else {
    // The destructor would trigger the logging of *Funnel*Abandoned* metrics.
    manager_ptr().reset();
  }
  ExpectCorrectFunnelRecording(histogram_tester);
}

class AutofillAiKeyMetricsTest : public BaseAutofillAiTest {};

TEST_F(AutofillAiKeyMetricsTest, FillingReadiness) {
  std::unique_ptr<FormStructure> passport_form = CreatePassportForm();
  {
    manager().OnFormSeen(*passport_form);
    base::HistogramTester histogram_tester;
    manager().OnFormSubmitted(*passport_form, /*ukm_source_id=*/{});
    histogram_tester.ExpectUniqueSample(
        "Autofill.Ai.KeyMetrics.FillingReadiness.Passport", 0, 1);
  }
  EntityInstance passport = test::GetPassportEntityInstance();
  AddOrUpdateEntityInstance(passport);
  {
    manager().OnFormSeen(*passport_form);
    base::HistogramTester histogram_tester;
    manager().OnFormSubmitted(*passport_form, /*ukm_source_id=*/{});
    histogram_tester.ExpectUniqueSample(
        "Autofill.Ai.KeyMetrics.FillingReadiness.Passport", 1, 1);
  }
}

TEST_F(AutofillAiKeyMetricsTest, FillingAssistance) {
  std::unique_ptr<FormStructure> vehicle_form = CreateVehicleForm();
  manager().OnFormSeen(*vehicle_form);
  {
    base::HistogramTester histogram_tester;
    manager().OnFormSubmitted(*vehicle_form, /*ukm_source_id=*/{});
    histogram_tester.ExpectUniqueSample(
        "Autofill.Ai.KeyMetrics.FillingAssistance.Vehicle", 0, 1);
  }
  {
    manager().OnSuggestionsShown(*vehicle_form, *vehicle_form->field(0),
                                 {kVehicle},
                                 /*ukm_source_id=*/{});
    manager().OnDidFillSuggestion(test::GetVehicleEntityInstance(),
                                  *vehicle_form, *vehicle_form->field(0),
                                  /*filled_fields=*/{},
                                  /*ukm_source_id=*/{});
    base::HistogramTester histogram_tester;
    manager().OnFormSubmitted(*vehicle_form, /*ukm_source_id=*/{});
    histogram_tester.ExpectUniqueSample(
        "Autofill.Ai.KeyMetrics.FillingAssistance.Vehicle", 1, 1);
  }
}

TEST_F(AutofillAiKeyMetricsTest, FillingAcceptance) {
  std::unique_ptr<FormStructure> drivers_license_form =
      CreateDriversLicenseForm();
  manager().OnFormSeen(*drivers_license_form);
  manager().OnSuggestionsShown(
      *drivers_license_form, *drivers_license_form->field(0), {kDriversLicense},
      /*ukm_source_id=*/{});
  {
    base::HistogramTester histogram_tester;
    manager().OnFormSubmitted(*drivers_license_form, /*ukm_source_id=*/{});
    histogram_tester.ExpectUniqueSample(
        "Autofill.Ai.KeyMetrics.FillingAcceptance.DriversLicense", 0, 1);
  }
  {
    manager().OnDidFillSuggestion(test::GetDriversLicenseEntityInstance(),
                                  *drivers_license_form,
                                  *drivers_license_form->field(0),
                                  /*filled_fields=*/{},
                                  /*ukm_source_id=*/{});
    base::HistogramTester histogram_tester;
    manager().OnFormSubmitted(*drivers_license_form, /*ukm_source_id=*/{});
    histogram_tester.ExpectUniqueSample(
        "Autofill.Ai.KeyMetrics.FillingAcceptance.DriversLicense", 1, 1);
  }
}

TEST_F(AutofillAiKeyMetricsTest, FillingCorrectness) {
  std::unique_ptr<FormStructure> passport_form = CreatePassportForm();
  manager().OnFormSeen(*passport_form);
  manager().OnSuggestionsShown(*passport_form, *passport_form->field(0),
                               {kPassport},
                               /*ukm_source_id=*/{});
  manager().OnDidFillSuggestion(test::GetPassportEntityInstance(),
                                *passport_form, *passport_form->field(0),
                                /*filled_fields=*/{passport_form->field(0)},
                                /*ukm_source_id=*/{});
  {
    base::HistogramTester histogram_tester;
    manager().OnFormSubmitted(*passport_form, /*ukm_source_id=*/{});
    histogram_tester.ExpectUniqueSample(
        "Autofill.Ai.KeyMetrics.FillingCorrectness.Passport", 1, 1);
  }
  {
    manager().OnEditedAutofilledField(*passport_form, *passport_form->field(0),
                                      /*ukm_source_id=*/{});
    base::HistogramTester histogram_tester;
    manager().OnFormSubmitted(*passport_form, /*ukm_source_id=*/{});
    histogram_tester.ExpectUniqueSample(
        "Autofill.Ai.KeyMetrics.FillingCorrectness.Passport", 0, 1);
  }
}

class AutofillAiMqlsMetricsTest : public BaseAutofillAiTest {
 public:
  AutofillAiMqlsMetricsTest() {
    logs_uploader_ = std::make_unique<
        optimization_guide::TestModelQualityLogsUploaderService>(&local_state_);

    optimization_guide::model_execution::prefs::RegisterLocalStatePrefs(
        local_state_.registry());
    optimization_guide::model_execution::prefs::RegisterProfilePrefs(
        local_state_.registry());
    ON_CALL(autofill_client(), GetMqlsUploadService)
        .WillByDefault(testing::Return(logs_uploader_.get()));
  }

  const std::vector<
      std::unique_ptr<optimization_guide::proto::LogAiDataRequest>>&
  mqls_logs() {
    return logs_uploader_->uploaded_logs();
  }

  const optimization_guide::proto::AutofillAiFieldEvent&
  GetLastFieldEventLogs() {
    return *(logs_uploader_->uploaded_logs()
                 .back()
                 ->mutable_forms_classifications()
                 ->mutable_quality()
                 ->mutable_field_event());
  }

  const optimization_guide::proto::AutofillAiKeyMetrics& GetKeyMetricsLogs() {
    return *(logs_uploader_->uploaded_logs()
                 .back()
                 ->mutable_forms_classifications()
                 ->mutable_quality()
                 ->mutable_key_metrics());
  }

  void ExpectCorrectMqlsFieldEventLogging(
      const optimization_guide::proto::AutofillAiFieldEvent& mqls_field_event,
      const FormStructure& form,
      const AutofillField& field,
      AutofillAiUkmLogger::EventType event_type,
      int event_order) {
    std::string event = [&] {
      switch (event_type) {
        case AutofillAiUkmLogger::EventType::kSuggestionShown:
          return "EventType: SuggestionShown";
        case AutofillAiUkmLogger::EventType::kSuggestionFilled:
          return "EventType: SuggestionFilled";
        case AutofillAiUkmLogger::EventType::kEditedAutofilledValue:
          return "EventType: EditedAutofilledValue";
        case AutofillAiUkmLogger::EventType::kFieldFilled:
          return "EventType: FieldFilled";
      }
    }();

    EXPECT_EQ(mqls_field_event.domain(), "myform_root.com") << event;
    EXPECT_EQ(mqls_field_event.form_signature(), form.form_signature().value())
        << event;
    EXPECT_EQ(mqls_field_event.form_session_identifier(),
              autofill_metrics::FormGlobalIdToHash64Bit(form.global_id()))
        << event;
    EXPECT_EQ(mqls_field_event.form_session_event_order(), event_order)
        << event;
    EXPECT_EQ(mqls_field_event.field_signature(),
              field.GetFieldSignature().value())
        << event;
    EXPECT_EQ(mqls_field_event.field_session_identifier(),
              autofill_metrics::FieldGlobalIdToHash64Bit(field.global_id()))
        << event;
    EXPECT_EQ(mqls_field_event.field_rank(), field.rank()) << event;
    EXPECT_EQ(mqls_field_event.field_rank_in_signature_group(),
              field.rank_in_signature_group())
        << event;
    EXPECT_EQ(mqls_field_event.field_type(),
              static_cast<int>(field.Type().GetStorableType()))
        << event;
    EXPECT_EQ(
        mqls_field_event.ai_field_type(),
        static_cast<int>(
            field.GetAutofillAiServerTypePredictions().value_or(UNKNOWN_TYPE)))
        << event;
    EXPECT_EQ(base::to_underlying(mqls_field_event.format_string_source()),
              base::to_underlying(field.format_string_source()))
        << event;
    EXPECT_EQ(base::to_underlying(mqls_field_event.form_control_type()),
              base::to_underlying(field.form_control_type()) + 1)
        << event;
    EXPECT_EQ(base::to_underlying(mqls_field_event.event_type()),
              base::to_underlying(event_type))
        << event;
    EXPECT_EQ(base::to_underlying(mqls_field_event.entity_type()), 1) << event;
  }

 private:
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<optimization_guide::TestModelQualityLogsUploaderService>
      logs_uploader_;
};

TEST_F(AutofillAiMqlsMetricsTest, FieldEvent) {
  std::unique_ptr<FormStructure> form = CreatePassportForm();

  test_api(manager()).logger().OnSuggestionsShown(*form, *form->field(0),
                                                  {kPassport},
                                                  /*ukm_source_id=*/{});
  ASSERT_EQ(mqls_logs().size(), 1u);
  ExpectCorrectMqlsFieldEventLogging(
      GetLastFieldEventLogs(), *form, *form->field(0),
      AutofillAiUkmLogger::EventType::kSuggestionShown, /*event_order=*/0);

  test_api(manager()).logger().OnDidFillSuggestion(*form, *form->field(0),
                                                   kPassport,
                                                   /*ukm_source_id=*/{});
  ASSERT_EQ(mqls_logs().size(), 2u);
  ExpectCorrectMqlsFieldEventLogging(
      GetLastFieldEventLogs(), *form, *form->field(0),
      AutofillAiUkmLogger::EventType::kSuggestionFilled, /*event_order=*/1);

  test_api(manager()).logger().OnDidFillField(
      *form, *form->field(0), EntityType(EntityTypeName::kPassport),
      /*ukm_source_id=*/{});
  ASSERT_EQ(mqls_logs().size(), 3u);
  ExpectCorrectMqlsFieldEventLogging(
      GetLastFieldEventLogs(), *form, *form->field(0),
      AutofillAiUkmLogger::EventType::kFieldFilled, /*event_order=*/2);

  test_api(manager()).logger().OnEditedAutofilledField(*form, *form->field(0),
                                                       /*ukm_source_id=*/{});
  ASSERT_EQ(mqls_logs().size(), 4u);
  ExpectCorrectMqlsFieldEventLogging(
      GetLastFieldEventLogs(), *form, *form->field(0),
      AutofillAiUkmLogger::EventType::kEditedAutofilledValue,
      /*event_order=*/3);
}

TEST_F(AutofillAiMqlsMetricsTest, KeyMetrics) {
  std::unique_ptr<FormStructure> form = CreatePassportForm();

  test_api(manager()).logger().OnFormHasDataToFill(form->global_id(),
                                                   {kPassport});
  test_api(manager()).logger().OnSuggestionsShown(*form, *form->field(1),
                                                  {kPassport},
                                                  /*ukm_source_id=*/{});
  form->field(0)->set_is_autofilled(true);
  form->field(0)->set_filling_product(FillingProduct::kAddress);
  form->field(1)->set_is_autofilled(true);
  form->field(1)->set_filling_product(FillingProduct::kAutofillAi);
  form->field(2)->set_is_autofilled(true);
  form->field(2)->set_filling_product(FillingProduct::kAutocomplete);

  test_api(manager()).logger().OnDidFillSuggestion(*form, *form->field(1),
                                                   kPassport,
                                                   /*ukm_source_id=*/{});
  test_api(manager()).logger().OnDidFillField(*form, *form->field(1), kPassport,
                                              /*ukm_source_id=*/{});

  test_api(manager()).logger().OnEditedAutofilledField(*form, *form->field(1),
                                                       /*ukm_source_id=*/{});

  test_api(manager()).logger().RecordFormMetrics(*form, /*ukm_source_id=*/{},
                                                 /*submission_state=*/true,
                                                 /*opt_in_status=*/true);
  ASSERT_EQ(mqls_logs().size(), 5u);
  const optimization_guide::proto::AutofillAiKeyMetrics& mqls_key_metrics =
      GetKeyMetricsLogs();

  EXPECT_EQ(mqls_key_metrics.domain(), "myform_root.com");
  EXPECT_EQ(mqls_key_metrics.form_signature(), form->form_signature().value());
  EXPECT_EQ(mqls_key_metrics.form_session_identifier(),
            autofill_metrics::FormGlobalIdToHash64Bit(form->global_id()));
  EXPECT_TRUE(mqls_key_metrics.filling_readiness());
  EXPECT_TRUE(mqls_key_metrics.filling_assistance());
  EXPECT_TRUE(mqls_key_metrics.filling_acceptance());
  EXPECT_FALSE(mqls_key_metrics.filling_correctness());
  EXPECT_EQ(mqls_key_metrics.autofill_filled_field_count(), 2);
  EXPECT_EQ(mqls_key_metrics.autofill_ai_filled_field_count(), 1);
  EXPECT_EQ(base::to_underlying(mqls_key_metrics.entity_type()), 1);
}

// Tests that KeyMetrics MQLS metrics aren't recorded if the user is not opted
// in for Autofill AI.
TEST_F(AutofillAiMqlsMetricsTest, KeyMetrics_OptOut) {
  SetAutofillAiOptInStatus(autofill_client(), AutofillAiOptInStatus::kOptedOut);
  std::unique_ptr<FormStructure> form = CreatePassportForm();
  test_api(manager()).logger().RecordFormMetrics(*form, /*ukm_source_id=*/{},
                                                 /*submission_state=*/true,
                                                 /*opt_in_status=*/false);
  EXPECT_TRUE(mqls_logs().empty());
}

// Tests that KeyMetrics MQLS metrics aren't recorded if the form was abandoned
// and not submitted.
TEST_F(AutofillAiMqlsMetricsTest, KeyMetrics_FormAbandoned) {
  std::unique_ptr<FormStructure> form = CreatePassportForm();

  test_api(manager()).logger().RecordFormMetrics(*form, /*ukm_source_id=*/{},
                                                 /*submission_state=*/false,
                                                 /*opt_in_status=*/true);
  EXPECT_TRUE(mqls_logs().empty());
}

// Tests that metrics are not recorded in MQLS if the enterprise policy forbids
// it.
TEST_F(AutofillAiMqlsMetricsTest, NoMqlsMetricsIfDisabledByEnterprisePolicy) {
  autofill_client().GetPrefs()->SetInteger(
      optimization_guide::prefs::
          kAutofillPredictionImprovementsEnterprisePolicyAllowed,
      base::to_underlying(optimization_guide::model_execution::prefs::
                              ModelExecutionEnterprisePolicyValue::kDisable));

  std::unique_ptr<FormStructure> form = CreatePassportForm();
  test_api(manager()).logger().OnSuggestionsShown(*form, *form->field(0),
                                                  {kPassport},
                                                  /*ukm_source_id=*/{});
  test_api(manager()).logger().RecordFormMetrics(*form, /*ukm_source_id=*/{},
                                                 /*submitted_state=*/true,
                                                 /*opt_in_status=*/true);
  EXPECT_TRUE(mqls_logs().empty());
}

// Tests that metrics are not recorded in MQLS when off-the-record.
TEST_F(AutofillAiMqlsMetricsTest, NoMqlsMetricsWhenOffTheRecord) {
  autofill_client().set_is_off_the_record(true);

  std::unique_ptr<FormStructure> form = CreatePassportForm();
  test_api(manager()).logger().OnSuggestionsShown(*form, *form->field(0),
                                                  {kPassport},
                                                  /*ukm_source_id=*/{});
  test_api(manager()).logger().RecordFormMetrics(*form, /*ukm_source_id=*/{},
                                                 /*submitted_state=*/true,
                                                 /*opt_in_status=*/true);
  EXPECT_TRUE(mqls_logs().empty());
}

}  // namespace

}  // namespace autofill
