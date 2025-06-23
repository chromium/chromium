// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/filling/autofill_ai/field_filling_entity_util.h"

#include "base/containers/to_vector.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/data_quality/addresses/address_normalizer_impl.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_processing/autofill_ai/determine_attribute_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
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
using FieldPrediction =
    AutofillQueryResponse::FormSuggestion::FieldSuggestion::FieldPrediction;

FieldPrediction CreatePrediction(
    FieldType type,
    FieldPrediction::Source source = FieldPrediction::SOURCE_AUTOFILL_AI) {
  FieldPrediction prediction;
  prediction.set_type(type);
  prediction.set_source(source);
  return prediction;
}

// Wrapper for GetFillValueForEntity() that calls DetermineAttributeTypes() for
// the single `field`.
std::u16string GetFillValueForEntity(
    const EntityInstance& entity,
    const std::unique_ptr<AutofillField>& field,
    mojom::ActionPersistence action_persistence,
    const std::string& app_locale = kAppLocaleUS,
    AddressNormalizer* address_normalizer = nullptr) {
  std::vector<AutofillFieldWithAttributeType> fields_and_types =
      DetermineAttributeTypes(base::span_from_ref(field), field->section(),
                              entity.type());

  // For a name field fake that there are other fields that dynamically
  // propagate to the name field.
  if (GroupTypeOfFieldType(field->Type().GetStorableType()) ==
          FieldTypeGroup::kName &&
      base::FeatureList::IsEnabled(features::kAutofillAiNoTagTypes)) {
    auto attribute_type = [&entity]() -> std::optional<AttributeType> {
      switch (entity.type().name()) {
        case EntityTypeName::kDriversLicense:
          return AttributeType(AttributeTypeName::kDriversLicenseName);
        case EntityTypeName::kPassport:
          return AttributeType(AttributeTypeName::kPassportName);
        case EntityTypeName::kVehicle:
          return AttributeType(AttributeTypeName::kVehicleOwner);
      }
      return std::nullopt;
    }();
    if (attribute_type) {
      fields_and_types.emplace_back(*field, *attribute_type);
    }
  }

  return GetFillValueForEntity(entity, fields_and_types, *field,
                               action_persistence, app_locale,
                               address_normalizer);
}

