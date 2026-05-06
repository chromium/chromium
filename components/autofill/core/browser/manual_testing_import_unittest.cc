// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/manual_testing_import.h"

#include <optional>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using enum AttributeTypeName;

AttributeInstance CreateAttribute(AttributeTypeName name,
                                  std::string_view value) {
  AttributeType type = AttributeType(name);
  AttributeInstance instance = AttributeInstance(type);
  instance.SetRawInfo(std::nullopt, base::UTF8ToUTF16(value),
                      VerificationStatus::kObserved);
  instance.FinalizeInfo();
  return instance;
}

// Matches two AutofillProfiles and expects that they `Compare()` equal. This
// means that their values and verification statuses match for every field type,
// but their GUID, usage data, etc might differ.
MATCHER(DataModelsCompareEqual, "") {
  return std::get<0>(arg).Compare(std::get<1>(arg)) == 0;
}

class ManualTestingImportTest : public testing::Test {
  void SetUp() override { ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir()); }

 public:
  base::FilePath GetFilePath() const {
    return scoped_temp_dir.GetPath().Append(
        FILE_PATH_LITERAL("test_file.json"));
  }
  base::ScopedTempDir scoped_temp_dir;
};

// Tests that profiles are converted correctly.
TEST_F(ManualTestingImportTest, LoadCreditCardsFromFile_Valid) {
  base::FilePath file_path = GetFilePath();
  base::WriteFile(file_path, R"({
    "credit-cards" : [
      {
        "CREDIT_CARD_NAME_FULL" : "first1 last1",
        "CREDIT_CARD_NAME_FIRST" : "first1",
        "CREDIT_CARD_NAME_LAST" : "last1",
        "CREDIT_CARD_NUMBER" : "4111111111111111",
        "CREDIT_CARD_EXP_MONTH" : "10",
        "CREDIT_CARD_EXP_2_DIGIT_YEAR" : "27",
        "CREDIT_CARD_VERIFICATION_CODE" : "123"
      },
      {
        "nickname" : "some nickname",
        "CREDIT_CARD_NAME_FULL" : "first2 last2",
        "CREDIT_CARD_NAME_FIRST" : "first2",
        "CREDIT_CARD_NAME_LAST" : "last2",
        "CREDIT_CARD_NUMBER" : "6011111111111117",
        "CREDIT_CARD_EXP_MONTH" : "10",
        "CREDIT_CARD_EXP_2_DIGIT_YEAR" : "27",
        "CREDIT_CARD_VERIFICATION_CODE" : "321"
      }
    ]
  })");

  CreditCard expected_card1;
  expected_card1.SetRawInfoWithVerificationStatus(
      CREDIT_CARD_NAME_FULL, u"first1 last1",
      VerificationStatus::kUserVerified);
  expected_card1.SetRawInfoWithVerificationStatus(
      CREDIT_CARD_NAME_FIRST, u"first1", VerificationStatus::kUserVerified);
  expected_card1.SetRawInfoWithVerificationStatus(
      CREDIT_CARD_NAME_LAST, u"last1", VerificationStatus::kUserVerified);
  expected_card1.SetRawInfoWithVerificationStatus(
      CREDIT_CARD_NUMBER, u"4111111111111111",
      VerificationStatus::kUserVerified);
  expected_card1.SetRawInfoWithVerificationStatus(
      CREDIT_CARD_EXP_MONTH, u"10", VerificationStatus::kUserVerified);
  expected_card1.SetRawInfoWithVerificationStatus(
      CREDIT_CARD_EXP_2_DIGIT_YEAR, u"27", VerificationStatus::kUserVerified);
  expected_card1.SetRawInfoWithVerificationStatus(
      CREDIT_CARD_VERIFICATION_CODE, u"123", VerificationStatus::kUserVerified);

  CreditCard expected_card2;
  expected_card2.SetNickname(u"some nickname");
  expected_card2.SetRawInfoWithVerificationStatus(
      CREDIT_CARD_NAME_FULL, u"first2 last2",
      VerificationStatus::kUserVerified);
  expected_card2.SetRawInfoWithVerificationStatus(
      CREDIT_CARD_NAME_FIRST, u"first2", VerificationStatus::kUserVerified);
  expected_card2.SetRawInfoWithVerificationStatus(
      CREDIT_CARD_NAME_LAST, u"last2", VerificationStatus::kUserVerified);
  expected_card2.SetRawInfoWithVerificationStatus(
      CREDIT_CARD_NUMBER, u"6011111111111117",
      VerificationStatus::kUserVerified);
  expected_card2.SetRawInfoWithVerificationStatus(
      CREDIT_CARD_EXP_MONTH, u"10", VerificationStatus::kUserVerified);
  expected_card2.SetRawInfoWithVerificationStatus(
      CREDIT_CARD_EXP_2_DIGIT_YEAR, u"27", VerificationStatus::kUserVerified);
  expected_card2.SetRawInfoWithVerificationStatus(
      CREDIT_CARD_VERIFICATION_CODE, u"321", VerificationStatus::kUserVerified);

  EXPECT_THAT(LoadCreditCardsFromFile(file_path),
              testing::Optional(testing::Pointwise(
                  DataModelsCompareEqual(), {expected_card1, expected_card2})));
}

