// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_profile_sync_util.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_component.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill/core/browser/proto/autofill_sync.pb.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/sync/protocol/entity_data.h"

using autofill::data_util::TruncateUTF8;
using base::UTF16ToUTF8;
using base::UTF8ToUTF16;
using sync_pb::AutofillProfileSpecifics;
using syncer::EntityData;

namespace autofill {

namespace {

//  Converts the verification status representation used in the
//  AutofillProfileSpecifics to the representation used in AutofillProfile.
VerificationStatus ConvertSpecificsToProfileVerificationStatus(
    AutofillProfileSpecifics::VerificationStatus entity_status) {
  switch (entity_status) {
    case sync_pb::
        AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED:
      return VerificationStatus::kNoStatus;
    case sync_pb::AutofillProfileSpecifics_VerificationStatus_PARSED:
      return VerificationStatus::kParsed;
    case sync_pb::AutofillProfileSpecifics_VerificationStatus_FORMATTED:
      return VerificationStatus::kFormatted;
    case sync_pb::AutofillProfileSpecifics_VerificationStatus_OBSERVED:
      return VerificationStatus::kObserved;
    case sync_pb::AutofillProfileSpecifics_VerificationStatus_USER_VERIFIED:
      return VerificationStatus::kUserVerified;
    case sync_pb::AutofillProfileSpecifics_VerificationStatus_SERVER_PARSED:
      return VerificationStatus::kServerParsed;
  }
}

// Converts the verification status representation used AutofillProfiles to the
// one used in AutofillProfileSpecifics.
AutofillProfileSpecifics::VerificationStatus
ConvertProfileToSpecificsVerificationStatus(VerificationStatus profile_status) {
  switch (profile_status) {
    case (VerificationStatus::kNoStatus):
      return sync_pb::
          AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED;
    case (VerificationStatus::kParsed):
      return sync_pb::AutofillProfileSpecifics_VerificationStatus_PARSED;
    case (VerificationStatus::kFormatted):
      return sync_pb::AutofillProfileSpecifics_VerificationStatus_FORMATTED;
    case (VerificationStatus::kObserved):
      return sync_pb::AutofillProfileSpecifics_VerificationStatus_OBSERVED;
    case (VerificationStatus::kUserVerified):
      return sync_pb::AutofillProfileSpecifics_VerificationStatus_USER_VERIFIED;
    case (VerificationStatus::kServerParsed):
      return sync_pb::AutofillProfileSpecifics_VerificationStatus_SERVER_PARSED;
  }
}

bool IsAutofillProfileSpecificsValid(
    const AutofillProfileSpecifics& specifics) {
  return base::Uuid::ParseCaseInsensitive(specifics.guid()).is_valid();
}

}  // namespace

std::unique_ptr<EntityData> CreateEntityDataFromAutofillProfile(
    const AutofillProfile& entry) {
  // Validity of the guid is guaranteed by the database layer.
  DCHECK(base::Uuid::ParseCaseInsensitive(entry.guid()).is_valid());

  // Profiles fall into two categories, kLocalOrSyncable and kAccount.
  // kLocalOrSyncable profiles are synced through the AutofillProfileSyncBridge,
  // while kAccount profiles are synced through the ContactInfoSyncBridge. Make
  // sure that syncing a profile through the wrong sync bridge fails early.
  if (entry.source() != AutofillProfile::Source::kLocalOrSyncable)
    return nullptr;

  auto entity_data = std::make_unique<EntityData>();
  entity_data->name = entry.guid();
  AutofillProfileSpecifics* specifics =
      entity_data->specifics.mutable_autofill_profile();

  specifics->set_guid(entry.guid());
  // TODO(crbug.com/1441905): Remove the origin field from
  // AutofillProfileSpecifics. AutofillProfile::origin was already deprecated,
  // effectively treating all profiles as unverified. However, older clients
  // reject updates to verified profiles from unverified profiles. To retain
  // syncing functionality, all profiles are explicitly synced as verified.
  specifics->set_deprecated_origin(kSettingsOrigin);

  if (!entry.profile_label().empty())
    specifics->set_profile_label(entry.profile_label());

  specifics->set_use_count(entry.use_count());
  specifics->set_use_date(entry.use_date().ToTimeT());
  specifics->set_address_home_language_code(
      TruncateUTF8(entry.language_code()));

  // Set name-related values.
  specifics->add_name_honorific(
      TruncateUTF8(UTF16ToUTF8(entry.GetRawInfo(NAME_HONORIFIC_PREFIX))));
  specifics->add_name_first(
      TruncateUTF8(UTF16ToUTF8(entry.GetRawInfo(NAME_FIRST))));
  specifics->add_name_middle(
      TruncateUTF8(UTF16ToUTF8(entry.GetRawInfo(NAME_MIDDLE))));
  specifics->add_name_last(
      TruncateUTF8(UTF16ToUTF8(entry.GetRawInfo(NAME_LAST))));
  specifics->add_name_last_first(
      TruncateUTF8(UTF16ToUTF8(entry.GetRawInfo(NAME_LAST_FIRST))));
  specifics->add_name_last_second(
      TruncateUTF8(UTF16ToUTF8(entry.GetRawInfo(NAME_LAST_SECOND))));
  specifics->add_name_last_conjunction(
      TruncateUTF8(UTF16ToUTF8(entry.GetRawInfo(NAME_LAST_CONJUNCTION))));
  specifics->add_name_full(
      TruncateUTF8(UTF16ToUTF8(entry.GetRawInfo(NAME_FULL))));
  specifics->add_name_full_with_honorific(TruncateUTF8(
      UTF16ToUTF8(entry.GetRawInfo(NAME_FULL_WITH_HONORIFIC_PREFIX))));

  // Set address-related statuses.
  specifics->add_name_honorific_status(
      ConvertProfileToSpecificsVerificationStatus(
          entry.GetVerificationStatus(NAME_HONORIFIC_PREFIX)));
  specifics->add_name_first_status(ConvertProfileToSpecificsVerificationStatus(
      entry.GetVerificationStatus(NAME_FIRST)));
  specifics->add_name_middle_status(ConvertProfileToSpecificsVerificationStatus(
      entry.GetVerificationStatus(NAME_MIDDLE)));
  specifics->add_name_last_status(ConvertProfileToSpecificsVerificationStatus(
      entry.GetVerificationStatus(NAME_LAST)));
  specifics->add_name_last_first_status(
      ConvertProfileToSpecificsVerificationStatus(
          entry.GetVerificationStatus(NAME_LAST_FIRST)));
  specifics->add_name_last_conjunction_status(
      ConvertProfileToSpecificsVerificationStatus(
          entry.GetVerificationStatus(NAME_LAST_CONJUNCTION)));
  specifics->add_name_last_second_status(
      ConvertProfileToSpecificsVerificationStatus(
          entry.GetVerificationStatus(NAME_LAST_SECOND)));
  specifics->add_name_full_status(ConvertProfileToSpecificsVerificationStatus(
      entry.GetVerificationStatus(NAME_FULL)));
  specifics->add_name_full_with_honorific_status(
      ConvertProfileToSpecificsVerificationStatus(
          entry.GetVerificationStatus(NAME_FULL_WITH_HONORIFIC_PREFIX)));

  // Set email, phone and company values.
  specifics->add_email_address(
      TruncateUTF8(UTF16ToUTF8(entry.GetRawInfo(EMAIL_ADDRESS))));
  specifics->add_phone_home_whole_number(
      TruncateUTF8(UTF16ToUTF8(entry.GetRawInfo(PHONE_HOME_WHOLE_NUMBER))));
  specifics->set_company_name(
      TruncateUTF8(UTF16ToUTF8(entry.GetRawInfo(COMPANY_NAME))));

  // Set address-related values.
  specifics->set_address_home_city(
      TruncateUTF8(UTF16ToUTF8(entry.GetRawInfo(ADDRESS_HOME_CITY))));
  specifics->set_address_home_state(
      TruncateUTF8(UTF16ToUTF8(entry.GetRawInfo(ADDRESS_HOME_STATE))));
  specifics->set_address_home_zip(
      TruncateUTF8(UTF16ToUTF8(entry.GetRawInfo(ADDRESS_HOME_ZIP))));
  specifics->set_address_home_sorting_code(
      TruncateUTF8(UTF16ToUTF8(entry.GetRawInfo(ADDRESS_HOME_SORTING_CODE))));
  specifics->set_address_home_dependent_locality(TruncateUTF8(
      UTF16ToUTF8(entry.GetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY))));
  specifics->set_address_home_country(
      TruncateUTF8(UTF16ToUTF8(entry.GetRawInfo(ADDRESS_HOME_COUNTRY))));
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForAddressOverflow)) {
    specifics->set_address_home_overflow(
        TruncateUTF8(UTF16ToUTF8(entry.GetRawInfo(ADDRESS_HOME_OVERFLOW))));
  }
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForLandmark)) {
    specifics->set_address_home_landmark(
        TruncateUTF8(UTF16ToUTF8(entry.GetRawInfo(ADDRESS_HOME_LANDMARK))));
  }
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForBetweenStreetsOrLandmark)) {
    specifics->set_address_home_between_streets_or_landmark(
        TruncateUTF8(UTF16ToUTF8(
            entry.GetRawInfo(ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK))));
  }
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForAddressOverflowAndLandmark)) {
    specifics->set_address_home_overflow_and_landmark(TruncateUTF8(
        UTF16ToUTF8(entry.GetRawInfo(ADDRESS_HOME_OVERFLOW_AND_LANDMARK))));
  }
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForBetweenStreets)) {
    specifics->set_address_home_between_streets(TruncateUTF8(
        UTF16ToUTF8(entry.GetRawInfo(ADDRESS_HOME_BETWEEN_STREETS))));
    specifics->set_address_home_between_streets_1(TruncateUTF8(
        UTF16ToUTF8(entry.GetRawInfo(ADDRESS_HOME_BETWEEN_STREETS_1))));
    specifics->set_address_home_between_streets_2(TruncateUTF8(
        UTF16ToUTF8(entry.GetRawInfo(ADDRESS_HOME_BETWEEN_STREETS_2))));
  }
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForAdminLevel2)) {
    specifics->set_address_home_admin_level_2(
        TruncateUTF8(UTF16ToUTF8(entry.GetRawInfo(ADDRESS_HOME_ADMIN_LEVEL2))));
  }
  specifics->set_address_home_street_address(
      TruncateUTF8(UTF16ToUTF8(entry.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS))));
  specifics->set_address_home_line1(
      TruncateUTF8(UTF16ToUTF8(entry.GetRawInfo(ADDRESS_HOME_LINE1))));
  specifics->set_address_home_line2(
      TruncateUTF8(UTF16ToUTF8(entry.GetRawInfo(ADDRESS_HOME_LINE2))));
  specifics->set_address_home_thoroughfare_name(
      UTF16ToUTF8(entry.GetRawInfo(ADDRESS_HOME_STREET_NAME)));
  specifics->set_address_home_thoroughfare_number(
      UTF16ToUTF8(entry.GetRawInfo(ADDRESS_HOME_HOUSE_NUMBER)));
  specifics->set_address_home_street_location(
      UTF16ToUTF8(entry.GetRawInfo(ADDRESS_HOME_STREET_LOCATION)));
  specifics->set_address_home_subpremise_name(
      UTF16ToUTF8(entry.GetRawInfo(ADDRESS_HOME_SUBPREMISE)));
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForApartmentNumbers)) {
    specifics->set_address_home_apt(
        UTF16ToUTF8(entry.GetRawInfo(ADDRESS_HOME_APT)));
    specifics->set_address_home_apt_num(
        UTF16ToUTF8(entry.GetRawInfo(ADDRESS_HOME_APT_NUM)));
    specifics->set_address_home_apt_type(
        UTF16ToUTF8(entry.GetRawInfo(ADDRESS_HOME_APT_TYPE)));
  }
  specifics->set_address_home_floor(
      UTF16ToUTF8(entry.GetRawInfo(ADDRESS_HOME_FLOOR)));

  // Set address-related statuses.
  specifics->set_address_home_city_status(
      ConvertProfileToSpecificsVerificationStatus(
          entry.GetVerificationStatus(ADDRESS_HOME_CITY)));
  specifics->set_address_home_state_status(
      ConvertProfileToSpecificsVerificationStatus(
          entry.GetVerificationStatus(ADDRESS_HOME_STATE)));
  specifics->set_address_home_zip_status(
      ConvertProfileToSpecificsVerificationStatus(
          entry.GetVerificationStatus(ADDRESS_HOME_ZIP)));
  specifics->set_address_home_sorting_code_status(
      ConvertProfileToSpecificsVerificationStatus(
          entry.GetVerificationStatus(ADDRESS_HOME_SORTING_CODE)));
  specifics->set_address_home_dependent_locality_status(
      ConvertProfileToSpecificsVerificationStatus(
          entry.GetVerificationStatus(ADDRESS_HOME_DEPENDENT_LOCALITY)));
  specifics->set_address_home_country_status(
      ConvertProfileToSpecificsVerificationStatus(
          entry.GetVerificationStatus(ADDRESS_HOME_COUNTRY)));
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForAddressOverflow)) {
    specifics->set_address_home_overflow_status(
        ConvertProfileToSpecificsVerificationStatus(
            entry.GetVerificationStatus(ADDRESS_HOME_OVERFLOW)));
  }
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForLandmark)) {
    specifics->set_address_home_landmark_status(
        ConvertProfileToSpecificsVerificationStatus(
            entry.GetVerificationStatus(ADDRESS_HOME_LANDMARK)));
  }
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForBetweenStreets)) {
    specifics->set_address_home_between_streets_status(
        ConvertProfileToSpecificsVerificationStatus(
            entry.GetVerificationStatus(ADDRESS_HOME_BETWEEN_STREETS)));
    specifics->set_address_home_between_streets_1_status(
        ConvertProfileToSpecificsVerificationStatus(
            entry.GetVerificationStatus(ADDRESS_HOME_BETWEEN_STREETS_1)));
    specifics->set_address_home_between_streets_2_status(
        ConvertProfileToSpecificsVerificationStatus(
            entry.GetVerificationStatus(ADDRESS_HOME_BETWEEN_STREETS_2)));
  }
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForBetweenStreetsOrLandmark)) {
    specifics->set_address_home_between_streets_or_landmark_status(
        ConvertProfileToSpecificsVerificationStatus(entry.GetVerificationStatus(
            ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK)));
  }
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForAddressOverflowAndLandmark)) {
    specifics->set_address_home_overflow_and_landmark_status(
        ConvertProfileToSpecificsVerificationStatus(
            entry.GetVerificationStatus(ADDRESS_HOME_OVERFLOW_AND_LANDMARK)));
  }
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForAdminLevel2)) {
    specifics->set_address_home_admin_level_2_status(
        ConvertProfileToSpecificsVerificationStatus(
            entry.GetVerificationStatus(ADDRESS_HOME_ADMIN_LEVEL2)));
  }
  specifics->set_address_home_street_address_status(
      ConvertProfileToSpecificsVerificationStatus(
          entry.GetVerificationStatus(ADDRESS_HOME_STREET_ADDRESS)));
  specifics->set_address_home_thoroughfare_name_status(
      ConvertProfileToSpecificsVerificationStatus(
          entry.GetVerificationStatus(ADDRESS_HOME_STREET_NAME)));
  specifics->set_address_home_thoroughfare_number_status(
      ConvertProfileToSpecificsVerificationStatus(
          entry.GetVerificationStatus(ADDRESS_HOME_HOUSE_NUMBER)));
  specifics->set_address_home_street_location_status(
      ConvertProfileToSpecificsVerificationStatus(
          entry.GetVerificationStatus(ADDRESS_HOME_STREET_LOCATION)));
  specifics->set_address_home_subpremise_name_status(
      ConvertProfileToSpecificsVerificationStatus(
          entry.GetVerificationStatus(ADDRESS_HOME_SUBPREMISE)));
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForApartmentNumbers)) {
    specifics->set_address_home_apt_status(
        ConvertProfileToSpecificsVerificationStatus(
            entry.GetVerificationStatus(ADDRESS_HOME_APT)));
    specifics->set_address_home_apt_num_status(
        ConvertProfileToSpecificsVerificationStatus(
            entry.GetVerificationStatus(ADDRESS_HOME_APT_NUM)));
    specifics->set_address_home_apt_type_status(
        ConvertProfileToSpecificsVerificationStatus(
            entry.GetVerificationStatus(ADDRESS_HOME_APT_TYPE)));
  }
  specifics->set_address_home_floor_status(
      ConvertProfileToSpecificsVerificationStatus(
          entry.GetVerificationStatus(ADDRESS_HOME_FLOOR)));

  // Set birthdate-related values.
  specifics->set_birthdate_day(entry.GetRawInfoAsInt(BIRTHDATE_DAY));
  specifics->set_birthdate_month(entry.GetRawInfoAsInt(BIRTHDATE_MONTH));
  specifics->set_birthdate_year(entry.GetRawInfoAsInt(BIRTHDATE_4_DIGIT_YEAR));

  return entity_data;
}

