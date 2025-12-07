// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_import_utils.h"

#include <memory>
#include <string>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/icu_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_test_helper.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Property;
using ::testing::UnorderedElementsAre;
using enum AttributeTypeName;

std::vector<std::string> GetMonths() {
  static const std::vector<std::string> kMonths = {
      "January", "February", "March",     "April",   "May",      "June",
      "July",    "August",   "September", "October", "November", "December"};
  return kMonths;
}

// Creates a range containing [min, min+1, ..., max] (both inclusive).
std::vector<std::string> Range(int min, int max) {
  std::vector<std::string> v;
  if (min <= max) {
    v.reserve(max - min + 1);
    while (min <= max) {
      v.push_back(base::NumberToString(min++));
    }
  }
  return v;
}

AttributeInstance CreateAttribute(AttributeTypeName name,
                                  std::string_view value) {
  AttributeType type = AttributeType(name);
  AttributeInstance instance = AttributeInstance(type);
  instance.SetRawInfo(type.field_type(), base::UTF8ToUTF16(value),
                      VerificationStatus::kObserved);
  instance.FinalizeInfo();
  return instance;
}

void AddPrediction(AutofillField& field,
                   const std::vector<FieldType>& field_types) {
  field.set_server_predictions(
      base::ToVector(field_types, [](FieldType field_type) {
        FieldPrediction prediction;
        prediction.set_type(field_type);
        prediction.set_source(
            AutofillQueryResponse::FormSuggestion::FieldSuggestion::
                FieldPrediction::SOURCE_AUTOFILL_AI);
        return prediction;
      }));
}

std::unique_ptr<AutofillField> CreateInput(
    FormControlType form_control_type,
    const std::vector<FieldType>& field_types,
    std::string_view value,
    std::optional<AutofillFormatString> format_string = std::nullopt,
    std::string_view initial_value = "") {
  auto field = std::make_unique<AutofillField>(test::CreateTestFormField(
      /*label=*/"",
      /*name=*/"",
      /*value=*/initial_value, form_control_type));
  // Explicitly set the value here to ensure that it differs from the initial
  // value.
  field->set_value(base::UTF8ToUTF16(value));
  if (format_string) {
    field->set_format_string_unless_overruled(
        std::move(*format_string), AutofillFormatStringSource::kServer);
  }
  AddPrediction(*field, field_types);
  return field;
}

std::unique_ptr<AutofillField> CreateInput(
    FormControlType form_control_type,
    FieldType field_type,
    std::string_view value,
    std::optional<AutofillFormatString> format_string = std::nullopt,
    std::string_view initial_value = "") {
  return CreateInput(form_control_type, std::vector<FieldType>{field_type},
                     value, std::move(format_string), initial_value);
}

std::unique_ptr<AutofillField> CreateSelect(
    std::vector<std::string> values,
    std::vector<std::string> texts,
    const std::vector<FieldType>& field_types,
    std::string value) {
  values.resize(std::max(values.size(), texts.size()));
  texts.resize(std::max(values.size(), texts.size()));
  auto field = std::make_unique<AutofillField>(test::CreateTestSelectField(
      /*label=*/"", /*name=*/"", /*value=*/value,
      /*autocomplete=*/"",
      /*values=*/base::ToVector(values, &std::string::c_str),
      /*contents=*/base::ToVector(texts, &std::string::c_str)));
  AddPrediction(*field, field_types);
  return field;
}

std::unique_ptr<AutofillField> CreateSelect(std::vector<std::string> values,
                                            std::vector<std::string> texts,
                                            FieldType field_type,
                                            std::string value) {
  return CreateSelect(values, texts, std::vector<FieldType>{field_type}, value);
}

class AutofillAiImportUtilsTest : public testing::Test {
 public:
  AutofillAiImportUtilsTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kAutofillAiWithDataSchema,
                              features::kAutofillAiWalletVehicleRegistration},
        /*disabled_features=*/{});
    autofill_client().set_sync_service(&sync_service_);
    autofill_client().set_entity_data_manager(
        std::make_unique<EntityDataManager>(
            autofill_client().GetPrefs(),
            autofill_client().GetIdentityManager(),
            autofill_client().GetSyncService(),
            webdata_helper_.autofill_webdata_service(),
            /*history_service=*/nullptr,
            /*strike_database=*/nullptr));
    autofill_client().SetUpPrefsAndIdentityForAutofillAi();
    autofill_client().GetSyncService()->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kPayments, true);
    autofill_client().set_app_locale("en-US");
    autofill_client().SetVariationConfigCountryCode(GeoIpCountryCode("US"));
  }

  TestAutofillClient& autofill_client() { return autofill_client_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  test::AutofillUnitTestEnvironment autofill_environment_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  AutofillWebDataServiceTestHelper webdata_helper_{
      std::make_unique<EntityTable>()};
  syncer::TestSyncService sync_service_;
  TestAutofillClient autofill_client_;
};

