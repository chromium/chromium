// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_structured_address_format_provider.h"

#include <string>

#include "base/containers/fixed_flat_map.h"
#include "base/no_destructor.h"

namespace autofill {

namespace {

std::u16string GetHomeStreetAddressPattern(std::string_view country_code) {
  static constexpr auto kHomeStreetAddressCountryMap = base::MakeFixedFlatMap<
      std::string_view, std::u16string_view>(
      {{"BR",
        u"${ADDRESS_HOME_STREET_NAME}${ADDRESS_HOME_HOUSE_NUMBER;, }"
        u"${ADDRESS_HOME_FLOOR;, ;º andar}${ADDRESS_HOME_APT_NUM;, apto ;}"},
       {"DE",
        u"${ADDRESS_HOME_STREET_NAME} ${ADDRESS_HOME_HOUSE_NUMBER}"
        u"${ADDRESS_HOME_FLOOR;, ;. Stock}${ADDRESS_HOME_APT_NUM;, ;. "
        u"Wohnung}"},
       {"MX",
        u"${ADDRESS_HOME_STREET_NAME} ${ADDRESS_HOME_HOUSE_NUMBER}"
        u"${ADDRESS_HOME_FLOOR; - Piso ;}${ADDRESS_HOME_APT_NUM; - ;}"},
       {"ES",
        u"${ADDRESS_HOME_STREET_NAME} ${ADDRESS_HOME_HOUSE_NUMBER}"
        u"${ADDRESS_HOME_FLOOR;, ;º}${ADDRESS_HOME_APT_NUM;, ;ª}"}});

  if (auto it = kHomeStreetAddressCountryMap.find(country_code);
      it != kHomeStreetAddressCountryMap.end()) {
    return std::u16string(it->second);
  }

  // Use the format for US/UK as the default.
  return u"${ADDRESS_HOME_HOUSE_NUMBER} ${ADDRESS_HOME_STREET_NAME} "
         u"${ADDRESS_HOME_FLOOR;FL } ${ADDRESS_HOME_APT_NUM;APT }";
}

std::u16string GetFullNamePattern(bool name_has_cjk_characteristics) {
  if (name_has_cjk_characteristics) {
    return u"${NAME_LAST}${NAME_FIRST}";
  }
  return u"${NAME_FIRST} ${NAME_MIDDLE} ${NAME_LAST}";
}

}  // namespace

StructuredAddressesFormatProvider::StructuredAddressesFormatProvider() =
    default;

// static
StructuredAddressesFormatProvider*
StructuredAddressesFormatProvider::GetInstance() {
  static base::NoDestructor<StructuredAddressesFormatProvider>
      g_expression_provider;
  return g_expression_provider.get();
}

std::u16string StructuredAddressesFormatProvider::GetPattern(
    FieldType type,
    std::string_view country_code,
    const ContextInfo& info) const {
  switch (type) {
    case ADDRESS_HOME_STREET_ADDRESS:
      return GetHomeStreetAddressPattern(country_code);
    case NAME_FULL:
      return GetFullNamePattern(info.name_has_cjk_characteristics);
    default:
      return u"";
  }
}

}  // namespace autofill
