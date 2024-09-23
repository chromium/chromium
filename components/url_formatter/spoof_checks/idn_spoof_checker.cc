// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_formatter/spoof_checks/idn_spoof_checker.h"

#include <bit>
#include <cstdint>
#include <string_view>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_local_storage.h"
#include "build/build_config.h"
#include "net/base/lookup_string_in_fixed_set.h"
#include "third_party/icu/source/common/unicode/schriter.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/i18n/unicode/regex.h"
#include "third_party/icu/source/i18n/unicode/translit.h"
#include "third_party/icu/source/i18n/unicode/uspoof.h"
#include "url/url_features.h"

namespace url_formatter {

namespace {

class TopDomainPreloadDecoder : public net::extras::PreloadDecoder {
 public:
  using net::extras::PreloadDecoder::PreloadDecoder;
  ~TopDomainPreloadDecoder() override = default;

  bool ReadEntry(net::extras::PreloadDecoder::BitReader* reader,
                 const std::string& search,
                 size_t current_search_offset,
                 bool* out_found) override {
    // Make sure the assigned bit length is enough to encode all SkeletonType
    // values.
    DCHECK_EQ(kSkeletonTypeBitLength,
              std::bit_width<uint32_t>(url_formatter::SkeletonType::kMaxValue));

    bool is_same_skeleton;

    if (!reader->Next(&is_same_skeleton))
      return false;

    TopDomainEntry top_domain;
    if (!reader->Next(&top_domain.is_top_bucket)) {
      return false;
    }
    uint32_t skeletontype_value;
    if (!reader->Read(kSkeletonTypeBitLength, &skeletontype_value))
      return false;
    top_domain.skeleton_type =
        static_cast<url_formatter::SkeletonType>(skeletontype_value);
    if (is_same_skeleton) {
      top_domain.domain = search;
    } else {
      bool has_com_suffix = false;
      if (!reader->Next(&has_com_suffix))
        return false;

      for (char c;; top_domain.domain += c) {
        huffman_decoder().Decode(reader, &c);
        if (c == net::extras::PreloadDecoder::kEndOfTable)
          break;
      }
      if (has_com_suffix)
        top_domain.domain += ".com";
    }
    if (current_search_offset == 0) {
      *out_found = true;
      DCHECK(!top_domain.domain.empty());
      result_ = top_domain;
    }
    return true;
  }

  TopDomainEntry matching_top_domain() const { return result_; }

