// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_formatter/spoof_checks/skeleton_generator.h"

#include "base/memory/ptr_util.h"
#include "base/strings/string_piece.h"
#include "third_party/icu/source/i18n/unicode/regex.h"
#include "third_party/icu/source/i18n/unicode/translit.h"
#include "third_party/icu/source/i18n/unicode/uspoof.h"

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
  // NOTE: Adding a digit-lookalike? Add it to digit_lookalikes_ in
  // idn_spoof_checker.cc, too.
  //   - {U+00E6 (æ), U+04D5 (ӕ)}  => "ae"
  //   - {U+03FC (ϼ), U+048F (ҏ)} => p
  //   - {U+0127 (ħ), U+043D (н), U+045B (ћ), U+04A3 (ң), U+04A5 (ҥ),
  //      U+04C8 (ӈ), U+04CA (ӊ), U+050B (ԋ), U+0527 (ԧ), U+0529 (ԩ)} => h
  //   - {U+0138 (ĸ), U+03BA (κ), U+043A (к), U+049B (қ), U+049D (ҝ),
  //      U+049F (ҟ), U+04A1(ҡ), U+04C4 (ӄ), U+051F (ԟ)} => k
  //   - {U+014B (ŋ), U+043F (п), U+0525 (ԥ), U+0E01 (ก), U+05D7 (ח)} => n
  //   - U+0153 (œ) => "ce"
  // TODO(crbug/843352): Handle multiple skeletons for U+0525 and U+0153.
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
  //      U+0B66 (୦), U+0CE6 (೦)} => o,
  //   - {U+09ED (৭), U+0A67 (੧), U+0AE7 (૧)} => q,
  //   - {U+0E1A (บ), U+0E9A (ບ)} => u,
  //   - {U+03B8 (θ)} => 0,
  //   - {U+0968 (२), U+09E8 (২), U+0A68 (੨), U+0A68 (੨), U+0AE8 (૨),
  //      U+0ce9 (೩), U+0ced (೭), U+0577 (շ)} => 2,
  //   - {U+0437 (з), U+0499 (ҙ), U+04E1 (ӡ), U+0909 (उ), U+0993 (ও),
  //      U+0A24 (ਤ), U+0A69 (੩), U+0AE9 (૩), U+0C69 (౩),
  //      U+1012 (ဒ), U+10D5 (ვ), U+10DE (პ)} => 3
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
              "[ĸκкқҝҟҡӄԟ] > k; [ŋпԥกח] > n; œ > ce;"
              "[ŧтҭԏ七丅丆丁] > t; [ƅьҍв] > b;  [ωшщพฟພຟ] > w;"
              "[мӎ] > m; [єҽҿၔ] > e; ґ > r; [ғӻ] > f;"
              "[ҫင] > c; [ұ丫] > y; [χҳӽӿ乂] > x;"
              "[ԃძ]  > d; [ԍဌ] > g; [ടรຣຮ] > s; ၂ > j;"
              "[०০੦૦ଠ୦೦] > o;"
              "[৭੧૧] > q;"
              "[บບ] > u;"
              "[θ] > 0;"
              "[२২੨੨૨೩೭շ] > 2;"
              "[зҙӡउওਤ੩૩౩ဒვპ] > 3;"
              "[੫丩ㄐ] > 4;"
              "[ճ] > 6;"
              "[৪੪୫] > 8;"
              "[૭୨౨] > 9;"
              "[—一―⸺⸻] > \\-;"),
          UTRANS_FORWARD, parse_error, status));
  DCHECK(U_SUCCESS(status))
      << "Skeleton generator initialization failed due to an error: "
      << u_errorName(status);
}

SkeletonGenerator::~SkeletonGenerator() = default;

Skeletons SkeletonGenerator::GetSkeletons(base::StringPiece16 hostname) {
  Skeletons skeletons;
  size_t hostname_length = hostname.length() - (hostname.back() == '.' ? 1 : 0);
  icu::UnicodeString host(false, hostname.data(), hostname_length);
  // If input has any characters outside Latin-Greek-Cyrillic and [0-9._-],
  // there is no point in getting rid of diacritics because combining marks
  // attached to non-LGC characters are already blocked.
  if (ShouldRemoveDiacriticsFromLabel(host))
    diacritic_remover_->transliterate(host);
  extra_confusable_mapper_->transliterate(host);

  UErrorCode status = U_ZERO_ERROR;
  icu::UnicodeString ustr_skeleton;

  // Map U+04CF (ӏ) to lowercase L in addition to what uspoof_getSkeleton does
  // (mapping it to lowercase I).
  AddSkeletonMapping(host, 0x4CF /* ӏ */, 0x6C /* lowercase L */, &skeletons);

  uspoof_getSkeletonUnicodeString(checker_, 0, host, ustr_skeleton, &status);
  if (U_SUCCESS(status)) {
    std::string skeleton;
    ustr_skeleton.toUTF8String(skeleton);
    skeletons.insert(skeleton);
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
