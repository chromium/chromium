// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/filling/entities/field_filling_entity_util.h"

#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/data_quality/addresses/address_normalizer_impl.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map_test_utils.h"
#include "components/autofill/core/browser/proto/api_v1.pb.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_test_helper.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/null_storage.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/source.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/storage.h"
#include "third_party/libaddressinput/src/cpp/test/testdata_source.h"

namespace autofill {
namespace {

constexpr char kAppLocaleUS[] = "en-US";

using ::i18n::addressinput::NullStorage;
using ::i18n::addressinput::Source;
using ::i18n::addressinput::Storage;
using ::i18n::addressinput::TestdataSource;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using FieldPrediction = autofill::AutofillQueryResponse::FormSuggestion::
    FieldSuggestion::FieldPrediction;

class GetFieldsFillableByAutofillAiTest : public testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();
    test_api(form_).PushField(
        test::CreateTestFormField("", "", "", FormControlType::kInputText));
    test_api(form_).PushField(
        test::CreateTestFormField("", "", "", FormControlType::kInputText));
  }

  void AddOrUpdateEntityInstance(EntityInstance entity) {
    edm_.AddOrUpdateEntityInstance(std::move(entity));
    helper_.WaitUntilIdle();
  }

  EntityDataManager& edm() { return edm_; }
  FormStructure& form() { return form_; }

  FieldGlobalId field(size_t i) const { return form_.fields()[i]->global_id(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAutofillAiWithDataSchema};
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  autofill::test::AutofillUnitTestEnvironment autofill_environment_;
  AutofillWebDataServiceTestHelper helper_{std::make_unique<EntityTable>()};
  EntityDataManager edm_{helper_.autofill_webdata_service(),
                         /*history_service=*/nullptr,
                         /*strike_database=*/nullptr};
  FormStructure form_{{}};
};

// If there are no Autofill AI fields, none is blocked.
TEST_F(GetFieldsFillableByAutofillAiTest, NoAutofillAiField) {
  AddOrUpdateEntityInstance(test::GetPassportEntityInstance());
  test_api(form()).SetFieldTypes({CREDIT_CARD_NAME_FULL, NAME_FULL});
  EXPECT_THAT(GetFieldsFillableByAutofillAi(form(), edm()), IsEmpty());
}

// If there is no Autofill AI entity that could fill the field, none is blocked.
TEST_F(GetFieldsFillableByAutofillAiTest, NameInFormButNotInEntity) {
  // The name is absent in the entity.
  AddOrUpdateEntityInstance(test::GetPassportEntityInstance({.name = nullptr}));
  test_api(form()).SetFieldTypes({CREDIT_CARD_NAME_FULL, NAME_FULL},
                                 {CREDIT_CARD_NAME_FULL, PASSPORT_NAME_TAG});
  EXPECT_THAT(GetFieldsFillableByAutofillAi(form(), edm()), IsEmpty());
}

// If there is a fillable AI field, it is blocked.
TEST_F(GetFieldsFillableByAutofillAiTest, FillableName) {
  AddOrUpdateEntityInstance(test::GetPassportEntityInstance());
  test_api(form()).SetFieldTypes({CREDIT_CARD_NAME_FULL, NAME_FULL},
                                 {CREDIT_CARD_NAME_FULL, PASSPORT_NAME_TAG});
  EXPECT_THAT(GetFieldsFillableByAutofillAi(form(), edm()),
              ElementsAre(field(1)));
}

// If there is a fillable AI field, it is blocked.
TEST_F(GetFieldsFillableByAutofillAiTest, FillableNumber) {
  AddOrUpdateEntityInstance(test::GetPassportEntityInstance());
  test_api(form()).SetFieldTypes({CREDIT_CARD_NAME_FULL, NAME_FULL},
                                 {CREDIT_CARD_NAME_FULL, PASSPORT_NUMBER});
  EXPECT_THAT(GetFieldsFillableByAutofillAi(form(), edm()),
              ElementsAre(field(1)));
}

class GetFillValueAndTypeForEntityTest : public testing::Test {
 public:
  GetFillValueAndTypeForEntityTest() = default;

 private:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
};

TEST_F(GetFillValueAndTypeForEntityTest, UnobfuscatedAttributes) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAiWithDataSchema};
  AutofillField field;
  {
    FieldPrediction prediction1;
    prediction1.set_type(NAME_FIRST);
    prediction1.set_source(
        autofill::AutofillQueryResponse::FormSuggestion::FieldSuggestion::
            FieldPrediction::SOURCE_AUTOFILL_DEFAULT);
    FieldPrediction prediction2;
    prediction1.set_type(PASSPORT_NAME_TAG);
    prediction1.set_source(
        autofill::AutofillQueryResponse::FormSuggestion::FieldSuggestion::
            FieldPrediction::SOURCE_AUTOFILL_AI);
    field.set_server_predictions({prediction1, prediction2});
  }

  constexpr char16_t kName[] = u"John";
  EntityInstance passport = test::GetPassportEntityInstance({.name = kName});
  EXPECT_EQ(GetFillValueAndTypeForEntity(
                passport, field, mojom::ActionPersistence::kPreview,
                kAppLocaleUS, /*address_normalizer=*/nullptr)
                .first,
            kName);
  EXPECT_EQ(GetFillValueAndTypeForEntity(
                passport, field, mojom::ActionPersistence::kFill, kAppLocaleUS,
                /*address_normalizer=*/nullptr)
                .first,
            kName);
}

