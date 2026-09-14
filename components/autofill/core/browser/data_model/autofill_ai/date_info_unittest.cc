// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_ai/date_info.h"

#include <optional>
#include <string>
#include <string_view>

#include "build/build_config.h"
#include "components/autofill/core/browser/data_model/data_model_util.h"
#include "components/personal_context/proto/features/common_data.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

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
  DateInfo info;
  info.SetDate(u"07/12/2028", u"DD/MM/YYYY");
  EXPECT_EQ(info.GetDate(u"YYYY-MM-DD"), u"2028-12-07");

  auto get = [&info](std::string_view locale) -> std::u16string {
    std::optional<std::u16string> pattern =
        data_util::LocalizePattern(u"MMM d", locale);
    if (!pattern) {
      return u"invalid";
    }
    return info.GetIcuDate(*pattern, locale);
  };

  // Those are the supported locales according to
  // ui/base/l10n/l10n_util_locales_list.inc.
  EXPECT_EQ(get("am"), u"ዲሴም 7");         // Amharic
  EXPECT_EQ(get("ar"), u"7 ديسمبر");      // Arabic
  EXPECT_EQ(get("bg"), u"7.12");          // Bulgarian
  EXPECT_EQ(get("ca"), u"7 de des.");     // Catalan
  EXPECT_EQ(get("cs"), u"7. 12.");        // Czech
  EXPECT_EQ(get("da"), u"7. dec.");       // Danish
  EXPECT_EQ(get("de"), u"7. Dez.");       // German
  EXPECT_EQ(get("el"), u"7 Δεκ");         // Greek
  EXPECT_EQ(get("en-GB"), u"7 Dec");      // English (UK)
  EXPECT_EQ(get("en-US"), u"Dec 7");      // English (US)
  EXPECT_EQ(get("es"), u"7 dic");         // Spanish
  EXPECT_EQ(get("es-419"), u"7 dic");     // Spanish (Latin America)
  EXPECT_EQ(get("fi"), u"7.12.");         // Finnish.
  EXPECT_EQ(get("fil"), u"Dis 7");        // Filipino
  EXPECT_EQ(get("fr"), u"7 déc.");        // French
  EXPECT_EQ(get("he"), u"7 בדצמ׳");       // Hebrew
  EXPECT_EQ(get("hi"), u"7 दिस॰");        // Hindi
  EXPECT_EQ(get("hr"), u"7. pro");        // Croatian
  EXPECT_EQ(get("hu"), u"dec. 7.");       // Hungarian
  EXPECT_EQ(get("id"), u"7 Des");         // Indonesian
  EXPECT_EQ(get("it"), u"7 dic");         // Italian
  EXPECT_EQ(get("ja"), u"12月7日");       // Japanese
  EXPECT_EQ(get("ko"), u"12월 7일");      // Korean
  EXPECT_EQ(get("lt"), u"12-07");         // Lithuanian.
  EXPECT_EQ(get("lv"), u"7. dec.");       // Latvian
  EXPECT_EQ(get("nb"), u"7. des.");       // Norwegian (Bokmal)
  EXPECT_EQ(get("nl"), u"7 dec");         // Dutch
  EXPECT_EQ(get("pl"), u"7 gru");         // Polish
  EXPECT_EQ(get("pt-BR"), u"7 de dez.");  // Portuguese (Brazil)
  EXPECT_EQ(get("pt-PT"), u"7/12");       // Portuguese (Portugal).
  EXPECT_EQ(get("ro"), u"7 dec.");        // Romanian
  EXPECT_EQ(get("ru"), u"7 дек.");        // Russian
  EXPECT_EQ(get("sk"), u"7. 12.");        // Slovak
  EXPECT_EQ(get("sl"), u"7. dec.");       // Slovenian
  EXPECT_EQ(get("sr"), u"7. дец");        // Serbian
  EXPECT_EQ(get("sv"), u"7 dec.");        // Swedish
  EXPECT_EQ(get("sw"), u"7 Des");         // Swahili
  EXPECT_EQ(get("tr"), u"7 Ara");         // Turkish
  EXPECT_EQ(get("uk"), u"7 груд.");       // Ukrainian
  EXPECT_EQ(get("vi"), u"7 thg 12");      // Vietnamese
  EXPECT_EQ(get("zh-CN"), u"12月7日");    // Chinese (China)
  EXPECT_EQ(get("zh-TW"), u"12月7日");    // Chinese (Taiwan)

#if !BUILDFLAG(IS_IOS)
  // iOS ICU data (third_party/icu/filters/ios.json) strips calendar data for
  // these desktop UI locales.
  EXPECT_EQ(get("af"), u"7 Des.");   // Afrikaans
  EXPECT_EQ(get("bn"), u"৭ ডিসে");   // Bengali
  EXPECT_EQ(get("et"), u"7. dets");  // Estonian
  EXPECT_EQ(get("fa"), u"۱۷ آذر");   // Persian
  EXPECT_EQ(get("gu"), u"7 ડિસે");    // Gujarati
  EXPECT_EQ(get("kn"), u"7 ಡಿಸೆಂ");    // Kannada
  EXPECT_EQ(get("ml"), u"ഡിസം 7");   // Malayalam
  EXPECT_EQ(get("mr"), u"७ डिसें");    // Marathi
  EXPECT_EQ(get("ms"), u"7 Dis");    // Malay
  EXPECT_EQ(get("ta"), u"டிச. 7");   // Tamil
  EXPECT_EQ(get("te"), u"7 డిసెం");    // Telugu
  EXPECT_EQ(get("th"), u"7 ธ.ค.");   // Thai
#endif

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_CHROMEOS)
  // Both iOS (ios.json) and ChromeOS (chromeos.json) strip calendar data for
  // Urdu.
  EXPECT_EQ(get("ur"), u"7 دسمبر");  // Urdu
#endif
}

}  // namespace
}  // namespace autofill
