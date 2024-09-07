// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/addresses/autofill_profile_sync_util.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_test_api.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_component.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/webdata/autofill_table_utils.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/sync/protocol/autofill_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
namespace autofill {

namespace {
using base::ASCIIToUTF16;
using base::UTF16ToUTF8;
using base::UTF8ToUTF16;
using sync_pb::AutofillProfileSpecifics;
using syncer::EntityData;

// Some guids for testing.
const char kGuid[] = "EDC609ED-7EEE-4F27-B00C-423242A9C44A";
const char kGuidInvalid[] = "EDC609ED";

const base::Time kJune2017 = base::Time::FromSecondsSinceUnixEpoch(1497552271);

// Returns a profile with all fields set.  Contains identical data to the data
// returned from ConstructBaseSpecifics().
AutofillProfile ConstructBaseProfile(
    AddressCountryCode country_code = AddressCountryCode("ES")) {
  AutofillProfile profile(kGuid, AutofillProfile::RecordType::kLocalOrSyncable,
                          country_code);

  profile.set_use_count(7);
  profile.set_use_date(base::Time::FromTimeT(1423182152));

  profile.set_profile_label("profile_label");

  // Set testing values and statuses for the name.
  profile.SetRawInfoWithVerificationStatus(NAME_FULL, u"John K. Doe",
                                           VerificationStatus::kUserVerified);
  profile.SetRawInfoWithVerificationStatus(NAME_FIRST, u"John",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(NAME_MIDDLE, u"K.",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(NAME_LAST, u"Doe",
                                           VerificationStatus::kFormatted);
  profile.SetRawInfoWithVerificationStatus(NAME_LAST_FIRST, u"D",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(NAME_LAST_SECOND, u"e",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(NAME_LAST_CONJUNCTION, u"o",
                                           VerificationStatus::kParsed);

  // Set email, phone and company testing values.
  profile.SetRawInfo(EMAIL_ADDRESS, u"user@example.com");
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"1.800.555.1234");
  profile.SetRawInfo(COMPANY_NAME, u"Google, Inc.");
  profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_ADDRESS,
      u"123 Fake St. Premise Marcos y Oliva\n"
      u"Apt. 10 Floor 2",
      VerificationStatus::kObserved);

  // Set testing values and statuses for the address.
  EXPECT_EQ(u"123 Fake St. Premise Marcos y Oliva",
            profile.GetRawInfo(ADDRESS_HOME_LINE1));
  EXPECT_EQ(u"Apt. 10 Floor 2", profile.GetRawInfo(ADDRESS_HOME_LINE2));

  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_CITY, u"Mountain View",
                                           VerificationStatus::kObserved);

  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STATE, u"California",
                                           VerificationStatus::kObserved);

  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_ZIP, u"94043",
                                           VerificationStatus::kObserved);

  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_ADMIN_LEVEL2, u"Oxaca",
                                           VerificationStatus::kObserved);

  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_SORTING_CODE, u"CEDEX",
                                           VerificationStatus::kObserved);

  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                           u"Santa Clara",
                                           VerificationStatus::kObserved);

  profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_NAME, u"Fake St.", VerificationStatus::kFormatted);

  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_HOUSE_NUMBER, u"123",
                                           VerificationStatus::kFormatted);

  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STREET_LOCATION,
                                           u"123 Fake St.",
                                           VerificationStatus::kFormatted);

  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_SUBPREMISE,
                                           u"Apt. 10 Floor 2",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_APT_NUM, u"10",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_FLOOR, u"2",
                                           VerificationStatus::kParsed);
  profile.set_language_code("en");

  return profile;
}

AutofillProfile ConstructCompleteProfileBR() {
  AutofillProfile profile = ConstructBaseProfile(AddressCountryCode("BR"));
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_CITY, u"Belo Horizonte",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STATE, u"Minas Gerais",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                           u"Lourdes",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STREET_ADDRESS,
                                           u"Av. Bias Fortes, 382\n"
                                           u"apto. 1501, Top Hill Tower\n"
                                           u"30170-011 Belo Horizonte - MG",
                                           VerificationStatus::kObserved);

  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STREET_NAME,
                                           u"Av. Bias Fortes",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_HOUSE_NUMBER, u"382",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STREET_LOCATION,
                                           u"Av. Bias Fortes 382",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_LANDMARK, u"Top Hill Tower", VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_OVERFLOW, u"apto. 1501",
                                           VerificationStatus::kFormatted);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_OVERFLOW_AND_LANDMARK,
                                           u"apto. 1501, Top Hill Tower",
                                           VerificationStatus::kFormatted);
  profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_SUBPREMISE, u"apto. 1501", VerificationStatus::kFormatted);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_APT, u"apto. 1501",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_APT_NUM, u"1501",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_APT_TYPE, u"apto.",
                                           VerificationStatus::kParsed);

  // Reset unused tokens from the default profile.
  profile.ClearFields({ADDRESS_HOME_FLOOR, ADDRESS_HOME_ADMIN_LEVEL2,
                       ADDRESS_HOME_SORTING_CODE});
  return profile;
}

