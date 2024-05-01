// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_IDN_SPOOF_CHECKER_H_
#define COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_IDN_SPOOF_CHECKER_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "components/url_formatter/spoof_checks/idna_metrics.h"
#include "components/url_formatter/spoof_checks/skeleton_generator.h"
#include "net/extras/preload_data/decoder.h"
#include "third_party/icu/source/common/unicode/uniset.h"
#include "third_party/icu/source/common/unicode/utypes.h"
#include "third_party/icu/source/common/unicode/uversion.h"

// 'icu' does not work. Use U_ICU_NAMESPACE.
namespace U_ICU_NAMESPACE {

class UnicodeString;

}  // namespace U_ICU_NAMESPACE

struct USpoofChecker;

namespace url_formatter {
FORWARD_DECLARE_TEST(UrlFormatterTest, IDNToUnicode);

using Skeletons = base::flat_set<std::string>;

// The |SkeletonType| and |TopDomainEntry| are mirrored in trie_entry.h. These
// are used to insert and read nodes from the Trie.
// The type of skeleton in the trie node.
enum SkeletonType {
  // The skeleton represents the full domain (e.g. google.corn).
  kFull = 0,
  // The skeleton represents the domain with '.'s and '-'s removed (e.g.
  // googlecorn).
  kSeparatorsRemoved = 1,
  // Max value used to determine the number of different types. Update this and
  // |kSkeletonTypeBitLength| when new SkeletonTypes are added.
  kMaxValue = kSeparatorsRemoved
};

const uint8_t kSkeletonTypeBitLength = 1;

// Represents a top domain entry in the trie.
struct TopDomainEntry {
  // The domain in ASCII (punycode for IDN).
  std::string domain;
  // True if the domain is in the top bucket (i.e. in the most popular subset of
  // top domains). These domains can have additional skeletons associated with
  // them.
  bool is_top_bucket = false;
  // Type of the skeleton stored in the trie node.
  SkeletonType skeleton_type;
};

// A helper class for IDN Spoof checking, used to ensure that no IDN input is
// spoofable per Chromium's standard of spoofability. For a more thorough
// explanation of how spoof checking works in Chromium, see
// http://dev.chromium.org/developers/design-documents/idn-in-google-chrome .
class IDNSpoofChecker {
 public:
  struct HuffmanTrieParams {
    const uint8_t* huffman_tree;
    size_t huffman_tree_size;
    const uint8_t* trie;
    size_t trie_bits;
    size_t trie_root_position;
  };

  enum class Result {
    // Spoof checks weren't performed because the domain wasn't IDN. Should
    // never be returned from SafeToDisplayAsUnicode.
    kNone,
    // The domain passed all spoof checks.
    kSafe,
    // Failed ICU's standard spoof checks such as Greek mixing with Latin.
    kICUSpoofChecks,
    // Domain contains deviation characters.
    kDeviationCharacters,
    // Domain contains characters that are only allowed for certain TLDs, such
    // as thorn (þ) used outside Icelandic.
    kTLDSpecificCharacters,
    // Domain has an unsafe middle dot.
    kUnsafeMiddleDot,
    // Domain is composed of only Latin-like characters from non Latin scripts.
    // E.g. apple.com but apple in Cyrillic (xn--80ak6aa92e.com).
    kWholeScriptConfusable,
    // Domain is composed of only characters that look like digits.
    kDigitLookalikes,
    // Domain mixes Non-ASCII Latin with Non-Latin characters.
    kNonAsciiLatinCharMixedWithNonLatin,
    // Domain contains dangerous patterns that are mostly found when mixing
    // Latin and CJK scripts. E.g. Katakana iteration mark (U+30FD) not preceded
    // by Katakana.
    kDangerousPattern,
  };

  IDNSpoofChecker();
  ~IDNSpoofChecker();
  IDNSpoofChecker(const IDNSpoofChecker&) = delete;
  IDNSpoofChecker& operator=(const IDNSpoofChecker&) = delete;

  // Returns kSafe if |label| is safe to display as Unicode. Some of the checks
  // depend on the TLD of the full domain name, so this function also takes
  // the ASCII (including punycode) TLD in |top_level_domain| and its unicode
  // version in |top_level_domain_unicode|.
  // This method doesn't check for similarity to a top domain: If the input
  // matches a top domain but is otherwise safe (e.g. googlé.com), the result
  // will be kSafe.
  // In the event of library failure, all IDN inputs will be treated as unsafe
  // and the return value will be kICUSpoofChecks.
  // See the function body for details on the specific safety checks performed.
  // |top_level_domain_unicode| can be passed as empty if |top_level_domain| is
  // not well formed punycode.
  // Example usages:
  // - SafeToDisplayAsUnicode(L"google", "com", "com") -> kSafe
  // - SafeToDisplayAsUnicode(L"аррӏе", "com", "com") -> kWholeScriptConfusable
  // - SafeToDisplayAsUnicode(L"аррӏе", "xn--p1ai", "рф") -> kSafe (xn--p1ai is
  //   the punycode form of рф)
  Result SafeToDisplayAsUnicode(std::u16string_view label,
                                std::string_view top_level_domain,
                                std::u16string_view top_level_domain_unicode);

