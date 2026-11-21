// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_formatter/spoof_checks/skeleton_generator.h"

#include <ostream>
#include <queue>
#include <string_view>

#include "base/compiler_specific.h"
#include "base/i18n/unicodestring.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/icu/source/i18n/unicode/regex.h"
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
  // removal; NFC" (defined in `//tools/crates/icu4x_datagen/config.toml` and
  // `//base/i18n/transliterator.cc`).
  // TODO(jshin): Revisit "ł > l; ø > o" mapping.
  diacritic_remover_ = base::i18n::CreateTransliterator("und-t-und-x0-dropacc");

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

  // Supplement the Unicode confusable list with additional URL spoof-check
  // mappings. The rules and per-character comments live in
  // `//third_party/rust/icu4x_data/transliterators/extra_conf.txt` (long-term
  // home) and `//base/i18n/transliterator.cc` (while the ICU4C fallback is
  // active).
  // IMPORTANT: Adding a digit-lookalike? Add it to `digit_lookalikes_` in
  // `idn_spoof_checker.cc`, too.
  extra_confusable_mapper_ =
      base::i18n::CreateTransliterator("und-t-und-x0-xconf");
  DCHECK(extra_confusable_mapper_ != nullptr)
      << "Skeleton generator initialization failed due to an error: ";

  // Characters that look like multiple characters.
  character_map_[u'þ'] = {"b", "p"};
  character_map_[u'œ'] = {"ce", "oe"};
  // https://crbug.com/1250993:
  character_map_[u'ł'] = {"l", "t"};
  // https://crbug.com/40072736:
  character_map_[u'ı'] = {"l", "i"};
  // https://crbug.com/563001657:
  character_map_[u'ɩ'] = {"l", "i"};

  // Find the characters with diacritics that have multiple skeletons.
  for (const auto& it : character_map_) {
    std::u16string char_str(1, it.first);
    if (char_str != MaybeRemoveDiacritics(char_str)) {
      characters_with_multiple_skeletons_with_diacritics_.insert(it.first);
    }
  }
}

SkeletonGenerator::~SkeletonGenerator() = default;

std::u16string SkeletonGenerator::MaybeRemoveDiacritics(
    std::u16string_view hostname) {
  size_t hostname_length = hostname.length() - (hostname.back() == '.' ? 1 : 0);
  std::u16string_view host(hostname.data(), hostname_length);
  // If input has any characters outside Latin-Greek-Cyrillic and [0-9._-],
  // there is no point in getting rid of diacritics because combining marks
  // attached to non-LGC characters are already blocked.
  if (ShouldRemoveDiacriticsFromLabel(
          icu::UnicodeString(host.data(), host.length()))) {
    return diacritic_remover_->Transliterate(host);
  }
  return std::u16string(host.data(), host.length());
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
    std::u16string transliterated = extra_confusable_mapper_->Transliterate(
        std::u16string_view(hostname.data(), hostname_length));
    icu::UnicodeString hostname_unicode(transliterated.data(),
                                        transliterated.length());

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
  for (char16_t* uc = UNSAFE_TODO(buffer + src_pos);
       uc < UNSAFE_TODO(buffer + length); UNSAFE_TODO(++uc)) {
    if (*uc == src_char) {
      *uc = mapped_char;
    }
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
    char16_t c = UNSAFE_TODO(input_buffer[i]);
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
  q.emplace();

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
    char16_t c = UNSAFE_TODO(input_buffer[current.size()]);
    QueueItem new_item1 = current;
    new_item1.emplace_back(1, c);
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
