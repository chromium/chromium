// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_profile_sync_util.h"

#include "base/guid.h"
// TODO(crbug.com/904390): Remove when the investigation is over.
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
// TODO(crbug.com/904390): Remove when the investigation is over.
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill/core/browser/proto/autofill_sync.pb.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/sync/model/entity_data.h"

using base::UTF16ToUTF8;
using base::UTF8ToUTF16;
using autofill::data_util::TruncateUTF8;
using sync_pb::AutofillProfileSpecifics;
using syncer::EntityData;

namespace autofill {
namespace {

bool IsAutofillProfileSpecificsValid(
    const AutofillProfileSpecifics& specifics) {
  return base::IsValidGUID(specifics.guid());
}

}  // namespace

std::unique_ptr<EntityData> CreateEntityDataFromAutofillProfile(
    const AutofillProfile& entry) {
  // Validity of the guid is guaranteed by the database layer.
  DCHECK(base::IsValidGUID(entry.guid()));

  auto entity_data = std::make_unique<EntityData>();
  entity_data->name = entry.guid();
  AutofillProfileSpecifics* specifics =
      entity_data->specifics.mutable_autofill_profile();

  specifics->set_guid(entry.guid());
  specifics->set_origin(entry.origin());

  specifics->set_use_count(entry.use_count());
  specifics->set_use_date(entry.use_date().ToTimeT());
  specifics->set_address_home_language_code(
      TruncateUTF8(entry.language_code()));
  specifics->set_validity_state_bitfield(
      entry.GetClientValidityBitfieldValue());
  specifics->set_is_client_validity_states_updated(
      entry.is_client_validity_states_updated());

  // Set repeated fields.
  specifics->add_name_first(
      TruncateUTF8(UTF16ToUTF8(entry.GetRawInfo(NAME_FIRST))));
  specifics->add_name_middle(
      TruncateUTF8(UTF16ToUTF8(entry.GetRawInfo(NAME_MIDDLE))));
  specifics->add_name_last(
      TruncateUTF8(UTF16ToUTF8(entry.GetRawInfo(NAME_LAST))));
  specifics->add_name_full(
      TruncateUTF8(UTF16ToUTF8(entry.GetRawInfo(NAME_FULL))));
  specifics->add_email_address(
      TruncateUTF8(UTF16ToUTF8(entry.GetRawInfo(EMAIL_ADDRESS))));
  specifics->add_phone_home_whole_number(
      TruncateUTF8(UTF16ToUTF8(entry.GetRawInfo(PHONE_HOME_WHOLE_NUMBER))));

  // Set simple single-valued fields.
  specifics->set_company_name(
      TruncateUTF8(UTF16ToUTF8(entry.GetRawInfo(COMPANY_NAME))));
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
  specifics->set_address_home_street_address(
      TruncateUTF8(UTF16ToUTF8(entry.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS))));
  specifics->set_address_home_line1(
      TruncateUTF8(UTF16ToUTF8(entry.GetRawInfo(ADDRESS_HOME_LINE1))));
  specifics->set_address_home_line2(
      TruncateUTF8(UTF16ToUTF8(entry.GetRawInfo(ADDRESS_HOME_LINE2))));

  return entity_data;
}

std::unique_ptr<AutofillProfile> CreateAutofillProfileFromSpecifics(
    const AutofillProfileSpecifics& specifics) {
  if (!IsAutofillProfileSpecificsValid(specifics)) {
    return nullptr;
  }
  std::unique_ptr<AutofillProfile> profile =
      std::make_unique<AutofillProfile>(specifics.guid(), specifics.origin());

  // Set info that has a default value (and does not distinguish whether it is
  // set or not).
  profile->set_use_count(specifics.use_count());
  profile->set_use_date(base::Time::FromTimeT(specifics.use_date()));
  profile->set_language_code(specifics.address_home_language_code());
  profile->SetClientValidityFromBitfieldValue(
      specifics.validity_state_bitfield());

  // Set repeated fields.
  profile->SetRawInfo(NAME_FIRST, UTF8ToUTF16(specifics.name_first_size()
                                                  ? specifics.name_first(0)
                                                  : std::string()));
  profile->SetRawInfo(NAME_MIDDLE, UTF8ToUTF16(specifics.name_middle_size()
                                                   ? specifics.name_middle(0)
                                                   : std::string()));
  profile->SetRawInfo(
      NAME_LAST, UTF8ToUTF16(specifics.name_last_size() ? specifics.name_last(0)
                                                        : std::string()));
  profile->SetRawInfo(
      EMAIL_ADDRESS,
      UTF8ToUTF16(specifics.email_address_size() ? specifics.email_address(0)
                                                 : std::string()));
  profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                      UTF8ToUTF16(specifics.phone_home_whole_number_size()
                                      ? specifics.phone_home_whole_number(0)
                                      : std::string()));

  // Older versions don't have a separate full name; don't overwrite full name
  // in this case.
  if (specifics.name_full_size() > 0) {
    profile->SetRawInfo(NAME_FULL, UTF8ToUTF16(specifics.name_full(0)));
  }

  // Set simple single-valued fields.
  profile->SetRawInfo(COMPANY_NAME, UTF8ToUTF16(specifics.company_name()));
  profile->SetRawInfo(ADDRESS_HOME_CITY,
                      UTF8ToUTF16(specifics.address_home_city()));
  profile->SetRawInfo(ADDRESS_HOME_STATE,
                      UTF8ToUTF16(specifics.address_home_state()));
  profile->SetRawInfo(ADDRESS_HOME_ZIP,
                      UTF8ToUTF16(specifics.address_home_zip()));
  profile->SetRawInfo(ADDRESS_HOME_SORTING_CODE,
                      UTF8ToUTF16(specifics.address_home_sorting_code()));
  profile->SetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY,
                      UTF8ToUTF16(specifics.address_home_dependent_locality()));

  // Update the country field, which can contain either a country code (if set
  // by a newer version of Chrome), or a country name (if set by an older
  // version of Chrome).
  // TODO(jkrcal): Move this migration logic into Address::SetRawInfo()?
  base::string16 country_name_or_code =
      base::ASCIIToUTF16(specifics.address_home_country());
  std::string country_code =
      CountryNames::GetInstance()->GetCountryCode(country_name_or_code);
  profile->SetRawInfo(ADDRESS_HOME_COUNTRY, UTF8ToUTF16(country_code));

  // Set either the deprecated subparts (line1 & line2) or the full address
  // (street_address) if it is present. This is needed because all the address
  // fields are backed by the same storage.
  if (specifics.has_address_home_street_address()) {
    profile->SetRawInfo(ADDRESS_HOME_STREET_ADDRESS,
                        UTF8ToUTF16(specifics.address_home_street_address()));
  } else {
    profile->SetRawInfo(ADDRESS_HOME_LINE1,
                        UTF8ToUTF16(specifics.address_home_line1()));
    profile->SetRawInfo(ADDRESS_HOME_LINE2,
                        UTF8ToUTF16(specifics.address_home_line2()));
  }

  // This has to be the last one, otherwise setting the raw info may change it.
  profile->set_is_client_validity_states_updated(
      specifics.is_client_validity_states_updated());

  return profile;
}

std::string GetStorageKeyFromAutofillProfile(const AutofillProfile& entry) {
  // Validity of the guid is guaranteed by the database layer.
  DCHECK(base::IsValidGUID(entry.guid()));
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
