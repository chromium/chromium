// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_regexes.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/i18n/unicodestring.h"
#include "base/no_destructor.h"

namespace {

// Maximum length of the string to match to avoid causing an icu::RegexMatcher
// stack overflow. (crbug.com/1198219)
constexpr int kMaxStringLength = 5000;

}  // namespace

namespace autofill {

AutofillRegexes::AutofillRegexes(ThreadSafe thread_safe)
    : thread_safe_(thread_safe) {
  if (!thread_safe_)
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

AutofillRegexes::~AutofillRegexes() {
  if (!thread_safe_)
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

icu::RegexMatcher* AutofillRegexes::GetMatcher(
    const base::StringPiece16& pattern) {
  if (!thread_safe_)
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = matchers_.find(pattern);
  if (it == matchers_.end()) {
    const icu::UnicodeString icu_pattern(false, pattern.data(),
                                         pattern.length());

    UErrorCode status = U_ZERO_ERROR;
    auto matcher = std::make_unique<icu::RegexMatcher>(
        icu_pattern, UREGEX_CASE_INSENSITIVE, status);

    auto result = matchers_.insert(std::make_pair(pattern, std::move(matcher)));
    DCHECK(result.second);
    it = result.first;
    DCHECK(U_SUCCESS(status));
  }
  return it->second.get();
}

bool AutofillRegexes::MatchesPattern(const base::StringPiece16& input,
                                     const base::StringPiece16& pattern,
                                     std::vector<std::u16string>* groups) {
  if (input.size() > kMaxStringLength)
    return false;

  auto Match = [&]() {
    UErrorCode status = U_ZERO_ERROR;
    // `icu_input` must outlive `matcher` because it holds a reference to it.
    icu::UnicodeString icu_input(false, input.data(), input.length());
    icu::RegexMatcher* matcher = GetMatcher(pattern);
    matcher->reset(icu_input);
    UBool matched = matcher->find(0, status);
    DCHECK(U_SUCCESS(status));

    if (matched && groups) {
      int32_t matched_groups = matcher->groupCount();
      groups->resize(matched_groups + 1);

      for (int32_t i = 0; i < matched_groups + 1; ++i) {
        icu::UnicodeString match_unicode = matcher->group(i, status);
        DCHECK(U_SUCCESS(status));
        (*groups)[i] = base::i18n::UnicodeStringToString16(match_unicode);
      }
    }

    return matched;
  };

  if (!thread_safe_) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return Match();
  }

  base::AutoLock lock(lock_);
  return Match();
}

bool MatchesPattern(const base::StringPiece16& input,
                    const base::StringPiece16& pattern,
                    std::vector<std::u16string>* groups) {
  static base::NoDestructor<AutofillRegexes> g_autofill_regexes(
      ThreadSafe(true));
  return g_autofill_regexes->MatchesPattern(input, pattern, groups);
}

}  // namespace autofill