TEST_F(GetFillValueAndTypeForEntityTest, ObfuscatedAttributes) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAiWithDataSchema};
  AutofillField field;
  {
    FieldPrediction prediction;
    prediction.set_type(PASSPORT_NUMBER);
    prediction.set_source(
        autofill::AutofillQueryResponse::FormSuggestion::FieldSuggestion::
            FieldPrediction::SOURCE_AUTOFILL_AI);
    field.set_server_predictions({prediction});
  }

  constexpr char16_t kNumber[] = u"12";
  EntityInstance passport =
      test::GetPassportEntityInstance({.number = kNumber});
  EXPECT_EQ(GetFillValueAndTypeForEntity(
                passport, field, mojom::ActionPersistence::kPreview,
                kAppLocaleUS, /*address_normalizer=*/nullptr)
                .first,
            u"\u2022\u2060\u2006\u2060\u2022\u2060\u2006\u2060");
  EXPECT_EQ(GetFillValueAndTypeForEntity(
                passport, field, mojom::ActionPersistence::kFill, kAppLocaleUS,
                /*address_normalizer=*/nullptr)
                .first,
            kNumber);
}

// Tests that we can correctly fill structured name information into fields.
TEST_F(GetFillValueAndTypeForEntityTest, FillingStructuredNames) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAiWithDataSchema};
  EntityInstance passport = test::GetPassportEntityInstance();
  for (const auto& [type, expectation] :
       std::vector<std::pair<FieldType, std::u16string>>{
           {NAME_FULL, u"Pippi Långstrump"},
           {NAME_FIRST, u"Pippi"},
           {NAME_LAST, u"Långstrump"}}) {
    AutofillField field;
    FieldPrediction prediction;
    prediction.set_type(PASSPORT_NAME_TAG);
    prediction.set_source(
        autofill::AutofillQueryResponse::FormSuggestion::FieldSuggestion::
            FieldPrediction::SOURCE_AUTOFILL_AI);
    field.set_server_predictions({prediction});
    field.SetTypeTo(type, AutofillPredictionSource::kServerCrowdsourcing);

    EXPECT_EQ(GetFillValueAndTypeForEntity(
                  passport, field, mojom::ActionPersistence::kFill,
                  kAppLocaleUS, /*address_normalizer=*/nullptr)
                  .first,
              expectation)
        << FieldTypeToStringView(type);
  }
}

// Tests that we can correctly fill country information into input fields
// according to various locales.
TEST_F(GetFillValueAndTypeForEntityTest, FillingLocalizedCountries) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAiWithDataSchema};
  EntityInstance passport =
      test::GetPassportEntityInstance({.country = u"Lebanon"});
  for (const auto& [locale, expectation] :
       std::vector<std::pair<std::string, std::u16string>>{
           {"en-US", u"Lebanon"},
           {"fr-FR", u"Liban"},
           {"de-DE", u"Libanon"},
           {"ar-LB", u"لبنان"}}) {
    AutofillField field;
    FieldPrediction prediction;
    prediction.set_type(PASSPORT_ISSUING_COUNTRY_TAG);
    prediction.set_source(
        autofill::AutofillQueryResponse::FormSuggestion::FieldSuggestion::
            FieldPrediction::SOURCE_AUTOFILL_AI);
    field.set_server_predictions({prediction});
    field.SetTypeTo(ADDRESS_HOME_COUNTRY,
                    AutofillPredictionSource::kServerCrowdsourcing);

    EXPECT_EQ(GetFillValueAndTypeForEntity(
                  passport, field, mojom::ActionPersistence::kFill, locale,
                  /*address_normalizer=*/nullptr)
                  .first,
              expectation)
        << locale;
  }
}

// Test that we can correctly fill country information into select fields,
// regardless of whether the internal representation of the element uses country
// names or codes.
TEST_F(GetFillValueAndTypeForEntityTest, FillingSelectControlWithCountries) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAiWithDataSchema};
  EntityInstance passport = test::GetPassportEntityInstance();
  for (const auto& [options, expectation] :
       std::vector<std::pair<std::vector<const char*>, std::u16string>>{
           {{"FR", "CA", "SE", "BR"}, u"SE"},
           {{"France", "Sweden", "Canada", "Brazil"}, u"Sweden"}}) {
    AutofillField field{test::CreateTestSelectField(options)};
    FieldPrediction prediction;
    prediction.set_type(PASSPORT_ISSUING_COUNTRY_TAG);
    prediction.set_source(
        autofill::AutofillQueryResponse::FormSuggestion::FieldSuggestion::
            FieldPrediction::SOURCE_AUTOFILL_AI);
    field.set_server_predictions({prediction});
    field.SetTypeTo(ADDRESS_HOME_COUNTRY,
                    AutofillPredictionSource::kServerCrowdsourcing);

    EXPECT_EQ(GetFillValueAndTypeForEntity(
                  passport, field, mojom::ActionPersistence::kFill,
                  kAppLocaleUS, /*address_normalizer=*/nullptr)
                  .first,
              expectation);
  }
}

