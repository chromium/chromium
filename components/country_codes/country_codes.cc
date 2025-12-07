// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/country_codes/country_codes.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)
#include <locale.h>
#endif

#if BUILDFLAG(IS_APPLE)
#include <CoreFoundation/CoreFoundation.h>
#endif

#include "base/strings/string_util.h"

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

#if BUILDFLAG(IS_WIN)

// For reference, a list of GeoIDs can be found at
// http://msdn.microsoft.com/en-us/library/dd374073.aspx .
CountryId GeoIDToCountryID(GEOID geo_id) {
  const int kISOBufferSize = 3;  // Two plus one for the terminator.
  wchar_t isobuf[kISOBufferSize] = {};
  int retval = GetGeoInfo(geo_id, GEO_ISO2, isobuf, kISOBufferSize, 0);

  if (retval == kISOBufferSize && !(isobuf[0] == L'X' && isobuf[1] == L'X')) {
    char code[2]{static_cast<char>(isobuf[0]), static_cast<char>(isobuf[1])};
    return CountryId(std::string_view(code, std::size(code)));
  }

  // Various locations have ISO codes that Windows does not return.
  switch (geo_id) {
    case 0x144:  // Guernsey
      return CountryId("GG");
    case 0x148:  // Jersey
      return CountryId("JE");
    case 0x3B16:  // Isle of Man
      return CountryId("IM");

    // 'UM' (U.S. Minor Outlying Islands)
    case 0x7F:    // Johnston Atoll
    case 0x102:   // Wake Island
    case 0x131:   // Baker Island
    case 0x146:   // Howland Island
    case 0x147:   // Jarvis Island
    case 0x149:   // Kingman Reef
    case 0x152:   // Palmyra Atoll
    case 0x52FA:  // Midway Islands
      return CountryId("UM");

    // 'SH' (Saint Helena)
    case 0x12F:  // Ascension Island
    case 0x15C:  // Tristan da Cunha
      return CountryId("SH");

    // 'IO' (British Indian Ocean Territory)
    case 0x13A:  // Diego Garcia
      return CountryId("IO");

    // Other cases where there is no ISO country code; we assign countries that
    // can serve as reasonable defaults.
    case 0x154:  // Rota Island
    case 0x155:  // Saipan
    case 0x15A:  // Tinian Island
      return CountryId("US");
    case 0x134:  // Channel Islands
      return CountryId("GB");
    case 0x143:  // Guantanamo Bay
    default:
      return CountryId();
  }
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace

#if BUILDFLAG(IS_WIN)

CountryId GetCurrentCountryID() {
  // Calls to GetCurrentCountryID occur fairly frequently and incur a heavy
  // registry hit within the Windows GetUserGeoID API call. Registry hits can be
  // impactful to perf, particularly on virtualized systems.  To mitigate this
  // we store the result of the first call in a static. The Id is only
  // updated by calls to SetUserGeoID or the user manually updating the
  // language and region settings.  It is expected that if it changes the user
  // would need to restart applications to ensure the updated value is
  // respected.
  static CountryId id = GeoIDToCountryID(GetUserGeoID(GEOCLASS_NATION));
  return id;
}

#elif BUILDFLAG(IS_APPLE)

CountryId GetCurrentCountryID() {
  base::apple::ScopedCFTypeRef<CFLocaleRef> locale(CFLocaleCopyCurrent());
  CFStringRef country =
      (CFStringRef)CFLocaleGetValue(locale.get(), kCFLocaleCountryCode);
  if (!country) {
    return CountryId();
  }

  UniChar isobuf[2];
  CFRange char_range = CFRangeMake(0, 2);
  CFStringGetCharacters(country, char_range, isobuf);

  char code[2]{static_cast<char>(isobuf[0]), static_cast<char>(isobuf[1])};

  return CountryId(std::string_view(code, std::size(code)));
}

#elif BUILDFLAG(IS_ANDROID)

CountryId GetCurrentCountryID() {
  return CountryId(base::android::GetDefaultCountryCode());
}

#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

CountryId GetCurrentCountryID() {
  const char* locale = setlocale(LC_MESSAGES, nullptr);
  if (!locale) {
    return CountryId();
  }

  // The format of a locale name is:
  // language[_territory][.codeset][@modifier], where territory is an ISO 3166
  // country code, which is what we want.

  // First remove the language portion.
  std::string locale_str(locale);
  size_t territory_delim = locale_str.find('_');
  if (territory_delim == std::string::npos) {
    return CountryId();
  }
  locale_str.erase(0, territory_delim + 1);

  // Next remove any codeset/modifier portion and uppercase.
  return CountryId(
      base::ToUpperASCII(locale_str.substr(0, locale_str.find_first_of(".@"))));
}

#endif  // OS_*
}  // namespace country_codes