class GetFieldsFillableByAutofillAiTest : public testing::Test {
 public:
  GetFieldsFillableByAutofillAiTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kAutofillAiWithDataSchema,
                              features::kAutofillAiNoTagTypes},
        /*disabled_features=*/{});

    client().set_entity_data_manager(std::make_unique<EntityDataManager>(
        helper_.autofill_webdata_service(), /*history_service=*/nullptr,
        /*strike_database=*/nullptr));
    client().SetUpPrefsAndIdentityForAutofillAi();

    test_api(form_).PushField(
        test::CreateTestFormField("", "", "", FormControlType::kInputText));
    test_api(form_).PushField(
        test::CreateTestFormField("", "", "", FormControlType::kInputText));
  }

  void AddOrUpdateEntityInstance(EntityInstance entity) {
    edm().AddOrUpdateEntityInstance(std::move(entity));
    helper_.WaitUntilIdle();
  }

  TestAutofillClient& client() { return client_; }
  EntityDataManager& edm() { return *client().GetEntityDataManager(); }
  FormStructure& form() { return form_; }

  FieldGlobalId field(size_t i) const { return form_.fields()[i]->global_id(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::AutofillUnitTestEnvironment autofill_environment_;
  TestAutofillClient client_;
  AutofillWebDataServiceTestHelper helper_{std::make_unique<EntityTable>()};
  FormStructure form_{{}};
};

// If there are no Autofill AI fields, none is blocked.
TEST_F(GetFieldsFillableByAutofillAiTest, NoAutofillAiField) {
  AddOrUpdateEntityInstance(test::GetPassportEntityInstance());
  test_api(form()).SetFieldTypes({CREDIT_CARD_NAME_FULL, NAME_FULL});
  EXPECT_THAT(GetFieldsFillableByAutofillAi(form(), client()), IsEmpty());
}

// If there is no Autofill AI entity that could fill the field, none is blocked.
TEST_F(GetFieldsFillableByAutofillAiTest, NameInFormButNotInEntity) {
  // The name is absent in the entity.
  AddOrUpdateEntityInstance(test::GetPassportEntityInstance({.name = nullptr}));
  test_api(form()).SetFieldTypes({CREDIT_CARD_NAME_FULL, NAME_FULL},
                                 {CREDIT_CARD_NAME_FULL, NO_SERVER_DATA});
  EXPECT_THAT(GetFieldsFillableByAutofillAi(form(), client()), IsEmpty());
}

// If there is a fillable AI field, it is blocked.
TEST_F(GetFieldsFillableByAutofillAiTest, FillableName) {
  AddOrUpdateEntityInstance(test::GetPassportEntityInstance());
  test_api(form()).SetFieldTypes({NO_SERVER_DATA, NAME_FULL},
                                 {PASSPORT_EXPIRATION_DATE, NO_SERVER_DATA});
  EXPECT_THAT(GetFieldsFillableByAutofillAi(form(), client()),
              ElementsAre(field(0), field(1)));
}

// If there is a fillable AI field, it is blocked.
TEST_F(GetFieldsFillableByAutofillAiTest, FillableNumber) {
  AddOrUpdateEntityInstance(test::GetPassportEntityInstance());
  test_api(form()).SetFieldTypes({CREDIT_CARD_NAME_FULL, NAME_FULL},
                                 {CREDIT_CARD_NAME_FULL, PASSPORT_NUMBER});
  EXPECT_THAT(GetFieldsFillableByAutofillAi(form(), client()),
              ElementsAre(field(1)));
}

// If filling for AutofillAI is not permitted, no fields are blocked even if
// there is an AutofillAI-related field and there is data in the
// EntityDataManager.
TEST_F(GetFieldsFillableByAutofillAiTest, FillingUnavailable) {
  client().SetCanUseModelExecutionFeatures(false);
  AddOrUpdateEntityInstance(test::GetPassportEntityInstance());
  test_api(form()).SetFieldTypes({CREDIT_CARD_NAME_FULL, NAME_FULL},
                                 {CREDIT_CARD_NAME_FULL, NO_SERVER_DATA});
  EXPECT_THAT(GetFieldsFillableByAutofillAi(form(), client()), IsEmpty());
}

class GetFillValueForEntityTest : public testing::Test {
 public:
  GetFillValueForEntityTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kAutofillAiWithDataSchema,
                              features::kAutofillAiNoTagTypes},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
};

TEST_F(GetFillValueForEntityTest, UnobfuscatedAttributes) {
  auto field = std::make_unique<AutofillField>();
  field->set_server_predictions(
      {CreatePrediction(NAME_FIRST, FieldPrediction::SOURCE_AUTOFILL_DEFAULT)});
  field->SetTypeTo(NAME_FIRST, AutofillPredictionSource::kServerCrowdsourcing);

  EntityInstance passport =
      test::GetPassportEntityInstance({.name = u"John Doe"});
  EXPECT_EQ(GetFillValueForEntity(passport, field,
                                  mojom::ActionPersistence::kPreview),
            u"John");
  EXPECT_EQ(
      GetFillValueForEntity(passport, field, mojom::ActionPersistence::kFill),
      u"John");
}

TEST_F(GetFillValueForEntityTest, ObfuscatedAttributes) {
  auto field = std::make_unique<AutofillField>();
  field->set_server_predictions({CreatePrediction(PASSPORT_NUMBER)});

  constexpr char16_t kNumber[] = u"12";
  EntityInstance passport =
      test::GetPassportEntityInstance({.number = kNumber});
  EXPECT_EQ(GetFillValueForEntity(passport, field,
                                  mojom::ActionPersistence::kPreview),
            u"\u2022\u2060\u2006\u2060\u2022\u2060\u2006\u2060");
  EXPECT_EQ(
      GetFillValueForEntity(passport, field, mojom::ActionPersistence::kFill),
      kNumber);
}