// Tests import that includes and a date distributed over three <input>
// elements.
TEST_F(AutofillAiImportUtilsTest, ImportFromInput) {
  std::vector<std::unique_ptr<AutofillField>> fields;
  fields.push_back(CreateInput(FormControlType::kInputText,
                               FieldType::PASSPORT_NUMBER, "123"));
  fields.push_back(CreateInput(FormControlType::kInputText,
                               FieldType::NAME_FULL, "Karlsson on the Roof"));
  // Input fields for which the initial value is the same as the current value
  // are ignored during import.
  fields.push_back(CreateInput(FormControlType::kInputText,
                               FieldType::PASSPORT_ISSUING_COUNTRY, "Sweden",
                               /*format_string=*/std::nullopt, "Sweden"));
  fields.push_back(
      CreateInput(FormControlType::kInputText, FieldType::PASSPORT_ISSUE_DATE,
                  "24", AutofillFormatString(u"DD", FormatString_Type_DATE)));
  fields.push_back(
      CreateInput(FormControlType::kInputText, FieldType::PASSPORT_ISSUE_DATE,
                  "12", AutofillFormatString(u"MM", FormatString_Type_DATE)));
  fields.push_back(CreateInput(
      FormControlType::kInputText, FieldType::PASSPORT_ISSUE_DATE, "2025",
      AutofillFormatString(u"YYYY", FormatString_Type_DATE)));

  EXPECT_THAT(GetPossibleEntitiesFromSubmittedForm(fields, autofill_client()),
              ElementsAre(Property(
                  &EntityInstance::attributes,
                  UnorderedElementsAre(
                      CreateAttribute(kPassportNumber, "123"),
                      CreateAttribute(kPassportName, "Karlsson on the Roof"),
                      CreateAttribute(kPassportIssueDate, "2025-12-24")))));
}

// Tests that non walletable entities, such as passports, are saved locally.
TEST_F(AutofillAiImportUtilsTest,
       ImportFromInput_RecordType_NonWalletableEntity_Local) {
  std::vector<std::unique_ptr<AutofillField>> fields;
  fields.push_back(CreateInput(FormControlType::kInputText,
                               FieldType::PASSPORT_NUMBER, "123"));
  fields.push_back(CreateInput(FormControlType::kInputText,
                               FieldType::PASSPORT_ISSUING_COUNTRY, "Germany"));

  std::vector<EntityInstance> possible_entities =
      GetPossibleEntitiesFromSubmittedForm(fields, autofill_client());
  ASSERT_EQ(possible_entities.size(), 1u);
  EXPECT_EQ(possible_entities[0].record_type(),
            EntityInstance::RecordType::kLocal);
}

// Tests that walletable entities, such as vehicles are saved on the Google
// Wallet server.
TEST_F(AutofillAiImportUtilsTest, ImportFromInput_RecordType_Server) {
  std::vector<std::unique_ptr<AutofillField>> fields;
  fields.push_back(
      CreateInput(FormControlType::kInputText, FieldType::VEHICLE_VIN, "123"));
  fields.push_back(CreateInput(FormControlType::kInputText,
                               FieldType::VEHICLE_LICENSE_PLATE, "321"));

  std::vector<EntityInstance> possible_entities =
      GetPossibleEntitiesFromSubmittedForm(fields, autofill_client());
  ASSERT_EQ(possible_entities.size(), 1u);
  EXPECT_EQ(possible_entities[0].record_type(),
            EntityInstance::RecordType::kServerWallet);
}

// Tests that walletable entities are not saved on the Google Wallet servers if
// sync us disabled.
TEST_F(AutofillAiImportUtilsTest, ImportFromInput_RecordType_SyncOff_Local) {
  autofill_client().GetSyncService()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kPayments, false);
  std::vector<std::unique_ptr<AutofillField>> fields;
  fields.push_back(
      CreateInput(FormControlType::kInputText, FieldType::VEHICLE_VIN, "123"));
  fields.push_back(CreateInput(FormControlType::kInputText,
                               FieldType::VEHICLE_LICENSE_PLATE, "321"));

  std::vector<EntityInstance> possible_entities =
      GetPossibleEntitiesFromSubmittedForm(fields, autofill_client());
  ASSERT_EQ(possible_entities.size(), 1u);
  EXPECT_EQ(possible_entities[0].record_type(),
            EntityInstance::RecordType::kLocal);
}