AutofillProfile ConstructCompleteProfileAU() {
  AutofillProfile profile = ConstructBaseProfile(AddressCountryCode("AU"));
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_CITY, u"Sydney",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STATE, u"NWS",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_ADDRESS, u"Unit 7 Level 8 189 Great Eastern Highway",
      VerificationStatus::kObserved);

  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STREET_NAME,
                                           u"Great Eastern Highway",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_HOUSE_NUMBER, u"189",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STREET_LOCATION,
                                           u"189 Great Eastern Highway",
                                           VerificationStatus::kFormatted);

  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_SUBPREMISE,
                                           u"Unit 7 Level 8",
                                           VerificationStatus::kFormatted);

  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_APT, u"Unit 7",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_APT_NUM, u"7",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_APT_TYPE, u"Unit",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_FLOOR, u"8",
                                           VerificationStatus::kParsed);

  profile.ClearFields({ADDRESS_HOME_ADMIN_LEVEL2, ADDRESS_HOME_SORTING_CODE,
                       ADDRESS_HOME_DEPENDENT_LOCALITY});
  return profile;
}

AutofillProfile ConstructCompleteProfileDE() {
  AutofillProfile profile = ConstructBaseProfile(AddressCountryCode("DE"));
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_CITY, u"Enkenbach",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_ADDRESS, u"Mozartstr. 9\n Obergeschoss 2 Wohnung 3",
      VerificationStatus::kObserved);

  profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_NAME, u"Mozartstr.", VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_HOUSE_NUMBER, u"9",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STREET_LOCATION,
                                           u"Mozartstr. 9",
                                           VerificationStatus::kFormatted);

  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_OVERFLOW,
                                           u"Obergeschoss 2 Wohnung 3",
                                           VerificationStatus::kParsed);

  profile.ClearFields({ADDRESS_HOME_ADMIN_LEVEL2, ADDRESS_HOME_SORTING_CODE,
                       ADDRESS_HOME_FLOOR, ADDRESS_HOME_STATE,
                       ADDRESS_HOME_DEPENDENT_LOCALITY});
  return profile;
}

AutofillProfile ConstructCompleteProfileMX() {
  AutofillProfile profile = ConstructBaseProfile(AddressCountryCode("MX"));
  profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_CITY, u"Ciudad de México", VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STATE, u"CDMX",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                           u"Lomas de Chapultepec",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_ADMIN_LEVEL2,
                                           u"Miguel Hidalgo",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_ADDRESS,
      u"C. Montes Urales 445\n"
      u"Piso 4 - 34. Entre calles Paseo de la Reforma y Avenida Juarez - "
      u"Edificio azul",
      VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STREET_LOCATION,
                                           u"C. Montes Urales 445",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STREET_NAME,
                                           u"C. Montes Urales",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_HOUSE_NUMBER, u"445",
                                           VerificationStatus::kParsed);

  profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_LANDMARK, u"Edificio azul", VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_OVERFLOW,
      u"Entre Calles Paseo de la Reforma y Avenida Juarez - Edificio azul",
      VerificationStatus::kFormatted);
  profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK,
      u"Entre Calles Paseo de la Reforma y Avenida Juarez - Edificio azul",
      VerificationStatus::kFormatted);
  profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_BETWEEN_STREETS, u"Paseo de la Reforma y Avenida Juarez",
      VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_BETWEEN_STREETS_1,
                                           u"Paseo de la Reforma",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_BETWEEN_STREETS_2,
                                           u"Avenida Juarez",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_SUBPREMISE, u"Piso 4 - 34", VerificationStatus::kFormatted);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_APT, u"34",
                                           VerificationStatus::kFormatted);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_APT_NUM, u"34",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_FLOOR, u"4",
                                           VerificationStatus::kParsed);

  // Reset unused tokens from the default profile.
  profile.ClearFields({ADDRESS_HOME_SORTING_CODE});
  return profile;
}

AutofillProfile ConstructCompleteProfileIN() {
  AutofillProfile profile = ConstructBaseProfile(AddressCountryCode("IN"));
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_CITY, u"Hyderabad",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STATE, u"Telangana",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_ADDRESS,
      u"12/110, Flat no. 504, Raja Apartments\n"
      u"Kondapur, Opp to Ayyappa Swamy temple",
      VerificationStatus::kFormatted);
  profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_LOCATION, u"12/110, Flat no. 504, Raja Apartments",
      VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                           u"Kondapur",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_LANDMARK,
                                           u"Opp to Ayyappa Swamy temple",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_LOCATION_AND_LOCALITY,
      u"12/110, Flat no. 504, Raja Apartments, Kondapur",
      VerificationStatus::kFormatted);

  return profile;
}