// Tests that the conversion fails when a non-CC type is passed for a credit
// card.
TEST_F(ManualTestingImportTest, LoadCreditCardsFromFile_Invalid_NoDictionary) {
  base::FilePath file_path = GetFilePath();
  base::WriteFile(file_path, R"([
        "4111111111111111",
        "10",
        "27",
        "street-address"
    ])");
  EXPECT_FALSE(LoadCreditCardsFromFile(file_path).has_value());
}

// Tests that the conversion fails when a non-CC type is passed for a credit
// card.
TEST_F(ManualTestingImportTest,
       LoadCreditCardsFromFile_Invalid_NonRelatedType) {
  base::FilePath file_path = GetFilePath();
  base::WriteFile(file_path, R"({
    "credit-cards" : [
      {
        "CREDIT_CARD_NUMBER" : "4111111111111111",
        "CREDIT_CARD_EXP_MONTH" : "10",
        "CREDIT_CARD_EXP_2_DIGIT_YEAR" : "27",
        "ADDRESS_HOME_STREET_ADDRESS" : "street-address"
      }
    ]
  })");

  EXPECT_FALSE(LoadCreditCardsFromFile(file_path).has_value());
}

// Tests that the conversion fails when an unrecognized field type is present.
TEST_F(ManualTestingImportTest,
       LoadCreditCardsFromFile_Invalid_UnrecognizedType) {
  base::FilePath file_path = GetFilePath();
  base::WriteFile(file_path, R"({
    "credit-cards" : [
      {
        "CREDIT_CARD_NAME_LOST" : "..."
      }
    ]
  })");
  EXPECT_FALSE(LoadCreditCardsFromFile(file_path).has_value());
}

// Tests that the conversion fails when an invalid description is provided.
TEST_F(ManualTestingImportTest, LoadCreditCardsFromFile_NotValid) {
  base::FilePath file_path = GetFilePath();
  base::WriteFile(file_path, R"({
    "credit-cards" : [
      {
        "CREDIT_CARD_NAME_FULL" : "first last"
      }
    ]
  })");
  EXPECT_FALSE(LoadCreditCardsFromFile(file_path).has_value());
}

