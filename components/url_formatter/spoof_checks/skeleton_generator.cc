// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/url_formatter/spoof_checks/skeleton_generator.h"

#include <ostream>
#include <queue>
#include <string_view>

#include "base/i18n/unicodestring.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/icu/source/i18n/unicode/regex.h"
#include "third_party/icu/source/i18n/unicode/translit.h"
#include "third_party/icu/source/i18n/unicode/uspoof.h"

namespace {

using QueueItem = std::vector<std::u16string>;

// Maximum length of a hostname whose supplemental hostnames we'll calculate.
// For hostnames longer than this length, the supplemental hostnames will be
// empty.
const size_t kMaxHostnameLengthToComputeSupplementalHostnames = 32;

// Maximum number of supplemental hostname to generate for a given input.
// If this number is too high, we may end up DOSing the browser process.
// If it's too low, we may not be able to cover some lookalike URLs.
const size_t kMaxSupplementalHostnames = 128;

// Maximum number of characters with multiple skeletons in a hostname (i.e.
// interesting characters). The number of interesting characters directly affect
// how many supplemental hostnames are generated. Assuming an interesting
// character has 3 skeletons (1 original skeleton, 2 supplemental skeletons),
// this will generate pow(3, kMaxCharactersWithMultipleSkeletons) supplemental
// hostnames, so we cap it.
// If a hostname has too many interesting characters, it's unlikely to be a
// convincing spoof.
const size_t kMaxCharactersWithMultipleSkeletons = 5;

// Limit the number of maximum supplemental skeletons for a given character to a
// reasonable number. This can be adjusted in the future as needed.
const size_t kMaxSupplementalSkeletonsPerCharacter = 3;

}  // namespace