 private:
  TopDomainEntry result_;
};

// Stores whole-script-confusable information about a written script.
// Used to populate a list of WholeScriptConfusable structs.
struct WholeScriptConfusableData {
  const char* const script_regex;
  const char* const latin_lookalike_letters;
  const std::vector<std::string> allowed_tlds;
};

void OnThreadTermination(void* regex_matcher) {
  delete reinterpret_cast<icu::RegexMatcher*>(regex_matcher);
}

base::ThreadLocalStorage::Slot& DangerousPatternTLS() {
  static base::NoDestructor<base::ThreadLocalStorage::Slot>
      dangerous_pattern_tls(&OnThreadTermination);
  return *dangerous_pattern_tls;
}

// Allow middle dot (U+00B7) only on Catalan domains when between two 'l's, to
// permit the Catalan character ela geminada to be expressed.
// See https://tools.ietf.org/html/rfc5892#appendix-A.3 for details.
bool HasUnsafeMiddleDot(const icu::UnicodeString& label_string,
                        std::string_view top_level_domain) {
  int last_index = 0;
  while (true) {
    int index = label_string.indexOf("·", last_index);
    if (index < 0) {
      break;
    }
    DCHECK_LT(index, label_string.length());
    if (top_level_domain != "cat") {
      // Non-Catalan domains cannot contain middle dot.
      return true;
    }
    // Middle dot at the beginning or end.
    if (index == 0 || index == label_string.length() - 1) {
      return true;
    }
    // Middle dot not surrounded by an 'l'.
    if (label_string[index - 1] != 'l' || label_string[index + 1] != 'l') {
      return true;
    }
    last_index = index + 1;
  }
  return false;
}

bool IsSubdomainOf(std::u16string_view hostname,
                   const std::u16string& top_domain) {
  DCHECK_NE(hostname, top_domain);
  DCHECK(!hostname.empty());
  DCHECK(!top_domain.empty());
  return base::EndsWith(hostname, u"." + top_domain,
                        base::CompareCase::INSENSITIVE_ASCII);
}

#include "components/url_formatter/spoof_checks/top_domains/domains-trie-inc.cc"

// All the domains in the above file have 4 or fewer labels.
const size_t kNumberOfLabelsToCheck = 4;

IDNSpoofChecker::HuffmanTrieParams g_trie_params{
    kTopDomainsHuffmanTree, sizeof(kTopDomainsHuffmanTree), kTopDomainsTrie,
    kTopDomainsTrieBits, kTopDomainsRootPosition};

// Allow these common words that are whole script confusables. They aren't
// confusable with any words in Latin scripts.
const char16_t* kAllowedWholeScriptConfusableWords[] = {
    u"секс",  u"как",   u"коса",    u"курс",    u"парк",
    u"такий", u"укроп", u"сахарок", u"покраска"};

}  // namespace

IDNSpoofChecker::WholeScriptConfusable::WholeScriptConfusable(
    std::unique_ptr<icu::UnicodeSet> arg_all_letters,
    std::unique_ptr<icu::UnicodeSet> arg_latin_lookalike_letters,
    const std::vector<std::string>& arg_allowed_tlds)
    : all_letters(std::move(arg_all_letters)),
      latin_lookalike_letters(std::move(arg_latin_lookalike_letters)),
      allowed_tlds(arg_allowed_tlds) {}

IDNSpoofChecker::WholeScriptConfusable::~WholeScriptConfusable() = default;

IDNSpoofChecker::IDNSpoofChecker() {
  UErrorCode status = U_ZERO_ERROR;
  checker_ = uspoof_open(&status);
  if (U_FAILURE(status)) {
    checker_ = nullptr;
    return;
  }

  // At this point, USpoofChecker has all the checks enabled except
  // for USPOOF_CHAR_LIMIT (USPOOF_{RESTRICTION_LEVEL, INVISIBLE,
  // MIXED_SCRIPT_CONFUSABLE, WHOLE_SCRIPT_CONFUSABLE, MIXED_NUMBERS, ANY_CASE})
  // This default configuration is adjusted below as necessary.

  // Set the restriction level to high. It allows mixing Latin with one logical
  // CJK script (+ COMMON and INHERITED), but does not allow any other script
  // mixing (e.g. Latin + Cyrillic, Latin + Armenian, Cyrillic + Greek). Note
  // that each of {Han + Bopomofo} for Chinese, {Hiragana, Katakana, Han} for
  // Japanese, and {Hangul, Han} for Korean is treated as a single logical
  // script.
  // See http://www.unicode.org/reports/tr39/#Restriction_Level_Detection
  uspoof_setRestrictionLevel(checker_, USPOOF_HIGHLY_RESTRICTIVE);

  // Sets allowed characters in IDN labels and turns on USPOOF_CHAR_LIMIT.
  SetAllowedUnicodeSet(&status);

  // Enable the return of auxillary (non-error) information.
  // We used to disable WHOLE_SCRIPT_CONFUSABLE check explicitly, but as of
  // ICU 58.1, WSC is a no-op in a single string check API.
  int32_t checks = uspoof_getChecks(checker_, &status) | USPOOF_AUX_INFO;
  uspoof_setChecks(checker_, checks, &status);

  // Four characters handled differently by IDNA 2003 and IDNA 2008. UTS46
  // transitional processing treats them as IDNA 2003 does; maps U+00DF and
  // U+03C2 and drops U+200[CD].
  deviation_characters_ = icu::UnicodeSet(
      UNICODE_STRING_SIMPLE("[\\u00df\\u03c2\\u200c\\u200d]"), status);
  deviation_characters_.freeze();

  // Latin letters outside ASCII. 'Script_Extensions=Latin' is not necessary
  // because additional characters pulled in with scx=Latn are not included in
  // the allowed set.
  non_ascii_latin_letters_ =
      icu::UnicodeSet(UNICODE_STRING_SIMPLE("[[:Latin:] - [a-zA-Z]]"), status);
  non_ascii_latin_letters_.freeze();

  // The following two sets are parts of |dangerous_patterns_|.
  kana_letters_exceptions_ = icu::UnicodeSet(
      UNICODE_STRING_SIMPLE("[\\u3078-\\u307a\\u30d8-\\u30da\\u30fb-\\u30fe]"),
      status);
  kana_letters_exceptions_.freeze();
  combining_diacritics_exceptions_ =
      icu::UnicodeSet(UNICODE_STRING_SIMPLE("[\\u0300-\\u0339]"), status);
  combining_diacritics_exceptions_.freeze();

  const WholeScriptConfusableData kWholeScriptConfusables[] = {
      {// Armenian
       "[[:Armn:]]",
       "[ագզէլհյոսւօՙ]",
       {"am"}},
      {// Cyrillic
       "[[:Cyrl:]]",
       "[аысԁеԍһіюкјӏорԗԛѕԝхуъьҽпгѵѡ]",
       // TLDs containing most of the Cyrillic domains.
       {"bg", "by", "kz", "pyc", "ru", "su", "ua", "uz"}},
      {// Ethiopic (Ge'ez). Variants of these characters such as ሁ and ሡ could
       // arguably be added to this list. However, we are only restricting
       // the more obvious characters to keep the list short and to reduce the
       // probability of false positives.
       // Potential set: [ሀሁሃሠሡሰሱሲስበቡቢተቱቲታነከኩኪካኬክዐዑዕዖዘዙዚዛዝዞጠጡጢጣጦፐፒꬁꬂꬅ]
       "[[:Ethi:]]",
       "[ሀሠሰስበነተከዐዕዘጠፐꬅ]",
       {"er", "et"}},
      {// Georgian
       "[[:Geor:]]",
       "[იოყძხჽჿ]",
       {"ge"}},
      {// Greek
       "[[:Grek:]]",
       // This ignores variants such as ά, έ, ή, ί.
       "[αικνρυωηοτ]",
       {"gr"}},
      {// Hebrew
       "[[:Hebr:]]",
       "[דוחיןסװײ׳ﬦ]",
       // TLDs containing most of the Hebrew domains.
       {"il"}},
      // Indic scripts in the recommended set. No ccTLDs are allowlisted.
      {// Bengali
       "[[:Beng:]]", "[০৭]"},
      {// Devanagari
       "[[:Deva:]]", "[ऽ०ॱ]"},
      {// Gujarati
       "[[:Gujr:]]", "[ડટ૦૧]"},
      {// Gurmukhi
       "[[:Guru:]]", "[੦੧]"},
      {// Kannada
       "[[:Knda:]]", "[ಽ೦೧]"},
      {// Malayalam
       "[[:Mlym:]]", "[ടഠധനറ൦]"},
      {// Oriya
       "[[:Orya:]]", "[ଠ୦୮]"},
      {// Tamil
       "[[:Taml:]]", "[டப௦]"},
      {// Telugu
       "[[:Telu:]]", "[౦౧]"},
      {// Myanmar. Shan digits (႐႑႕႖႗) are already blocked from mixing with
       // other Myanmar characters. However, they can still be used to form
       // WSC spoofs, so they are included here (they are encoded because macOS
       // doesn't display them properly).
       // U+104A (၊) and U+U+104A(။) are excluded as they are signs and are
       // blocked.
       "[[:Mymr:]]",
       "[ခဂငထပဝ၀၂ၔၜ\u1090\u1091\u1095\u1096\u1097]",
       {"mm"}},
      {// Thai
       "[[:Thai:]]",
  // Some of the Thai characters are only confusable on Linux, so the Linux
  // set is larger than other platforms. Ideally we don't want to have any
  // differences between platforms, but doing so is unavoidable here as
  // these characters look significantly different between Linux and other
  // platforms.
  // The ideal fix would be to change the omnibox font used for Thai. In
  // that case, the Linux-only list should be revisited and potentially
  // removed.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
       "[ทนบพรหเแ๐ดลปฟม]",
#else
       "[บพเแ๐]",
#endif
       {"th"}},
  };
  for (const WholeScriptConfusableData& data : kWholeScriptConfusables) {
    auto all_letters = std::make_unique<icu::UnicodeSet>(
        icu::UnicodeString::fromUTF8(data.script_regex), status);
    DCHECK(U_SUCCESS(status));

    // Lookalike letter list must be all lower case letters. Domain name labels
    // are canonicalized to lower case, so having upper case letters in this
    // list will result in a non-match.
    const icu::UnicodeString latin_lookalike_letters =
        icu::UnicodeString::fromUTF8(data.latin_lookalike_letters);
    icu::UnicodeString latin_lookalike_letters_lowercase =
        latin_lookalike_letters;
    latin_lookalike_letters_lowercase.toLower();
    DCHECK(latin_lookalike_letters == latin_lookalike_letters_lowercase);
    auto latin_lookalikes = std::make_unique<icu::UnicodeSet>(
        latin_lookalike_letters_lowercase, status);

    DCHECK(U_SUCCESS(status));
    auto script = std::make_unique<WholeScriptConfusable>(
        std::move(all_letters), std::move(latin_lookalikes), data.allowed_tlds);
    wholescriptconfusables_.push_back(std::move(script));
  }

  // These characters are, or look like, digits. A domain label entirely made of
  // digit-lookalikes or digits is blocked.
  // IMPORTANT: When you add a new character here, make sure to add it to
  // extra_confusable_mapper_ in skeleton_generator.cc too.
  digits_ = icu::UnicodeSet(UNICODE_STRING_SIMPLE("[0-9]"), status);
  digits_.freeze();
  digit_lookalikes_ = icu::UnicodeSet(
      icu::UnicodeString::fromUTF8("[θ२২੨੨૨೩೭շзҙӡउওਤ੩૩౩ဒვპੜკ੫丩ㄐճ৪੪୫૭୨౨]"),
      status);
  digit_lookalikes_.freeze();

  DCHECK(U_SUCCESS(status));

  // Latin small letter thorn ("þ", U+00FE) can be used to spoof both b and p.
  // It's used in modern Icelandic orthography, so allow it for the Icelandic
  // ccTLD (.is) but block in any other TLD. Also block Latin small letter eth
  // ("ð", U+00F0) which can be used to spoof the letter o.
  icelandic_characters_ =
      icu::UnicodeSet(UNICODE_STRING_SIMPLE("[\\u00fe\\u00f0]"), status);
  icelandic_characters_.freeze();

  DCHECK(U_SUCCESS(status))
      << "Spoofchecker initalization failed due to an error: "
      << u_errorName(status);

  skeleton_generator_ = std::make_unique<SkeletonGenerator>(checker_);
}

IDNSpoofChecker::~IDNSpoofChecker() {
  uspoof_close(checker_);
}

IDNSpoofChecker::Result IDNSpoofChecker::SafeToDisplayAsUnicode(
    std::u16string_view label,
    std::string_view top_level_domain,
    std::u16string_view top_level_domain_unicode) {
  UErrorCode status = U_ZERO_ERROR;
  int32_t result =
      uspoof_check(checker_, label.data(),
                   base::checked_cast<int32_t>(label.size()), nullptr, &status);
  // If uspoof_check fails (due to library failure), or if any of the checks
  // fail, treat the IDN as unsafe.
  if (U_FAILURE(status) || (result & USPOOF_ALL_CHECKS)) {
    return Result::kICUSpoofChecks;
  }

  icu::UnicodeString label_string(false /* isTerminated */, label.data(),
                                  base::checked_cast<int32_t>(label.size()));

  // A punycode label with 'xn--' prefix is not subject to the URL
  // canonicalization and is stored as it is in GURL. If it encodes a deviation
  // character (UTS 46; e.g. U+00DF/sharp-s), it should be still shown in
  // punycode instead of Unicode. Without this check, xn--fu-hia for
  // 'fu<sharp-s>' would be converted to 'fu<sharp-s>' for display because
  // "UTS 46 section 4 Processing step 4" applies validity criteria for
  // non-transitional processing (i.e. do not map deviation characters) to any
  // punycode labels regardless of whether transitional or non-transitional is
  // chosen. On the other hand, 'fu<sharp-s>' typed or copy and pasted
  // as Unicode would be canonicalized to 'fuss' by GURL and is displayed as
  // such. See http://crbug.com/595263 .
  if (!url::IsUsingIDNA2008NonTransitional() &&
      deviation_characters_.containsSome(label_string)) {
    return Result::kDeviationCharacters;
  }

  // Disallow Icelandic confusables for domains outside Icelandic and Faroese
  // ccTLD (.is, .fo). Faroese keyboard layout doesn't contain letter ⟨þ⟩, but
  // we don't separate it here to avoid technical complexity, and because
  // Faroese speakers are more likely to notice spoofs containing ⟨þ⟩ than other
  // language speakers.
  if (label_string.length() > 1 && top_level_domain != "is" &&
      top_level_domain != "fo" &&
      icelandic_characters_.containsSome(label_string)) {
    return Result::kTLDSpecificCharacters;
  }

  // Disallow Latin Schwa (U+0259) for domains outside Azerbaijan's ccTLD (.az).
  if (label_string.length() > 1 && top_level_domain != "az" &&
      label_string.indexOf("ə") != -1) {
    return Result::kTLDSpecificCharacters;
  }

  // Disallow middle dot (U+00B7) when unsafe.
  if (HasUnsafeMiddleDot(label_string, top_level_domain)) {
    return Result::kUnsafeMiddleDot;
  }

  // If there's no script mixing, the input is regarded as safe without any
  // extra check unless it falls into one of three categories:
  //   - contains Kana letter exceptions
  //   - the TLD is ASCII and the input is made entirely of whole script
  //     characters confusable that look like Latin letters.
  //   - it has combining diacritic marks.
  // Note that the following combinations of scripts are treated as a 'logical'
  // single script.
  //  - Chinese: Han, Bopomofo, Common
  //  - Japanese: Han, Hiragana, Katakana, Common
  //  - Korean: Hangul, Han, Common
  result &= USPOOF_RESTRICTION_LEVEL_MASK;
  if (result == USPOOF_ASCII)
    return Result::kSafe;

  if (result == USPOOF_SINGLE_SCRIPT_RESTRICTIVE &&
      kana_letters_exceptions_.containsNone(label_string) &&
      combining_diacritics_exceptions_.containsNone(label_string)) {
    for (auto const& script : wholescriptconfusables_) {
      if (IsLabelWholeScriptConfusableForScript(*script, label_string) &&
          !IsWholeScriptConfusableAllowedForTLD(*script, top_level_domain,
                                                top_level_domain_unicode) &&
          !base::Contains(kAllowedWholeScriptConfusableWords, label)) {
        return Result::kWholeScriptConfusable;
      }
    }
    // Disallow domains that contain only numbers and number-spoofs.
    // This check is reached if domain characters come from single script.
    if (IsDigitLookalike(label_string))
      return Result::kDigitLookalikes;

    return Result::kSafe;
  }

  // Disallow domains that contain only numbers and number-spoofs.
  // This check is reached if domain characters are from different scripts.
  // This is generally rare. An example case when it would return true is when
  // the domain contains Latin + Japanese characters that are also digit
  // lookalikes.
  if (IsDigitLookalike(label_string))
    return Result::kDigitLookalikes;

  // Additional checks for |label| with multiple scripts, one of which is Latin.
  // Disallow non-ASCII Latin letters to mix with a non-Latin script.
  // Note that the non-ASCII Latin check should not be applied when the entire
  // label is made of Latin. Checking with lgc_letters set here should be fine
  // because script mixing of LGC is already rejected.
  if (non_ascii_latin_letters_.containsSome(label_string) &&
      !(skeleton_generator_ &&
        skeleton_generator_->ShouldRemoveDiacriticsFromLabel(label_string))) {
    return Result::kNonAsciiLatinCharMixedWithNonLatin;
  }

  icu::RegexMatcher* dangerous_pattern =
      reinterpret_cast<icu::RegexMatcher*>(DangerousPatternTLS().Get());
  if (!dangerous_pattern) {
    // The parentheses in the below strings belong to the raw string sequence
    // R"(...)". They are NOT part of the regular expression. Each sub
    // regex is OR'ed with the | operator.
    dangerous_pattern = new icu::RegexMatcher(
        icu::UnicodeString(
            // Disallow the following as they may be mistaken for slashes when
            // they're surrounded by non-Japanese scripts (i.e. has non-Katakana
            // Hiragana or Han scripts on both sides):
            // "ノ" (Katakana no, U+30ce), "ソ" (Katakana so, U+30bd),
            // "ゾ" (Katakana zo, U+30be), "ン" (Katakana n, U+30f3),
            // "丶" (CJK unified ideograph, U+4E36),
            // "乀" (CJK unified ideograph, U+4E40),
            // "乁" (CJK unified ideograph, U+4E41),
            // "丿" (CJK unified ideograph, U+4E3F).
            // If {no, so, zo, n} next to a
            // non-Japanese script on either side is disallowed, legitimate
            // cases like '{vitamin in Katakana}b6' are blocked. Note that
            // trying to block those characters when used alone as a label is
            // futile because those cases would not reach here. Also disallow
            // what used to be blocked by mixed-script-confusable (MSC)
            // detection. ICU 58 does not detect MSC any more for a single input
            // string. See http://bugs.icu-project.org/trac/ticket/12823 .
            // TODO(jshin): adjust the pattern once the above ICU bug is fixed.
            R"([^\p{scx=kana}\p{scx=hira}\p{scx=hani}])"
            R"([\u30ce\u30f3\u30bd\u30be\u4e36\u4e40\u4e41\u4e3f])"
            R"([^\p{scx=kana}\p{scx=hira}\p{scx=hani}]|)"

            // Disallow U+30FD (Katakana iteration mark) and U+30FE (Katakana
            // voiced iteration mark) unless they're preceded by a Katakana.
            R"([^\p{scx=kana}][\u30fd\u30fe]|^[\u30fd\u30fe]|)"

            // Disallow three Hiragana letters (U+307[8-A]) or Katakana letters
            // (U+30D[8-A]) that look exactly like each other when they're used
            // in a label otherwise entirely in Katakana or Hiragana.
            R"(^[\p{scx=kana}]+[\u3078-\u307a][\p{scx=kana}]+$|)"
            R"(^[\p{scx=hira}]+[\u30d8-\u30da][\p{scx=hira}]+$|)"

            // Disallow U+30FB (Katakana Middle Dot) and U+30FC (Hiragana-
            // Katakana Prolonged Sound) used out-of-context.
            R"([^\p{scx=kana}\p{scx=hira}]\u30fc|^\u30fc|)"
            R"([a-z]\u30fb|\u30fb[a-z]|)"

            // Disallow these CJK ideographs if they are next to non-CJK
            // characters. These characters can be used to spoof Latin
            // characters or punctuation marks:
            // U+4E00 (一), U+3127 (ㄧ), U+4E28 (丨), U+4E5B (乛), U+4E03 (七),
            // U+4E05 (丅), U+5341 (十), U+3007 (〇), U+3112 (ㄒ), U+311A (ㄚ),
            // U+311F (ㄟ), U+3128 (ㄨ), U+3129 (ㄩ), U+3108 (ㄈ), U+31BA (ㆺ),
            // U+31B3 (ㆳ), U+5DE5 (工), U+31B2 (ㆲ), U+8BA0 (讠), U+4E01 (丁)
            // These characters are already blocked:
            // U+2F00 (⼀) (normalized to U+4E00), U+3192 (㆒), U+2F02 (⼂),
            // U+2F17 (⼗) and U+3038 (〸) (both normalized to U+5341 (十)).
            // Check if there is non-{Hiragana, Katagana, Han, Bopomofo} on the
            // left.
            R"([^\p{scx=kana}\p{scx=hira}\p{scx=hani}\p{scx=bopo}])"
            R"([\u4e00\u3127\u4e28\u4e5b\u4e03\u4e05\u5341\u3007\u3112)"
            R"(\u311a\u311f\u3128\u3129\u3108\u31ba\u31b3\u5dE5)"
            R"(\u31b2\u8ba0\u4e01]|)"
            // Check if there is non-{Hiragana, Katagana, Han, Bopomofo} on the
            // right.
            R"([\u4e00\u3127\u4e28\u4e5b\u4e03\u4e05\u5341\u3007\u3112)"
            R"(\u311a\u311f\u3128\u3129\u3108\u31ba\u31b3\u5de5)"
            R"(\u31b2\u8ba0\u4e01])"
            R"([^\p{scx=kana}\p{scx=hira}\p{scx=hani}\p{scx=bopo}]|)"

            // Disallow combining diacritical mark (U+0300-U+0339) after a
            // non-LGC character. Other combining diacritical marks are not in
            // the allowed character set.
            R"([^\p{scx=latn}\p{scx=grek}\p{scx=cyrl}][\u0300-\u0339]|)"

            // Disallow dotless i (U+0131) followed by a combining mark.
            R"(\u0131[\u0300-\u0339]|)"

            // Disallow combining Kana voiced sound marks.
            R"(\u3099|\u309A|)"

            // Disallow U+0307 (dot above) after 'i', 'j', 'l' or dotless i
            // (U+0131). Dotless j (U+0237) is not in the allowed set to begin
            // with.
            R"([ijl]\u0307)",
            -1, US_INV),
        0, status);
    DangerousPatternTLS().Set(dangerous_pattern);
  }
  dangerous_pattern->reset(label_string);
  if (dangerous_pattern->find()) {
    return Result::kDangerousPattern;
  }
  return Result::kSafe;
}

TopDomainEntry IDNSpoofChecker::GetSimilarTopDomain(
    std::u16string_view hostname) {
  DCHECK(!hostname.empty());
  for (const std::string& skeleton : GetSkeletons(hostname)) {
    DCHECK(!skeleton.empty());
    TopDomainEntry matching_top_domain = LookupSkeletonInTopDomains(skeleton);
    if (!matching_top_domain.domain.empty()) {
      const std::u16string top_domain =
          base::UTF8ToUTF16(matching_top_domain.domain);
      // Return an empty result if hostname is a top domain itself, or a
      // subdomain of top domain. This prevents subdomains of top domains from
      // being marked as spoofs. Without this check, éxample.blogspot.com
      // would return blogspot.com and treated as a top domain lookalike.
      if (hostname == top_domain || IsSubdomainOf(hostname, top_domain)) {
        return TopDomainEntry();
      }
      return matching_top_domain;
    }
  }
  return TopDomainEntry();
}

Skeletons IDNSpoofChecker::GetSkeletons(std::u16string_view hostname) const {
  return skeleton_generator_ ? skeleton_generator_->GetSkeletons(hostname)
                             : Skeletons();
}

TopDomainEntry IDNSpoofChecker::LookupSkeletonInTopDomains(
    const std::string& skeleton,
    SkeletonType skeleton_type) {
  DCHECK(!skeleton.empty());
  // There are no other guarantees about a skeleton string such as not including
  // a dot. Skeleton of certain characters are dots (e.g. "۰" (U+06F0)).
  TopDomainPreloadDecoder preload_decoder(
      g_trie_params.huffman_tree, g_trie_params.huffman_tree_size,
      g_trie_params.trie, g_trie_params.trie_bits,
      g_trie_params.trie_root_position);
  auto labels = base::SplitStringPiece(skeleton, ".", base::KEEP_WHITESPACE,
                                       base::SPLIT_WANT_ALL);

  if (labels.size() > kNumberOfLabelsToCheck) {
    labels.erase(labels.begin(),
                 labels.begin() + labels.size() - kNumberOfLabelsToCheck);
  }

  while (labels.size() > 0) {
    // A full skeleton needs at least two labels to match.
    if (labels.size() == 1 && skeleton_type == SkeletonType::kFull) {
      break;
    }
    std::string partial_skeleton = base::JoinString(labels, ".");
    bool match = false;
    bool decoded = preload_decoder.Decode(partial_skeleton, &match);
    DCHECK(decoded);
    if (!decoded)
      return TopDomainEntry();

    if (match)
      return preload_decoder.matching_top_domain();

    labels.erase(labels.begin());
  }
  return TopDomainEntry();
}

std::u16string IDNSpoofChecker::MaybeRemoveDiacritics(
    const std::u16string& hostname) {
  return skeleton_generator_
             ? skeleton_generator_->MaybeRemoveDiacritics(hostname)
             : hostname;
}

IDNA2008DeviationCharacter IDNSpoofChecker::GetDeviationCharacter(
    std::u16string_view hostname) const {
  if (hostname.find(u"\u00df") != std::u16string_view::npos) {
    return IDNA2008DeviationCharacter::kEszett;
  }
  if (hostname.find(u"\u03c2") != std::u16string_view::npos) {
    return IDNA2008DeviationCharacter::kGreekFinalSigma;
  }
  if (hostname.find(u"\u200d") != std::u16string_view::npos) {
    return IDNA2008DeviationCharacter::kZeroWidthJoiner;
  }
  if (hostname.find(u"\u200c") != std::u16string_view::npos) {
    return IDNA2008DeviationCharacter::kZeroWidthNonJoiner;
  }
  return IDNA2008DeviationCharacter::kNone;
}

void IDNSpoofChecker::SetAllowedUnicodeSet(UErrorCode* status) {
  if (U_FAILURE(*status))
    return;

  // The recommended set is a set of characters for identifiers in a
  // security-sensitive environment taken from UTR 39
  // (http://unicode.org/reports/tr39/) and
  // http://www.unicode.org/Public/security/latest/xidmodifications.txt .
  // The inclusion set comes from "Candidate Characters for Inclusion
  // in idenfiers" of UTR 31 (http://www.unicode.org/reports/tr31). The list
  // may change over the time and will be updated whenever the version of ICU
  // used in Chromium is updated.
  const icu::UnicodeSet* recommended_set =
      uspoof_getRecommendedUnicodeSet(status);
  icu::UnicodeSet allowed_set;
  allowed_set.addAll(*recommended_set);
  const icu::UnicodeSet* inclusion_set = uspoof_getInclusionUnicodeSet(status);
  allowed_set.addAll(*inclusion_set);

  // The sections below refer to Mozilla's IDN blacklist:
  // http://kb.mozillazine.org/Network.IDN.blacklist_chars
  //
  // U+0338 (Combining Long Solidus Overlay) is included in the recommended set,
  // but is blacklisted by Mozilla. It is dropped because it can look like a
  // slash when rendered with a broken font.
  allowed_set.remove(0x338u);
  // U+05F4 (Hebrew Punctuation Gershayim) is in the inclusion set, but is
  // blacklisted by Mozilla. We keep it, even though it can look like a double
  // quotation mark. Using it in Hebrew should be safe. When used with a
  // non-Hebrew script, it'd be filtered by other checks in place.

  // The following 5 characters are disallowed because they're in NV8 (invalid
  // in IDNA 2008).
  allowed_set.remove(0x58au);  // Armenian Hyphen
  // U+2010 (Hyphen) is in the inclusion set, but we drop it because it can be
  // confused with an ASCII U+002D (Hyphen-Minus).
  allowed_set.remove(0x2010u);
  // U+2019 is hard to notice when sitting next to a regular character.
  allowed_set.remove(0x2019u);  // Right Single Quotation Mark
  // U+2027 (Hyphenation Point) is in the inclusion set, but is blacklisted by
  // Mozilla. It is dropped, as it can be confused with U+30FB (Katakana Middle
  // Dot).
  allowed_set.remove(0x2027u);
  allowed_set.remove(0x30a0u);  // Katakana-Hiragana Double Hyphen

  // Block {Single,double}-quotation-mark look-alikes.
  allowed_set.remove(0x2bbu);  // Modifier Letter Turned Comma
  allowed_set.remove(0x2bcu);  // Modifier Letter Apostrophe

  // Block modifier letter voicing.
  allowed_set.remove(0x2ecu);

  // Block historic character Latin Kra (also blocked by Mozilla).
  allowed_set.remove(0x0138);

  // No need to block U+144A (Canadian Syllabics West-Cree P) separately
  // because it's blocked from mixing with other scripts including Latin.

#if BUILDFLAG(IS_APPLE)
  // The following characters are reported as present in the default macOS
  // system UI font, but they render as blank. Remove them from the allowed
  // set to prevent spoofing until the font issue is resolved.

  // Arabic letter KASHMIRI YEH. Not used in Arabic and Persian.
  allowed_set.remove(0x0620u);

  // Tibetan characters used for transliteration of ancient texts:
  allowed_set.remove(0x0F8Cu);
  allowed_set.remove(0x0F8Du);
  allowed_set.remove(0x0F8Eu);
  allowed_set.remove(0x0F8Fu);
#endif

  // Disallow extremely rarely used LGC character blocks.
  // Cyllic Ext A is not in the allowed set. Neither are Latin Ext-{C,E}.
  allowed_set.remove(0x01CDu, 0x01DCu);  // Latin Ext B; Pinyin
  allowed_set.remove(0x1C80u, 0x1C8Fu);  // Cyrillic Extended-C
  allowed_set.remove(0x1E00u, 0x1E9Bu);  // Latin Extended Additional
  allowed_set.remove(0x1F00u, 0x1FFFu);  // Greek Extended
  allowed_set.remove(0xA640u, 0xA69Fu);  // Cyrillic Extended-B
  allowed_set.remove(0xA720u, 0xA7FFu);  // Latin Extended-D

#if U_ICU_VERSION_MAJOR_NUM < 72
  // Unicode 15 changes ZWJ and ZWNJ from allowed to restricted. Restrict them
  // in lower versions too. This only relevant in Non-Transitional Mode as
  // Transitional Mode maps these characters out.
  // TODO(crbug.com/40879611): Remove these after ICU 72 is rolled out.
  allowed_set.remove(0x200Cu);  // Zero Width Non-Joiner
  allowed_set.remove(0x200Du);  // Zero Width Joiner
#endif

  uspoof_setAllowedUnicodeSet(checker_, &allowed_set, status);
}

bool IDNSpoofChecker::IsDigitLookalike(const icu::UnicodeString& label) {
  bool has_lookalike_char = false;
  icu::StringCharacterIterator it(label);
  for (it.setToStart(); it.hasNext();) {
    const UChar32 c = it.next32PostInc();
    if (digits_.contains(c)) {
      continue;
    }
    if (digit_lookalikes_.contains(c)) {
      has_lookalike_char = true;
      continue;
    }
    return false;
  }
  return has_lookalike_char;
}

// static
bool IDNSpoofChecker::IsWholeScriptConfusableAllowedForTLD(
    const WholeScriptConfusable& script,
    std::string_view tld,
    std::u16string_view tld_unicode) {
  icu::UnicodeString tld_string(
      false /* isTerminated */, tld_unicode.data(),
      base::checked_cast<int32_t>(tld_unicode.size()));
  // Allow if the TLD contains any letter from the script, in which case it's
  // likely to be a TLD in that script.
  if (script.all_letters->containsSome(tld_string)) {
    return true;
  }
  return base::Contains(script.allowed_tlds, tld);
}

// static
bool IDNSpoofChecker::IsLabelWholeScriptConfusableForScript(
    const WholeScriptConfusable& script,
    const icu::UnicodeString& label) {
  // Collect all the letters of |label| using |script.all_letters| and see if
  // they're a subset of |script.latin_lookalike_letters|.
  // An alternative approach is to include [0-9] and [_-] in script.all_letters
  // and checking if it contains all letters of |label|. However, this would not
  // work if a label has non-letters outside ASCII.

  icu::UnicodeSet label_characters_belonging_to_script;
  icu::StringCharacterIterator it(label);
  for (it.setToStart(); it.hasNext();) {
    const UChar32 c = it.next32PostInc();
    if (script.all_letters->contains(c)) {
      label_characters_belonging_to_script.add(c);
    }
  }
  return !label_characters_belonging_to_script.isEmpty() &&
         script.latin_lookalike_letters->containsAll(
             label_characters_belonging_to_script);
}

// static
void IDNSpoofChecker::SetTrieParamsForTesting(
    const HuffmanTrieParams& trie_params) {
  g_trie_params = trie_params;
}

// static
void IDNSpoofChecker::RestoreTrieParamsForTesting() {
  g_trie_params = HuffmanTrieParams{
      kTopDomainsHuffmanTree, sizeof(kTopDomainsHuffmanTree), kTopDomainsTrie,
      kTopDomainsTrieBits, kTopDomainsRootPosition};
}

}  // namespace url_formatter