// Tests that entities are converted correctly.
TEST_F(ManualTestingImportTest, LoadEntitiesFromFile_Valid) {
  base::FilePath file_path = GetFilePath();
  base::WriteFile(file_path, R"({
    "entities" : [
      {
        "entity_type" : "Passport",
        "attributes" : {
          "Number" : "123456789",
          "Name" : "John Doe"
        }
      },
      {
        "entity_type": "Vehicle",
        "attributes": {
          "Make": "Tesla",
          "Model": "Model 3",
          "Year": "2023",
          "Plate number": "CHROME1",
          "VIN": "1ABCD2EF3GHI45678"
        }
      },
      {
        "entity_type": "Flight Reservation",
        "attributes": {
          "Flight number": "UA 123",
          "Ticket number": "0161234567890",
          "Confirmation code": "ABCDEF",
          "Passenger name": "John Doe",
          "Departure airport": "SFO",
          "Arrival airport": "JFK",
          "Departure date": "2024-12-25"
        }
      }
    ]
  })");

  // Expected Passport
  EntityInstance expected_passport(
      EntityType(EntityTypeName::kPassport),
      {CreateAttribute(AttributeTypeName::kPassportNumber, "123456789"),
       CreateAttribute(AttributeTypeName::kPassportName, "John Doe")},
      EntityInstance::EntityId(base::Uuid::GenerateRandomV4()),
      /*nickname=*/"", base::Time::Now(), /*use_count=*/0,
      /*use_date=*/base::Time(), EntityInstance::RecordType::kLocal,
      EntityInstance::AreAttributesReadOnly(false),
      /*frecency_override=*/"");

  // Expected Vehicle
  EntityInstance expected_vehicle(
      EntityType(EntityTypeName::kVehicle),
      {CreateAttribute(AttributeTypeName::kVehicleMake, "Tesla"),
       CreateAttribute(AttributeTypeName::kVehicleModel, "Model 3"),
       CreateAttribute(AttributeTypeName::kVehicleYear, "2023"),
       CreateAttribute(AttributeTypeName::kVehiclePlateNumber, "CHROME1"),
       CreateAttribute(AttributeTypeName::kVehicleVin, "1ABCD2EF3GHI45678")},
      EntityInstance::EntityId(base::Uuid::GenerateRandomV4()),
      /*nickname=*/"", base::Time::Now(), /*use_count=*/0,
      /*use_date=*/base::Time(), EntityInstance::RecordType::kLocal,
      EntityInstance::AreAttributesReadOnly(false),
      /*frecency_override=*/"");

  // Expected Flight Reservation
  EntityInstance expected_flight(
      EntityType(EntityTypeName::kFlightReservation),
      {CreateAttribute(AttributeTypeName::kFlightReservationFlightNumber,
                       "UA 123"),
       CreateAttribute(AttributeTypeName::kFlightReservationTicketNumber,
                       "0161234567890"),
       CreateAttribute(AttributeTypeName::kFlightReservationConfirmationCode,
                       "ABCDEF"),
       CreateAttribute(AttributeTypeName::kFlightReservationPassengerName,
                       "John Doe"),
       CreateAttribute(AttributeTypeName::kFlightReservationDepartureAirport,
                       "SFO"),
       CreateAttribute(AttributeTypeName::kFlightReservationArrivalAirport,
                       "JFK"),
       CreateAttribute(AttributeTypeName::kFlightReservationDepartureDate,
                       "2024-12-25")},
      EntityInstance::EntityId(base::Uuid::GenerateRandomV4()),
      /*nickname=*/"", base::Time::Now(), /*use_count=*/0,
      /*use_date=*/base::Time(), EntityInstance::RecordType::kLocal,
      EntityInstance::AreAttributesReadOnly(true),
      /*frecency_override=*/"");

  std::optional<std::vector<EntityInstance>> entities =
      LoadEntitiesFromFile(file_path);
  ASSERT_TRUE(entities.has_value());
  ASSERT_EQ(entities->size(), 3u);

  EXPECT_EQ(entities->at(0).type(), expected_passport.type());
  EXPECT_EQ(entities->at(0).attributes(), expected_passport.attributes());

  EXPECT_EQ(entities->at(1).type(), expected_vehicle.type());
  EXPECT_EQ(entities->at(1).attributes(), expected_vehicle.attributes());

  EXPECT_EQ(entities->at(2).type(), expected_flight.type());
  EXPECT_EQ(entities->at(2).attributes(), expected_flight.attributes());
}

// Tests that the conversion fails when an entity type is invalid.
TEST_F(ManualTestingImportTest, LoadEntitiesFromFile_Invalid_EntityType) {
  base::FilePath file_path = GetFilePath();
  base::WriteFile(file_path, R"({
    "entities" : [
      {
        "entity_type" : "UnknownEntity",
        "attributes" : {
          "Number" : "123456789"
        }
      }
    ]
  })");
  EXPECT_FALSE(LoadEntitiesFromFile(file_path).has_value());
}

// Tests that the conversion fails when an attribute type is invalid.
TEST_F(ManualTestingImportTest, LoadEntitiesFromFile_Invalid_AttributeType) {
  base::FilePath file_path = GetFilePath();
  base::WriteFile(file_path, R"({
    "entities" : [
      {
        "entity_type" : "Passport",
        "attributes" : {
          "UnknownAttribute" : "123456789"
        }
      }
    ]
  })");
  EXPECT_FALSE(LoadEntitiesFromFile(file_path).has_value());
}

