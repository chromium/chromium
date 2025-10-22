// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_IDN_SPOOF_CHECKER_TYPES_H_
#define COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_IDN_SPOOF_CHECKER_TYPES_H_

#include <stdint.h>

#include <string>

namespace url_formatter {

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

inline constexpr uint8_t kSkeletonTypeBitLength = 1;

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

enum class IDNSpoofCheckerResult {
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

}  // namespace url_formatter

#endif  // COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_IDN_SPOOF_CHECKER_TYPES_H_