class GetFillValueAndTypeForEntityStateTest
    : public GetFillValueAndTypeForEntityTest {
 public:
  GetFillValueAndTypeForEntityStateTest() {
    base::FilePath file_path;
    CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &file_path));
    file_path = file_path.Append(FILE_PATH_LITERAL("third_party"))
                    .Append(FILE_PATH_LITERAL("libaddressinput"))
                    .Append(FILE_PATH_LITERAL("src"))
                    .Append(FILE_PATH_LITERAL("testdata"))
                    .Append(FILE_PATH_LITERAL("countryinfo.txt"));

    normalizer_ = std::make_unique<AddressNormalizerImpl>(
        std::unique_ptr<Source>(
            new TestdataSource(true, file_path.AsUTF8Unsafe())),
        std::unique_ptr<Storage>(new NullStorage), "en-US");

    test::PopulateAlternativeStateNameMapForTesting(
        "US", "California",
        {{.canonical_name = "California",
          .abbreviations = {"CA"},
          .alternative_names = {}}});
    test::PopulateAlternativeStateNameMapForTesting(
        "US", "North Carolina",
        {{.canonical_name = "North Carolina",
          .abbreviations = {"NC"},
          .alternative_names = {}}});
    // Make sure the normalizer is done initializing its member(s) in
    // background task(s).
    task_environment_.RunUntilIdle();
  }

  GetFillValueAndTypeForEntityStateTest(
      const GetFillValueAndTypeForEntityStateTest&) = delete;
  GetFillValueAndTypeForEntityStateTest& operator=(
      const GetFillValueAndTypeForEntityStateTest&) = delete;

 protected:
  AddressNormalizer* normalizer() { return normalizer_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<AddressNormalizerImpl> normalizer_;
};

// Tests that we can correctly fill state information into input fields
// regardless whether the field is asking for the full name or the abbreviation.
TEST_F(GetFillValueAndTypeForEntityStateTest, FillingStateValueIntoInput) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAiWithDataSchema};
  EntityInstance drivers_license =
      test::GetDriversLicenseEntityInstance({.region = u"California"});
  for (const auto& [max_length, expectation] :
       std::vector<std::pair<size_t, std::u16string>>{{50u, u"California"},
                                                      {2u, u"CA"}}) {
    AutofillField field;
    FieldPrediction prediction;
    prediction.set_type(DRIVERS_LICENSE_REGION);
    prediction.set_source(
        autofill::AutofillQueryResponse::FormSuggestion::FieldSuggestion::
            FieldPrediction::SOURCE_AUTOFILL_AI);
    field.set_server_predictions({prediction});
    field.SetTypeTo(ADDRESS_HOME_STATE,
                    AutofillPredictionSource::kServerCrowdsourcing);
    field.set_max_length(max_length);

    EXPECT_EQ(GetFillValueAndTypeForEntity(drivers_license, field,
                                           mojom::ActionPersistence::kFill,
                                           /*app_locale=*/"", normalizer())
                  .first,
              expectation)
        << max_length;
  }
}

// Tests that we can correctly fill state information into select fields
// regardless whether the field is asking for the full name or the abbreviation.
TEST_F(GetFillValueAndTypeForEntityStateTest, FillingSelectControlWithState) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAiWithDataSchema};
  EntityInstance drivers_license =
      test::GetDriversLicenseEntityInstance({.region = u"California"});
  for (const auto& [options, expectation] :
       std::vector<std::pair<std::vector<const char*>, std::u16string>>{
           {{"NY", "CA", "IL", "NV"}, u"CA"},
           {{"New York", "California", "Illinois", "Nevada"}, u"California"}}) {
    AutofillField field{test::CreateTestSelectField(options)};
    FieldPrediction prediction;
    prediction.set_type(DRIVERS_LICENSE_REGION);
    prediction.set_source(
        autofill::AutofillQueryResponse::FormSuggestion::FieldSuggestion::
            FieldPrediction::SOURCE_AUTOFILL_AI);
    field.set_server_predictions({prediction});
    field.SetTypeTo(ADDRESS_HOME_STATE,
                    AutofillPredictionSource::kServerCrowdsourcing);

    EXPECT_EQ(GetFillValueAndTypeForEntity(drivers_license, field,
                                           mojom::ActionPersistence::kFill,
                                           /*app_locale=*/"", normalizer())
                  .first,
              expectation);
  }
}

}  // namespace
}  // namespace autofill