std::unique_ptr<AutofillProfile> CreateAutofillProfileFromSpecifics(
    const AutofillProfileSpecifics& specifics) {
  if (!IsAutofillProfileSpecificsValid(specifics)) {
    return nullptr;
  }
  // Update the country field, which can contain either a country code (if set
  // by a newer version of Chrome), or a country name (if set by an older
  // version of Chrome).
  std::u16string country_name_or_code =
      base::ASCIIToUTF16(specifics.address_home_country());
  std::string country_code =
      CountryNames::GetInstance()->GetCountryCode(country_name_or_code);

  std::unique_ptr<AutofillProfile> profile = std::make_unique<AutofillProfile>(
      specifics.guid(), AutofillProfile::Source::kLocalOrSyncable,
      AddressCountryCode(country_code));

  // Set info that has a default value (and does not distinguish whether it is
  // set or not).
  profile->set_use_count(specifics.use_count());
  profile->set_use_date(base::Time::FromTimeT(specifics.use_date()));
  profile->set_language_code(specifics.address_home_language_code());

  // Set the profile label if it exists.
  if (specifics.has_profile_label())
    profile->set_profile_label(specifics.profile_label());

  // Set repeated fields.
  profile->SetRawInfoWithVerificationStatus(
      NAME_HONORIFIC_PREFIX,
      UTF8ToUTF16(specifics.name_honorific_size() ? specifics.name_honorific(0)
                                                  : std::string()),
      ConvertSpecificsToProfileVerificationStatus(
          specifics.name_honorific_status_size()
              ? specifics.name_honorific_status(0)
              : AutofillProfileSpecifics::VerificationStatus::
                    AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED));

  profile->SetRawInfoWithVerificationStatus(
      NAME_FULL_WITH_HONORIFIC_PREFIX,
      UTF8ToUTF16(specifics.name_full_with_honorific_size()
                      ? specifics.name_full_with_honorific(0)
                      : std::string()),
      ConvertSpecificsToProfileVerificationStatus(
          specifics.name_full_with_honorific_status_size()
              ? specifics.name_full_with_honorific_status(0)
              : AutofillProfileSpecifics::VerificationStatus::
                    AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED));

  profile->SetRawInfoWithVerificationStatus(
      NAME_FIRST,
      UTF8ToUTF16(specifics.name_first_size() ? specifics.name_first(0)
                                              : std::string()),
      ConvertSpecificsToProfileVerificationStatus(
          specifics.name_first_status_size()
              ? specifics.name_first_status(0)
              : AutofillProfileSpecifics::VerificationStatus::
                    AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED));

  profile->SetRawInfoWithVerificationStatus(
      NAME_MIDDLE,
      UTF8ToUTF16(specifics.name_middle_size() ? specifics.name_middle(0)
                                               : std::string()),
      ConvertSpecificsToProfileVerificationStatus(
          specifics.name_middle_status_size()
              ? specifics.name_middle_status(0)
              : AutofillProfileSpecifics::VerificationStatus::
                    AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED));

  profile->SetRawInfoWithVerificationStatus(
      NAME_LAST,
      UTF8ToUTF16(specifics.name_last_size() ? specifics.name_last(0)
                                             : std::string()),
      ConvertSpecificsToProfileVerificationStatus(
          specifics.name_last_status_size()
              ? specifics.name_last_status(0)
              : AutofillProfileSpecifics::VerificationStatus::
                    AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED));

  profile->SetRawInfoWithVerificationStatus(
      NAME_LAST_FIRST,
      UTF8ToUTF16(specifics.name_last_first_size()
                      ? specifics.name_last_first(0)
                      : std::string()),
      ConvertSpecificsToProfileVerificationStatus(
          specifics.name_last_first_status_size()
              ? specifics.name_last_first_status(0)
              : AutofillProfileSpecifics::VerificationStatus::
                    AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED));

  profile->SetRawInfoWithVerificationStatus(
      NAME_LAST_CONJUNCTION,
      UTF8ToUTF16(specifics.name_last_conjunction_size()
                      ? specifics.name_last_conjunction(0)
                      : std::string()),
      ConvertSpecificsToProfileVerificationStatus(
          specifics.name_last_conjunction_status_size()
              ? specifics.name_last_conjunction_status(0)
              : AutofillProfileSpecifics::VerificationStatus::
                    AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED));

  profile->SetRawInfoWithVerificationStatus(
      NAME_LAST_SECOND,
      UTF8ToUTF16(specifics.name_last_second_size()
                      ? specifics.name_last_second(0)
                      : std::string()),
      ConvertSpecificsToProfileVerificationStatus(
          specifics.name_last_second_status_size()
              ? specifics.name_last_second_status(0)
              : AutofillProfileSpecifics::VerificationStatus::
                    AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED));

  // Older versions don't have a separate full name; don't overwrite full name
  // in this case.
  if (specifics.name_full_size() > 0) {
    profile->SetRawInfoWithVerificationStatus(
        NAME_FULL, UTF8ToUTF16(specifics.name_full(0)),
        ConvertSpecificsToProfileVerificationStatus(
            specifics.name_full_status_size()
                ? specifics.name_full_status(0)
                : AutofillProfileSpecifics::VerificationStatus::
                      AutofillProfileSpecifics_VerificationStatus_VERIFICATION_STATUS_UNSPECIFIED));
  }

  profile->SetRawInfo(
      EMAIL_ADDRESS,
      UTF8ToUTF16(specifics.email_address_size() ? specifics.email_address(0)
                                                 : std::string()));
  profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                      UTF8ToUTF16(specifics.phone_home_whole_number_size()
                                      ? specifics.phone_home_whole_number(0)
                                      : std::string()));

  // Set simple single-valued fields.
  profile->SetRawInfo(COMPANY_NAME, UTF8ToUTF16(specifics.company_name()));

  profile->SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_CITY, UTF8ToUTF16(specifics.address_home_city()),
      ConvertSpecificsToProfileVerificationStatus(
          specifics.address_home_city_status()));

  profile->SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STATE, UTF8ToUTF16(specifics.address_home_state()),
      ConvertSpecificsToProfileVerificationStatus(
          specifics.address_home_state_status()));

  profile->SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_ZIP, UTF8ToUTF16(specifics.address_home_zip()),
      ConvertSpecificsToProfileVerificationStatus(
          specifics.address_home_zip_status()));

  profile->SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_SORTING_CODE,
      UTF8ToUTF16(specifics.address_home_sorting_code()),
      ConvertSpecificsToProfileVerificationStatus(
          specifics.address_home_sorting_code_status()));

  profile->SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_DEPENDENT_LOCALITY,
      UTF8ToUTF16(specifics.address_home_dependent_locality()),
      ConvertSpecificsToProfileVerificationStatus(
          specifics.address_home_dependent_locality_status()));


  profile->SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_COUNTRY, UTF8ToUTF16(country_code),
      ConvertSpecificsToProfileVerificationStatus(
          specifics.address_home_country_status()));

  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForAddressOverflow)) {
    profile->SetRawInfoWithVerificationStatus(
        ADDRESS_HOME_OVERFLOW, UTF8ToUTF16(specifics.address_home_overflow()),
        ConvertSpecificsToProfileVerificationStatus(
            specifics.address_home_overflow_status()));
  }

  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForBetweenStreetsOrLandmark)) {
    profile->SetRawInfoWithVerificationStatus(
        ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK,
        UTF8ToUTF16(specifics.address_home_between_streets_or_landmark()),
        ConvertSpecificsToProfileVerificationStatus(
            specifics.address_home_between_streets_or_landmark_status()));
  }

  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForAddressOverflowAndLandmark)) {
    profile->SetRawInfoWithVerificationStatus(
        ADDRESS_HOME_OVERFLOW_AND_LANDMARK,
        UTF8ToUTF16(specifics.address_home_overflow_and_landmark()),
        ConvertSpecificsToProfileVerificationStatus(
            specifics.address_home_overflow_and_landmark_status()));
  }

  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForLandmark)) {
    profile->SetRawInfoWithVerificationStatus(
        ADDRESS_HOME_LANDMARK, UTF8ToUTF16(specifics.address_home_landmark()),
        ConvertSpecificsToProfileVerificationStatus(
            specifics.address_home_landmark_status()));
  }

  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForBetweenStreets)) {
    profile->SetRawInfoWithVerificationStatus(
        ADDRESS_HOME_BETWEEN_STREETS,
        UTF8ToUTF16(specifics.address_home_between_streets()),
        ConvertSpecificsToProfileVerificationStatus(
            specifics.address_home_between_streets_status()));
    profile->SetRawInfoWithVerificationStatus(
        ADDRESS_HOME_BETWEEN_STREETS_1,
        UTF8ToUTF16(specifics.address_home_between_streets_1()),
        ConvertSpecificsToProfileVerificationStatus(
            specifics.address_home_between_streets_1_status()));
    profile->SetRawInfoWithVerificationStatus(
        ADDRESS_HOME_BETWEEN_STREETS_2,
        UTF8ToUTF16(specifics.address_home_between_streets_2()),
        ConvertSpecificsToProfileVerificationStatus(
            specifics.address_home_between_streets_2_status()));
  }

  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForAdminLevel2)) {
    profile->SetRawInfoWithVerificationStatus(
        ADDRESS_HOME_ADMIN_LEVEL2,
        UTF8ToUTF16(specifics.address_home_admin_level_2()),
        ConvertSpecificsToProfileVerificationStatus(
            specifics.address_home_admin_level_2_status()));
  }

  // Set either the deprecated subparts (line1 & line2) or the full address
  // (street_address) if it is present. This is needed because all the address
  // fields are backed by the same storage.
  if (specifics.has_address_home_street_address()) {
    profile->SetRawInfoWithVerificationStatus(
        ADDRESS_HOME_STREET_ADDRESS,
        UTF8ToUTF16(specifics.address_home_street_address()),
        ConvertSpecificsToProfileVerificationStatus(
            specifics.address_home_street_address_status()));
  } else {
    profile->SetRawInfo(ADDRESS_HOME_LINE1,
                        UTF8ToUTF16(specifics.address_home_line1()));
    profile->SetRawInfo(ADDRESS_HOME_LINE2,
                        UTF8ToUTF16(specifics.address_home_line2()));
  }

  profile->SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_NAME,
      UTF8ToUTF16(specifics.address_home_thoroughfare_name()),
      ConvertSpecificsToProfileVerificationStatus(
          specifics.address_home_thoroughfare_name_status()));

  profile->SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_HOUSE_NUMBER,
      UTF8ToUTF16(specifics.address_home_thoroughfare_number()),
      ConvertSpecificsToProfileVerificationStatus(
          specifics.address_home_thoroughfare_number_status()));

  profile->SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_LOCATION,
      UTF8ToUTF16(specifics.address_home_street_location()),
      ConvertSpecificsToProfileVerificationStatus(
          specifics.address_home_street_location_status()));

  profile->SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_SUBPREMISE,
      UTF8ToUTF16(specifics.address_home_subpremise_name()),
      ConvertSpecificsToProfileVerificationStatus(
          specifics.address_home_subpremise_name_status()));

  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForApartmentNumbers)) {
    profile->SetRawInfoWithVerificationStatus(
        ADDRESS_HOME_APT, UTF8ToUTF16(specifics.address_home_apt()),
        ConvertSpecificsToProfileVerificationStatus(
            specifics.address_home_apt_status()));
    profile->SetRawInfoWithVerificationStatus(
        ADDRESS_HOME_APT_NUM, UTF8ToUTF16(specifics.address_home_apt_num()),
        ConvertSpecificsToProfileVerificationStatus(
            specifics.address_home_apt_num_status()));
    profile->SetRawInfoWithVerificationStatus(
        ADDRESS_HOME_APT_TYPE, UTF8ToUTF16(specifics.address_home_apt_type()),
        ConvertSpecificsToProfileVerificationStatus(
            specifics.address_home_apt_type_status()));
  }

  profile->SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_FLOOR, UTF8ToUTF16(specifics.address_home_floor()),
      ConvertSpecificsToProfileVerificationStatus(
          specifics.address_home_floor_status()));

  // Set birthdate-related fields.
  profile->SetRawInfoAsInt(BIRTHDATE_DAY, specifics.birthdate_day());
  profile->SetRawInfoAsInt(BIRTHDATE_MONTH, specifics.birthdate_month());
  profile->SetRawInfoAsInt(BIRTHDATE_4_DIGIT_YEAR, specifics.birthdate_year());

  // When adding field types, ensure that they don't need to be added here and
  // update the last checked value.
  static_assert(ServerFieldType::MAX_VALID_FIELD_TYPE == 161,
                "New field type needs to be reviewed for inclusion in sync");

  // The profile may be in a legacy state. By calling |FinalizeAfterImport()|
  // * The profile is migrated if the name structure is in legacy state.
  // * Nothing happens if the profile is already migrated and therefore
  // finalized.
  // * If structured names are not enabled, this operation is a noop.
  //
  // Here, the return value of the finalization step does not have an
  // implication.
  profile->FinalizeAfterImport();
  return profile;
}

std::string GetStorageKeyFromAutofillProfile(const AutofillProfile& entry) {
  // Validity of the guid is guaranteed by the database layer.
  DCHECK(base::Uuid::ParseCaseInsensitive(entry.guid()).is_valid());
  return entry.guid();
}

std::string GetStorageKeyFromAutofillProfileSpecifics(
    const AutofillProfileSpecifics& specifics) {
  if (!IsAutofillProfileSpecificsValid(specifics)) {
    return std::string();
  }
  return specifics.guid();
}

}  // namespace autofill
