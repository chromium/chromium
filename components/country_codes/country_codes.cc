// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/country_codes/country_codes.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)
#include <locale.h>
#endif

#include "base/strings/string_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#undef IN  // On Windows, windef.h defines this, which screws up "India" cases.
#elif BUILDFLAG(IS_APPLE)
#include "base/apple/scoped_cftyperef.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/locale_utils.h"
#endif

namespace country_codes {

namespace {

// TODO(hcarmona/johntlee): remove this function after confirming if it only
// pertains to obsolete OSes.
int CountryCharsToCountryIDWithUpdate(char c1, char c2) {
  // SPECIAL CASE: In 2003, Yugoslavia renamed itself to Serbia and Montenegro.
  // Serbia and Montenegro dissolved their union in June 2006. Yugoslavia was
  // ISO 'YU' and Serbia and Montenegro were ISO 'CS'. Serbia was subsequently
  // issued 'RS' and Montenegro 'ME'. Windows XP and Mac OS X Leopard still use
  // the value 'YU'. If we get a value of 'YU' or 'CS' we will map it to 'RS'.
  if ((c1 == 'Y' && c2 == 'U') || (c1 == 'C' && c2 == 'S')) {
    c1 = 'R';
    c2 = 'S';
  }

  // SPECIAL CASE: Timor-Leste changed from 'TP' to 'TL' in 2002. Windows XP
  // predates this; we therefore map this value.
  if (c1 == 'T' && c2 == 'P')
    c2 = 'L';

  return CountryCharsToCountryID(c1, c2);
}

#if BUILDFLAG(IS_WIN)

// For reference, a list of GeoIDs can be found at
// http://msdn.microsoft.com/en-us/library/dd374073.aspx .
int GeoIDToCountryID(GEOID geo_id) {
  const int kISOBufferSize = 3;  // Two plus one for the terminator.
  wchar_t isobuf[kISOBufferSize] = {};
  int retval = GetGeoInfo(geo_id, GEO_ISO2, isobuf, kISOBufferSize, 0);

  if (retval == kISOBufferSize && !(isobuf[0] == L'X' && isobuf[1] == L'X')) {
    return CountryCharsToCountryIDWithUpdate(static_cast<char>(isobuf[0]),
                                             static_cast<char>(isobuf[1]));
  }

  // Various locations have ISO codes that Windows does not return.
  switch (geo_id) {
    case 0x144:  // Guernsey
      return CountryCharsToCountryID('G', 'G');
    case 0x148:  // Jersey
      return CountryCharsToCountryID('J', 'E');
    case 0x3B16:  // Isle of Man
      return CountryCharsToCountryID('I', 'M');

    // 'UM' (U.S. Minor Outlying Islands)
    case 0x7F:    // Johnston Atoll
    case 0x102:   // Wake Island
    case 0x131:   // Baker Island
    case 0x146:   // Howland Island
    case 0x147:   // Jarvis Island
    case 0x149:   // Kingman Reef
    case 0x152:   // Palmyra Atoll
    case 0x52FA:  // Midway Islands
      return CountryCharsToCountryID('U', 'M');

    // 'SH' (Saint Helena)
    case 0x12F:  // Ascension Island
    case 0x15C:  // Tristan da Cunha
      return CountryCharsToCountryID('S', 'H');

    // 'IO' (British Indian Ocean Territory)
    case 0x13A:  // Diego Garcia
      return CountryCharsToCountryID('I', 'O');

    // Other cases where there is no ISO country code; we assign countries that
    // can serve as reasonable defaults.
    case 0x154:  // Rota Island
    case 0x155:  // Saipan
    case 0x15A:  // Tinian Island
      return CountryCharsToCountryID('U', 'S');
    case 0x134:  // Channel Islands
      return CountryCharsToCountryID('G', 'B');
    case 0x143:  // Guantanamo Bay
    default:
      return kCountryIDUnknown;
  }
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace

const char kCountryIDAtInstall[] = "countryid_at_install";

int CountryStringToCountryID(const std::string& country) {
  return (country.length() == 2)
             ? CountryCharsToCountryIDWithUpdate(country[0], country[1])
             : kCountryIDUnknown;
}

int GetCountryIDFromPrefs(PrefService* prefs) {
  if (!prefs)
    return GetCurrentCountryID();

  // Cache first run Country ID value in prefs, and use it afterwards.  This
  // ensures that just because the user moves around, we won't automatically
  // make major changes to their available search providers, which would feel
  // surprising.
  if (!prefs->HasPrefPath(country_codes::kCountryIDAtInstall)) {
    prefs->SetInteger(country_codes::kCountryIDAtInstall,
                      GetCurrentCountryID());
  }
  return prefs->GetInteger(country_codes::kCountryIDAtInstall);
}

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(country_codes::kCountryIDAtInstall,
                                kCountryIDUnknown);
}

#if BUILDFLAG(IS_WIN)

int GetCurrentCountryID() {
  // Calls to GetCurrentCountryID occur fairly frequently and incur a heavy
  // registry hit within the GetUserGeoID api call. Registry hits can be
  // impactful to perf, particularly on virtualized systems.  To mitigate this
  // we store the result of the first call in a static. The Id is only
  // updated by calls to SetUserGeoID or the user manually updating the
  // language and region settings.  It is expected that if it changes the user
  // would need to restart applications to ensure the updated value is
  // respected.
  static int id = GeoIDToCountryID(GetUserGeoID(GEOCLASS_NATION));
  return id;
}

#elif BUILDFLAG(IS_APPLE)

int GetCurrentCountryID() {
  base::apple::ScopedCFTypeRef<CFLocaleRef> locale(CFLocaleCopyCurrent());
  CFStringRef country =
      (CFStringRef)CFLocaleGetValue(locale.get(), kCFLocaleCountryCode);
  if (!country)
    return kCountryIDUnknown;

  UniChar isobuf[2];
  CFRange char_range = CFRangeMake(0, 2);
  CFStringGetCharacters(country, char_range, isobuf);

  return CountryCharsToCountryIDWithUpdate(static_cast<char>(isobuf[0]),
                                           static_cast<char>(isobuf[1]));
}

#elif BUILDFLAG(IS_ANDROID)

int GetCurrentCountryID() {
  return CountryStringToCountryID(base::android::GetDefaultCountryCode());
}

#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

int GetCurrentCountryID() {
  const char* locale = setlocale(LC_MESSAGES, nullptr);
  if (!locale)
    return kCountryIDUnknown;

  // The format of a locale name is:
  // language[_territory][.codeset][@modifier], where territory is an ISO 3166
  // country code, which is what we want.

  // First remove the language portion.
  std::string locale_str(locale);
  size_t territory_delim = locale_str.find('_');
  if (territory_delim == std::string::npos)
    return kCountryIDUnknown;
  locale_str.erase(0, territory_delim + 1);

  // Next remove any codeset/modifier portion and uppercase.
  return CountryStringToCountryID(
      base::ToUpperASCII(locale_str.substr(0, locale_str.find_first_of(".@"))));
}

#endif  // OS_*

std::string CountryIDToCountryString(int country_id) {
  // We only use the lowest 16 bits to build two ASCII characters. If there is
  // more than that, the ID is invalid. The check for positive integers also
  // handles the |kCountryIDUnknown| case.
  if ((country_id & 0xFFFF) != country_id || country_id < 0)
    return kCountryCodeUnknown;

  // Decode the country code string from the provided integer. The first two
  // bytes of the country ID represent two ASCII chars.
  std::string country_code = {static_cast<char>(country_id >> 8),
                              static_cast<char>(country_id)};
  country_code = base::ToUpperASCII(country_code);

  // Validate the code that was produced by feeding it back into the system.
  return (CountryStringToCountryID(country_code) == country_id)
             ? country_code
             : kCountryCodeUnknown;
}

std::string GetCurrentCountryCode() {
  return CountryIDToCountryString(GetCurrentCountryID());
}

}  // namespace country_codes