// Returns AutofillProfileSpecifics with all Autofill profile fields set.
// Contains identical data to the data returned from ConstructBaseProfile().
AutofillProfileSpecifics ConstructBaseSpecifics() {
  AutofillProfileSpecifics specifics;

  specifics.set_guid(kGuid);
  // TODO(crbug.com/40266694): Remove. See comment in
  // `CreateEntityDataFromAutofillProfile()`.
  specifics.set_deprecated_origin(kSettingsOrigin);
  specifics.set_use_count(7);
  specifics.set_use_date(1423182152);
  specifics.set_profile_label("profile_label");

  // Set values and statuses for the names.
  specifics.add_name_first("John");
  specifics.add_name_first_status(
      AutofillProfileSpecifics::VerificationStatus::
          AutofillProfileSpecifics_VerificationStatus_OBSERVED);

  specifics.add_name_middle("K.");
  specifics.add_name_middle_status(
      AutofillProfileSpecifics::VerificationStatus::
          AutofillProfileSpecifics_VerificationStatus_OBSERVED);

  specifics.add_name_last("Doe");
  specifics.add_name_last_status(
      AutofillProfileSpecifics::VerificationStatus::
          AutofillProfileSpecifics_VerificationStatus_FORMATTED);

  specifics.add_name_last_first("D");
  specifics.add_name_last_first_status(
      AutofillProfileSpecifics::VerificationStatus::
          AutofillProfileSpecifics_VerificationStatus_PARSED);

  specifics.add_name_last_second("e");
  specifics.add_name_last_second_status(
      AutofillProfileSpecifics::VerificationStatus::
          AutofillProfileSpecifics_VerificationStatus_PARSED);

  specifics.add_name_last_conjunction("o");
  specifics.add_name_last_conjunction_status(
      AutofillProfileSpecifics::VerificationStatus::
          AutofillProfileSpecifics_VerificationStatus_PARSED);

  specifics.add_name_full("John K. Doe");
  specifics.add_name_full_status(
      AutofillProfileSpecifics::VerificationStatus::
          AutofillProfileSpecifics_VerificationStatus_USER_VERIFIED);

  // Set testing values for email, phone and company.
  specifics.add_email_address("user@example.com");
  specifics.add_phone_home_whole_number("1.800.555.1234");
  specifics.set_company_name("Google, Inc.");

  // Set values and statuses for the address.
  // Address lines are derived from the home street address and do not have an
  // independent status.
  specifics.set_address_home_line1("123 Fake St. Premise Marcos y Oliva");
  specifics.set_address_home_line2("Apt. 10 Floor 2");
  specifics.set_address_home_street_address(
      "123 Fake St. Premise Marcos y Oliva\n"
      "Apt. 10 Floor 2");
  specifics.set_address_home_street_address_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus::
          AutofillProfileSpecifics_VerificationStatus_OBSERVED);

  specifics.set_address_home_thoroughfare_name("Fake St.");
  specifics.set_address_home_thoroughfare_name_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_FORMATTED);

  specifics.set_address_home_thoroughfare_number("123");
  specifics.set_address_home_thoroughfare_number_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_FORMATTED);

  specifics.set_address_home_street_location("123 Fake St.");
  specifics.set_address_home_street_location_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_FORMATTED);

  specifics.set_address_home_subpremise_name("Apt. 10 Floor 2");
  specifics.set_address_home_subpremise_name_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_OBSERVED);

  specifics.set_address_home_apt_num("10");
  specifics.set_address_home_apt_num_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_PARSED);

  specifics.set_address_home_floor("2");
  specifics.set_address_home_floor_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_PARSED);

  specifics.set_address_home_city("Mountain View");
  specifics.set_address_home_city_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_OBSERVED);

  specifics.set_address_home_state("California");
  specifics.set_address_home_state_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_OBSERVED);

  specifics.set_address_home_zip("94043");
  specifics.set_address_home_zip_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_OBSERVED);

  specifics.set_address_home_country("ES");
  specifics.set_address_home_country_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_OBSERVED);

  specifics.set_address_home_admin_level_2("Oxaca");
  specifics.set_address_home_admin_level_2_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_OBSERVED);

  specifics.set_address_home_sorting_code("CEDEX");
  specifics.set_address_home_sorting_code_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_OBSERVED);

  specifics.set_address_home_dependent_locality("Santa Clara");
  specifics.set_address_home_dependent_locality_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_OBSERVED);

  specifics.set_address_home_language_code("en");

  // All of the following types are not part of the default address model, but
  // rather belong to a model customized for a particular country. Nevertheless
  // they should be listed here for completeness. Note that these fields are
  // always set during `ContactInfoSpecificsFromAutofillProfile()`, potentially
  // with empty values. If these are not set explicitly on the tests (even with
  // empty values), the serialized values won't match.

  specifics.set_address_home_landmark("");
  specifics.set_address_home_landmark_status(
      sync_pb::
          AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED);

  specifics.set_address_home_between_streets("");
  specifics.set_address_home_between_streets_status(
      sync_pb::
          AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED);

  specifics.set_address_home_between_streets_1("");
  specifics.set_address_home_between_streets_1_status(
      sync_pb::
          AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED);

  specifics.set_address_home_between_streets_2("");
  specifics.set_address_home_between_streets_2_status(
      sync_pb::
          AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED);
  specifics.set_address_home_overflow("");
  specifics.set_address_home_overflow_status(
      sync_pb::
          AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED);
  specifics.set_address_home_between_streets_or_landmark("");
  specifics.set_address_home_between_streets_or_landmark_status(
      sync_pb::
          AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED);

  specifics.set_address_home_overflow_and_landmark("");
  specifics.set_address_home_overflow_and_landmark_status(
      sync_pb::
          AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED);

  specifics.set_address_home_street_location_and_locality("");
  specifics.set_address_home_street_location_and_locality_status(
      sync_pb::
          AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED);

  specifics.set_address_home_apt("");
  specifics.set_address_home_apt_status(
      sync_pb::
          AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED);

  specifics.set_address_home_apt_type("");
  specifics.set_address_home_apt_type_status(
      sync_pb::
          AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED);

  specifics.set_address_home_thoroughfare_number_and_apt("");
  specifics.set_address_home_thoroughfare_number_and_apt_status(
      sync_pb::
          AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED);

  return specifics;
}