// Tests that profiles are converted correctly.
TEST_F(ManualTestingImportTest, LoadProfilesFromFile_Valid) {
  base::FilePath file_path = GetFilePath();
  base::WriteFile(file_path, R"({
    "profiles" : [
      {
        "NAME_FULL" : "first last",
        "NAME_FIRST" : "first",
        "NAME_LAST" : "last",
        "NAME_LAST_SECOND" : "last"
      },
      {
        "record_type" : "account",
        "initial_creator_id": 999,
        "ADDRESS_HOME_STREET_ADDRESS" : "street 123",
        "ADDRESS_HOME_STREET_NAME" : "street",
        "ADDRESS_HOME_HOUSE_NUMBER" : "123"
      }
    ]
  })");

  AutofillProfile expected_profile1(
      AutofillProfile::RecordType::kLocalOrSyncable,
      i18n_model_definition::kLegacyHierarchyCountryCode);
  expected_profile1.SetRawInfoWithVerificationStatus(
      NAME_FULL, u"first last", VerificationStatus::kObserved);
  expected_profile1.SetRawInfoWithVerificationStatus(
      NAME_FIRST, u"first", VerificationStatus::kObserved);
  expected_profile1.SetRawInfoWithVerificationStatus(
      NAME_LAST, u"last", VerificationStatus::kObserved);
  expected_profile1.SetRawInfoWithVerificationStatus(
      NAME_LAST_SECOND, u"last", VerificationStatus::kObserved);

  AutofillProfile expected_profile2(
      AutofillProfile::RecordType::kAccount,
      i18n_model_definition::kLegacyHierarchyCountryCode);
  expected_profile2.set_initial_creator_id(999);
  expected_profile2.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_ADDRESS, u"street 123",
      VerificationStatus::kObserved);
  expected_profile2.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_NAME, u"street", VerificationStatus::kObserved);
  expected_profile2.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_HOUSE_NUMBER, u"123", VerificationStatus::kObserved);

  EXPECT_THAT(
      LoadProfilesFromFile(file_path),
      testing::Optional(testing::Pointwise(
          DataModelsCompareEqual(), {expected_profile1, expected_profile2})));
}

// Tests that conversion fails when the file doesn't exist
TEST_F(ManualTestingImportTest,
       AutofillProfileFromJSON_Invalid_FailedToOpenFile) {
  EXPECT_FALSE(LoadProfilesFromFile(GetFilePath()).has_value());
}

// Tests that the conversion fails when the passed JSON file isn't a dictionary
// at its top level.
TEST_F(ManualTestingImportTest, LoadProfilesFromFile_Invalid_NotDictionary) {
  base::FilePath file_path = GetFilePath();
  base::WriteFile(file_path, R"([
      "first last"
    ])");
  EXPECT_FALSE(LoadProfilesFromFile(file_path).has_value());
}

// Tests that the conversion fails when the passed JSON file isn't valid.
TEST_F(ManualTestingImportTest,
       LoadProfilesFromFile_Invalid_FailedToParseJSON) {
  base::FilePath file_path = GetFilePath();
  base::WriteFile(file_path, R"([
      "first last"
    })");
  EXPECT_FALSE(LoadProfilesFromFile(file_path).has_value());
}

// Tests that the conversion fails when an unrecognized field type is present.
TEST_F(ManualTestingImportTest, LoadProfilesFromFile_Invalid_UnrecognizedType) {
  base::FilePath file_path = GetFilePath();
  base::WriteFile(file_path, R"({
    "profiles" : [
      {
        "NAME_FULLLLL" : "..."
      }
    ]
  })");
  EXPECT_FALSE(LoadProfilesFromFile(file_path).has_value());
}

// Tests that the conversion fails when the "record_type" has an unrecognized
// value.
TEST_F(ManualTestingImportTest,
       LoadProfilesFromFile_Invalid_UnrecognizedRecordType) {
  base::FilePath file_path = GetFilePath();
  base::WriteFile(file_path, R"({
    "profiles" : [
      {
        "record_type" : "invalid"
      }
    ]
  })");
  EXPECT_FALSE(LoadProfilesFromFile(file_path).has_value());
}

