// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/contact_info_sync_util.h"

#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

namespace {

using sync_pb::ContactInfoSpecifics;

// Converts the verification status representation used in AutofillProfile to
// the one used in ContactInfoSpeicifics.
ContactInfoSpecifics::VerificationStatus
ConvertProfileToSpecificsVerificationStatus(VerificationStatus status) {
  switch (status) {
    case VerificationStatus::kNoStatus:
      return ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED;
    case VerificationStatus::kParsed:
      return ContactInfoSpecifics::PARSED;
    case VerificationStatus::kFormatted:
      return ContactInfoSpecifics::FORMATTED;
    case VerificationStatus::kObserved:
      return ContactInfoSpecifics::OBSERVED;
    case VerificationStatus::kUserVerified:
      return ContactInfoSpecifics::USER_VERIFIED;
    case VerificationStatus::kServerParsed:
      return ContactInfoSpecifics::SERVER_PARSED;
  }
}

// Converts the verification status representation used in
// ContactInfoSpecifics to the one used in AutofillProfile.
VerificationStatus ConvertSpecificsToProfileVerificationStatus(
    ContactInfoSpecifics::VerificationStatus status) {
  switch (status) {
    case ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED:
      return VerificationStatus::kNoStatus;
    case ContactInfoSpecifics::PARSED:
      return VerificationStatus::kParsed;
    case ContactInfoSpecifics::FORMATTED:
      return VerificationStatus::kFormatted;
    case ContactInfoSpecifics::OBSERVED:
      return VerificationStatus::kObserved;
    case ContactInfoSpecifics::USER_VERIFIED:
      return VerificationStatus::kUserVerified;
    case ContactInfoSpecifics::SERVER_PARSED:
      return VerificationStatus::kServerParsed;
  }
}

// Helper class to simplify setting the value and metadata of
// ContactInfoSpecifics String- and IntegerTokens from an AutofillProfile.
class EntryDataSetter {
 public:
  explicit EntryDataSetter(const AutofillProfile& profile)
      : profile_(profile) {}

  void Set(ContactInfoSpecifics::StringToken* token,
           ServerFieldType type) const {
    token->set_value(base::UTF16ToUTF8(profile_.GetRawInfo(type)));
    SetMetadata(token->mutable_metadata(), type);
  }

  void Set(ContactInfoSpecifics::IntegerToken* token,
           ServerFieldType type) const {
    token->set_value(profile_.GetRawInfoAsInt(type));
    SetMetadata(token->mutable_metadata(), type);
  }

 private:
  void SetMetadata(ContactInfoSpecifics::TokenMetadata* metadata,
                   ServerFieldType type) const {
    metadata->set_status(ConvertProfileToSpecificsVerificationStatus(
        profile_.GetVerificationStatus(type)));
  }

  const AutofillProfile& profile_;
};

// Helper class to set the info and verification status of an AutofillProfile
// from ContactInfoSpecifics String- and Integer tokens.
class ProfileSetter {
 public:
  explicit ProfileSetter(AutofillProfile& profile) : profile_(profile) {}

  void Set(const ContactInfoSpecifics::StringToken& token,
           ServerFieldType type) {
    profile_.SetRawInfoWithVerificationStatus(
        type, base::UTF8ToUTF16(token.value()),
        ConvertSpecificsToProfileVerificationStatus(token.metadata().status()));
  }

  void Set(const ContactInfoSpecifics::IntegerToken& token,
           ServerFieldType type) {
    profile_.SetRawInfoAsIntWithVerificationStatus(
        type, token.value(),
        ConvertSpecificsToProfileVerificationStatus(token.metadata().status()));
  }

 private:
  AutofillProfile& profile_;
};

}  // namespace

