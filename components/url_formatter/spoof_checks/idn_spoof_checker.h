// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_IDN_SPOOF_CHECKER_H_
#define COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_IDN_SPOOF_CHECKER_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_span.h"
#include "components/url_formatter/spoof_checks/idn_spoof_checker_types.h"
#include "third_party/icu/source/common/unicode/uniset.h"
#include "third_party/icu/source/common/unicode/utypes.h"
#include "third_party/icu/source/common/unicode/uversion.h"
#include "url/gurl.h"

// 'icu' does not work. Use U_ICU_NAMESPACE.
namespace U_ICU_NAMESPACE {

class UnicodeString;

}  // namespace U_ICU_NAMESPACE

class SkeletonGenerator;
struct USpoofChecker;

namespace url_formatter {

FORWARD_DECLARE_TEST(UrlFormatterTest, IDNToUnicode);

using Skeletons = base::flat_set<std::string>;

// A helper class for IDN Spoof checking, used to ensure that no IDN input is
// spoofable per Chromium's standard of spoofability. For a more thorough
// explanation of how spoof checking works in Chromium, see
// http://dev.chromium.org/developers/design-documents/idn-in-google-chrome .
class IDNSpoofChecker {
 public:
  struct HuffmanTrieParams {
    ~HuffmanTrieParams();
    base::raw_span<const uint8_t> huffman_tree;
    base::raw_span<const uint8_t> trie;
    size_t trie_bits;
    size_t trie_root_position;
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
  IDNSpoofCheckerResult SafeToDisplayAsUnicode(
      std::u16string_view label,
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

  // Returns true if the domain represented by |url| is one of the top domains
  // listed in domains.list or is a subdomain of one of the top domains.
  bool IsTopDomain(const GURL& url);

  // Checks if the given |domain_and_registry| string (representing the
  // registrable domain, or eTLD+1) is one of the top domains listed in
  // domains.list or is a subdomain of one of the top domains. This functions
  // calculates the skeleton of |domain_and_registry| and looks it up in the
  // pre-calculated skeleton list of top domains.
  bool IsDomainAndRegistryATopDomain(const std::string& domain_and_registry);

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
