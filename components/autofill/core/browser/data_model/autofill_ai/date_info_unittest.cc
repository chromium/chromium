// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_ai/date_info.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/map_util.h"
#include "build/blink_buildflags.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/data_model/data_model_util.h"
#include "components/personal_context/proto/features/common_data.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

// Tests setting the date incrementally.
TEST(DateInfo, SetDateIncrementally) {
  DateInfo info;
  EXPECT_EQ(info.GetDate(u"YYYY-MM-DD"), u"");

  info.SetDate(u"12/2022", u"MM/YYYY");
  EXPECT_EQ(info.GetDate(u"YYYY-MM"), u"2022-12");
  EXPECT_EQ(info.GetDate(u"YYYY-MM-DD"), u"");  // Because `info` has no day.

  info.SetDate(u"16", u"DD");
  EXPECT_EQ(info.GetDate(u""), u"");
  EXPECT_EQ(info.GetDate(u"YYYY-MM-DD"), u"2022-12-16");
}

// Tests setting the date incrementally.
TEST(DateInfo, SetDateOrReset) {
  DateInfo info;
  EXPECT_EQ(info.GetDate(u"YYYY-MM-DD"), u"");

  info.SetDate(u"16/12/2022", u"DD/MM/YYYY");
  EXPECT_EQ(info.GetDate(u"YYYY-MM-DD"), u"2022-12-16");

  info.SetDate(u"foobar", u"DD/MM/YYYY");
  EXPECT_EQ(info.GetDate(u""), u"");
}

// Tests that GetDate() returns an empty string for invalid date formats.
TEST(DateInfo, InvalidDateFormatReturnsEmpty) {
  DateInfo info;
  info.SetDate(u"16/12/2022", u"DD/MM/YYYY");
  EXPECT_EQ(info.GetDate(u"YYYY-MM-DD"), u"2022-12-16");

  EXPECT_EQ(info.GetDate(u""), u"");
  EXPECT_EQ(info.GetDate(u"-4"), u"");
  EXPECT_EQ(info.GetDate(u"N"), u"");
  EXPECT_EQ(info.GetDate(u"invalid"), u"");
  EXPECT_EQ(info.GetDate(u"YYYY/MM-DD"), u"");
}

// Tests that GetIcuDate() returns an empty string if the date is not fully
// set.
TEST(DateInfo, GetIcuDate_IncrementalSet) {
  DateInfo info;
  EXPECT_EQ(info.GetDate(u"YYYY-MM-DD"), u"");

  info.SetDate(u"12/2022", u"MM/YYYY");
  EXPECT_EQ(info.GetIcuDate(u"YYYY-MM-DD", "en_US"), u"");

  info.SetDate(u"16", u"DD");
  EXPECT_EQ(info.GetIcuDate(u"YYYY-MM-dd", "en_US"), u"2022-12-16");
}

// Tests that GetIcuDate() returns the localized date.
TEST(DateInfo, GetIcuDate_LocalizedOutput) {
  DateInfo info;
  info.SetDate(u"16/12/2022", u"DD/MM/YYYY");
  EXPECT_EQ(info.GetDate(u"YYYY-MM-DD"), u"2022-12-16");

  EXPECT_EQ(info.GetIcuDate(u"MMM dd", "en_US"), u"Dec 16");
  EXPECT_EQ(info.GetIcuDate(u"MMM dd", "pl_PL"), u"gru 16");
  EXPECT_EQ(info.GetIcuDate(u"MMM dd", "de_DE"), u"Dez. 16");
}

// Tests that GetDateProto() populates a proto with  year, month, and day.
TEST(DateInfo, GetDateProto) {
  DateInfo info;
  info.SetDate(u"16/12/2022", u"DD/MM/YYYY");
  personal_context::proto::Date proto = info.GetDateProto();
  EXPECT_EQ(proto.year(), 2022);
  EXPECT_EQ(proto.month(), 12);
  EXPECT_EQ(proto.day(), 16);
}