SkeletonGenerator::SkeletonGenerator(const USpoofChecker* checker)
    : checker_(checker) {
  UErrorCode status = U_ZERO_ERROR;
  // Used for diacritics-removal before the skeleton calculation. Add
  // "ł > l; ø > o; đ > d" that are not handled by "NFD; Nonspacing mark
  // removal; NFC".
  // TODO(jshin): Revisit "ł > l; ø > o" mapping.
  UParseError parse_error;
  diacritic_remover_ = base::WrapUnique(icu::Transliterator::createFromRules(
      UNICODE_STRING_SIMPLE("DropAcc"),
      icu::UnicodeString::fromUTF8("::NFD; ::[:Nonspacing Mark:] Remove; ::NFC;"
                                   " ł > l; ø > o; đ > d;"),
      UTRANS_FORWARD, parse_error, status));

  // This set is used to determine whether or not to apply a slow
  // transliteration to remove diacritics to a given hostname before the
  // confusable skeleton calculation for comparison with top domain names. If
  // it has any character outside the set, the expensive step will be skipped
  // because it cannot match any of top domain names.
  // The last ([\u0300-\u0339] is a shorthand for "[:Identifier_Status=Allowed:]
  // & [:Script_Extensions=Inherited:] - [\\u200C\\u200D]". The latter is a
  // subset of the former but it does not matter because hostnames with
  // characters outside the latter set would be rejected in an earlier step.
  lgc_letters_n_ascii_ = icu::UnicodeSet(
      UNICODE_STRING_SIMPLE("[[:Latin:][:Greek:][:Cyrillic:][0-9\\u002e_"
                            "\\u002d][\\u0300-\\u0339]]"),
      status);
  lgc_letters_n_ascii_.freeze();

  // Supplement the Unicode confusable list by the following mapping.
  // IMPORTANT: Adding a digit-lookalike? Add it to digit_lookalikes_ in
  // idn_spoof_checker.cc, too.
  //   - {U+00E6 (æ), U+04D5 (ӕ)}  => "ae"
  //   - {U+03FC (ϼ), U+048F (ҏ)} => p
  //   - {U+0127 (ħ), U+043D (н), U+045B (ћ), U+04A3 (ң), U+04A5 (ҥ),
  //      U+04C8 (ӈ), U+04CA (ӊ), U+050B (ԋ), U+0527 (ԧ), U+0529 (ԩ)} => h
  //   - {U+0138 (ĸ), U+03BA (κ), U+043A (к), U+049B (қ), U+049D (ҝ),
  //      U+049F (ҟ), U+04A1(ҡ), U+04C4 (ӄ), U+051F (ԟ)} => k
  //   - {U+014B (ŋ), U+043F (п), U+0525 (ԥ), U+0E01 (ก), U+05D7 (ח)} => n
  // TODO(crbug.com/40091387): Handle multiple skeletons for U+0525 and U+0153.
  //   - {U+0167 (ŧ), U+0442 (т), U+04AD (ҭ), U+050F (ԏ), U+4E03 (七),
  //     U+4E05 (丅), U+4E06 (丆), U+4E01 (丁)} => t
  //   - {U+0185 (ƅ), U+044C (ь), U+048D (ҍ), U+0432 (в)} => b
  //   - {U+03C9 (ω), U+0448 (ш), U+0449 (щ), U+0E1E (พ),
  //      U+0E1F (ฟ), U+0E9E (ພ), U+0E9F (ຟ)} => w
  //   - {U+043C (м), U+04CE (ӎ)} => m
  //   - {U+0454 (є), U+04BD (ҽ), U+04BF (ҿ), U+1054 (ၔ)} => e
  //   - U+0491 (ґ) => r
  //   - {U+0493 (ғ), U+04FB (ӻ)} => f
  //   - {U+04AB (ҫ), U+1004 (င)} => c
  //   - {U+04B1 (ұ), U+4E2B (丫)} => y
  //   - {U+03C7 (χ), U+04B3 (ҳ), U+04FD (ӽ), U+04FF (ӿ), U+4E42 (乂)} => x
  //   - {U+0503 (ԃ), U+10EB (ძ)} => d
  //   - {U+050D (ԍ), U+100c (ဌ)} => g
  //   - {U+0D1F (ട), U+0E23 (ร), U+0EA3 (ຣ), U+0EAE (ຮ)} => s
  //   - U+1042 (၂) => j
  //   - {U+0966 (०), U+09E6 (০), U+0A66 (੦), U+0AE6 (૦), U+0B30 (ଠ),
  //      U+0B66 (୦), U+0CE6 (೦), U+1005 (စ)} => o,
  //   - {U+09ED (৭), U+0A67 (੧), U+0AE7 (૧)} => q,
  //   - {U+0E1A (บ), U+0E9A (ບ)} => u,
  //   - {U+03B8 (θ)} => 0,
  //   - {U+0968 (२), U+09E8 (২), U+0A68 (੨), U+0A68 (੨), U+0AE8 (૨),
  //      U+0ce9 (೩), U+0ced (೭), U+0577 (շ)} => 2,
  //   - {U+0437 (з), U+0499 (ҙ), U+04E1 (ӡ), U+0909 (उ), U+0993 (ও),
  //      U+0A24 (ਤ), U+0A69 (੩), U+0AE9 (૩), U+0C69 (౩),
  //      U+1012 (ဒ), U+10D5 (ვ), U+10DE (პ), U+0A5C (ੜ), U+10D9 (კ)} => 3
  //   - {U+0A6B (੫), U+4E29 (丩), U+3110 (ㄐ)} => 4,
  //   - U+0573 (ճ) => 6
  //   - {U+09EA (৪), U+0A6A (੪), U+0b6b (୫)} => 8,
  //   - {U+0AED (૭), U+0b68 (୨), U+0C68 (౨)} => 9,
  //   Map a few dashes that ICU doesn't map. These are already blocked by ICU,
  //   but mapping them allows us to detect same skeletons.
  //   - {U+2014 (—), U+4E00 (一), U+2015 (―), U+23EA (⸺), U+2E3B (⸻)} => -,
  extra_confusable_mapper_ =
      base::WrapUnique(icu::Transliterator::createFromRules(
          UNICODE_STRING_SIMPLE("ExtraConf"),
          icu::UnicodeString::fromUTF8(
              "[æӕ] > ae; [ϼҏ] > p; [ħнћңҥӈӊԋԧԩ] > h;"
              "[ĸκкқҝҟҡӄԟ] > k; [ŋпԥกח] > n;"
              "[ŧтҭԏ七丅丆丁] > t; [ƅьҍвß] > b;  [ωшщพฟພຟ] > w;"
              "[мӎ] > m; [єҽҿၔ] > e; ґ > r; [ғӻ] > f;"
              "[ҫင] > c; [ұ丫] > y; [χҳӽӿ乂] > x;"
              "[ԃძ]  > d; [ԍဌ] > g; [ടรຣຮ] > s; ၂ > j;"
              "[०০੦૦ଠ୦೦စ] > o;"
              "[৭੧૧] > q;"
              "[บບ] > u;"
              "[θ] > 0;"
              "[२২੨੨૨೩೭շ] > 2;"
              "[зҙӡउওਤ੩૩౩ဒვპੜკ] > 3;"
              "[੫丩ㄐ] > 4;"
              "[ճ] > 6;"
              "[৪੪୫] > 8;"
              "[૭୨౨] > 9;"
              "[—一―⸺⸻] > \\-;"),
          UTRANS_FORWARD, parse_error, status));
  DCHECK(U_SUCCESS(status))
      << "Skeleton generator initialization failed due to an error: "
      << u_errorName(status);

  // Characters that look like multiple characters.
  character_map_[u'þ'] = {"b", "p"};
  character_map_[u'œ'] = {"ce", "oe"};
  // https://crbug.com/1250993:
  character_map_[u'ł'] = {"l", "t"};

  // Find the characters with diacritics that have multiple skeletons.
  for (const auto& it : character_map_) {
    std::u16string char_str(1, it.first);
    if (char_str != MaybeRemoveDiacritics(char_str)) {
      characters_with_multiple_skeletons_with_diacritics_.insert(it.first);
    }
  }
}

