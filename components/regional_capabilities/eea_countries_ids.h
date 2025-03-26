// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REGIONAL_CAPABILITIES_EEA_COUNTRIES_IDS_H_
#define COMPONENTS_REGIONAL_CAPABILITIES_EEA_COUNTRIES_IDS_H_

#include "base/containers/fixed_flat_set.h"
#include "components/country_codes/country_codes.h"

namespace regional_capabilities {

using CountryId = country_codes::CountryId;

// Google-internal reference: http://go/geoscope-comparisons.
inline constexpr auto kEeaChoiceCountriesIds =
    base::MakeFixedFlatSet<CountryId>({
        CountryId("AT"),  // Austria
        CountryId("AX"),  // Åland Islands
        CountryId("BE"),  // Belgium
        CountryId("BG"),  // Bulgaria
        CountryId("BL"),  // St. Barthélemy
        CountryId("CY"),  // Cyprus
        CountryId("CZ"),  // Czech Republic
        CountryId("DE"),  // Germany
        CountryId("DK"),  // Denmark
        CountryId("EA"),  // Ceuta & Melilla
        CountryId("EE"),  // Estonia
        CountryId("ES"),  // Spain
        CountryId("FI"),  // Finland
        CountryId("FR"),  // France
        CountryId("GF"),  // French Guiana
        CountryId("GP"),  // Guadeloupe
        CountryId("GR"),  // Greece
        CountryId("HR"),  // Croatia
        CountryId("HU"),  // Hungary
        CountryId("IC"),  // Canary Islands
        CountryId("IE"),  // Ireland
        CountryId("IS"),  // Iceland
        CountryId("IT"),  // Italy
        CountryId("LI"),  // Liechtenstein
        CountryId("LT"),  // Lithuania
        CountryId("LU"),  // Luxembourg
        CountryId("LV"),  // Latvia
        CountryId("MF"),  // St. Martin
        CountryId("MQ"),  // Martinique
        CountryId("MT"),  // Malta
        CountryId("NC"),  // New Caledonia
        CountryId("NL"),  // Netherlands
        CountryId("NO"),  // Norway
        CountryId("PF"),  // French Polynesia
        CountryId("PL"),  // Poland
        CountryId("PM"),  // St. Pierre & Miquelon
        CountryId("PT"),  // Portugal
        CountryId("RE"),  // Réunion
        CountryId("RO"),  // Romania
        CountryId("SE"),  // Sweden
        CountryId("SI"),  // Slovenia
        CountryId("SJ"),  // Svalbard & Jan Mayen
        CountryId("SK"),  // Slovakia
        CountryId("TF"),  // French Southern Territories
        CountryId("VA"),  // Vatican City
        CountryId("WF"),  // Wallis & Futuna
        CountryId("YT"),  // Mayotte
    });

}  // namespace regional_capabilities

#endif  // COMPONENTS_REGIONAL_CAPABILITIES_EEA_COUNTRIES_IDS_H_