// Tests that walletable entities are not saved on the Google Wallet server if
// the flag is off.
TEST_F(AutofillAiImportUtilsTest, ImportFromInput_RecordType_FeatureOff_Local) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAutofillAiWalletVehicleRegistration);
  std::vector<std::unique_ptr<AutofillField>> fields;
  fields.push_back(
      CreateInput(FormControlType::kInputText, FieldType::VEHICLE_VIN, "123"));
  fields.push_back(CreateInput(FormControlType::kInputText,
                               FieldType::VEHICLE_LICENSE_PLATE, "321"));

  std::vector<EntityInstance> possible_entities =
      GetPossibleEntitiesFromSubmittedForm(fields, autofill_client());
  ASSERT_EQ(possible_entities.size(), 1u);
  EXPECT_EQ(possible_entities[0].record_type(),
            EntityInstance::RecordType::kLocal);
}

// Tests that we do not import any attribute whose value has a value email
// address format
TEST_F(AutofillAiImportUtilsTest, NoEmailAddressImport) {
  std::vector<std::unique_ptr<AutofillField>> fields;
  fields.push_back(CreateInput(FormControlType::kInputText,
                               FieldType::PASSPORT_NUMBER, "foo@bar.com"));

  EXPECT_THAT(GetPossibleEntitiesFromSubmittedForm(fields, autofill_client()),
              IsEmpty());
}

// Tests import that includes a date distributed over three <select> elements.
TEST_F(AutofillAiImportUtilsTest, ImportFromDateSelect) {
  std::vector<std::unique_ptr<AutofillField>> fields;
  fields.push_back(CreateInput(FormControlType::kInputText,
                               FieldType::PASSPORT_NUMBER, "123"));
  fields.push_back(CreateSelect(Range(2000, 2099), {},
                                FieldType::PASSPORT_ISSUE_DATE, "2025"));
  fields.push_back(CreateSelect(GetMonths(), {}, FieldType::PASSPORT_ISSUE_DATE,
                                "December"));
  fields.push_back(
      CreateSelect(Range(0, 30), {}, FieldType::PASSPORT_ISSUE_DATE, "23"));

  EXPECT_THAT(GetPossibleEntitiesFromSubmittedForm(fields, autofill_client()),
              ElementsAre(Property(
                  &EntityInstance::attributes,
                  UnorderedElementsAre(
                      CreateAttribute(kPassportNumber, "123"),
                      CreateAttribute(kPassportIssueDate, "2025-12-24")))));
}

// Tests that importing from a non-date <select> element uses the `text` of the
// select option and not its `value`.
TEST_F(AutofillAiImportUtilsTest, ImportFromNonDateSelect) {
  std::vector<std::unique_ptr<AutofillField>> fields;
  fields.push_back(CreateInput(FormControlType::kInputText,
                               FieldType::PASSPORT_NUMBER, "123"));
  fields.push_back(CreateSelect(Range(1, 3), {"Germany", "USA", "Vietnam"},
                                FieldType::PASSPORT_ISSUING_COUNTRY, "2"));

  // `CreateAttribute` requires that we use the country code.
  EXPECT_THAT(
      GetPossibleEntitiesFromSubmittedForm(fields, autofill_client()),
      ElementsAre(Property(
          &EntityInstance::attributes,
          UnorderedElementsAre(CreateAttribute(kPassportNumber, "123"),
                               CreateAttribute(kPassportCountry, "US")))));
}