AutofillProfileSpecifics ConstructCompleteSpecificsBR() {
  AutofillProfileSpecifics specifics = ConstructBaseSpecifics();

  specifics.set_address_home_country("BR");
  specifics.set_address_home_country_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_OBSERVED);
  specifics.set_address_home_city("Belo Horizonte");
  specifics.set_address_home_city_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_OBSERVED);
  specifics.set_address_home_state("Minas Gerais");
  specifics.set_address_home_state_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_OBSERVED);

  specifics.set_address_home_dependent_locality("Lourdes");
  specifics.set_address_home_dependent_locality_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_OBSERVED);

  specifics.set_address_home_street_address(
      "Av. Bias Fortes, 382\n"
      "apto. 1501, Top Hill Tower\n"
      "30170-011 Belo Horizonte - MG");
  specifics.set_address_home_line1("Av. Bias Fortes, 382");
  specifics.set_address_home_line2("apto. 1501, Top Hill Tower");

  specifics.set_address_home_street_address_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus::
          AutofillProfileSpecifics_VerificationStatus_OBSERVED);

  specifics.set_address_home_thoroughfare_name("Av. Bias Fortes");
  specifics.set_address_home_thoroughfare_name_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_PARSED);

  specifics.set_address_home_thoroughfare_number("382");
  specifics.set_address_home_thoroughfare_number_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_PARSED);

  specifics.set_address_home_street_location("Av. Bias Fortes 382");
  specifics.set_address_home_street_location_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_PARSED);

  specifics.set_address_home_landmark("Top Hill Tower");
  specifics.set_address_home_landmark_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_PARSED);

  specifics.set_address_home_overflow("apto. 1501");
  specifics.set_address_home_overflow_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_FORMATTED);

  specifics.set_address_home_overflow_and_landmark(
      "apto. 1501, Top Hill Tower");
  specifics.set_address_home_overflow_and_landmark_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_FORMATTED);

  specifics.set_address_home_subpremise_name("apto. 1501");
  specifics.set_address_home_subpremise_name_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_FORMATTED);

  specifics.set_address_home_apt("apto. 1501");
  specifics.set_address_home_apt_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_PARSED);

  specifics.set_address_home_apt_num("1501");
  specifics.set_address_home_apt_num_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_PARSED);

  specifics.set_address_home_apt_type("apto.");
  specifics.set_address_home_apt_type_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_PARSED);

  // Reset unused tokens from the default info.
  specifics.set_address_home_floor("");
  specifics.set_address_home_floor_status(
      sync_pb::
          AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED);
  specifics.set_address_home_sorting_code("");
  specifics.set_address_home_sorting_code_status(
      sync_pb::
          AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED);
  specifics.set_address_home_admin_level_2("");
  specifics.set_address_home_admin_level_2_status(
      sync_pb::
          AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED);

  return specifics;
}

AutofillProfileSpecifics ConstructCompleteSpecificsAU() {
  AutofillProfileSpecifics specifics = ConstructBaseSpecifics();

  specifics.set_address_home_country("AU");
  specifics.set_address_home_country_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_OBSERVED);
  specifics.set_address_home_city("Sydney");
  specifics.set_address_home_city_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_OBSERVED);
  specifics.set_address_home_state("NWS");
  specifics.set_address_home_state_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_OBSERVED);

  specifics.set_address_home_street_address(
      "Unit 7 Level 8 189 Great Eastern Highway");
  specifics.set_address_home_line1("Unit 7 Level 8 189 Great Eastern Highway");

  specifics.set_address_home_street_address_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus::
          AutofillProfileSpecifics_VerificationStatus_OBSERVED);

  specifics.set_address_home_thoroughfare_name("Great Eastern Highway");
  specifics.set_address_home_thoroughfare_name_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_PARSED);

  specifics.set_address_home_thoroughfare_number("189");
  specifics.set_address_home_thoroughfare_number_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_PARSED);

  specifics.set_address_home_street_location("189 Great Eastern Highway");
  specifics.set_address_home_street_location_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_FORMATTED);

  specifics.set_address_home_subpremise_name("Unit 7 Level 8");
  specifics.set_address_home_subpremise_name_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_FORMATTED);

  specifics.set_address_home_apt("Unit 7");
  specifics.set_address_home_apt_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_PARSED);

  specifics.set_address_home_apt_num("7");
  specifics.set_address_home_apt_num_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_PARSED);

  specifics.set_address_home_apt_type("Unit");
  specifics.set_address_home_apt_type_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_PARSED);
  specifics.set_address_home_floor("8");
  specifics.set_address_home_floor_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_PARSED);

  // Reset unused tokens from the default info.
  specifics.set_address_home_sorting_code("");
  specifics.set_address_home_sorting_code_status(
      sync_pb::
          AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED);
  specifics.set_address_home_admin_level_2("");

  specifics.set_address_home_admin_level_2_status(
      sync_pb::
          AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED);
  specifics.set_address_home_dependent_locality("");
  specifics.set_address_home_dependent_locality_status(
      sync_pb::
          AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED);

  specifics.set_address_home_line2("");

  return specifics;
}