// Tests that we can correctly fill structured name information into fields.
TEST_F(GetFillValueForEntityTest, FillingStructuredNames) {
  EntityInstance passport = test::GetPassportEntityInstance();
  for (const auto& [type, expectation] :
       std::vector<std::pair<FieldType, std::u16string>>{
           {NAME_FULL, u"Pippi Långstrump"},
           {NAME_FIRST, u"Pippi"},
           {NAME_LAST, u"Långstrump"}}) {
    auto field = std::make_unique<AutofillField>();
    field->SetTypeTo(type, AutofillPredictionSource::kServerCrowdsourcing);

    EXPECT_EQ(
        GetFillValueForEntity(passport, field, mojom::ActionPersistence::kFill),
        expectation)
        << FieldTypeToStringView(type);
  }
}

// Tests that we can correctly fill country information into input fields
// according to various locales.
TEST_F(GetFillValueForEntityTest, FillingLocalizedCountries) {
  EntityInstance passport =
      test::GetPassportEntityInstance({.country = u"Lebanon"});
  for (const auto& [locale, expectation] :
       std::vector<std::pair<std::string, std::u16string>>{
           {"en-US", u"Lebanon"},
           {"fr-FR", u"Liban"},
           {"de-DE", u"Libanon"},
           {"ar-LB", u"لبنان"}}) {
    auto field = std::make_unique<AutofillField>();
    field->set_server_predictions({CreatePrediction(PASSPORT_ISSUING_COUNTRY)});
    field->SetTypeTo(ADDRESS_HOME_COUNTRY,
                     AutofillPredictionSource::kServerCrowdsourcing);

    EXPECT_EQ(GetFillValueForEntity(passport, field,
                                    mojom::ActionPersistence::kFill, locale),
              expectation)
        << locale;
  }
}

// Test that we can correctly fill country information into select fields,
// regardless of whether the internal representation of the element uses country
// names or codes.
TEST_F(GetFillValueForEntityTest, FillingSelectControlWithCountries) {
  EntityInstance passport = test::GetPassportEntityInstance();
  for (const auto& [options, expectation] :
       std::vector<std::pair<std::vector<const char*>, std::u16string>>{
           {{"FR", "CA", "SE", "BR"}, u"SE"},
           {{"France", "Sweden", "Canada", "Brazil"}, u"Sweden"}}) {
    auto field =
        std::make_unique<AutofillField>(test::CreateTestSelectField(options));
    field->set_server_predictions({CreatePrediction(PASSPORT_ISSUING_COUNTRY)});
    field->SetTypeTo(ADDRESS_HOME_COUNTRY,
                     AutofillPredictionSource::kServerCrowdsourcing);

    EXPECT_EQ(
        GetFillValueForEntity(passport, field, mojom::ActionPersistence::kFill),
        expectation);
  }
}

TEST_F(GetFillValueForEntityTest, DifferentEntities) {
  auto field = std::make_unique<AutofillField>();
  field->set_server_predictions({CreatePrediction(VEHICLE_LICENSE_PLATE)});

  EntityInstance drivers_license = test::GetDriversLicenseEntityInstance();
  EXPECT_EQ(GetFillValueForEntity(drivers_license, field,
                                  mojom::ActionPersistence::kPreview),
            u"");
}