SkeletonGenerator::~SkeletonGenerator() = default;

void SkeletonGenerator::MaybeRemoveDiacritics(icu::UnicodeString& hostname) {
  // If input has any characters outside Latin-Greek-Cyrillic and [0-9._-],
  // there is no point in getting rid of diacritics because combining marks
  // attached to non-LGC characters are already blocked.
  if (ShouldRemoveDiacriticsFromLabel(hostname))
    diacritic_remover_->transliterate(hostname);
}

std::u16string SkeletonGenerator::MaybeRemoveDiacritics(
    std::u16string_view hostname) {
  size_t hostname_length = hostname.length() - (hostname.back() == '.' ? 1 : 0);
  icu::UnicodeString host(false, hostname.data(), hostname_length);
  MaybeRemoveDiacritics(host);
  return base::i18n::UnicodeStringToString16(host);
}

bool SkeletonGenerator::ShouldComputeSupplementalHostnamesWithDiacritics(
    std::u16string_view input_hostname) const {
  for (const char16_t c : characters_with_multiple_skeletons_with_diacritics_) {
    if (input_hostname.find(c) != std::u16string_view::npos) {
      return true;
    }
  }
  return false;
}

Skeletons SkeletonGenerator::GetSkeletons(std::u16string_view input_hostname) {
  // Generate supplemental hostnames for the input hostname with and without
  // diacritics. We do this to cover characters whose diacritic versions can
  // look like completely other characters, such as LATIN SMALL LETTER L WITH
  // STROKE (ł) looking like t. By doing this, we can generate multiple
  // skeletons for ł (l and t).
  //
  // Ideally, we'd compute a hostname variant for each character with and
  // without its diacritic. That would result in 2^n hostname variants where n
  // is the number of characters in the hostname with diacritics, which is too
  // expensive. Currently, there is only one character with a diacritic that has
  // multiple skeletons (ł), so this isn't needed.
  std::u16string hostname_no_diacritics = MaybeRemoveDiacritics(input_hostname);
  base::flat_set<std::u16string> all_variants = GenerateSupplementalHostnames(
      hostname_no_diacritics, kMaxSupplementalHostnames, character_map_);
  if (ShouldComputeSupplementalHostnamesWithDiacritics(input_hostname)) {
    base::flat_set<std::u16string> diacritic_variants =
        GenerateSupplementalHostnames(input_hostname, kMaxSupplementalHostnames,
                                      character_map_);
    all_variants.insert(diacritic_variants.begin(), diacritic_variants.end());
  }

  // Extract skeletons of all hostname variants.
  Skeletons skeletons;
  for (const std::u16string& hostname : all_variants) {
    size_t hostname_length =
        hostname.length() - (hostname.back() == '.' ? 1 : 0);
    icu::UnicodeString hostname_unicode(false, hostname.data(),
                                        hostname_length);
    extra_confusable_mapper_->transliterate(hostname_unicode);

    UErrorCode status = U_ZERO_ERROR;
    icu::UnicodeString ustr_skeleton;

    // Map U+04CF (ӏ) to lowercase L in addition to what uspoof_getSkeleton does
    // (mapping it to lowercase I).
    AddSkeletonMapping(hostname_unicode, 0x4CF /* ӏ */, 0x6C /* lowercase L */,
                       &skeletons);

    uspoof_getSkeletonUnicodeString(checker_, 0, hostname_unicode,
                                    ustr_skeleton, &status);
    if (U_SUCCESS(status)) {
      std::string skeleton;
      ustr_skeleton.toUTF8String(skeleton);
      skeletons.insert(skeleton);
    }
  }
  return skeletons;
}

