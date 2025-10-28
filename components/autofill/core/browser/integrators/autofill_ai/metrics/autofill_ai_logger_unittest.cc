// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/autofill_ai/metrics/autofill_ai_logger.h"

#include <memory>
#include <optional>
#include <string_view>
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
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_manager.h"
#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_manager_test_api.h"
#include "components/autofill/core/browser/integrators/autofill_ai/metrics/autofill_ai_metrics.h"
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

using ::base::Bucket;
using ::base::BucketsAre;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::UnorderedElementsAreArray;

constexpr char kDefaultUrl[] = "https://example.com";

Suggestion GetSuggestion(const EntityInstance& entity) {
  Suggestion suggestion(SuggestionType::kFillAutofillAi);
  suggestion.payload = Suggestion::AutofillAiPayload(entity.guid());
  return suggestion;
}

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
                              features::kAutofillAiRedressNumber,
                              features::kAutofillAiWalletFlightReservation},
        /*disabled_features=*/{});
    autofill_client().set_entity_data_manager(
        std::make_unique<EntityDataManager>(
            autofill_client().GetPrefs(),
            autofill_client().GetIdentityManager(),
            autofill_client().GetSyncService(),
            webdata_helper_.autofill_webdata_service(),
            /*history_service=*/nullptr,
            /*strike_database=*/nullptr));
    RecreateManager();
    autofill_client().SetUpPrefsAndIdentityForAutofillAi();
  }

  void RecreateManager() {
    manager_ = std::make_unique<AutofillAiManager>(&autofill_client_,
                                                   &strike_database_);
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
    return CreateFormStructure(
        {NAME_FULL, PASSPORT_NUMBER, PHONE_HOME_WHOLE_NUMBER}, std::move(url));
  }

  [[nodiscard]] std::unique_ptr<FormStructure> CreateVehicleForm(
      std::string url = std::string(kDefaultUrl)) {
    return CreateFormStructure({NAME_FULL, VEHICLE_LICENSE_PLATE},
                               std::move(url));
  }

  [[nodiscard]] std::unique_ptr<FormStructure> CreateDriversLicenseForm(
      std::string url = std::string(kDefaultUrl)) {
    return CreateFormStructure(
        {NAME_FULL, DRIVERS_LICENSE_NUMBER, DRIVERS_LICENSE_REGION,
         DRIVERS_LICENSE_ISSUE_DATE, DRIVERS_LICENSE_EXPIRATION_DATE},
        std::move(url));
  }

  [[nodiscard]] std::unique_ptr<FormStructure> CreateKnownTravelerNumberForm(
      std::string url = std::string(kDefaultUrl)) {
    return CreateFormStructure(
        {KNOWN_TRAVELER_NUMBER, KNOWN_TRAVELER_NUMBER_EXPIRATION_DATE},
        std::move(url));
  }

  [[nodiscard]] std::unique_ptr<FormStructure> CreateRedressNumberForm(
      std::string url = std::string(kDefaultUrl)) {
    return CreateFormStructure({REDRESS_NUMBER}, std::move(url));
  }

  [[nodiscard]] std::unique_ptr<FormStructure> CreateNationalIdCardForm(
      std::string url = std::string(kDefaultUrl)) {
    return CreateFormStructure(
        {NATIONAL_ID_CARD_NUMBER, NATIONAL_ID_CARD_ISSUING_COUNTRY,
         NATIONAL_ID_CARD_ISSUE_DATE, NATIONAL_ID_CARD_EXPIRATION_DATE},
        std::move(url));
  }

  [[nodiscard]] std::unique_ptr<FormStructure> CreateFlightReservationForm(
      std::string url = std::string(kDefaultUrl)) {
    return CreateFormStructure(
        {NAME_FULL, FLIGHT_RESERVATION_FLIGHT_NUMBER,
         FLIGHT_RESERVATION_TICKET_NUMBER, FLIGHT_RESERVATION_CONFIRMATION_CODE,
         FLIGHT_RESERVATION_ARRIVAL_AIRPORT,
         FLIGHT_RESERVATION_DEPARTURE_AIRPORT},
        std::move(url));
  }

  [[nodiscard]] std::unique_ptr<FormStructure> CreateMergedForm(
      const std::vector<const FormStructure*>& forms,
      std::string url = std::string(kDefaultUrl)) {
    std::vector<FieldType> field_types;
    for (const FormStructure* const form : forms) {
      for (const std::unique_ptr<AutofillField>& field : form->fields()) {
        // Adds the first type stored in each field.
        if (!field->Type().GetTypes().empty()) {
          field_types.push_back(*field->Type().GetTypes().begin());
        }
      }
    }
    return CreateFormStructure(field_types, std::move(url));
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

  std::unique_ptr<FormStructure> CreateForm(EntityType type) {
    switch (type.name()) {
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
      case EntityTypeName::kFlightReservation:
        return CreateFlightReservationForm();
    }
    NOTREACHED();
  }

  EntityInstance CreateEntity(EntityType type,
                              EntityInstance::RecordType record_type) {
    switch (type.name()) {
      case EntityTypeName::kPassport:
        return test::GetPassportEntityInstance({.record_type = record_type});
      case EntityTypeName::kDriversLicense:
        return test::GetDriversLicenseEntityInstance(
            {.record_type = record_type});
      case EntityTypeName::kKnownTravelerNumber:
        return test::GetKnownTravelerNumberInstance(
            {.record_type = record_type});
      case EntityTypeName::kRedressNumber:
        return test::GetRedressNumberEntityInstance(
            {.record_type = record_type});
      case EntityTypeName::kVehicle:
        return test::GetVehicleEntityInstance({.record_type = record_type});
      case EntityTypeName::kNationalIdCard:
        return test::GetNationalIdCardEntityInstance(
            {.record_type = record_type});
      case EntityTypeName::kFlightReservation:
        return test::GetFlightReservationEntityInstance(
            {.record_type = record_type});
    }
    NOTREACHED();
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
// testing, and the `RecordType` of the entity.
class AutofillAiFunnelMetricsTest
    : public BaseAutofillAiTest,
      public testing::WithParamInterface<
          std::tuple<bool, EntityType, EntityInstance::RecordType>> {
  static constexpr char kFunnelUmaMask[] = "Autofill.Ai.Funnel.%s.%s%s%s";

 public:
  AutofillAiFunnelMetricsTest() = default;

  bool submitted() { return std::get<0>(GetParam()); }
  EntityType entity_type() { return std::get<1>(GetParam()); }
  EntityInstance::RecordType record_type() { return std::get<2>(GetParam()); }

  std::unique_ptr<FormStructure> CreateForm() {
    return BaseAutofillAiTest::CreateForm(entity_type());
  }

  EntityInstance CreateEntity() {
    return BaseAutofillAiTest::CreateEntity(entity_type(), record_type());
  }

  void ExpectFunnelRecording(const base::HistogramTester& histogram_tester,
                             std::string_view funnel_name,
                             int sample) {
    for (bool use_submitted : {false, true}) {
      auto expect_correct_histogram =
          [&](std::string_view histogram_name, bool use_entity_type,
              bool use_record_type) {
            std::string histogram_name_str = GetFunnelHistogram(
                histogram_name,
                use_submitted ? std::optional(submitted()) : std::nullopt,
                use_entity_type ? std::optional(entity_type()) : std::nullopt,
                use_record_type ? std::optional(record_type()) : std::nullopt);

            histogram_tester.ExpectUniqueSample(histogram_name_str, sample, 1);
          };

      expect_correct_histogram(funnel_name, /*use_entity_type=*/false,
                               /*use_record_type=*/false);
      expect_correct_histogram(funnel_name, /*use_entity_type=*/true,
                               /*use_record_type=*/false);
      expect_correct_histogram(funnel_name, /*use_entity_type=*/true,
                               /*use_record_type=*/true);
    }
  }

  void SubmitOrAbandonForm(const FormStructure& form) {
    if (submitted()) {
      manager().OnFormSubmitted(form, /*ukm_source_id=*/{});
    } else {
      // The destructor would trigger the logging of *Funnel*Abandoned* metrics.
      RecreateManager();
    }
  }

  std::string GetFunnelHistogram(
      std::string_view funnel_state,
      std::optional<bool> submitted,
      std::optional<EntityType> entity_type,
      std::optional<EntityInstance::RecordType> record_type) {
    EXPECT_FALSE(!entity_type && record_type)
        << "Only entity-type-specific histograms are split by record type.";
    std::string_view submission_state = "Aggregate";
    if (submitted) {
      submission_state = *submitted ? "Submitted" : "Abandoned";
    }
    return base::StringPrintf(
        kFunnelUmaMask, submission_state, funnel_state,
        entity_type
            ? base::StrCat({".", EntityTypeToMetricsString(*entity_type)})
            : "",
        record_type
            ? base::StrCat({".", EntityRecordTypeToMetricsString(*record_type)})
            : "");
  }
};

// TODO(crbug.com/445679087): Remove the funnel value parameterization and
// instead have a separate test for each funnel histogram.
INSTANTIATE_TEST_SUITE_P(
    AutofillAiTest,
    AutofillAiFunnelMetricsTest,
    testing::Combine(
        testing::Bool(),
        testing::ValuesIn(DenseSet<EntityType>::all()),
        testing::ValuesIn(DenseSet<EntityInstance::RecordType>::all())));

// Tests Autofill.Ai.Funnel.*.Eligibility2.
TEST_P(AutofillAiFunnelMetricsTest, Eligibility) {
  std::unique_ptr<FormStructure> form = CreateForm();
  manager().OnFormSeen(*form);

  base::HistogramTester histogram_tester;
  SubmitOrAbandonForm(*form);

  histogram_tester.ExpectUniqueSample(
      GetFunnelHistogram("Eligibility2", /*submitted=*/std::nullopt,
                         /*entity_type=*/std::nullopt,
                         /*record_type=*/std::nullopt),
      base::to_underlying(entity_type().name()), 1);
  histogram_tester.ExpectUniqueSample(
      GetFunnelHistogram("Eligibility2", submitted(),
                         /*entity_type=*/std::nullopt,
                         /*record_type=*/std::nullopt),
      base::to_underlying(entity_type().name()), 1);
}

// Tests Autofill.Ai.Funnel.*.ReadinessAfterEligibility.*.
TEST_P(AutofillAiFunnelMetricsTest, ReadinessAfterEligibility) {
  std::unique_ptr<FormStructure> form = CreateForm();
  {  // The user has no entities stored.
    manager().OnFormSeen(*form);
    base::HistogramTester histogram_tester;
    SubmitOrAbandonForm(*form);
    ExpectFunnelRecording(histogram_tester, "ReadinessAfterEligibility",
                          /*sample=*/0);
  }
  {  // The user has entities stored.
    AddOrUpdateEntityInstance(CreateEntity());
    manager().OnFormSeen(*form);
    base::HistogramTester histogram_tester;
    SubmitOrAbandonForm(*form);
    ExpectFunnelRecording(histogram_tester, "ReadinessAfterEligibility",
                          /*sample=*/1);
  }
}

// Tests Autofill.Ai.Funnel.*.SuggestionAfterReadiness.*.
TEST_P(AutofillAiFunnelMetricsTest, SuggestionAfterReadiness) {
  std::unique_ptr<FormStructure> form = CreateForm();
  EntityInstance entity = CreateEntity();
  AddOrUpdateEntityInstance(entity);
  {  // The user didn't trigger suggestions.
    manager().OnFormSeen(*form);
    base::HistogramTester histogram_tester;
    SubmitOrAbandonForm(*form);
    ExpectFunnelRecording(histogram_tester, "SuggestionAfterReadiness",
                          /*sample=*/0);
  }
  {  // The user triggered suggestions.
    manager().OnFormSeen(*form);
    manager().OnSuggestionsShown(*form, *form->field(0),
                                 {GetSuggestion(entity)},
                                 /*ukm_source_id=*/{});
    base::HistogramTester histogram_tester;
    SubmitOrAbandonForm(*form);
    ExpectFunnelRecording(histogram_tester, "SuggestionAfterReadiness",
                          /*sample=*/1);
  }
}

// Tests Autofill.Ai.Funnel.*.FillAfterSuggestion.*.
TEST_P(AutofillAiFunnelMetricsTest, FillAfterSuggestion) {
  std::unique_ptr<FormStructure> form = CreateForm();
  EntityInstance entity = CreateEntity();
  AddOrUpdateEntityInstance(entity);
  auto set_up_funnel = [&] {
    manager().OnFormSeen(*form);
    manager().OnSuggestionsShown(*form, *form->field(0),
                                 {GetSuggestion(entity)},
                                 /*ukm_source_id=*/{});
  };
  {  // The user didn't fill suggestions.
    set_up_funnel();
    base::HistogramTester histogram_tester;
    SubmitOrAbandonForm(*form);
    ExpectFunnelRecording(histogram_tester, "FillAfterSuggestion",
                          /*sample=*/0);
  }
  {  // The user filled suggestions.
    set_up_funnel();
    manager().OnDidFillSuggestion(entity, *form, *form->field(0),
                                  {form->field(0)},
                                  /*ukm_source_id=*/{});
    base::HistogramTester histogram_tester;
    SubmitOrAbandonForm(*form);
    ExpectFunnelRecording(histogram_tester, "FillAfterSuggestion",
                          /*sample=*/1);
  }
}

// Tests Autofill.Ai.Funnel.*.CorrectionAfterFill.*.
TEST_P(AutofillAiFunnelMetricsTest, CorrectionAfterFill) {
  std::unique_ptr<FormStructure> form = CreateForm();
  EntityInstance entity = CreateEntity();
  AddOrUpdateEntityInstance(entity);
  auto set_up_funnel = [&] {
    manager().OnFormSeen(*form);
    manager().OnSuggestionsShown(*form, *form->field(0),
                                 {GetSuggestion(entity)},
                                 /*ukm_source_id=*/{});
    manager().OnDidFillSuggestion(entity, *form, *form->field(0),
                                  {form->field(0)},
                                  /*ukm_source_id=*/{});
  };
  {  // The user didn't correct a filled field.
    set_up_funnel();
    base::HistogramTester histogram_tester;
    SubmitOrAbandonForm(*form);
    ExpectFunnelRecording(histogram_tester, "CorrectionAfterFill",
                          /*sample=*/0);
  }
  {  // The user corrected a filled field.
    set_up_funnel();
    manager().OnEditedAutofilledField(*form, *form->field(0),
                                      /*ukm_source_id=*/{});
    base::HistogramTester histogram_tester;
    SubmitOrAbandonForm(*form);
    ExpectFunnelRecording(histogram_tester, "CorrectionAfterFill",
                          /*sample=*/1);
  }
}

class AutofillAiKeyMetricsTest
    : public BaseAutofillAiTest,
      public testing::WithParamInterface<
          std::tuple<EntityType, EntityInstance::RecordType>> {
  static constexpr char kKeyMetricsUmaMask[] = "Autofill.Ai.KeyMetrics.%s%s%s";

 public:
  AutofillAiKeyMetricsTest() = default;

  EntityType entity_type() { return std::get<0>(GetParam()); }
  EntityInstance::RecordType record_type() { return std::get<1>(GetParam()); }

  std::unique_ptr<FormStructure> CreateForm() {
    return BaseAutofillAiTest::CreateForm(entity_type());
  }

  EntityInstance CreateEntity() {
    return BaseAutofillAiTest::CreateEntity(entity_type(), record_type());
  }

  void ExpectKeyMetricsRecording(const base::HistogramTester& histogram_tester,
                                 std::string_view key_metric_name,
                                 int sample) {
    auto expect_correct_histogram = [&](std::optional<EntityType> entity_type,
                                        std::optional<
                                            EntityInstance::RecordType>
                                            record_type) {
      EXPECT_FALSE(!entity_type && record_type)
          << "Only entity-type-specific histograms are split by record type.";
      histogram_tester.ExpectUniqueSample(
          base::StringPrintf(
              kKeyMetricsUmaMask, key_metric_name,
              entity_type
                  ? base::StrCat({".", EntityTypeToMetricsString(*entity_type)})
                  : "",
              record_type ? base::StrCat({".", EntityRecordTypeToMetricsString(
                                                   *record_type)})
                          : ""),
          sample, 1);
      return;
    };

    expect_correct_histogram(/*entity_type=*/std::nullopt,
                             /*record_type=*/std::nullopt);
    expect_correct_histogram(entity_type(), /*record_type=*/std::nullopt);
    expect_correct_histogram(entity_type(), record_type());
  }
};

INSTANTIATE_TEST_SUITE_P(
    AutofillAiTest,
    AutofillAiKeyMetricsTest,
    testing::Combine(
        testing::ValuesIn(DenseSet<EntityType>::all()),
        testing::ValuesIn(DenseSet<EntityInstance::RecordType>::all())));

TEST_P(AutofillAiKeyMetricsTest, FillingReadiness) {
  std::unique_ptr<FormStructure> form = CreateForm();
  {
    manager().OnFormSeen(*form);
    base::HistogramTester histogram_tester;
    manager().OnFormSubmitted(*form, /*ukm_source_id=*/{});
    ExpectKeyMetricsRecording(histogram_tester, "FillingReadiness",
                              /*sample=*/0);
  }
  {
    AddOrUpdateEntityInstance(CreateEntity());
    manager().OnFormSeen(*form);
    base::HistogramTester histogram_tester;
    manager().OnFormSubmitted(*form, /*ukm_source_id=*/{});
    ExpectKeyMetricsRecording(histogram_tester, "FillingReadiness",
                              /*sample=*/1);
  }
}

TEST_P(AutofillAiKeyMetricsTest, FillingAssistance) {
  std::unique_ptr<FormStructure> form = CreateForm();
  manager().OnFormSeen(*form);
  EntityInstance entity = CreateEntity();
  AddOrUpdateEntityInstance(entity);
  {
    base::HistogramTester histogram_tester;
    manager().OnFormSubmitted(*form, /*ukm_source_id=*/{});
    ExpectKeyMetricsRecording(histogram_tester, "FillingAssistance",
                              /*sample=*/0);
  }
  {
    manager().OnSuggestionsShown(*form, *form->field(0),
                                 {GetSuggestion(entity)},
                                 /*ukm_source_id=*/{});
    manager().OnDidFillSuggestion(entity, *form, *form->field(0),
                                  /*filled_fields=*/{},
                                  /*ukm_source_id=*/{});
    base::HistogramTester histogram_tester;
    manager().OnFormSubmitted(*form, /*ukm_source_id=*/{});
    ExpectKeyMetricsRecording(histogram_tester, "FillingAssistance",
                              /*sample=*/1);
  }
}

TEST_P(AutofillAiKeyMetricsTest, FillingAcceptance) {
  std::unique_ptr<FormStructure> form = CreateForm();
  EntityInstance entity = CreateEntity();
  AddOrUpdateEntityInstance(entity);
  manager().OnFormSeen(*form);
  manager().OnSuggestionsShown(*form, *form->field(0), {GetSuggestion(entity)},
                               /*ukm_source_id=*/{});
  {
    base::HistogramTester histogram_tester;
    manager().OnFormSubmitted(*form, /*ukm_source_id=*/{});
    ExpectKeyMetricsRecording(histogram_tester, "FillingAcceptance",
                              /*sample=*/0);
  }
  {
    manager().OnDidFillSuggestion(entity, *form, *form->field(0),
                                  /*filled_fields=*/{},
                                  /*ukm_source_id=*/{});
    base::HistogramTester histogram_tester;
    manager().OnFormSubmitted(*form, /*ukm_source_id=*/{});
    ExpectKeyMetricsRecording(histogram_tester, "FillingAcceptance",
                              /*sample=*/1);
  }
}

TEST_P(AutofillAiKeyMetricsTest, FillingCorrectness) {
  std::unique_ptr<FormStructure> form = CreateForm();
  EntityInstance entity = CreateEntity();
  AddOrUpdateEntityInstance(entity);
  manager().OnFormSeen(*form);
  manager().OnSuggestionsShown(*form, *form->field(0), {GetSuggestion(entity)},
                               /*ukm_source_id=*/{});
  manager().OnDidFillSuggestion(entity, *form, *form->field(0),
                                /*filled_fields=*/{form->field(0)},
                                /*ukm_source_id=*/{});
  {
    base::HistogramTester histogram_tester;
    manager().OnFormSubmitted(*form, /*ukm_source_id=*/{});
    ExpectKeyMetricsRecording(histogram_tester, "FillingCorrectness",
                              /*sample=*/1);
  }
  {
    manager().OnEditedAutofilledField(*form, *form->field(0),
                                      /*ukm_source_id=*/{});
    base::HistogramTester histogram_tester;
    manager().OnFormSubmitted(*form, /*ukm_source_id=*/{});
    ExpectKeyMetricsRecording(histogram_tester, "FillingCorrectness",
                              /*sample=*/0);
  }
}

// Tests that for a form fillable by multiple `EntityType`'s, we
TEST_F(BaseAutofillAiTest, KeyMetrics_MixedForm) {
  std::unique_ptr<FormStructure> vehicle_form = CreateVehicleForm();
  std::unique_ptr<FormStructure> drivers_license_form =
      CreateDriversLicenseForm();
  std::unique_ptr<FormStructure> mixed_form =
      CreateMergedForm({vehicle_form.get(), drivers_license_form.get()});

  // Readiness should be true for both.
  EntityInstance vehicle_entity = test::GetVehicleEntityInstance();
  EntityInstance drivers_license_entity =
      test::GetDriversLicenseEntityInstance();
  AddOrUpdateEntityInstance(vehicle_entity);
  AddOrUpdateEntityInstance(drivers_license_entity);
  manager().OnFormSeen(*mixed_form);

  // Assistance should be true for both.
  manager().OnSuggestionsShown(*mixed_form, *mixed_form->fields().front(),
                               {GetSuggestion(vehicle_entity)},
                               /*ukm_source_id=*/{});
  manager().OnSuggestionsShown(*mixed_form, *mixed_form->fields().back(),
                               {GetSuggestion(drivers_license_entity)},
                               /*ukm_source_id=*/{});

  // Acceptance should be true for both.
  manager().OnDidFillSuggestion(
      vehicle_entity, *mixed_form, *mixed_form->fields().front(),
      /*filled_fields=*/{mixed_form->fields().front().get()},
      /*ukm_source_id=*/{});
  manager().OnDidFillSuggestion(
      drivers_license_entity, *mixed_form, *mixed_form->fields().back(),
      /*filled_fields=*/{mixed_form->fields().back().get()},
      /*ukm_source_id=*/{});

  // Correctness should be true for Vehicle and false for DriversLicense.
  manager().OnEditedAutofilledField(*mixed_form, *mixed_form->fields().back(),
                                    /*ukm_source_id=*/{});

  base::HistogramTester histogram_tester;
  manager().OnFormSubmitted(*mixed_form, /*ukm_source_id=*/{});

  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.KeyMetrics.FillingReadiness.Vehicle", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.KeyMetrics.FillingReadiness.DriversLicense", 1, 1);
  histogram_tester.ExpectUniqueSample("Autofill.Ai.KeyMetrics.FillingReadiness",
                                      1, 1);

  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.KeyMetrics.FillingAssistance.Vehicle", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.KeyMetrics.FillingAssistance.DriversLicense", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.KeyMetrics.FillingAssistance", 1, 1);

  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.KeyMetrics.FillingAcceptance.Vehicle", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.KeyMetrics.FillingAcceptance.DriversLicense", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.KeyMetrics.FillingAcceptance", 1, 1);

  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.KeyMetrics.FillingCorrectness.Vehicle", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.KeyMetrics.FillingCorrectness.DriversLicense", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.KeyMetrics.FillingCorrectness", 0, 1);
}

class AutofillAiPromptMetricsTest
    : public BaseAutofillAiTest,
      public testing::WithParamInterface<
          std::tuple<EntityType,
                     AutofillClient::AutofillAiImportPromptType,
                     AutofillClient::AutofillAiBubbleClosedReason,
                     EntityInstance::RecordType>> {
 public:
  AutofillAiPromptMetricsTest() = default;

  EntityType entity_type() { return std::get<0>(GetParam()); }
  AutofillClient::AutofillAiImportPromptType prompt_type() {
    return std::get<1>(GetParam());
  }
  AutofillClient::AutofillAiBubbleClosedReason close_reason() {
    return std::get<2>(GetParam());
  }
  EntityInstance::RecordType record_type() { return std::get<3>(GetParam()); }

  FormData CreateForm() {
    return BaseAutofillAiTest::CreateForm(entity_type())->ToFormData();
  }
};

INSTANTIATE_TEST_SUITE_P(
    AutofillAiTest,
    AutofillAiPromptMetricsTest,
    testing::Combine(
        testing::ValuesIn(DenseSet<EntityType>::all()),
        testing::ValuesIn(
            DenseSet<AutofillClient::AutofillAiImportPromptType>::all()),
        testing::ValuesIn(
            DenseSet<AutofillClient::AutofillAiBubbleClosedReason>::all()),
        testing::ValuesIn(DenseSet<EntityInstance::RecordType>::all())));

TEST_P(AutofillAiPromptMetricsTest, PromptMetrics) {
  constexpr std::string_view kPromptHistogramMask = "Autofill.Ai.%s.%s%s";
  base::HistogramTester histogram_tester;
  test_api(manager()).logger().OnImportPromptResult(
      CreateForm(), prompt_type(), entity_type(), record_type(), close_reason(),
      /*ukm_source_id=*/0);

  const std::string_view prompt_type_str =
      EntityPromptTypeToMetricsString(prompt_type());
  const std::string_view entity_type_str =
      EntityTypeToMetricsString(entity_type());
  const std::string record_type_str =
      base::StrCat({".", EntityRecordTypeToMetricsString(record_type())});

  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(kPromptHistogramMask, prompt_type_str, entity_type_str,
                         record_type_str),
      close_reason(), 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(kPromptHistogramMask, prompt_type_str, entity_type_str,
                         ""),
      close_reason(), 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(kPromptHistogramMask, prompt_type_str, "AllEntities",
                         ""),
      close_reason(), 1);
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
    return logs_uploader_->uploaded_logs()
        .back()
        ->forms_classifications()
        .quality()
        .field_event();
  }

  const optimization_guide::proto::AutofillAiKeyMetrics& GetKeyMetricsLogs() {
    return logs_uploader_->uploaded_logs()
        .back()
        ->forms_classifications()
        .quality()
        .key_metrics();
  }

  const optimization_guide::proto::AutofillAiUserPromptMetrics&
  GetUserPromptMetrics() {
    return logs_uploader_->uploaded_logs()
        .back()
        ->forms_classifications()
        .quality()
        .user_prompt_metrics();
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
    EXPECT_EQ(mqls_field_event.field_type(), static_cast<int>([&field] {
                FieldTypeSet field_types = field.Type().GetTypes();
                return !field_types.empty() ? *field_types.begin()
                                            : UNKNOWN_TYPE;
              }()))
        << event;
    EXPECT_EQ(mqls_field_event.ai_field_type(), static_cast<int>([&field] {
                FieldTypeSet autofill_ai_types =
                    field.Type().GetAutofillAiTypes();
                return !autofill_ai_types.empty() ? *autofill_ai_types.begin()
                                                  : UNKNOWN_TYPE;
              }()))
        << event;
    EXPECT_THAT(mqls_field_event.field_types(),
                UnorderedElementsAreArray(field.Type().GetTypes()));
    EXPECT_THAT(mqls_field_event.ai_field_types(),
                UnorderedElementsAreArray(field.Type().GetAutofillAiTypes()));
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
    EXPECT_EQ(base::to_underlying(mqls_field_event.storage_type()), 2) << event;
  }

 private:
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<optimization_guide::TestModelQualityLogsUploaderService>
      logs_uploader_;
};

