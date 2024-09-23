// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/addresses/contact_info_sync_util.h"

#include "base/feature_list.h"
#include "base/hash/hash.h"
#include "base/strings/to_string.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile_test_api.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/profile_token_quality_test_api.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using sync_pb::ContactInfoSpecifics;

constexpr char kGuid[] = "00000000-0000-0000-0000-000000000001";
constexpr char kInvalidGuid[] = "1234";
constexpr int kNonChromeModifier = 1234;
const auto kUseDate = base::Time::FromSecondsSinceUnixEpoch(123);
const auto kUseDate2 = base::Time::FromSecondsSinceUnixEpoch(34);
// No third last use date, to test that absence is handled correctly.
const auto kModificationDate = base::Time::FromSecondsSinceUnixEpoch(456);

// Returns a profile with all fields set. Contains identical data to the data
// returned from `ConstructBaseSpecifics()`.
AutofillProfile ConstructBaseProfile(
    AddressCountryCode country_code = AddressCountryCode("ES")) {
  AutofillProfile profile(kGuid, AutofillProfile::RecordType::kAccount,
                          country_code);

  profile.set_use_count(123);
  profile.set_use_date(kUseDate, 1);
  profile.set_use_date(kUseDate2, 2);
  profile.set_modification_date(kModificationDate);
  profile.set_language_code("en");
  profile.set_profile_label("profile_label");
  profile.set_initial_creator_id(
      AutofillProfile::kInitialCreatorOrModifierChrome);
  profile.set_last_modifier_id(kNonChromeModifier);

  // Set name-related values and statuses.
  profile.SetRawInfoWithVerificationStatus(NAME_FIRST, u"John",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(NAME_MIDDLE, u"K.",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(NAME_LAST, u"Doe",
                                           VerificationStatus::kFormatted);
  profile.SetRawInfoWithVerificationStatus(NAME_LAST_FIRST, u"D",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(NAME_LAST_CONJUNCTION, u"o",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(NAME_LAST_SECOND, u"e",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(NAME_FULL, u"John K. Doe",
                                           VerificationStatus::kUserVerified);

  // Set address-related values and statuses.
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_CITY, u"Mountain View",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STATE, u"California",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_ZIP, u"94043",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_ADDRESS,
      u"123 Fake St. Premise Marcos y Oliva\n"
      u"Apt. 10 Floor 2 Red tree",
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
                                           u"Fake St. 123",
                                           VerificationStatus::kFormatted);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_SUBPREMISE,
                                           u"Apt. 10 Floor 2",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_APT_NUM, u"10",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_FLOOR, u"2",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_ADMIN_LEVEL2, u"Oxaca",
                                           VerificationStatus::kObserved);

  // All of the following types don't store verification statuses.
  // Set email, phone and company values.
  profile.SetRawInfo(EMAIL_ADDRESS, u"user@example.com");
  profile.SetRawInfo(COMPANY_NAME, u"Google, Inc.");
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"1.800.555.1234");

  // Add some `ProfileTokenQuality` observations.
  test_api(profile.token_quality())
      .AddObservation(NAME_FIRST,
                      ProfileTokenQuality::ObservationType::kAccepted,
                      ProfileTokenQualityTestApi::FormSignatureHash(12));
  test_api(profile.token_quality())
      .AddObservation(ADDRESS_HOME_CITY,
                      ProfileTokenQuality::ObservationType::kEditedFallback,
                      ProfileTokenQualityTestApi::FormSignatureHash(21));

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
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_APT_TYPE, u"apto",
                                           VerificationStatus::kParsed);

  // Reset unused tokens from the default profile.
  profile.ClearFields({ADDRESS_HOME_FLOOR, ADDRESS_HOME_ADMIN_LEVEL2,
                       ADDRESS_HOME_SORTING_CODE});
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
      ADDRESS_HOME_STREET_LOCATION_AND_LOCALITY,
      u"12/110, Flat no. 504, Raja Apartments, Kondapur",
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
  return profile;
}

// Helper function to set ContactInfoSpecifics::String- and IntegerToken
// together with their verification status and value_hash.
template <typename TokenType, typename Value>
void SetToken(TokenType* token,
              const Value& value,
              ContactInfoSpecifics::VerificationStatus status) {
  token->set_value(value);
  ContactInfoSpecifics::TokenMetadata* metadata = token->mutable_metadata();
  metadata->set_status(status);
  metadata->set_value_hash(base::PersistentHash(base::ToString(value)));
}

// Returns ContactInfoSpecifics with all fields set. Contains identical data to
// the profile returned from `ConstructBaseProfile()`.
ContactInfoSpecifics ConstructBaseSpecifics() {
  ContactInfoSpecifics specifics;

  specifics.set_guid(kGuid);
  specifics.set_address_type(ContactInfoSpecifics::REGULAR);
  specifics.set_use_count(123);
  specifics.set_use_date_unix_epoch_seconds(kUseDate.ToTimeT());
  specifics.set_use_date2_unix_epoch_seconds(kUseDate2.ToTimeT());
  specifics.set_date_modified_unix_epoch_seconds(kModificationDate.ToTimeT());
  specifics.set_language_code("en");
  specifics.set_profile_label("profile_label");
  specifics.set_initial_creator_id(
      AutofillProfile::kInitialCreatorOrModifierChrome);
  specifics.set_last_modifier_id(kNonChromeModifier);

  // Set name-related values and statuses.
  SetToken(specifics.mutable_name_first(), "John",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_name_middle(), "K.",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_name_last(), "Doe",
           ContactInfoSpecifics::FORMATTED);
  SetToken(specifics.mutable_name_last_first(), "D",
           ContactInfoSpecifics::PARSED);
  SetToken(specifics.mutable_name_last_conjunction(), "o",
           ContactInfoSpecifics::PARSED);
  SetToken(specifics.mutable_name_last_second(), "e",
           ContactInfoSpecifics::PARSED);
  SetToken(specifics.mutable_name_full(), "John K. Doe",
           ContactInfoSpecifics::USER_VERIFIED);

  // Set address-related values and statuses.
  SetToken(specifics.mutable_address_city(), "Mountain View",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_address_state(), "California",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_address_zip(), "94043",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_address_country(), "ES",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_address_street_address(),
           "123 Fake St. Premise Marcos y Oliva\n"
           "Apt. 10 Floor 2 Red tree",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_address_sorting_code(), "CEDEX",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_address_dependent_locality(), "Santa Clara",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_address_thoroughfare_name(), "Fake St.",
           ContactInfoSpecifics::FORMATTED);
  SetToken(specifics.mutable_address_thoroughfare_number(), "123",
           ContactInfoSpecifics::FORMATTED);
  SetToken(specifics.mutable_address_street_location(), "Fake St. 123",
           ContactInfoSpecifics::FORMATTED);
  SetToken(specifics.mutable_address_subpremise_name(), "Apt. 10 Floor 2",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_address_apt_num(), "10",
           ContactInfoSpecifics::PARSED);
  SetToken(specifics.mutable_address_floor(), "2",
           ContactInfoSpecifics::PARSED);
  SetToken(specifics.mutable_address_admin_level_2(), "Oxaca",
           ContactInfoSpecifics::OBSERVED);

  // All of the following types are not part of the default address model, but
  // rather belong to a model customized for a particular country. Nevertheless
  // they should be listed here for completeness.
  SetToken(specifics.mutable_address_landmark(), "",
           ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED);
  SetToken(specifics.mutable_address_between_streets(), "",
           ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED);
  SetToken(specifics.mutable_address_between_streets_1(), "",
           ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED);
  SetToken(specifics.mutable_address_between_streets_2(), "",
           ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED);
  SetToken(specifics.mutable_address_overflow(), "",
           ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED);
  SetToken(specifics.mutable_address_between_streets_or_landmark(), "",
           ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED);
  SetToken(specifics.mutable_address_overflow_and_landmark(), "",
           ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED);
  SetToken(specifics.mutable_address_apt(), "",
           ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED);
  SetToken(specifics.mutable_address_apt_type(), "",
           ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED);
  SetToken(specifics.mutable_address_street_location_and_locality(), "",
           ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED);
  SetToken(specifics.mutable_address_thoroughfare_number_and_apt(), "",
           ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED);

  // All of the following types don't store verification statuses in
  // AutofillProfile. This corresponds to `VERIFICATION_STATUS_UNSPECIFIED`.
  // Set email, phone and company values and statuses.
  SetToken(specifics.mutable_email_address(), "user@example.com",
           ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED);
  SetToken(specifics.mutable_company_name(), "Google, Inc.",
           ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED);
  SetToken(specifics.mutable_phone_home_whole_number(), "1.800.555.1234",
           ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED);

  // Add some `ProfileTokenQuality` observations.
  ContactInfoSpecifics::Observation* observation =
      specifics.mutable_name_first()->mutable_metadata()->add_observations();
  observation->set_type(
      static_cast<int>(ProfileTokenQuality::ObservationType::kAccepted));
  observation->set_form_hash(12);
  observation =
      specifics.mutable_address_city()->mutable_metadata()->add_observations();
  observation->set_type(
      static_cast<int>(ProfileTokenQuality::ObservationType::kEditedFallback));
  observation->set_form_hash(21);

  return specifics;
}

ContactInfoSpecifics ConstructCompleteSpecificsAU() {
  ContactInfoSpecifics specifics = ConstructBaseSpecifics();

  SetToken(specifics.mutable_address_country(), "AU",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_address_city(), "Sydney",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_address_state(), "NWS",
           ContactInfoSpecifics::OBSERVED);

  SetToken(specifics.mutable_address_street_address(),
           "Unit 7 Level 8 189 Great Eastern Highway",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_address_thoroughfare_name(),
           "Great Eastern Highway", ContactInfoSpecifics::PARSED);
  SetToken(specifics.mutable_address_thoroughfare_number(), "189",
           ContactInfoSpecifics::PARSED);
  SetToken(specifics.mutable_address_street_location(),
           "189 Great Eastern Highway", ContactInfoSpecifics::FORMATTED);
  SetToken(specifics.mutable_address_subpremise_name(), "Unit 7 Level 8",
           ContactInfoSpecifics::FORMATTED);
  SetToken(specifics.mutable_address_apt(), "Unit 7",
           ContactInfoSpecifics::PARSED);
  SetToken(specifics.mutable_address_apt_num(), "7",
           ContactInfoSpecifics::PARSED);
  SetToken(specifics.mutable_address_apt_type(), "Unit",
           ContactInfoSpecifics::PARSED);
  SetToken(specifics.mutable_address_floor(), "8",
           ContactInfoSpecifics::PARSED);

  // Reset unused tokens from the default info.
  SetToken(specifics.mutable_address_sorting_code(), "",
           ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED);
  SetToken(specifics.mutable_address_admin_level_2(), "",
           ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED);
  SetToken(specifics.mutable_address_dependent_locality(), "",
           ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED);

  return specifics;
}

ContactInfoSpecifics ConstructCompleteSpecificsDE() {
  ContactInfoSpecifics specifics = ConstructBaseSpecifics();

  SetToken(specifics.mutable_address_country(), "DE",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_address_city(), "Enkenbach",
           ContactInfoSpecifics::OBSERVED);

  SetToken(specifics.mutable_address_street_address(),
           "Mozartstr. 9\n Obergeschoss 2 Wohnung 3",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_address_thoroughfare_name(), "Mozartstr.",
           ContactInfoSpecifics::PARSED);
  SetToken(specifics.mutable_address_thoroughfare_number(), "9",
           ContactInfoSpecifics::PARSED);
  SetToken(specifics.mutable_address_street_location(), "Mozartstr. 9",
           ContactInfoSpecifics::FORMATTED);
  SetToken(specifics.mutable_address_overflow(), "Obergeschoss 2 Wohnung 3",
           ContactInfoSpecifics::PARSED);

  // Reset unused tokens from the default info.
  SetToken(specifics.mutable_address_sorting_code(), "",
           ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED);
  SetToken(specifics.mutable_address_admin_level_2(), "",
           ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED);
  SetToken(specifics.mutable_address_dependent_locality(), "",
           ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED);
  SetToken(specifics.mutable_address_state(), "",
           ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED);
  SetToken(specifics.mutable_address_apt_num(), "",
           ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED);
  SetToken(specifics.mutable_address_floor(), "",
           ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED);
  return specifics;
}

ContactInfoSpecifics ConstructCompleteSpecificsBR() {
  ContactInfoSpecifics specifics = ConstructBaseSpecifics();

  SetToken(specifics.mutable_address_country(), "BR",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_address_city(), "Belo Horizonte",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_address_state(), "Minas Gerais",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_address_dependent_locality(), "Lourdes",
           ContactInfoSpecifics::OBSERVED);

  SetToken(specifics.mutable_address_street_address(),
           "Av. Bias Fortes, 382\n"
           "apto. 1501, Top Hill Tower\n"
           "30170-011 Belo Horizonte - MG",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_address_thoroughfare_name(), "Av. Bias Fortes",
           ContactInfoSpecifics::PARSED);
  SetToken(specifics.mutable_address_thoroughfare_number(), "382",
           ContactInfoSpecifics::PARSED);
  SetToken(specifics.mutable_address_street_location(), "Av. Bias Fortes 382",
           ContactInfoSpecifics::PARSED);
  SetToken(specifics.mutable_address_landmark(), "Top Hill Tower",
           ContactInfoSpecifics::PARSED);
  SetToken(specifics.mutable_address_overflow(), "apto. 1501",
           ContactInfoSpecifics::FORMATTED);
  SetToken(specifics.mutable_address_overflow_and_landmark(),
           "apto. 1501, Top Hill Tower", ContactInfoSpecifics::FORMATTED);
  SetToken(specifics.mutable_address_subpremise_name(), "apto. 1501",
           ContactInfoSpecifics::FORMATTED);
  SetToken(specifics.mutable_address_apt(), "apto. 1501",
           ContactInfoSpecifics::PARSED);
  SetToken(specifics.mutable_address_apt_num(), "1501",
           ContactInfoSpecifics::PARSED);
  SetToken(specifics.mutable_address_apt_type(), "apto",
           ContactInfoSpecifics::PARSED);

  // Reset unused tokens from the default info.
  SetToken(specifics.mutable_address_floor(), "",
           ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED);
  SetToken(specifics.mutable_address_sorting_code(), "",
           ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED);
  SetToken(specifics.mutable_address_admin_level_2(), "",
           ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED);

  return specifics;
}

ContactInfoSpecifics ConstructCompleteSpecificsMX() {
  ContactInfoSpecifics specifics = ConstructBaseSpecifics();

  SetToken(specifics.mutable_address_country(), "MX",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_address_city(), "Ciudad de México",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_address_state(), "CDMX",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_address_dependent_locality(),
           "Lomas de Chapultepec", ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_address_admin_level_2(), "Miguel Hidalgo",
           ContactInfoSpecifics::OBSERVED);

  SetToken(specifics.mutable_address_street_address(),
           "C. Montes Urales 445\n"
           "Piso 4 - 34. Entre calles Paseo de la Reforma y Avenida Juarez - "
           "Edificio azul",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_address_street_location(), "C. Montes Urales 445",
           ContactInfoSpecifics::PARSED);
  SetToken(specifics.mutable_address_thoroughfare_name(), "C. Montes Urales",
           ContactInfoSpecifics::PARSED);
  SetToken(specifics.mutable_address_thoroughfare_number(), "445",
           ContactInfoSpecifics::PARSED);
  SetToken(specifics.mutable_address_landmark(), "Edificio azul",
           ContactInfoSpecifics::PARSED);
  SetToken(specifics.mutable_address_overflow(),
           "Entre Calles Paseo de la Reforma y Avenida Juarez - Edificio azul",
           ContactInfoSpecifics::FORMATTED);
  SetToken(specifics.mutable_address_between_streets_or_landmark(),
           "Entre Calles Paseo de la Reforma y Avenida Juarez - Edificio azul",
           ContactInfoSpecifics::FORMATTED);
  SetToken(specifics.mutable_address_between_streets(),
           "Paseo de la Reforma y Avenida Juarez",
           ContactInfoSpecifics::PARSED);
  SetToken(specifics.mutable_address_between_streets_1(), "Paseo de la Reforma",
           ContactInfoSpecifics::PARSED);
  SetToken(specifics.mutable_address_between_streets_2(), "Avenida Juarez",
           ContactInfoSpecifics::PARSED);
  SetToken(specifics.mutable_address_subpremise_name(), "Piso 4 - 34",
           ContactInfoSpecifics::FORMATTED);
  SetToken(specifics.mutable_address_apt(), "34",
           ContactInfoSpecifics::FORMATTED);
  SetToken(specifics.mutable_address_apt_num(), "34",
           ContactInfoSpecifics::PARSED);
  SetToken(specifics.mutable_address_floor(), "4",
           ContactInfoSpecifics::PARSED);

  // Reset unused tokens from the default info.
  SetToken(specifics.mutable_address_sorting_code(), "",
           ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED);

  return specifics;
}

ContactInfoSpecifics ConstructCompleteSpecificsIN() {
  ContactInfoSpecifics specifics = ConstructBaseSpecifics();

  SetToken(specifics.mutable_address_country(), "IN",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_address_city(), "Hyderabad",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_address_state(), "Telangana",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_address_street_location(),
           "12/110, Flat no. 504, Raja Apartments",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_address_dependent_locality(), "Kondapur",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_address_landmark(), "Opp to Ayyappa Swamy temple",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_address_street_location_and_locality(),
           "12/110, Flat no. 504, Raja Apartments, Kondapur",
           ContactInfoSpecifics::FORMATTED);
  SetToken(specifics.mutable_address_street_address(),
           "12/110, Flat no. 504, Raja Apartments\n"
           "Kondapur, Opp to Ayyappa Swamy temple",
           ContactInfoSpecifics::FORMATTED);

  // Reset unused tokens from the default info.
  SetToken(specifics.mutable_address_admin_level_2(), "",
           ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED);
  SetToken(specifics.mutable_address_thoroughfare_name(), "",
           ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED);
  SetToken(specifics.mutable_address_thoroughfare_number(), "",
           ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED);
  SetToken(specifics.mutable_address_subpremise_name(), "",
           ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED);
  SetToken(specifics.mutable_address_apt_num(), "",
           ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED);
  SetToken(specifics.mutable_address_floor(), "",
           ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED);
  SetToken(specifics.mutable_address_sorting_code(), "",
           ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED);
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
class ContactInfoSyncUtilTest
    : public testing::Test,
      public testing::WithParamInterface<I18nCountryModel> {
 public:
  ContactInfoSyncUtilTest() {
    features_.InitWithFeatures({features::kAutofillUseAUAddressModel,
                                features::kAutofillUseCAAddressModel,
                                features::kAutofillUseDEAddressModel,
                                features::kAutofillUseFRAddressModel,
                                features::kAutofillUseINAddressModel,
                                features::kAutofillUseITAddressModel,
                                features::kAutofillTrackMultipleUseDates},
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

  ContactInfoSpecifics GetContactInfoSpecificsForCountry(
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
  base::test::ScopedFeatureList features_;
};

// Test that converting AutofillProfile -> ContactInfoSpecifics works.
TEST_P(ContactInfoSyncUtilTest,
       CreateContactInfoEntityDataFromAutofillProfile) {
  AutofillProfile profile = GetAutofillProfileForCountry(GetParam());
  ContactInfoSpecifics specifics =
      GetContactInfoSpecificsForCountry(GetParam());

  std::unique_ptr<syncer::EntityData> entity_data =
      CreateContactInfoEntityDataFromAutofillProfile(
          profile, /*base_contact_info_specifics=*/{});

  ASSERT_TRUE(entity_data != nullptr);
  EXPECT_EQ(entity_data->name, profile.guid());
  EXPECT_EQ(specifics.SerializeAsString(),
            entity_data->specifics.contact_info().SerializeAsString());
}

// Test that only profiles with valid GUID are converted.
TEST_F(ContactInfoSyncUtilTest,
       CreateContactInfoEntityDataFromAutofillProfile_InvalidGUID) {
  AutofillProfile profile(kInvalidGuid, AutofillProfile::RecordType::kAccount,
                          i18n_model_definition::kLegacyHierarchyCountryCode);
  EXPECT_EQ(CreateContactInfoEntityDataFromAutofillProfile(
                profile, /*base_contact_info_specifics=*/{}),
            nullptr);
}

// Test that AutofillProfiles with invalid record type are not converted.
TEST_F(ContactInfoSyncUtilTest,
       CreateContactInfoEntityDataFromAutofillProfile_InvalidRecordType) {
  AutofillProfile profile(kGuid, AutofillProfile::RecordType::kLocalOrSyncable,
                          i18n_model_definition::kLegacyHierarchyCountryCode);
  EXPECT_EQ(CreateContactInfoEntityDataFromAutofillProfile(
                profile, /*base_contact_info_specifics=*/{}),
            nullptr);
}

// Tests that H/W record types are converted to
// ContactInfoSpecifics::address_type correctly.
TEST_F(ContactInfoSyncUtilTest,
       CreateContactInfoEntityDataFromAutofillProfile_HWRecordTypes) {
  AutofillProfile profile = ConstructBaseProfile();

  test_api(profile).set_record_type(AutofillProfile::RecordType::kAccountHome);
  EXPECT_EQ(CreateContactInfoEntityDataFromAutofillProfile(
                profile, /*base_contact_info_specifics=*/{})
                ->specifics.contact_info()
                .address_type(),
            ContactInfoSpecifics::HOME);

  test_api(profile).set_record_type(AutofillProfile::RecordType::kAccountWork);
  EXPECT_EQ(CreateContactInfoEntityDataFromAutofillProfile(
                profile, /*base_contact_info_specifics=*/{})
                ->specifics.contact_info()
                .address_type(),
            ContactInfoSpecifics::WORK);
}

// Test that supported fields and nested messages are successfully trimmed.
TEST_F(ContactInfoSyncUtilTest, TrimAllSupportedFieldsFromRemoteSpecifics) {
  sync_pb::ContactInfoSpecifics contact_info_specifics;
  contact_info_specifics.mutable_address_city()->set_value("City");
  contact_info_specifics.mutable_address_city()->mutable_metadata()->set_status(
      sync_pb::ContactInfoSpecifics::VerificationStatus::
          ContactInfoSpecifics_VerificationStatus_OBSERVED);

  sync_pb::ContactInfoSpecifics empty_contact_info_specifics;
  EXPECT_EQ(TrimContactInfoSpecificsDataForCaching(contact_info_specifics)
                .SerializeAsString(),
            empty_contact_info_specifics.SerializeAsString());
}

// Test that supported fields and nested messages are successfully trimmed but
// that unsupported fields are preserved.
TEST_F(ContactInfoSyncUtilTest,
       TrimAllSupportedFieldsFromRemoteSpecifics_PreserveUnsupportedFields) {
  sync_pb::ContactInfoSpecifics contact_info_specifics_with_only_unknown_fields;

  // Set an unsupported field in both the top-level message and also in a nested
  // message.
  *contact_info_specifics_with_only_unknown_fields.mutable_unknown_fields() =
      "unsupported_fields";
  *contact_info_specifics_with_only_unknown_fields.mutable_address_city()
       ->mutable_unknown_fields() = "unsupported_field_in_nested_message";

  // Create a copy and set a value to the same nested message that already
  // contains an unsupported field.
  sync_pb::ContactInfoSpecifics
      contact_info_specifics_with_known_and_unknown_fields =
          contact_info_specifics_with_only_unknown_fields;
  contact_info_specifics_with_known_and_unknown_fields.mutable_address_city()
      ->set_value("City");

  EXPECT_EQ(TrimContactInfoSpecificsDataForCaching(
                contact_info_specifics_with_known_and_unknown_fields)
                .SerializeAsString(),
            contact_info_specifics_with_only_unknown_fields
                .SerializePartialAsString());
}

// Test that the conversion of a profile to specifics preserve the unsupported
// fields.
TEST_P(ContactInfoSyncUtilTest, ContactInfoSpecificsFromAutofillProfile) {
  // Create the base message that only contains unsupported fields in both the
  // top-level and a nested message.
  sync_pb::ContactInfoSpecifics contact_info_specifics_with_only_unknown_fields;
  *contact_info_specifics_with_only_unknown_fields.mutable_unknown_fields() =
      "unsupported_fields";
  *contact_info_specifics_with_only_unknown_fields.mutable_address_city()
       ->mutable_unknown_fields() = "unsupported_field_in_nested_message";
  ContactInfoSpecifics contact_info_specifics_from_profile =
      ContactInfoSpecificsFromAutofillProfile(
          GetAutofillProfileForCountry(GetParam()),
          contact_info_specifics_with_only_unknown_fields);

  // Test that the unknown fields are preserved and that the rest of the
  // specifics match the expectations.
  sync_pb::ContactInfoSpecifics expected_contact_info =
      GetContactInfoSpecificsForCountry(GetParam());
  *expected_contact_info.mutable_unknown_fields() = "unsupported_fields";
  *expected_contact_info.mutable_address_city()->mutable_unknown_fields() =
      "unsupported_field_in_nested_message";

  EXPECT_EQ(contact_info_specifics_from_profile.SerializeAsString(),
            expected_contact_info.SerializeAsString());
}

// Test that converting ContactInfoSpecifics -> AutofillProfile works.
TEST_P(ContactInfoSyncUtilTest, CreateAutofillProfileFromContactInfoSpecifics) {
  ContactInfoSpecifics specifics =
      GetContactInfoSpecificsForCountry(GetParam());
  AutofillProfile profile = GetAutofillProfileForCountry(GetParam());

  std::optional<AutofillProfile> converted_profile =
      CreateAutofillProfileFromContactInfoSpecifics(specifics);
  ASSERT_TRUE(converted_profile.has_value());
  EXPECT_TRUE(test_api(profile).EqualsIncludingUsageStats(*converted_profile));
}

// Test that only specifics with valid GUID are converted.
TEST_F(ContactInfoSyncUtilTest,
       CreateAutofillProfileFromContactInfoSpecifics_InvalidGUID) {
  ContactInfoSpecifics specifics;
  specifics.set_guid(kInvalidGuid);
  EXPECT_FALSE(
      CreateAutofillProfileFromContactInfoSpecifics(specifics).has_value());
}

// Tests that H/W address types are converted to
// AutofillProfile::RecordType correctly.
TEST_F(ContactInfoSyncUtilTest,
       CreateAutofillProfileFromContactInfoSpecifics_AddressTypes) {
  ContactInfoSpecifics specifics = ConstructBaseSpecifics();

  specifics.set_address_type(ContactInfoSpecifics::HOME);
  EXPECT_EQ(
      CreateAutofillProfileFromContactInfoSpecifics(specifics)->record_type(),
      AutofillProfile::RecordType::kAccountHome);

  specifics.set_address_type(ContactInfoSpecifics::WORK);
  EXPECT_EQ(
      CreateAutofillProfileFromContactInfoSpecifics(specifics)->record_type(),
      AutofillProfile::RecordType::kAccountWork);
}

// Tests that if a token's `value` changes by external means, its observations
// are reset.
TEST_F(ContactInfoSyncUtilTest, ObservationResetting) {
  // Create a profile and collect an observation for NAME_FIRST.
  AutofillProfile profile = test::GetFullProfile();
  test_api(profile).set_record_type(AutofillProfile::RecordType::kAccount);
  test_api(profile.token_quality())
      .AddObservation(NAME_FIRST,
                      ProfileTokenQuality::ObservationType::kAccepted);

  // Simulate sending the `profile` to Sync and modifying its NAME_FIRST by an
  // external integrator. Since metadata is opaque to external integrators, the
  // metadata's `value_hash` is not updated.
  std::unique_ptr<syncer::EntityData> entity_data =
      CreateContactInfoEntityDataFromAutofillProfile(
          profile, /*base_contact_info_specifics=*/{});
  ASSERT_NE(entity_data, nullptr);
  ContactInfoSpecifics* specifics =
      entity_data->specifics.mutable_contact_info();
  specifics->mutable_name_first()->set_value("different " +
                                             specifics->name_first().value());

  // Simulate syncing the `specifics` back to Autofill. Expect that the
  // NAME_FIRST observations are cleared.
  std::optional<AutofillProfile> updated_profile =
      CreateAutofillProfileFromContactInfoSpecifics(*specifics);
  ASSERT_TRUE(updated_profile.has_value());
  EXPECT_TRUE(updated_profile->token_quality()
                  .GetObservationTypesForFieldType(NAME_FIRST)
                  .empty());
}

INSTANTIATE_TEST_SUITE_P(AutofillI18nModels,
                         ContactInfoSyncUtilTest,
                         testing::Values(I18nCountryModel::kLegacy,
                                         I18nCountryModel::kBR,
                                         I18nCountryModel::kMX,
                                         I18nCountryModel::kIN));

}  // namespace
}  // namespace autofill