bool SkeletonGenerator::ShouldRemoveDiacriticsFromLabel(
    const icu::UnicodeString& label) const {
  return lgc_letters_n_ascii_.containsAll(label);
}

void SkeletonGenerator::AddSkeletonMapping(const icu::UnicodeString& host,
                                           int32_t src_char,
                                           int32_t mapped_char,
                                           Skeletons* skeletons) {
  int32_t src_pos = host.indexOf(src_char);
  if (src_pos == -1) {
    return;
  }
  icu::UnicodeString host_alt(host);
  size_t length = host_alt.length();
  char16_t* buffer = host_alt.getBuffer(-1);
  for (char16_t* uc = buffer + src_pos; uc < buffer + length; ++uc) {
    if (*uc == src_char)
      *uc = mapped_char;
  }
  host_alt.releaseBuffer(length);
  UErrorCode status = U_ZERO_ERROR;
  icu::UnicodeString ustr_skeleton;
  uspoof_getSkeletonUnicodeString(checker_, 0, host_alt, ustr_skeleton,
                                  &status);
  if (U_SUCCESS(status)) {
    std::string skeleton;
    ustr_skeleton.toUTF8String(skeleton);
    skeletons->insert(skeleton);
  }
}

// static
base::flat_set<std::u16string> SkeletonGenerator::GenerateSupplementalHostnames(
    std::u16string_view input,
    size_t max_alternatives,
    const SkeletonMap& mapping) {
  base::flat_set<std::u16string> output;
  if (!input.size() ||
      input.size() > kMaxHostnameLengthToComputeSupplementalHostnames ||
      max_alternatives == 0) {
    return output;
  }
  icu::UnicodeString input_unicode =
      icu::UnicodeString::fromUTF8(base::UTF16ToUTF8(input));
  // Read only buffer, doesn't need to be released.
  const char16_t* input_buffer = input_unicode.getBuffer();
  const size_t input_length = static_cast<size_t>(input_unicode.length());

  // Count the characters that have multiple skeletons. If this number is high,
  // bail out early to avoid running the skeleton generation for too long.
  size_t characters_with_multiple_skeletons = 0;
  for (size_t i = 0; i < input_length; i++) {
    char16_t c = input_buffer[i];
    const auto it = mapping.find(c);
    if (it != mapping.end()) {
      characters_with_multiple_skeletons++;
    }
  }
  if (characters_with_multiple_skeletons >
      kMaxCharactersWithMultipleSkeletons) {
    return output;
  }

  // This queue contains vectors of skeleton strings. For each character in
  // the input string, its skeleton string will be appended to the queue item.
  // Thus, the number of skeleton strings in the queue item will always
  // correspond to the index of the input string processed so far.
  std::queue<QueueItem> q;
  q.push(QueueItem());

  while (!q.empty()) {
    QueueItem current = q.front();
    q.pop();

    if (current.size() == input_length) {
      // Reached the end of the original string. We now generated a complete
      // alternative string. Add the result to output.
      output.insert(base::JoinString(current, u""));
      if (output.size() == max_alternatives) {
        break;
      }
      continue;
    }

    // First, add the original character from input.
    char16_t c = input_buffer[current.size()];
    QueueItem new_item1 = current;
    new_item1.push_back(std::u16string(1, c));
    q.push(new_item1);

    // Then, find all alternative characters for the current input character and
    // generate new alternative strings by appending each alternative character
    // to the string generated so far.
    const auto it = mapping.find(c);
    if (it != mapping.end()) {
      DCHECK_LE(it->second.size(), kMaxSupplementalSkeletonsPerCharacter);
      for (auto alternative : it->second) {
        QueueItem new_item2 = current;
        new_item2.push_back(base::UTF8ToUTF16(alternative));
        q.push(new_item2);
      }
    }
  }
  return output;
}
