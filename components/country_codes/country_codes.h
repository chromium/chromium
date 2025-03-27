// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Please refer to ISO 3166-1 for information about the two-character country
// codes; http://en.wikipedia.org/wiki/ISO_3166-1_alpha-2 is useful. In the
// following (C++) code, we pack the two letters of the country code into an int
// value we call the CountryId.

#ifndef COMPONENTS_COUNTRY_CODES_COUNTRY_CODES_H_
#define COMPONENTS_COUNTRY_CODES_COUNTRY_CODES_H_

#include <array>
#include <string>

#include "base/component_export.h"

class PrefRegistrySimple;
class PrefService;

namespace country_codes {

// Class representing a Country Identifier based on ISO 3166-1 two-letter
// country codes.
class CountryId {
 public:
  // Constructs the default instance of CountryId pointing to an unspecified
  // country.
  constexpr CountryId() : CountryId(kUnknownCountryCode) {}

  // Constructs a CountryId from a two-character country code (ISO 3166-1).
  // If the provided code does not have exactly two uppercase letter characters,
  // the constructed CountryId will report invalid.
  explicit constexpr CountryId(std::string_view code) {
    if (code.size() != 2  // Country code is exactly 2 letters long
        || code[0] < 'A' || code[0] > 'Z'  // Country code requires exactly
        || code[1] < 'A' || code[1] > 'Z'  // two uppercase letters.
    ) {
      code = kUnknownCountryCode;
    }
    country_code_[0] = code[0];
    country_code_[1] = code[1];
  }

  // Deserializes a CountryId from an integer code.
  // This call complements the `Serialize()` method, and can be used to
  // de-serialize CountryId from persisted information.
  // If the supplied integer fails validation, the constructed CountryId will
  // report invalid.
  static constexpr CountryId Deserialize(int code) {
    // We only use the lowest 16 bits to build two ASCII characters. If there is
    // more than that, the ID is invalid.
    if (code <= 0 || code > 0xffff) {
      return CountryId();
    }

    char country_code[2]{static_cast<char>(code >> 8),
                         static_cast<char>(code & 0xff)};
    return CountryId(std::string_view(country_code, std::size(country_code)));
  }

  // Returns the integer representation of the CountryId. This is exposed for
  // serialization and testing but should not be used for any other purpose.
  constexpr int Serialize() const {
    return country_code_[0] << 8 | country_code_[1];
  }

  constexpr auto operator<=>(const CountryId& other) const = default;

  // Determines whether this instance of CountryId represents a valid country.
  constexpr bool IsValid() const {
    return CountryCode() != kUnknownCountryCode;
  }

  // Returns associated country code.
  // This call alone may not sufficiently help you determine the country, and
  // will return a country code associated with unknown or invalid territory,
  // when `IsValid()` returns `false`.
  constexpr std::string_view CountryCode() const {
    return std::string_view(country_code_.data(), country_code_.size());
  }

 private:
  // ISRC uses ZZ as a pseudo country code, if
  // - the origin is unknown, or
  // - the territory was not issued a country code through the ISRC agency, or
  // otherwise 'ZZ' is used for testing and to denote an unknown, or invalid
  // territory, and does not reference a real country.
  // See https://en.wikipedia.org/wiki/ISO_3166-1_alpha-2#ZZ
  static constexpr std::string_view kUnknownCountryCode = "ZZ";

  // Note: this must be a std::array, and not a char[] array to enforce proper
  // creation of `std::string_view`. This is because arrays implicitly decay to
  // pointers, which may confuse std::string_view and result in security issues.
  std::array<char, 2> country_code_{};
};

// Preference key containing the system Country ID the first time we checked
// the TemplateURLPrepopulateData. This is used to avoid adding a whole bunch
// of new search engine choices if prepopulation runs when the user's Country
// ID differs from their previous Country ID.  This pref does not exist until
// prepopulation has been run at least once.
inline constexpr char kCountryIDAtInstall[] = "countryid_at_install";

COMPONENT_EXPORT(COMPONENTS_COUNTRY_CODES)
void RegisterProfilePrefs(PrefRegistrySimple* registry);

// Returns the identifier for the user current country.
COMPONENT_EXPORT(COMPONENTS_COUNTRY_CODES)
CountryId GetCurrentCountryID();

// Returns the country identifier that was stored at install. If no such pref
// is available, it will return identifier of the current country instead.
COMPONENT_EXPORT(COMPONENTS_COUNTRY_CODES)
CountryId GetCountryIDFromPrefs(PrefService* prefs);

}  // namespace country_codes

#endif  // COMPONENTS_COUNTRY_CODES_COUNTRY_CODES_H_
