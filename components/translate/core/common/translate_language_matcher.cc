// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/common/translate_language_matcher.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "base/i18n/language_tag.h"
#include "base/i18n/tag_converters.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "components/language/core/common/locale_util.h"

namespace translate {
namespace {

using ::base::i18n::GetKnownLanguageTag;
using ::base::i18n::LanguageTag;

// The default list of languages the Google translation server supports.
// This list must be sorted in alphabetical order and contain no duplicates.
constexpr auto kDefaultSupportedLanguages = std::to_array<LanguageTag>({
    GetKnownLanguageTag("af"),        // Afrikaans
    GetKnownLanguageTag("ak"),        // Akan
    GetKnownLanguageTag("am"),        // Amharic
    GetKnownLanguageTag("ar"),        // Arabic
    GetKnownLanguageTag("as"),        // Assamese
    GetKnownLanguageTag("ay"),        // Aymara
    GetKnownLanguageTag("az"),        // Azerbaijani
    GetKnownLanguageTag("be"),        // Belarusian
    GetKnownLanguageTag("bg"),        // Bulgarian
    GetKnownLanguageTag("bho"),       // Bhojpuri
    GetKnownLanguageTag("bm"),        // Bambara
    GetKnownLanguageTag("bn"),        // Bengali
    GetKnownLanguageTag("bs"),        // Bosnian
    GetKnownLanguageTag("ca"),        // Catalan
    GetKnownLanguageTag("ceb"),       // Cebuano
    GetKnownLanguageTag("ckb"),       // Kurdish (Sorani)
    GetKnownLanguageTag("co"),        // Corsican
    GetKnownLanguageTag("cs"),        // Czech
    GetKnownLanguageTag("cy"),        // Welsh
    GetKnownLanguageTag("da"),        // Danish
    GetKnownLanguageTag("de"),        // German
    GetKnownLanguageTag("doi"),       // Dogri
    GetKnownLanguageTag("dv"),        // Dhivehi
    GetKnownLanguageTag("ee"),        // Ewe
    GetKnownLanguageTag("el"),        // Greek
    GetKnownLanguageTag("en"),        // English
    GetKnownLanguageTag("eo"),        // Esperanto
    GetKnownLanguageTag("es"),        // Spanish
    GetKnownLanguageTag("et"),        // Estonian
    GetKnownLanguageTag("eu"),        // Basque
    GetKnownLanguageTag("fa"),        // Persian
    GetKnownLanguageTag("fi"),        // Finnish
    GetKnownLanguageTag("fil"),       // Filipino
    GetKnownLanguageTag("fr"),        // French
    GetKnownLanguageTag("fy"),        // Frisian
    GetKnownLanguageTag("ga"),        // Irish
    GetKnownLanguageTag("gd"),        // Scots Gaelic
    GetKnownLanguageTag("gl"),        // Galician
    GetKnownLanguageTag("gu"),        // Gujarati
    GetKnownLanguageTag("ha"),        // Hausa
    GetKnownLanguageTag("haw"),       // Hawaiian
    GetKnownLanguageTag("he"),        // Hebrew
    GetKnownLanguageTag("hi"),        // Hindi
    GetKnownLanguageTag("hmn"),       // Hmong
    GetKnownLanguageTag("hr"),        // Croatian
    GetKnownLanguageTag("ht"),        // Haitian Creole
    GetKnownLanguageTag("hu"),        // Hungarian
    GetKnownLanguageTag("hy"),        // Armenian
    GetKnownLanguageTag("id"),        // Indonesian
    GetKnownLanguageTag("ig"),        // Igbo
    GetKnownLanguageTag("ilo"),       // Ilocano
    GetKnownLanguageTag("is"),        // Icelandic
    GetKnownLanguageTag("it"),        // Italian
    GetKnownLanguageTag("ja"),        // Japanese
    GetKnownLanguageTag("jv"),        // Javanese
    GetKnownLanguageTag("ka"),        // Georgian
    GetKnownLanguageTag("kk"),        // Kazakh
    GetKnownLanguageTag("km"),        // Khmer
    GetKnownLanguageTag("kn"),        // Kannada
    GetKnownLanguageTag("ko"),        // Korean
    GetKnownLanguageTag("kok"),       // Konkani
    GetKnownLanguageTag("kri"),       // Krio
    GetKnownLanguageTag("ku"),        // Kurdish (Kurmanji)
    GetKnownLanguageTag("ky"),        // Kyrgyz
    GetKnownLanguageTag("la"),        // Latin
    GetKnownLanguageTag("lb"),        // Luxembourgish
    GetKnownLanguageTag("lg"),        // Luganda
    GetKnownLanguageTag("ln"),        // Lingala
    GetKnownLanguageTag("lo"),        // Lao
    GetKnownLanguageTag("lt"),        // Lithuanian
    GetKnownLanguageTag("lus"),       // Mizo
    GetKnownLanguageTag("lv"),        // Latvian
    GetKnownLanguageTag("mai"),       // Maithili
    GetKnownLanguageTag("mg"),        // Malagasy
    GetKnownLanguageTag("mi"),        // Maori
    GetKnownLanguageTag("mk"),        // Macedonian
    GetKnownLanguageTag("ml"),        // Malayalam
    GetKnownLanguageTag("mn"),        // Mongolian
    GetKnownLanguageTag("mni-Mtei"),  // Meiteilon (Manipuri)
    GetKnownLanguageTag("mr"),        // Marathi
    GetKnownLanguageTag("ms"),        // Malay
    GetKnownLanguageTag("mt"),        // Maltese
    GetKnownLanguageTag("my"),        // Myanmar (Burmese)
    GetKnownLanguageTag("ne"),        // Nepali
    GetKnownLanguageTag("nl"),        // Dutch
    GetKnownLanguageTag("no"),        // Norwegian
    GetKnownLanguageTag("nso"),       // Sepedi
    GetKnownLanguageTag("ny"),        // Chichewa
    GetKnownLanguageTag("om"),        // Oromo
    GetKnownLanguageTag("or"),        // Odia (Oriya)
    GetKnownLanguageTag("pa"),        // Punjabi
    GetKnownLanguageTag("pl"),        // Polish
    GetKnownLanguageTag("ps"),        // Pashto
    GetKnownLanguageTag("pt"),        // Portuguese
    GetKnownLanguageTag("qu"),        // Quechua
    GetKnownLanguageTag("ro"),        // Romanian
    GetKnownLanguageTag("ru"),        // Russian
    GetKnownLanguageTag("rw"),        // Kinyarwanda
    GetKnownLanguageTag("sa"),        // Sanskrit
    GetKnownLanguageTag("sd"),        // Sindhi
    GetKnownLanguageTag("si"),        // Sinhala
    GetKnownLanguageTag("sk"),        // Slovak
    GetKnownLanguageTag("sl"),        // Slovenian
    GetKnownLanguageTag("sm"),        // Samoan
    GetKnownLanguageTag("sn"),        // Shona
    GetKnownLanguageTag("so"),        // Somali
    GetKnownLanguageTag("sq"),        // Albanian
    GetKnownLanguageTag("sr"),        // Serbian
    GetKnownLanguageTag("st"),        // Sesotho
    GetKnownLanguageTag("su"),        // Sundanese
    GetKnownLanguageTag("sv"),        // Swedish
    GetKnownLanguageTag("sw"),        // Swahili
    GetKnownLanguageTag("ta"),        // Tamil
    GetKnownLanguageTag("te"),        // Telugu
    GetKnownLanguageTag("tg"),        // Tajik
    GetKnownLanguageTag("th"),        // Thai
    GetKnownLanguageTag("ti"),        // Tigrinya
    GetKnownLanguageTag("tk"),        // Turkmen
    GetKnownLanguageTag("tr"),        // Turkish
    GetKnownLanguageTag("ts"),        // Tsonga
    GetKnownLanguageTag("tt"),        // Tatar
    GetKnownLanguageTag("ug"),        // Uyghur
    GetKnownLanguageTag("uk"),        // Ukrainian
    GetKnownLanguageTag("ur"),        // Urdu
    GetKnownLanguageTag("uz"),        // Uzbek
    GetKnownLanguageTag("vi"),        // Vietnamese
    GetKnownLanguageTag("xh"),        // Xhosa
    GetKnownLanguageTag("yi"),        // Yiddish
    GetKnownLanguageTag("yo"),        // Yoruba
    GetKnownLanguageTag("zh-CN"),     // Chinese (Simplified)
    GetKnownLanguageTag("zh-TW"),     // Chinese (Traditional)
    GetKnownLanguageTag("zu"),        // Zulu
});

}  // namespace

base::span<const base::i18n::LanguageTag> GetDefaultSupportedLanguages() {
  return base::span(kDefaultSupportedLanguages);
}

const base::i18n::LanguageTagMatcherWithDefault& GetTranslateLanguageMatcher() {
  static base::NoDestructor<base::i18n::LanguageTagMatcherWithDefault> matcher(
      base::i18n::LanguageTagMatcherWithDefault::Create(
          GetKnownLanguageTag("en"), kDefaultSupportedLanguages));
  return *matcher;
}

}  // namespace translate