std::unique_ptr<syncer::EntityData>
CreateContactInfoEntityDataFromAutofillProfile(const AutofillProfile& profile) {
  // Profiles fall into two categories, kLocalOrSyncable and kAccount.
  // kLocalOrSyncable profiles are synced through the AutofillProfileSyncBridge,
  // while kAccount profiles are synced through the ContactInfoSyncBridge. Make
  // sure that syncing a profile through the wrong sync bridge fails early.
  if (!base::IsValidGUID(profile.guid()) ||
      profile.source() != AutofillProfile::Source::kAccount) {
    return nullptr;
  }

  auto entity_data = std::make_unique<syncer::EntityData>();
  entity_data->name = profile.guid();

  ContactInfoSpecifics* specifics =
      entity_data->specifics.mutable_contact_info();

  specifics->set_guid(profile.guid());
  specifics->set_use_count(profile.use_count());
  specifics->set_use_date_windows_epoch_micros(profile.use_date().ToTimeT());
  specifics->set_date_modified_windows_epoch_micros(
      profile.modification_date().ToTimeT());
  specifics->set_language_code(profile.language_code());
  specifics->set_profile_label(profile.profile_label());
  specifics->set_initial_creator_id(profile.initial_creator_id());
  specifics->set_last_modifier_id(profile.last_modifier_id());

  EntryDataSetter s(profile);
  // Set name-related values and statuses.
  s.Set(specifics->mutable_name_honorific(), NAME_HONORIFIC_PREFIX);
  s.Set(specifics->mutable_name_first(), NAME_FIRST);
  s.Set(specifics->mutable_name_middle(), NAME_MIDDLE);
  s.Set(specifics->mutable_name_last(), NAME_LAST);
  s.Set(specifics->mutable_name_last_first(), NAME_LAST_FIRST);
  s.Set(specifics->mutable_name_last_conjunction(), NAME_LAST_CONJUNCTION);
  s.Set(specifics->mutable_name_last_second(), NAME_LAST_SECOND);
  s.Set(specifics->mutable_name_full(), NAME_FULL);
  s.Set(specifics->mutable_name_full_with_honorific(),
        NAME_FULL_WITH_HONORIFIC_PREFIX);

  // Set address-related values and statuses.
  s.Set(specifics->mutable_address_city(), ADDRESS_HOME_CITY);
  s.Set(specifics->mutable_address_state(), ADDRESS_HOME_STATE);
  s.Set(specifics->mutable_address_zip(), ADDRESS_HOME_ZIP);
  s.Set(specifics->mutable_address_country(), ADDRESS_HOME_COUNTRY);
  s.Set(specifics->mutable_address_street_address(),
        ADDRESS_HOME_STREET_ADDRESS);
  s.Set(specifics->mutable_address_sorting_code(), ADDRESS_HOME_SORTING_CODE);
  s.Set(specifics->mutable_address_dependent_locality(),
        ADDRESS_HOME_DEPENDENT_LOCALITY);
  s.Set(specifics->mutable_address_thoroughfare_name(),
        ADDRESS_HOME_STREET_NAME);
  s.Set(specifics->mutable_address_thoroughfare_number(),
        ADDRESS_HOME_HOUSE_NUMBER);
  s.Set(specifics->mutable_address_dependent_thoroughfare_name(),
        ADDRESS_HOME_DEPENDENT_STREET_NAME);
  s.Set(
      specifics->mutable_address_thoroughfare_and_dependent_thoroughfare_name(),
      ADDRESS_HOME_STREET_AND_DEPENDENT_STREET_NAME);
  s.Set(specifics->mutable_address_premise_name(), ADDRESS_HOME_PREMISE_NAME);
  s.Set(specifics->mutable_address_subpremise_name(), ADDRESS_HOME_SUBPREMISE);
  s.Set(specifics->mutable_address_apt_num(), ADDRESS_HOME_APT_NUM);
  s.Set(specifics->mutable_address_floor(), ADDRESS_HOME_FLOOR);

  // Set email, phone and company values and statuses.
  s.Set(specifics->mutable_email_address(), EMAIL_ADDRESS);
  s.Set(specifics->mutable_company_name(), COMPANY_NAME);
  s.Set(specifics->mutable_phone_home_whole_number(), PHONE_HOME_WHOLE_NUMBER);

  // Set birthdate-related values and statuses.
  s.Set(specifics->mutable_birthdate_day(), BIRTHDATE_DAY);
  s.Set(specifics->mutable_birthdate_month(), BIRTHDATE_MONTH);
  s.Set(specifics->mutable_birthdate_year(), BIRTHDATE_4_DIGIT_YEAR);

  DCHECK(AreContactInfoSpecificsValid(*specifics));
  return entity_data;
}