AutofillProfileSpecifics ConstructCompleteSpecificsDE() {
  AutofillProfileSpecifics specifics = ConstructBaseSpecifics();

  specifics.set_address_home_country("DE");
  specifics.set_address_home_country_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_OBSERVED);
  specifics.set_address_home_city("Enkenbach");
  specifics.set_address_home_city_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_OBSERVED);

  specifics.set_address_home_street_address(
      "Mozartstr. 9\n Obergeschoss 2 Wohnung 3");
  specifics.set_address_home_line1("Mozartstr. 9");
  specifics.set_address_home_line2("Obergeschoss 2 Wohnung 3");

  specifics.set_address_home_street_address_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus::
          AutofillProfileSpecifics_VerificationStatus_OBSERVED);

  specifics.set_address_home_thoroughfare_name("Mozartstr.");
  specifics.set_address_home_thoroughfare_name_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_PARSED);

  specifics.set_address_home_thoroughfare_number("9");
  specifics.set_address_home_thoroughfare_number_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_PARSED);

  specifics.set_address_home_street_location("Mozartstr. 9");
  specifics.set_address_home_street_location_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_FORMATTED);

  specifics.set_address_home_overflow("Obergeschoss 2 Wohnung 3");
  specifics.set_address_home_overflow_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_PARSED);

  // Reset unused tokens from the default info.
  specifics.set_address_home_sorting_code("");
  specifics.set_address_home_sorting_code_status(
      sync_pb::
          AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED);
  specifics.set_address_home_admin_level_2("");

  specifics.set_address_home_admin_level_2_status(
      sync_pb::
          AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED);
  specifics.set_address_home_dependent_locality("");
  specifics.set_address_home_dependent_locality_status(
      sync_pb::
          AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED);

  specifics.set_address_home_subpremise_name("");
  specifics.set_address_home_subpremise_name_status(
      sync_pb::
          AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED);

  specifics.set_address_home_apt_num("");
  specifics.set_address_home_apt_num_status(
      sync_pb::
          AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED);

  specifics.set_address_home_floor("");
  specifics.set_address_home_floor_status(
      sync_pb::
          AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED);

  specifics.set_address_home_state("");
  specifics.set_address_home_state_status(
      sync_pb::
          AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED);

  return specifics;
}

