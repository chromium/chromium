// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_EEA_COUNTRIES_IDS_H_
#define COMPONENTS_SEARCH_ENGINES_EEA_COUNTRIES_IDS_H_

#include "base/containers/fixed_flat_set.h"
#include "components/country_codes/country_codes.h"

namespace search_engines {

// Google-internal reference: http://go/geoscope-comparisons.
inline constexpr auto kEeaChoiceCountriesIds = base::MakeFixedFlatSet<int>({
    country_codes::CountryCharsToCountryID('A', 'T'),  // Austria
    country_codes::CountryCharsToCountryID('A', 'X'),  // Åland Islands
    country_codes::CountryCharsToCountryID('B', 'E'),  // Belgium
    country_codes::CountryCharsToCountryID('B', 'G'),  // Bulgaria
    country_codes::CountryCharsToCountryID('B', 'L'),  // St. Barthélemy
    country_codes::CountryCharsToCountryID('C', 'Y'),  // Cyprus
    country_codes::CountryCharsToCountryID('C', 'Z'),  // Czech Republic
    country_codes::CountryCharsToCountryID('D', 'E'),  // Germany
    country_codes::CountryCharsToCountryID('D', 'K'),  // Denmark
    country_codes::CountryCharsToCountryID('E', 'A'),  // Ceuta & Melilla
    country_codes::CountryCharsToCountryID('E', 'E'),  // Estonia
    country_codes::CountryCharsToCountryID('E', 'S'),  // Spain
    country_codes::CountryCharsToCountryID('F', 'I'),  // Finland
    country_codes::CountryCharsToCountryID('F', 'R'),  // France
    country_codes::CountryCharsToCountryID('G', 'F'),  // French Guiana
    country_codes::CountryCharsToCountryID('G', 'P'),  // Guadeloupe
    country_codes::CountryCharsToCountryID('G', 'R'),  // Greece
    country_codes::CountryCharsToCountryID('H', 'R'),  // Croatia
    country_codes::CountryCharsToCountryID('H', 'U'),  // Hungary
    country_codes::CountryCharsToCountryID('I', 'C'),  // Canary Islands
    country_codes::CountryCharsToCountryID('I', 'E'),  // Ireland
    country_codes::CountryCharsToCountryID('I', 'S'),  // Iceland
    country_codes::CountryCharsToCountryID('I', 'T'),  // Italy
    country_codes::CountryCharsToCountryID('L', 'I'),  // Liechtenstein
    country_codes::CountryCharsToCountryID('L', 'T'),  // Lithuania
    country_codes::CountryCharsToCountryID('L', 'U'),  // Luxembourg
    country_codes::CountryCharsToCountryID('L', 'V'),  // Latvia
    country_codes::CountryCharsToCountryID('M', 'F'),  // St. Martin
    country_codes::CountryCharsToCountryID('M', 'Q'),  // Martinique
    country_codes::CountryCharsToCountryID('M', 'T'),  // Malta
    country_codes::CountryCharsToCountryID('N', 'C'),  // New Caledonia
    country_codes::CountryCharsToCountryID('N', 'L'),  // Netherlands
    country_codes::CountryCharsToCountryID('N', 'O'),  // Norway
    country_codes::CountryCharsToCountryID('P', 'F'),  // French Polynesia
    country_codes::CountryCharsToCountryID('P', 'L'),  // Poland
    country_codes::CountryCharsToCountryID('P',
                                           'M'),       // St. Pierre & Miquelon
    country_codes::CountryCharsToCountryID('P', 'T'),  // Portugal
    country_codes::CountryCharsToCountryID('R', 'E'),  // Réunion
    country_codes::CountryCharsToCountryID('R', 'O'),  // Romania
    country_codes::CountryCharsToCountryID('S', 'E'),  // Sweden
    country_codes::CountryCharsToCountryID('S', 'I'),  // Slovenia
    country_codes::CountryCharsToCountryID('S',
                                           'J'),       // Svalbard & Jan Mayen
    country_codes::CountryCharsToCountryID('S', 'K'),  // Slovakia
    country_codes::CountryCharsToCountryID('T',
                                           'F'),  // French Southern Territories
    country_codes::CountryCharsToCountryID('V', 'A'),  // Vatican City
    country_codes::CountryCharsToCountryID('W', 'F'),  // Wallis & Futuna
    country_codes::CountryCharsToCountryID('Y', 'T'),  // Mayotte
});

}  // namespace search_engines

#endif  // COMPONENTS_SEARCH_ENGINES_EEA_COUNTRIES_IDS_H_