  // Returns the matching top domain if |hostname| or the last few components of
  // |hostname| looks similar to one of top domains listed in domains.list.
  // Returns empty result if |hostname| is a top domain itself, or is a
  // subdomain of a top domain.
  // Two checks are done:
  //   1. Calculate the skeleton of |hostname| based on the Unicode confusable
  //   character list and look it up in the pre-calculated skeleton list of
  //   top domains.
  //   2. Look up the diacritic-free version of |hostname| in the list of
  //   top domains. Note that non-IDN hostnames will not get here.
  TopDomainEntry GetSimilarTopDomain(std::u16string_view hostname);

  // Returns skeleton strings computed from |hostname|. This function can apply
  // extra mappings to some characters to produce multiple skeletons.
  Skeletons GetSkeletons(std::u16string_view hostname) const;

  // Returns a top domain from the top 10K list matching the given |skeleton|.
  // If |without_separators| is set, the skeleton will be compared against
  // skeletons without '.' and '-'s as well.
  TopDomainEntry LookupSkeletonInTopDomains(
      const std::string& skeleton,
      SkeletonType skeleton_type = SkeletonType::kFull);

  // Removes diacritics from |hostname| and returns the new string if the input
  // only contains Latin-Greek-Cyrillic characters. Otherwise, returns the
  // input string.
  std::u16string MaybeRemoveDiacritics(const std::u16string& hostname);

  // Returns the first IDNA 2008 deviation character if `hostname` contains any.
  // Deviation characters are four characters that are treated differently
  // between IDNA 2003 and IDNA 2008: ß, ς, ZERO WIDTH JOINER, ZERO WIDTH
  // NON-JOINER.
  // As a result, a domain containing deviation characters can map to a
  // different IP address between user agents that implement different IDNA
  // versions.
  // See
  // https://www.unicode.org/reports/tr46/tr46-27.html#Table_Deviation_Characters
  // for details.
  IDNA2008DeviationCharacter GetDeviationCharacter(
      std::u16string_view hostname) const;

  // Used for unit tests.
  static void SetTrieParamsForTesting(const HuffmanTrieParams& trie_params);
  static void RestoreTrieParamsForTesting();

 private:
  // Store information about various language scripts whose letters can be used
  // to make whole-script-confusable spoofs (e.g. ѕсоре[.]com where all letters
  // in ѕсоре are Cyrillic).
  struct WholeScriptConfusable {
    WholeScriptConfusable(
        std::unique_ptr<icu::UnicodeSet> arg_all_letters,
        std::unique_ptr<icu::UnicodeSet> arg_latin_lookalike_letters,
        const std::vector<std::string>& allowed_tlds);
    ~WholeScriptConfusable();

    // Captures all letters belonging to this script. See kScriptNameCodeList in
    // blink/renderer/platform/text/locale_to_script_mapping.cc for script
    // codes.
    std::unique_ptr<icu::UnicodeSet> all_letters;
    // The subset of all_letters that look like Latin ASCII letters. A domain
    // label entirely made of them is blocked as a simplified
    // whole-script-spoofable, unless the TLD of the domain is explicitly
    // allowed by |allowed_tlds|.
    std::unique_ptr<icu::UnicodeSet> latin_lookalike_letters;
    // List of top level domains where whole-script-confusable domains are
    // allowed for this script.
    const std::vector<std::string> allowed_tlds;
  };

  // Returns true if all the letters belonging to |script| in |label| also
  // belong to a set of Latin lookalike letters for that script.
  static bool IsLabelWholeScriptConfusableForScript(
      const WholeScriptConfusable& script,
      const icu::UnicodeString& label);
  // Returns true if |tld| is a top level domain most likely to contain a large
  // number of domains in |script| (as in, written script). |tld_unicode| can be
  // empty if |tld| is not well formed punycode.
  static bool IsWholeScriptConfusableAllowedForTLD(
      const WholeScriptConfusable& script,
      std::string_view tld,
      std::u16string_view tld_unicode);

  // Sets allowed characters in IDN labels and turns on USPOOF_CHAR_LIMIT.
  void SetAllowedUnicodeSet(UErrorCode* status);

  // Returns true if the string is entirely made up of either digits or
  // characters that look like digits (but not exclusively actual digits).
  bool IsDigitLookalike(const icu::UnicodeString& label);

  raw_ptr<USpoofChecker, DanglingUntriaged> checker_;
  icu::UnicodeSet deviation_characters_;
  icu::UnicodeSet non_ascii_latin_letters_;
  icu::UnicodeSet kana_letters_exceptions_;
  icu::UnicodeSet combining_diacritics_exceptions_;
  icu::UnicodeSet digits_;
  icu::UnicodeSet digit_lookalikes_;
  icu::UnicodeSet icelandic_characters_;

  // skeleton_generator_ may be null if uspoof_open fails. It's unclear why this
  // happens, see crbug.com/1169079.
  std::unique_ptr<SkeletonGenerator> skeleton_generator_;

  // List of scripts containing whole-script-confusable information.
  std::vector<std::unique_ptr<WholeScriptConfusable>> wholescriptconfusables_;
};

}  // namespace url_formatter

#endif  // COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_IDN_SPOOF_CHECKER_H_