// Tests that GetIcuDate() returns the localized date for a localized pattern
// across all Chrome UI platform locales (ui_l10n::GetPlatformLanguageTags()).
TEST(DateInfo, GetIcuDate_LocalizedPattern_AllPlatformLocales) {
  constexpr auto kPlatformLocales = std::to_array({
#define PLATFORM_LOCALE(locale) #locale,
#include "ui/base/l10n/l10n_util_locales_list.inc"
#undef PLATFORM_LOCALE
  });

  constexpr std::u16string_view kDateInIso = u"2028-12-07";

  // The expected localized versions of `kDateInIso`.
  // This is a superset of all_chrome_locales from build/config/locales.gni.
  // There are two limitations:
  // (1) Each platform supports only a subset of these locales.
  //     Therefore, further down below, the test only compares the locales from
  //     `kPlatformLocales`.
  // (2) On some platforms, the expectations differ, mostly because Chrome
  //     strips the ICU calendar data for some locales. For these cases, the
  //     test modifies the expectations down below.
  auto expected_dates =
      base::MakeFixedFlatMap<std::string_view, std::u16string_view>({
          {"af", u"7 Des."},        // Afrikaans
          {"am", u"ዲሴም 7"},         // Amharic
          {"ar", u"7 ديسمبر"},      // Arabic
          {"ar-XB", u"7 ديسمبر"},   // RTL Pseudolocale
          {"as", u"৭ ডিচে"},        // Assamese
          {"az", u"7 dek"},         // Azerbaijani
          {"be", u"7 сне"},         // Belarusian
          {"bg", u"7.12"},          // Bulgarian
          {"bn", u"৭ ডিসে"},        // Bengali
          {"bs", u"7. dec"},        // Bosnian
          {"ca", u"7 de des."},     // Catalan
          {"cs", u"7. 12."},        // Czech
          {"cy", u"7 Rhag"},        // Welsh
          {"da", u"7. dec."},       // Danish
          {"de", u"7. Dez."},       // German
          {"el", u"7 Δεκ"},         // Greek
          {"en", u"Dec 7"},         // English
          {"en-GB", u"7 Dec"},      // English (UK)
          {"en-US", u"Dec 7"},      // English (US)
          {"en-XA", u"Dec 7"},      // Long strings Pseudolocale
          {"es", u"7 dic"},         // Spanish
          {"es-419", u"7 dic"},     // Spanish (Latin America)
          {"es-MX", u"7 dic"},      // Spanish (Mexico)
          {"et", u"7. dets"},       // Estonian
          {"eu", u"abe. 7(a)"},     // Basque
          {"fa", u"۱۷ آذر"},        // Persian
          {"fi", u"7.12."},         // Finnish
          {"fil", u"Dis 7"},        // Filipino
          {"fr", u"7 déc."},        // French
          {"fr-CA", u"7 déc."},     // French (Canada)
          {"gl", u"7 de dec."},     // Galician
          {"gu", u"7 ડિસે"},         // Gujarati
          {"he", u"7 בדצמ׳"},       // Hebrew
          {"hi", u"7 दिस॰"},        // Hindi
          {"hr", u"7. pro"},        // Croatian
          {"hu", u"dec. 7."},       // Hungarian
          {"hy", u"7 դեկ"},         // Armenian
          {"id", u"7 Des"},         // Indonesian
          {"is", u"7. des."},       // Icelandic
          {"it", u"7 dic"},         // Italian
          {"ja", u"12月7日"},       // Japanese
          {"ka", u"7 დეკ"},         // Georgian
          {"kk", u"7 жел."},        // Kazakh
          {"km", u"7 ធ្នូ"},          // Khmer
          {"kn", u"7 ಡಿಸೆಂ"},         // Kannada
          {"ko", u"12월 7일"},      // Korean
          {"ky", u"7-дек."},        // Kyrgyz
          {"lo", u"7 ທ.ວ."},        // Lao
          {"lt", u"12-07"},         // Lithuanian
          {"lv", u"7. dec."},       // Latvian
          {"mk", u"7 дек."},        // Macedonian
          {"ml", u"ഡിസം 7"},        // Malayalam
          {"mn", u"12-р сарын 7"},  // Mongolian
          {"mr", u"७ डिसें"},         // Marathi
          {"ms", u"7 Dis"},         // Malay
          {"my", u"ဒီ ၇"},           // Burmese
          {"nb", u"7. des."},       // Norwegian (Bokmal)
          {"ne", u"डिसेम्बर ७"},      // Nepali
          {"nl", u"7 dec"},         // Dutch
          {"or", u"ଡିସେମ୍ବର 7"},      // Odia
          {"pa", u"7 ਦਸੰ"},          // Punjabi
          {"pl", u"7 gru"},         // Polish
          {"pt", u"7 de dez."},     // Portuguese
          {"pt-BR", u"7 de dez."},  // Portuguese (Brazil)
          {"pt-PT", u"7/12"},       // Portuguese (Portugal)
          {"ro", u"7 dec."},        // Romanian
          {"ru", u"7 дек."},        // Russian
          {"si", u"උඳුවප් 7"},        // Sinhala
          {"sk", u"7. 12."},        // Slovak
          {"sl", u"7. dec."},       // Slovenian
          {"sq", u"7 dhj"},         // Albanian
          {"sr", u"7. дец"},        // Serbian
          {"sr-Latn", u"7. dec"},   // Serbian (Latin)
          {"sv", u"7 dec."},        // Swedish
          {"sw", u"7 Des"},         // Swahili
          {"ta", u"டிச. 7"},        // Tamil
          {"te", u"7 డిసెం"},         // Telugu
          {"th", u"7 ธ.ค."},        // Thai
          {"tr", u"7 Ara"},         // Turkish
          {"uk", u"7 груд."},       // Ukrainian
          {"ur", u"7 دسمبر"},       // Urdu
          {"uz", u"7-dek"},         // Uzbek
          {"vi", u"7 thg 12"},      // Vietnamese
          {"zh-CN", u"12月7日"},    // Chinese (China)
          {"zh-HK", u"12月7日"},    // Chinese (Hong Kong)
          {"zh-TW", u"12月7日"},    // Chinese (Taiwan)
          {"zu", u"Dis 7"},         // Zulu
      });

  // On some platforms, Chrome ships `.pak` UI strings for certain locales
  // (so they appear in `l10n_util_locales_list.inc`), but strips or alters
  // their ICU data in `third_party/icu/filters/{android,chromeos,ios}.json`
  // to save binary size.
  constexpr std::u16string_view kRootLocaleDate [[maybe_unused]] = u"M12 7";
#if BUILDFLAG(IS_ANDROID)
  expected_dates.at("as") = u"Dec 7";         // Assamese
  expected_dates.at("be") = kRootLocaleDate;  // Belarusian
  expected_dates.at("bs") = kRootLocaleDate;  // Bosnian
  expected_dates.at("or") = kRootLocaleDate;  // Odia
#elif BUILDFLAG(IS_CHROMEOS)
  expected_dates.at("mn") = kRootLocaleDate;  // Mongolian
#elif BUILDFLAG(IS_IOS) && BUILDFLAG(USE_BLINK)
  expected_dates.at("gl") = kRootLocaleDate;  // Galician
  expected_dates.at("or") = kRootLocaleDate;  // Odia
  expected_dates.at("pa") = kRootLocaleDate;  // Punjabi
#elif BUILDFLAG(IS_IOS) && !BUILDFLAG(USE_BLINK)
  expected_dates.at("bn") = kRootLocaleDate;    // Bengali
  expected_dates.at("fa") = u"۱۷ سپتامبر";      // Persian
  expected_dates.at("gl") = kRootLocaleDate;    // Galician
  expected_dates.at("gu") = kRootLocaleDate;    // Gujarati
  expected_dates.at("kn") = kRootLocaleDate;    // Kannada
  expected_dates.at("ml") = kRootLocaleDate;    // Malayalam
  expected_dates.at("mr") = kRootLocaleDate;    // Marathi
  expected_dates.at("ms") = kRootLocaleDate;    // Malay
  expected_dates.at("or") = kRootLocaleDate;    // Odia
  expected_dates.at("pa") = kRootLocaleDate;    // Punjabi
  expected_dates.at("ta") = kRootLocaleDate;    // Tamil
  expected_dates.at("te") = kRootLocaleDate;    // Telugu
  expected_dates.at("th") = u"7 ├เดือน: ธ.ค.┤";  // Thai
  expected_dates.at("ur") = kRootLocaleDate;    // Urdu
#endif

  // We loop over `kPlatformLocales`, not `expected_dates`, so that we limit
  // ourselves to those locales supported by the platform.
  for (std::string_view locale : kPlatformLocales) {
    SCOPED_TRACE(testing::Message() << "Locale: " << locale);

    const std::u16string_view* expected_date =
        base::FindOrNull(expected_dates, locale);
    ASSERT_NE(expected_date, nullptr);

    const std::optional<std::u16string> pattern =
        data_util::LocalizePattern(u"MMM d", locale);
    ASSERT_TRUE(pattern);
    DateInfo info;
    info.SetDate(kDateInIso, u"YYYY-MM-DD");
    const std::u16string actual_date = info.GetIcuDate(*pattern, locale);

    EXPECT_EQ(actual_date, *expected_date);
  }
}

}  // namespace
}  // namespace autofill