class GetFillValueForEntityStateTest : public GetFillValueForEntityTest {
 public:
  GetFillValueForEntityStateTest() {
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

  GetFillValueForEntityStateTest(const GetFillValueForEntityStateTest&) =
      delete;
  GetFillValueForEntityStateTest& operator=(
      const GetFillValueForEntityStateTest&) = delete;

 protected:
  AddressNormalizer* normalizer() { return normalizer_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<AddressNormalizerImpl> normalizer_;
};

// Tests that we can correctly fill state information into input fields
// regardless whether the field is asking for the full name or the abbreviation.
TEST_F(GetFillValueForEntityStateTest, FillingStateValueIntoInput) {
  EntityInstance drivers_license =
      test::GetDriversLicenseEntityInstance({.region = u"California"});
  for (const auto& [max_length, expectation] :
       std::vector<std::pair<size_t, std::u16string>>{{50u, u"California"},
                                                      {2u, u"CA"}}) {
    auto field = std::make_unique<AutofillField>();
    field->set_server_predictions({CreatePrediction(DRIVERS_LICENSE_REGION)});
    field->SetTypeTo(ADDRESS_HOME_STATE,
                     AutofillPredictionSource::kServerCrowdsourcing);
    field->set_max_length(max_length);

    EXPECT_EQ(GetFillValueForEntity(drivers_license, field,
                                    mojom::ActionPersistence::kFill,
                                    /*app_locale=*/"", normalizer()),
              expectation)
        << max_length;
  }
}

// Tests that we can correctly fill state information into select fields
// regardless whether the field is asking for the full name or the abbreviation.
TEST_F(GetFillValueForEntityStateTest, FillingSelectControlWithState) {
  EntityInstance drivers_license =
      test::GetDriversLicenseEntityInstance({.region = u"California"});
  for (const auto& [options, expectation] :
       std::vector<std::pair<std::vector<const char*>, std::u16string>>{
           {{"NY", "CA", "IL", "NV"}, u"CA"},
           {{"New York", "California", "Illinois", "Nevada"}, u"California"}}) {
    auto field =
        std::make_unique<AutofillField>(test::CreateTestSelectField(options));
    field->set_server_predictions({CreatePrediction(DRIVERS_LICENSE_REGION)});
    field->SetTypeTo(ADDRESS_HOME_STATE,
                     AutofillPredictionSource::kServerCrowdsourcing);

    EXPECT_EQ(GetFillValueForEntity(drivers_license, field,
                                    mojom::ActionPersistence::kFill,
                                    /*app_locale=*/"", normalizer()),
              expectation);
  }
}

// Test fixture for date tests.
class GetFillValueForEntityTest_Date : public GetFillValueForEntityTest {
 public:
  GetFillValueForEntityTest_Date() = default;

  EntityInstance passport() const {
    return test::GetPassportEntityInstance({.issue_date = u"2022-12-16"});
  }

  std::unique_ptr<AutofillField> CreateInput(
      FormControlType form_control_type) {
    auto field = std::make_unique<AutofillField>(
        test::CreateTestFormField(/*label=*/"",
                                  /*name=*/"",
                                  /*value=*/"", form_control_type));
    field->set_server_predictions({CreatePrediction(PASSPORT_ISSUE_DATE)});
    return field;
  }

  std::unique_ptr<AutofillField> CreateSelect(
      const std::vector<std::string>& values,
      const std::vector<std::string>& texts) {
    auto field = std::make_unique<AutofillField>(test::CreateTestSelectField(
        /*label=*/"", /*name=*/"", /*value=*/"",
        /*autocomplete=*/"",
        /*values=*/base::ToVector(values, &std::string::c_str),
        /*contents=*/base::ToVector(texts, &std::string::c_str)));
    field->set_server_predictions({CreatePrediction(PASSPORT_ISSUE_DATE)});
    return field;
  }
};

// Tests that a date is filled into an input according to the format string.
TEST_F(GetFillValueForEntityTest_Date, FillingDateValueIntoTextInput) {
  auto field = CreateInput(FormControlType::kInputText);
  field->set_format_string_unless_overruled(
      u"DD/MM/YYYY", AutofillField::FormatStringSource::kServer);
  EXPECT_EQ(
      GetFillValueForEntity(passport(), field, mojom::ActionPersistence::kFill,
                            /*app_locale=*/"",
                            /*address_normalizer=*/nullptr),
      u"16/12/2022");
}

// Tests that a date is filled into an input according to the format string.
TEST_F(GetFillValueForEntityTest_Date, FillingDateValueIntoDateInput) {
  auto field = CreateInput(FormControlType::kInputDate);
  EXPECT_EQ(
      GetFillValueForEntity(passport(), field, mojom::ActionPersistence::kFill,
                            /*app_locale=*/"",
                            /*address_normalizer=*/nullptr),
      u"2022-12-16");
}

// Tests that a date is filled into an input according to the format string.
TEST_F(GetFillValueForEntityTest_Date, FillingDateValueIntoMonthInput) {
  auto field = CreateInput(FormControlType::kInputMonth);
  EXPECT_EQ(
      GetFillValueForEntity(passport(), field, mojom::ActionPersistence::kFill,
                            /*app_locale=*/"",
                            /*address_normalizer=*/nullptr),
      u"2022-12");
}

// Describes number-based options of a <select> field.
struct DateRangeParam {
  // Generates the values or texts of the <select> field.
  std::vector<std::string> Create() const {
    std::vector<int> nums;
    nums.reserve(max - min);
    for (int i = min; i <= max; ++i) {
      nums.push_back(i);
    }

    std::vector<std::string> range = base::ToVector(nums, [&](int value) {
      return base::StrCat({prefix, base::NumberToString(value), suffix});
    });

    if (header) {
      range.insert(range.begin(), header);
    }
    if (footer) {
      range.push_back(footer);
    }
    return range;
  }

  // Replaces digits with their written out representation.
  static std::string Obfuscate(std::string_view number) {
    std::string str;
    while (!number.empty()) {
      if (base::IsAsciiDigit(number.front())) {
        constexpr auto kDigits = std::to_array<std::string_view>(
            {"zero", "one", "two", "three", "four", "five", "six", "seven",
             "eight", "nine"});
        str += kDigits[number.front() - '0'];
      } else {
        str += number.front();
      }
      number = number.substr(1);
    }
    return str;
  }

  std::string_view expectation = "";  // Expected filled value.
  int min = 0;                        // Smallest number (inclusive).
  int max = -1;                       // Largest number (inclusive).
  std::string_view prefix = "";       // Prefix added to each option.
  std::string_view suffix = "";       // Suffix added to each option.
  const char* header = nullptr;       // First option (if any).
  const char* footer = nullptr;       // Last option (if any).
  enum {
    kValuesAndTexts,  // Have equal value and text in each option.
    kValuesOnly,      // Keep the values; obfuscate the texts.
    kTextsOnly        // Keep the texts; obfuscate the values.
  } meaningful = kValuesAndTexts;
};

class GetFillValueForEntityTest_Date_Select_Integral
    : public GetFillValueForEntityTest_Date,
      public testing::WithParamInterface<DateRangeParam> {};

// Test case for filling a date into a <select> whose values and/or texts
// contain numbers.
TEST_P(GetFillValueForEntityTest_Date_Select_Integral, GetFillValueForEntity) {
  const DateRangeParam& p = GetParam();
  std::vector<std::string> values = p.Create();
  std::vector<std::string> texts = p.Create();

  if (p.meaningful == DateRangeParam::kTextsOnly) {
    values = base::ToVector(values, DateRangeParam::Obfuscate);
  }
  if (p.meaningful == DateRangeParam::kValuesOnly) {
    texts = base::ToVector(texts, DateRangeParam::Obfuscate);
  }

  SCOPED_TRACE(testing::Message()
               << "values: " << base::JoinString(values, ", ")
               << " / texts = " << base::JoinString(texts, ", "));
  EXPECT_EQ(GetFillValueForEntity(passport(),
                                  CreateSelect(
                                      /*values=*/values,
                                      /*texts=*/texts),
                                  mojom::ActionPersistence::kFill,
                                  /*app_locale=*/""),
            base::UTF8ToUTF16(p.expectation));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    GetFillValueForEntityTest_Date_Select_Integral,
    testing::Values(
        // Day: positive cases.
        DateRangeParam{.expectation = "16", .min = 1, .max = 28},
        DateRangeParam{.expectation = "16", .min = 1, .max = 31},
        DateRangeParam{.expectation = "15",
                       .min = 0,
                       .max = 30,
                       .meaningful = DateRangeParam::kValuesOnly},
        DateRangeParam{.expectation = "foo-16",
                       .min = 1,
                       .max = 31,
                       .prefix = "foo-"},
        DateRangeParam{.expectation = "16-bar",
                       .min = 1,
                       .max = 31,
                       .suffix = "-bar"},
        DateRangeParam{.expectation = "foo-16-bar",
                       .min = 1,
                       .max = 31,
                       .prefix = "foo-",
                       .suffix = "-bar"},
        DateRangeParam{.expectation = "16",
                       .min = 1,
                       .max = 31,
                       .header = "Select day"},
        DateRangeParam{.expectation = "16",
                       .min = 1,
                       .max = 31,
                       .footer = "Select day"},
        DateRangeParam{.expectation = "16",
                       .min = 1,
                       .max = 31,
                       .header = "Select ...",
                       .footer = "... day"},

        // Day: negative cases.
        DateRangeParam{.expectation = "", .min = 1, .max = 10},
        DateRangeParam{.expectation = "",
                       .min = 0,
                       .max = 30,
                       .meaningful = DateRangeParam::kTextsOnly},
        // If there are fewer or more days than a
        // month has, it happens to fall back to the
        // year.
        DateRangeParam{.expectation = "22", .min = 1, .max = 27},
        DateRangeParam{.expectation = "22", .min = 1, .max = 32},

        // Month: positive cases.
        DateRangeParam{.expectation = "12", .min = 1, .max = 12},
        DateRangeParam{.expectation = "12", .min = 1, .max = 12},
        DateRangeParam{.expectation = "11",
                       .min = 0,
                       .max = 11,
                       .meaningful = DateRangeParam::kValuesOnly},
        DateRangeParam{.expectation = "foo-12",
                       .min = 1,
                       .max = 12,
                       .prefix = "foo-"},
        DateRangeParam{.expectation = "12-bar",
                       .min = 1,
                       .max = 12,
                       .suffix = "-bar"},
        DateRangeParam{.expectation = "foo-12-bar",
                       .min = 1,
                       .max = 12,
                       .prefix = "foo-",
                       .suffix = "-bar"},
        DateRangeParam{.expectation = "12",
                       .min = 1,
                       .max = 12,
                       .header = "Select month"},
        DateRangeParam{.expectation = "12",
                       .min = 1,
                       .max = 12,
                       .footer = "Select month"},
        DateRangeParam{.expectation = "12",
                       .min = 1,
                       .max = 12,
                       .header = "Select ...",
                       .footer = "... month"},

        // Month: negative cases.
        DateRangeParam{.expectation = "",
                       .min = 0,
                       .max = 11,
                       .meaningful = DateRangeParam::kTextsOnly},
        DateRangeParam{.expectation = "", .min = 1, .max = 11},
        DateRangeParam{.expectation = "", .min = 1, .max = 13},

        // Year: positive cases.
        DateRangeParam{.expectation = "2022", .min = 2020, .max = 2099},
        DateRangeParam{.expectation = "2022", .min = 2020, .max = 2032},
        DateRangeParam{.expectation = "22", .min = 20, .max = 32},
        DateRangeParam{.expectation = "Year 2022",
                       .min = 2000,
                       .max = 2099,
                       .prefix = "Year "},
        DateRangeParam{.expectation = "2022 n. Chr.",
                       .min = 2000,
                       .max = 2099,
                       .suffix = " n. Chr."}));

// Tests that a date is filled into a select element that seems to contain
// months.
TEST_F(GetFillValueForEntityTest_Date, MonthString) {
  std::vector<std::string> months = {
      "January", "February", "March",     "April",   "May",      "June",
      "July",    "August",   "September", "October", "November", "December"};
  auto field = CreateSelect(months, months);
  EXPECT_EQ(
      GetFillValueForEntity(passport(), field, mojom::ActionPersistence::kFill,
                            /*app_locale=*/""),
      u"December");
}

// Tests that a date is filled into a select element that seems to contain
// months.
TEST_F(GetFillValueForEntityTest_Date, MonthStringAbbreviations) {
  std::vector<std::string> months = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                     "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  auto field = CreateSelect(months, months);
  EXPECT_EQ(
      GetFillValueForEntity(passport(), field, mojom::ActionPersistence::kFill,
                            /*app_locale=*/""),
      u"Dec");
}

}  // namespace
}  // namespace autofill