TEST_F(AutofillAiMqlsMetricsTest, FieldEvent) {
  std::unique_ptr<FormStructure> form = CreatePassportForm();
  EntityInstance entity = test::GetPassportEntityInstance();
  test_api(manager()).logger().OnSuggestionsShown(*form, *form->field(0),
                                                  {&entity},
                                                  /*ukm_source_id=*/{});
  ASSERT_EQ(mqls_logs().size(), 1u);
  ExpectCorrectMqlsFieldEventLogging(
      GetLastFieldEventLogs(), *form, *form->field(0),
      AutofillAiUkmLogger::EventType::kSuggestionShown, /*event_order=*/0);

  test_api(manager()).logger().OnDidFillSuggestion(*form, *form->field(0),
                                                   entity,
                                                   /*ukm_source_id=*/{});
  ASSERT_EQ(mqls_logs().size(), 2u);
  ExpectCorrectMqlsFieldEventLogging(
      GetLastFieldEventLogs(), *form, *form->field(0),
      AutofillAiUkmLogger::EventType::kSuggestionFilled, /*event_order=*/1);

  test_api(manager()).logger().OnDidFillField(*form, *form->field(0), entity,
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

TEST_F(AutofillAiMqlsMetricsTest, UserPrompts) {
  std::unique_ptr<FormStructure> form = CreatePassportForm();
  test_api(manager()).logger().OnImportPromptResult(
      form->ToFormData(), AutofillClient::AutofillAiImportPromptType::kUpdate,
      EntityType(EntityTypeName::kPassport), EntityInstance::RecordType::kLocal,
      AutofillClient::AutofillAiBubbleClosedReason::kAccepted,
      /*ukm_source_id=*/{});
  ASSERT_EQ(mqls_logs().size(), 1u);

  const optimization_guide::proto::AutofillAiUserPromptMetrics&
      mqls_user_prompt = GetUserPromptMetrics();
  EXPECT_EQ(mqls_user_prompt.domain(), "myform_root.com");
  EXPECT_EQ(mqls_user_prompt.form_session_identifier(),
            autofill_metrics::FormGlobalIdToHash64Bit(form->global_id()));
  EXPECT_EQ(mqls_user_prompt.storage_type(),
            optimization_guide::proto::AUTOFILL_AI_ENTITY_STORAGE_TYPE_LOCAL);
  EXPECT_EQ(mqls_user_prompt.prompt_type(),
            optimization_guide::proto::AUTOFILL_AI_PROMPT_TYPE_UPDATE_ENTITY);
  EXPECT_EQ(mqls_user_prompt.entity_type(),
            optimization_guide::proto::AUTOFILL_AI_ENTITY_TYPE_PASSPORT);
  EXPECT_EQ(
      mqls_user_prompt.result(),
      optimization_guide::proto::AUTOFILL_AI_PROMPT_USER_DECISION_ACCEPTED);
}

TEST_F(AutofillAiMqlsMetricsTest, KeyMetrics) {
  std::unique_ptr<FormStructure> form = CreatePassportForm();
  EntityInstance entity = test::GetPassportEntityInstance();

  test_api(manager()).logger().OnFormHasDataToFill(form->global_id(),
                                                   {entity.type()}, {entity});
  test_api(manager()).logger().OnSuggestionsShown(*form, *form->field(1),
                                                  {&entity},
                                                  /*ukm_source_id=*/{});
  form->field(0)->set_is_autofilled(true);
  form->field(0)->set_filling_product(FillingProduct::kAddress);
  form->field(1)->set_is_autofilled(true);
  form->field(1)->set_filling_product(FillingProduct::kAutofillAi);
  form->field(2)->set_is_autofilled(true);
  form->field(2)->set_filling_product(FillingProduct::kAutocomplete);

  test_api(manager()).logger().OnDidFillSuggestion(*form, *form->field(1),
                                                   entity,
                                                   /*ukm_source_id=*/{});
  test_api(manager()).logger().OnDidFillField(*form, *form->field(1), entity,
                                              /*ukm_source_id=*/{});

  test_api(manager()).logger().OnEditedAutofilledField(*form, *form->field(1),
                                                       /*ukm_source_id=*/{});
  form->field(1)->set_is_autofilled(false);
  form->field(1)->set_is_user_edited(true);

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
  EXPECT_FALSE(mqls_key_metrics.perfect_filling());
  EXPECT_EQ(mqls_key_metrics.autofill_filled_field_count(), 2);
  EXPECT_EQ(mqls_key_metrics.autofill_ai_filled_field_count(), 1);
  EXPECT_EQ(base::to_underlying(mqls_key_metrics.entity_type()), 1);
}

TEST_F(AutofillAiMqlsMetricsTest, KeyMetrics_PerfectFilling) {
  std::unique_ptr<FormStructure> form = CreatePassportForm();
  EntityInstance entity = test::GetPassportEntityInstance();

  // Simulate a perfect filling (i.e. a fill where the user doesn't modify any
  // field).
  test_api(manager()).logger().OnFormHasDataToFill(form->global_id(),
                                                   {entity.type()}, {entity});
  test_api(manager()).logger().OnSuggestionsShown(*form, *form->field(1),
                                                  {&entity},
                                                  /*ukm_source_id=*/{});
  test_api(manager()).logger().OnDidFillSuggestion(*form, *form->field(1),
                                                   entity,
                                                   /*ukm_source_id=*/{});
  test_api(manager()).logger().OnDidFillField(*form, *form->field(1), entity,
                                              /*ukm_source_id=*/{});

  // The MQLS logs should record a perfect filling here.
  test_api(manager()).logger().RecordFormMetrics(*form, /*ukm_source_id=*/{},
                                                 /*submission_state=*/true,
                                                 /*opt_in_status=*/true);
  EXPECT_TRUE(GetKeyMetricsLogs().perfect_filling());

  // Simulate a user edit for some field.
  form->field(2)->set_is_user_edited(true);

  // Now the MQLS logs should not record a perfect filling.
  test_api(manager()).logger().RecordFormMetrics(*form, /*ukm_source_id=*/{},
                                                 /*submission_state=*/true,
                                                 /*opt_in_status=*/true);
  EXPECT_FALSE(GetKeyMetricsLogs().perfect_filling());
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

  EntityInstance entity = test::GetPassportEntityInstance();
  std::unique_ptr<FormStructure> form = CreatePassportForm();
  test_api(manager()).logger().OnSuggestionsShown(*form, *form->field(0),
                                                  {&entity},
                                                  /*ukm_source_id=*/{});
  test_api(manager()).logger().RecordFormMetrics(*form, /*ukm_source_id=*/{},
                                                 /*submitted_state=*/true,
                                                 /*opt_in_status=*/true);
  EXPECT_TRUE(mqls_logs().empty());
}

// Tests that metrics are not recorded in MQLS when off-the-record.
TEST_F(AutofillAiMqlsMetricsTest, NoMqlsMetricsWhenOffTheRecord) {
  autofill_client().set_is_off_the_record(true);

  EntityInstance entity = test::GetPassportEntityInstance();
  std::unique_ptr<FormStructure> form = CreatePassportForm();
  test_api(manager()).logger().OnSuggestionsShown(*form, *form->field(0),
                                                  {&entity},
                                                  /*ukm_source_id=*/{});
  test_api(manager()).logger().RecordFormMetrics(*form, /*ukm_source_id=*/{},
                                                 /*submitted_state=*/true,
                                                 /*opt_in_status=*/true);
  EXPECT_TRUE(mqls_logs().empty());
}

}  // namespace

}  // namespace autofill