std::unique_ptr<AutofillProfile> CreateAutofillProfileFromContactInfoSpecifics(
    const ContactInfoSpecifics& specifics) {
  if (!AreContactInfoSpecificsValid(specifics))
    return nullptr;

  std::unique_ptr<AutofillProfile> profile = std::make_unique<AutofillProfile>(
      specifics.guid(), /*origin=*/"", AutofillProfile::Source::kAccount);

  profile->set_use_count(specifics.use_count());
  profile->set_use_date(
      base::Time::FromTimeT(specifics.use_date_windows_epoch_micros()));
  profile->set_modification_date(
      base::Time::FromTimeT(specifics.date_modified_windows_epoch_micros()));
  profile->set_language_code(specifics.language_code());
  profile->set_profile_label(specifics.profile_label());
  profile->set_initial_creator_id(specifics.initial_creator_id());
  profile->set_last_modifier_id(specifics.last_modifier_id());

  ProfileSetter s(*profile);
  // Set name-related values and statuses.
  s.Set(specifics.name_honorific(), NAME_HONORIFIC_PREFIX);
  s.Set(specifics.name_first(), NAME_FIRST);
  s.Set(specifics.name_middle(), NAME_MIDDLE);
  s.Set(specifics.name_last(), NAME_LAST);
  s.Set(specifics.name_last_first(), NAME_LAST_FIRST);
  s.Set(specifics.name_last_conjunction(), NAME_LAST_CONJUNCTION);
  s.Set(specifics.name_last_second(), NAME_LAST_SECOND);
  s.Set(specifics.name_full(), NAME_FULL);
  s.Set(specifics.name_full_with_honorific(), NAME_FULL_WITH_HONORIFIC_PREFIX);

  // Set address-related values and statuses.
  s.Set(specifics.address_city(), ADDRESS_HOME_CITY);
  s.Set(specifics.address_state(), ADDRESS_HOME_STATE);
  s.Set(specifics.address_zip(), ADDRESS_HOME_ZIP);
  s.Set(specifics.address_country(), ADDRESS_HOME_COUNTRY);
  s.Set(specifics.address_street_address(), ADDRESS_HOME_STREET_ADDRESS);
  s.Set(specifics.address_sorting_code(), ADDRESS_HOME_SORTING_CODE);
  s.Set(specifics.address_dependent_locality(),
        ADDRESS_HOME_DEPENDENT_LOCALITY);
  s.Set(specifics.address_thoroughfare_name(), ADDRESS_HOME_STREET_NAME);
  s.Set(specifics.address_thoroughfare_number(), ADDRESS_HOME_HOUSE_NUMBER);
  s.Set(specifics.address_dependent_thoroughfare_name(),
        ADDRESS_HOME_DEPENDENT_STREET_NAME);
  s.Set(specifics.address_thoroughfare_and_dependent_thoroughfare_name(),
        ADDRESS_HOME_STREET_AND_DEPENDENT_STREET_NAME);
  s.Set(specifics.address_premise_name(), ADDRESS_HOME_PREMISE_NAME);
  s.Set(specifics.address_subpremise_name(), ADDRESS_HOME_SUBPREMISE);
  s.Set(specifics.address_apt_num(), ADDRESS_HOME_APT_NUM);
  s.Set(specifics.address_floor(), ADDRESS_HOME_FLOOR);

  // Set email, phone and company values and statuses.
  s.Set(specifics.email_address(), EMAIL_ADDRESS);
  s.Set(specifics.company_name(), COMPANY_NAME);
  s.Set(specifics.phone_home_whole_number(), PHONE_HOME_WHOLE_NUMBER);

  // Set birthdate-related values and statuses.
  s.Set(specifics.birthdate_day(), BIRTHDATE_DAY);
  s.Set(specifics.birthdate_month(), BIRTHDATE_MONTH);
  s.Set(specifics.birthdate_year(), BIRTHDATE_4_DIGIT_YEAR);

  profile->FinalizeAfterImport();
  return profile;
}

bool AreContactInfoSpecificsValid(
    const sync_pb::ContactInfoSpecifics& specifics) {
  return base::GUID::ParseLowercase(specifics.guid()).is_valid();
}

}  // namespace autofill