AutofillProfileSpecifics ConstructCompleteSpecificsMX() {
  AutofillProfileSpecifics specifics = ConstructBaseSpecifics();

  specifics.set_address_home_country("MX");
  specifics.set_address_home_country_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_OBSERVED);
  specifics.set_address_home_city("Ciudad de México");
  specifics.set_address_home_city_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_OBSERVED);
  specifics.set_address_home_state("CDMX");
  specifics.set_address_home_state_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_OBSERVED);
  specifics.set_address_home_dependent_locality("Lomas de Chapultepec");
  specifics.set_address_home_dependent_locality_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_OBSERVED);
  specifics.set_address_home_admin_level_2("Miguel Hidalgo");
  specifics.set_address_home_admin_level_2_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_OBSERVED);

  specifics.set_address_home_street_address(
      "C. Montes Urales 445\n"
      "Piso 4 - 34. Entre calles Paseo de la Reforma y Avenida Juarez - "
      "Edificio azul");

  specifics.set_address_home_line1("C. Montes Urales 445");
  specifics.set_address_home_line2(
      "Piso 4 - 34. Entre calles Paseo de la Reforma y Avenida Juarez - "
      "Edificio azul");

  specifics.set_address_home_street_address_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus::
          AutofillProfileSpecifics_VerificationStatus_OBSERVED);

  specifics.set_address_home_street_location("C. Montes Urales 445");
  specifics.set_address_home_street_location_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_PARSED);

  specifics.set_address_home_thoroughfare_name("C. Montes Urales");
  specifics.set_address_home_thoroughfare_name_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_PARSED);

  specifics.set_address_home_thoroughfare_number("445");
  specifics.set_address_home_thoroughfare_number_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_PARSED);

  specifics.set_address_home_landmark("Edificio azul");
  specifics.set_address_home_landmark_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_PARSED);

  specifics.set_address_home_overflow(
      "Entre Calles Paseo de la Reforma y Avenida Juarez - Edificio azul");
  specifics.set_address_home_overflow_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_FORMATTED);

  specifics.set_address_home_between_streets_or_landmark(
      "Entre Calles Paseo de la Reforma y Avenida Juarez - Edificio azul");
  specifics.set_address_home_between_streets_or_landmark_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_FORMATTED);

  specifics.set_address_home_between_streets(
      "Paseo de la Reforma y Avenida Juarez");
  specifics.set_address_home_between_streets_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_PARSED);

  specifics.set_address_home_between_streets_1("Paseo de la Reforma");
  specifics.set_address_home_between_streets_1_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_PARSED);

  specifics.set_address_home_between_streets_2("Avenida Juarez");
  specifics.set_address_home_between_streets_2_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_PARSED);

  specifics.set_address_home_subpremise_name("Piso 4 - 34");
  specifics.set_address_home_subpremise_name_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_FORMATTED);

  specifics.set_address_home_apt("34");
  specifics.set_address_home_apt_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_FORMATTED);

  specifics.set_address_home_apt_num("34");
  specifics.set_address_home_apt_num_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_PARSED);

  specifics.set_address_home_floor("4");
  specifics.set_address_home_floor_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_PARSED);

  specifics.set_address_home_sorting_code("");
  specifics.set_address_home_sorting_code_status(
      sync_pb::
          AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED);

  return specifics;
}

AutofillProfileSpecifics ConstructCompleteSpecificsIN() {
  AutofillProfileSpecifics specifics = ConstructBaseSpecifics();

  specifics.set_address_home_country("IN");
  specifics.set_address_home_country_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_OBSERVED);
  specifics.set_address_home_city("Hyderabad");
  specifics.set_address_home_city_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_OBSERVED);
  specifics.set_address_home_state("Telangana");
  specifics.set_address_home_state_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_OBSERVED);
  specifics.set_address_home_street_location(
      "12/110, Flat no. 504, Raja Apartments");
  specifics.set_address_home_street_location_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_OBSERVED);
  specifics.set_address_home_dependent_locality("Kondapur");
  specifics.set_address_home_dependent_locality_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_OBSERVED);
  specifics.set_address_home_landmark("Opp to Ayyappa Swamy temple");
  specifics.set_address_home_landmark_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_OBSERVED);
  specifics.set_address_home_street_location_and_locality(
      "12/110, Flat no. 504, Raja Apartments, Kondapur");
  specifics.set_address_home_street_location_and_locality_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_FORMATTED);

  specifics.set_address_home_street_address(
      "12/110, Flat no. 504, Raja Apartments\n"
      "Kondapur, Opp to Ayyappa Swamy temple");
  specifics.set_address_home_street_address_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus::
          AutofillProfileSpecifics_VerificationStatus_FORMATTED);

  specifics.set_address_home_line1("12/110, Flat no. 504, Raja Apartments");
  specifics.set_address_home_line2("Kondapur, Opp to Ayyappa Swamy temple");

  specifics.set_address_home_admin_level_2("");
  specifics.set_address_home_admin_level_2_status(
      sync_pb::
          AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED);

  specifics.set_address_home_thoroughfare_name("");
  specifics.set_address_home_thoroughfare_name_status(
      sync_pb::
          AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED);

  specifics.set_address_home_thoroughfare_number("");
  specifics.set_address_home_thoroughfare_number_status(
      sync_pb::
          AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED);

  specifics.set_address_home_subpremise_name("");
  specifics.set_address_home_subpremise_name_status(
      sync_pb::
          AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED);

  specifics.set_address_home_apt_num("");
  specifics.set_address_home_apt_num_status(
      sync_pb::
          AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED);

  specifics.set_address_home_floor("");
  specifics.set_address_home_floor_status(
      sync_pb::
          AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED);

  specifics.set_address_home_sorting_code("");
  specifics.set_address_home_sorting_code_status(
      sync_pb::
          AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED);

  return specifics;
}

enum class I18nCountryModel {
  kLegacy = 0,
  kAU = 1,
  kBR = 2,
  kDE = 3,
  kIN = 4,
  kMX = 5
};