// Tests that the conversion fails when the "initial_creator_id" has
// an invalid value.
TEST_F(ManualTestingImportTest, LoadProfilesFromFile_InvalidInitialCreatorId) {
  base::FilePath file_path1 =
      scoped_temp_dir.GetPath().Append(FILE_PATH_LITERAL("test_file1.json"));
  base::WriteFile(file_path1, R"({
    "profiles" : [
      {
        "initial_creator_id" : "123"
      }
    ]
  })");
  EXPECT_FALSE(LoadProfilesFromFile(file_path1).has_value());

  base::FilePath file_path2 =
      scoped_temp_dir.GetPath().Append(FILE_PATH_LITERAL("test_file2.json"));
  base::WriteFile(file_path2, R"({
    "profiles" : [
      {
        "initial_creator_id" : true
      }
    ]
  })");
  EXPECT_FALSE(LoadProfilesFromFile(file_path2).has_value());
}

// TODO(crbug.com/40268162): Re-enable this test.
// Tests that the conversion fails for non-fully structured profiles.
TEST_F(ManualTestingImportTest,
       DISABLED_LoadProfilesFromFile_Invalid_NotFullyStructured) {
  base::FilePath file_path = GetFilePath();
  base::WriteFile(file_path, R"({
    "profiles" : [
      {
        "NAME_FIRST" : "first",
        "NAME_LAST" : "last"
      }
    ]
  })");
  EXPECT_FALSE(LoadProfilesFromFile(file_path).has_value());
}

// Regression test for crbug.com/439823380.
TEST_F(ManualTestingImportTest, LoadProfilesFromFile_PhoneNumberTypes) {
  base::FilePath file_path = GetFilePath();
  base::WriteFile(file_path, R"({
    "profiles" : [
      {
        "PHONE_HOME_CITY_AND_NUMBER" : "123"
      }
    ]
  })");
  // Expect that the import fails gracefully (rater than crashes).
  EXPECT_FALSE(LoadProfilesFromFile(file_path).has_value());
}

class ManualTestingImportTesti18n : public ManualTestingImportTest {
 private:
  base::test::ScopedFeatureList features_{features::kAutofillUseINAddressModel};
};

// Tests that i18n profiles are converted correctly.
TEST_F(ManualTestingImportTesti18n, Loadi18nProfilesFromFile_Valid) {
  base::FilePath file_path = GetFilePath();
  base::WriteFile(file_path, R"({
    "profiles" : [
      {
        "NAME_FULL" : "first last",
        "NAME_FIRST" : "first",
        "NAME_LAST" : "last",
        "NAME_LAST_SECOND" : "last"
      },
      {
        "ADDRESS_HOME_COUNTRY" : "BR",
        "ADDRESS_HOME_STREET_ADDRESS" : "street 123",
        "ADDRESS_HOME_STREET_NAME" : "street",
        "ADDRESS_HOME_HOUSE_NUMBER" : "123"
      }
    ]
  })");

  AutofillProfile expected_profile1(
      AutofillProfile::RecordType::kLocalOrSyncable,
      i18n_model_definition::kLegacyHierarchyCountryCode);
  expected_profile1.SetRawInfoWithVerificationStatus(
      NAME_FULL, u"first last", VerificationStatus::kObserved);
  expected_profile1.SetRawInfoWithVerificationStatus(
      NAME_FIRST, u"first", VerificationStatus::kObserved);
  expected_profile1.SetRawInfoWithVerificationStatus(
      NAME_LAST, u"last", VerificationStatus::kObserved);
  expected_profile1.SetRawInfoWithVerificationStatus(
      NAME_LAST_SECOND, u"last", VerificationStatus::kObserved);

  AutofillProfile expected_profile2(
      AutofillProfile::RecordType::kLocalOrSyncable, AddressCountryCode("BR"));
  expected_profile2.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_ADDRESS, u"street 123",
      VerificationStatus::kObserved);
  expected_profile2.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_NAME, u"street", VerificationStatus::kObserved);
  expected_profile2.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_HOUSE_NUMBER, u"123", VerificationStatus::kObserved);
  std::optional<std::vector<AutofillProfile>> loaded_profiles =
      LoadProfilesFromFile(file_path);
  EXPECT_THAT(loaded_profiles, testing::Optional(testing::Pointwise(
                                   DataModelsCompareEqual(),
                                   {expected_profile1, expected_profile2})));
  EXPECT_TRUE(loaded_profiles.value().at(0).GetAddress().IsLegacyAddress());
  EXPECT_FALSE(loaded_profiles.value().at(1).GetAddress().IsLegacyAddress());
}

}  // namespace
}  // namespace autofill