// Tests that if a field is a proper affix, the entity is not imported.
TEST_F(AutofillAiImportUtilsTest, DoNotImportAffixes) {
  std::vector<std::unique_ptr<AutofillField>> fields;
  fields.push_back(CreateInput(FormControlType::kInputText,
                               FieldType::NAME_FULL, "Karlsson on the Roof"));
  fields.push_back(CreateInput(FormControlType::kInputText,
                               FieldType::PASSPORT_NUMBER, "123"));
  fields.push_back(CreateInput(FormControlType::kInputText,
                               FieldType::DRIVERS_LICENSE_NUMBER, "12345678"));
  ASSERT_THAT(
      GetPossibleEntitiesFromSubmittedForm(fields, autofill_client()),
      UnorderedElementsAre(
          Property(&EntityInstance::attributes,
                   UnorderedElementsAre(
                       CreateAttribute(kPassportNumber, "123"),
                       CreateAttribute(kPassportName, "Karlsson on the Roof"))),
          Property(&EntityInstance::attributes,
                   UnorderedElementsAre(
                       CreateAttribute(kDriversLicenseNumber, "12345678"),
                       CreateAttribute(kDriversLicenseName,
                                       "Karlsson on the Roof")))));

  auto from_affix = [](std::u16string fs) {
    return AutofillFormatString(std::move(fs), FormatString_Type_AFFIX);
  };
  fields[1]->set_format_string_unless_overruled(
      from_affix(u"3"), AutofillFormatStringSource::kServer);
  fields[2]->set_format_string_unless_overruled(
      from_affix(u"0"), AutofillFormatStringSource::kServer);
  EXPECT_THAT(
      GetPossibleEntitiesFromSubmittedForm(fields, autofill_client()),
      UnorderedElementsAre(Property(
          &EntityInstance::attributes,
          UnorderedElementsAre(
              CreateAttribute(kDriversLicenseNumber, "12345678"),
              CreateAttribute(kDriversLicenseName, "Karlsson on the Roof")))));
}

// Tests that entities created from overloaded fields (i.e., fields that
// contribute to multiple entities) are not offered for imported.
TEST_F(AutofillAiImportUtilsTest, DoNotImportOverloadedFields) {
  std::vector<std::unique_ptr<AutofillField>> fields;
  fields.push_back(CreateInput(FormControlType::kInputText,
                               {PASSPORT_NUMBER, DRIVERS_LICENSE_NUMBER},
                               "123"));
  fields.push_back(CreateInput(FormControlType::kInputText, NAME_FULL,
                               "Karlsson on the Roof"));
  EXPECT_THAT(GetPossibleEntitiesFromSubmittedForm(fields, autofill_client()),
              IsEmpty());

  fields.push_back(
      CreateInput(FormControlType::kInputText, VEHICLE_VIN, "456"));
  EXPECT_THAT(
      GetPossibleEntitiesFromSubmittedForm(fields, autofill_client()),
      ElementsAre(Property(
          &EntityInstance::attributes,
          UnorderedElementsAre(
              CreateAttribute(kVehicleVin, "456"),
              CreateAttribute(kVehicleOwner, "Karlsson on the Roof")))));
}

// Tests that national id cards are imported unless there the country code
// belongs to India.
TEST_F(AutofillAiImportUtilsTest, DoNotImportNationalIdCardInIndia) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAiNationalIdCard};

  std::vector<std::unique_ptr<AutofillField>> fields;
  fields.push_back(
      CreateInput(FormControlType::kInputText, NATIONAL_ID_CARD_NUMBER, "123"));
  EXPECT_THAT(
      GetPossibleEntitiesFromSubmittedForm(fields, autofill_client()),
      ElementsAre(Property(&EntityInstance::type,
                           EntityType(EntityTypeName::kNationalIdCard))));
  autofill_client().SetVariationConfigCountryCode(GeoIpCountryCode("IN"));
  EXPECT_THAT(GetPossibleEntitiesFromSubmittedForm(fields, autofill_client()),
              IsEmpty());
}

TEST_F(AutofillAiImportUtilsTest, MaybeGetLocalizedDate) {
  using enum AttributeTypeName;
  base::test::ScopedRestoreICUDefaultLocale restore_default_locale;

  EntityInstance entity =
      test::GetPassportEntityInstance({.expiry_date = u"2025-12-30"});
  {
    AttributeInstance a =
        entity.attribute(AttributeType(kPassportExpirationDate)).value();
    auto optional = [](std::u16string s) { return Optional(s); };
    base::i18n::SetICUDefaultLocale("en_US");
    EXPECT_THAT(MaybeGetLocalizedDate(a), optional(u"Dec 30, 2025"));
    base::i18n::SetICUDefaultLocale("en_GB");
    EXPECT_THAT(MaybeGetLocalizedDate(a), optional(u"30 Dec 2025"));
    base::i18n::SetICUDefaultLocale("de_DE");
    EXPECT_THAT(MaybeGetLocalizedDate(a), optional(u"30. Dez. 2025"));
    base::i18n::SetICUDefaultLocale("fr_FR");
    EXPECT_THAT(MaybeGetLocalizedDate(a), optional(u"30 déc. 2025"));
  }
  {
    AttributeInstance a =
        entity.attribute(AttributeType(kPassportName)).value();
    EXPECT_EQ(MaybeGetLocalizedDate(a), std::nullopt);
  }
}

}  // namespace
}  // namespace autofill
