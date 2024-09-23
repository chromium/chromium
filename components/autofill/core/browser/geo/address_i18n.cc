// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/geo/address_i18n.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_metadata.h"

namespace autofill {
namespace i18n {

using ::i18n::addressinput::AddressData;
using ::i18n::addressinput::AddressField;

std::unique_ptr<::i18n::addressinput::AddressData>
CreateAddressDataFromAutofillProfile(const AutofillProfile& profile,
                                     const std::string& app_locale) {
  auto get_info = [&profile, &app_locale](const AutofillType& type) {
    return base::UTF16ToUTF8(profile.GetInfo(type, app_locale));
  };

  auto address_data = std::make_unique<AddressData>();
  address_data->recipient = get_info(AutofillType(NAME_FULL));
  address_data->organization = get_info(AutofillType(COMPANY_NAME));
  address_data->region_code =
      get_info(AutofillType(HtmlFieldType::kCountryCode));
  address_data->administrative_area =
      get_info(AutofillType(ADDRESS_HOME_STATE));
  address_data->locality = get_info(AutofillType(ADDRESS_HOME_CITY));
  address_data->dependent_locality =
      get_info(AutofillType(ADDRESS_HOME_DEPENDENT_LOCALITY));
  address_data->sorting_code =
      get_info(AutofillType(ADDRESS_HOME_SORTING_CODE));
  address_data->postal_code = get_info(AutofillType(ADDRESS_HOME_ZIP));
  address_data->address_line =
      base::SplitString(get_info(AutofillType(ADDRESS_HOME_STREET_ADDRESS)),
                        "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  address_data->language_code = profile.language_code();
  return address_data;
}

FieldType TypeForField(AddressField address_field) {
  switch (address_field) {
    case ::i18n::addressinput::COUNTRY:
      return ADDRESS_HOME_COUNTRY;
    case ::i18n::addressinput::ADMIN_AREA:
      return ADDRESS_HOME_STATE;
    case ::i18n::addressinput::LOCALITY:
      return ADDRESS_HOME_CITY;
    case ::i18n::addressinput::DEPENDENT_LOCALITY:
      return ADDRESS_HOME_DEPENDENT_LOCALITY;
    case ::i18n::addressinput::POSTAL_CODE:
      return ADDRESS_HOME_ZIP;
    case ::i18n::addressinput::SORTING_CODE:
      return ADDRESS_HOME_SORTING_CODE;
    case ::i18n::addressinput::STREET_ADDRESS:
      return ADDRESS_HOME_STREET_ADDRESS;
    case ::i18n::addressinput::ORGANIZATION:
      return COMPANY_NAME;
    case ::i18n::addressinput::RECIPIENT:
      return NAME_FULL;
  }
  NOTREACHED_IN_MIGRATION();
  return UNKNOWN_TYPE;
}

bool FieldForType(FieldType server_type, AddressField* field) {
  switch (server_type) {
    case ADDRESS_HOME_COUNTRY:
      if (field)
        *field = ::i18n::addressinput::COUNTRY;
      return true;
    case ADDRESS_HOME_STATE:
      if (field)
        *field = ::i18n::addressinput::ADMIN_AREA;
      return true;
    case ADDRESS_HOME_CITY:
      if (field)
        *field = ::i18n::addressinput::LOCALITY;
      return true;
    case ADDRESS_HOME_DEPENDENT_LOCALITY:
      if (field)
        *field = ::i18n::addressinput::DEPENDENT_LOCALITY;
      return true;
    case ADDRESS_HOME_SORTING_CODE:
      if (field)
        *field = ::i18n::addressinput::SORTING_CODE;
      return true;
    case ADDRESS_HOME_ZIP:
      if (field)
        *field = ::i18n::addressinput::POSTAL_CODE;
      return true;
    case ADDRESS_HOME_STREET_ADDRESS:
    case ADDRESS_HOME_LINE1:
    case ADDRESS_HOME_LINE2:
      if (field)
        *field = ::i18n::addressinput::STREET_ADDRESS;
      return true;
    case COMPANY_NAME:
      if (field)
        *field = ::i18n::addressinput::ORGANIZATION;
      return true;
    case NAME_FULL:
      if (field)
        *field = ::i18n::addressinput::RECIPIENT;
      return true;
    default:
      return false;
  }
}

bool IsFieldRequired(FieldType server_type, const std::string& country_code) {
  ::i18n::addressinput::AddressField field_enum;
  if (FieldForType(server_type, &field_enum)) {
    return ::i18n::addressinput::IsFieldRequired(field_enum, country_code);
  }
  return false;
}

}  // namespace i18n
}  // namespace autofill