// The tests are parametrized with a country to assert that all custom address
// models are supported.
class AutofillProfileSyncUtilTest
    : public testing::Test,
      public testing::WithParamInterface<I18nCountryModel> {
 public:
  AutofillProfileSyncUtilTest() {
    // Fix a time for implicitly constructed use_dates in AutofillProfile.
    test_clock_.SetNow(kJune2017);
    features_.InitWithFeatures({features::kAutofillUseAUAddressModel,
                                features::kAutofillUseCAAddressModel,
                                features::kAutofillUseDEAddressModel,
                                features::kAutofillUseFRAddressModel,
                                features::kAutofillUseINAddressModel,
                                features::kAutofillUseITAddressModel},
                               {});
  }

  AutofillProfile GetAutofillProfileForCountry(I18nCountryModel country_model) {
    switch (country_model) {
      case I18nCountryModel::kLegacy:
        return ConstructBaseProfile();
      case I18nCountryModel::kAU:
        return ConstructCompleteProfileAU();
      case I18nCountryModel::kBR:
        return ConstructCompleteProfileBR();
      case I18nCountryModel::kDE:
        return ConstructCompleteProfileDE();
      case I18nCountryModel::kMX:
        return ConstructCompleteProfileMX();
      case I18nCountryModel::kIN:
        return ConstructCompleteProfileIN();
    }
  }

  AutofillProfileSpecifics GetAutofillProfileSpecificsForCountry(
      I18nCountryModel country_model) {
    switch (country_model) {
      case I18nCountryModel::kLegacy:
        return ConstructBaseSpecifics();
      case I18nCountryModel::kAU:
        return ConstructCompleteSpecificsAU();
      case I18nCountryModel::kBR:
        return ConstructCompleteSpecificsBR();
      case I18nCountryModel::kDE:
        return ConstructCompleteSpecificsDE();
      case I18nCountryModel::kMX:
        return ConstructCompleteSpecificsMX();
      case I18nCountryModel::kIN:
        return ConstructCompleteSpecificsIN();
    }
  }

 private:
  autofill::TestAutofillClock test_clock_;
  base::test::ScopedFeatureList features_;
};

// Ensure that all profile fields are able to be synced up from the client to
// the server.
TEST_P(AutofillProfileSyncUtilTest, CreateEntityDataFromAutofillProfile) {
  AutofillProfile profile = GetAutofillProfileForCountry(GetParam());
  AutofillProfileSpecifics specifics =
      GetAutofillProfileSpecificsForCountry(GetParam());
  std::unique_ptr<EntityData> entity_data =
      CreateEntityDataFromAutofillProfile(profile);
  // The non-unique name should be set to the guid of the profile.
  EXPECT_EQ(entity_data->name, profile.guid());

  EXPECT_EQ(specifics.SerializeAsString(),
            entity_data->specifics.autofill_profile().SerializeAsString());
}

// Test that fields not set for the input are empty in the output.
TEST_F(AutofillProfileSyncUtilTest, CreateEntityDataFromAutofillProfile_Empty) {
  AutofillProfile profile(kGuid, AutofillProfile::RecordType::kLocalOrSyncable,
                          i18n_model_definition::kLegacyHierarchyCountryCode);
  ASSERT_FALSE(profile.HasRawInfo(NAME_FULL));
  ASSERT_FALSE(profile.HasRawInfo(COMPANY_NAME));

  std::unique_ptr<EntityData> entity_data =
      CreateEntityDataFromAutofillProfile(profile);
  EXPECT_EQ(1, entity_data->specifics.autofill_profile().name_full_size());
  EXPECT_EQ("", entity_data->specifics.autofill_profile().name_full(0));
  EXPECT_TRUE(entity_data->specifics.autofill_profile().has_company_name());
  EXPECT_EQ("", entity_data->specifics.autofill_profile().company_name());
}

// Test that long fields get trimmed.
TEST_F(AutofillProfileSyncUtilTest,
       CreateEntityDataFromAutofillProfile_Trimmed) {
  std::string kNameLong(kMaxDataLengthForDatabase + 1, 'a');
  std::string kNameTrimmed(kMaxDataLengthForDatabase, 'a');

  AutofillProfile profile(kGuid, AutofillProfile::RecordType::kLocalOrSyncable,
                          i18n_model_definition::kLegacyHierarchyCountryCode);
  profile.SetRawInfo(NAME_FULL, ASCIIToUTF16(kNameLong));

  std::unique_ptr<EntityData> entity_data =
      CreateEntityDataFromAutofillProfile(profile);

  EXPECT_EQ(kNameTrimmed,
            entity_data->specifics.autofill_profile().name_full(0));
}

// Test that long non-ascii fields get correctly trimmed.
TEST_F(AutofillProfileSyncUtilTest,
       CreateEntityDataFromAutofillProfile_TrimmedNonASCII) {
  // Make the UTF8 string have odd number of bytes and append many 2-bytes
  // characters so that simple ASCII trimming would make the UTF8 string
  // invalid.
  std::string kNameLong("aä");
  std::string kNameTrimmed("a");

  for (unsigned int i = 0; i < kMaxDataLengthForDatabase / 2 - 1; ++i) {
    kNameLong += "ä";
    kNameTrimmed += "ä";
  }

  AutofillProfile profile(kGuid, AutofillProfile::RecordType::kLocalOrSyncable,
                          i18n_model_definition::kLegacyHierarchyCountryCode);
  profile.SetRawInfo(NAME_FULL, UTF8ToUTF16(kNameLong));

  std::unique_ptr<EntityData> entity_data =
      CreateEntityDataFromAutofillProfile(profile);

  EXPECT_EQ(kNameTrimmed,
            entity_data->specifics.autofill_profile().name_full(0));
}

// Ensure that all profile fields are able to be synced down from the server to
// the client (and nothing gets uploaded back).
TEST_P(AutofillProfileSyncUtilTest, CreateAutofillProfileFromSpecifics) {
  // Fix a time for implicitly constructed use_dates in AutofillProfile.
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(kJune2017);

  AutofillProfileSpecifics specifics =
      GetAutofillProfileSpecificsForCountry(GetParam());
  AutofillProfile profile = GetAutofillProfileForCountry(GetParam());

  std::optional<AutofillProfile> converted_profile =
      CreateAutofillProfileFromSpecifics(specifics);
  EXPECT_TRUE(test_api(profile).EqualsIncludingUsageStats(*converted_profile));
}

// Test that fields not set for the input are also not set on the output.
TEST_F(AutofillProfileSyncUtilTest, CreateAutofillProfileFromSpecifics_Empty) {
  AutofillProfileSpecifics specifics;
  specifics.set_guid(kGuid);

  std::optional<AutofillProfile> profile =
      CreateAutofillProfileFromSpecifics(specifics);

  EXPECT_FALSE(profile->HasRawInfo(NAME_FULL));
  EXPECT_FALSE(profile->HasRawInfo(COMPANY_NAME));
}

// Test that nullopt is produced if the input guid is invalid.
TEST_F(AutofillProfileSyncUtilTest,
       CreateAutofillProfileFromSpecifics_Invalid) {
  AutofillProfileSpecifics specifics;
  specifics.set_guid(kGuidInvalid);

  EXPECT_FALSE(CreateAutofillProfileFromSpecifics(specifics).has_value());
}

// Test that if conflicting info is set for address home, the (deprecated) line1
// & line2 fields get overwritten by the street_address field.
TEST_F(AutofillProfileSyncUtilTest,
       CreateAutofillProfileFromSpecifics_HomeAddressWins) {
  AutofillProfileSpecifics specifics;
  specifics.set_guid(kGuid);

  specifics.set_address_home_street_address(
      "123 New St.\n"
      "Apt. 42");
  specifics.set_address_home_line1("456 Old St.");
  specifics.set_address_home_line2("Apt. 43");

  std::optional<AutofillProfile> profile =
      CreateAutofillProfileFromSpecifics(specifics);

  EXPECT_EQ("123 New St.",
            UTF16ToUTF8(profile->GetRawInfo(ADDRESS_HOME_LINE1)));
  EXPECT_EQ("Apt. 42", UTF16ToUTF8(profile->GetRawInfo(ADDRESS_HOME_LINE2)));
}

// Test that country names (used in the past for the field) get correctly parsed
// into country code.
TEST_F(AutofillProfileSyncUtilTest,
       CreateAutofillProfileFromSpecifics_CountryNameParsed) {
  AutofillProfileSpecifics specifics;
  specifics.set_guid(kGuid);

  specifics.set_address_home_country("Germany");
  EXPECT_EQ("DE", UTF16ToUTF8(
                      CreateAutofillProfileFromSpecifics(specifics)->GetRawInfo(
                          ADDRESS_HOME_COUNTRY)));

  specifics.set_address_home_country("united states");
  EXPECT_EQ("US", UTF16ToUTF8(
                      CreateAutofillProfileFromSpecifics(specifics)->GetRawInfo(
                          ADDRESS_HOME_COUNTRY)));
}

// Tests that guid is returned as storage key.
TEST_F(AutofillProfileSyncUtilTest, GetStorageKeyFromAutofillProfile) {
  AutofillProfile profile(kGuid, AutofillProfile::RecordType::kLocalOrSyncable,
                          i18n_model_definition::kLegacyHierarchyCountryCode);

  EXPECT_EQ(kGuid, GetStorageKeyFromAutofillProfile(profile));
}

// Tests that guid is returned as storage key.
TEST_F(AutofillProfileSyncUtilTest, GetStorageKeyFromAutofillProfileSpecifics) {
  AutofillProfileSpecifics specifics;
  specifics.set_guid(kGuid);

  EXPECT_EQ(kGuid, GetStorageKeyFromAutofillProfileSpecifics(specifics));
}

// Tests that empty string is returned for entry with invalid guid.
TEST_F(AutofillProfileSyncUtilTest,
       GetStorageKeyFromAutofillProfileSpecifics_Invalid) {
  AutofillProfileSpecifics specifics;
  specifics.set_guid(kGuidInvalid);

  EXPECT_EQ(std::string(),
            GetStorageKeyFromAutofillProfileSpecifics(specifics));
}

INSTANTIATE_TEST_SUITE_P(AutofillI18nModels,
                         AutofillProfileSyncUtilTest,
                         testing::Values(I18nCountryModel::kLegacy,
                                         I18nCountryModel::kAU,
                                         I18nCountryModel::kBR,
                                         I18nCountryModel::kDE,
                                         I18nCountryModel::kMX,
                                         I18nCountryModel::kIN));

}  // namespace
}  // namespace autofill
