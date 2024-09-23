// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_regexes.h"

#include <tuple>

#include "base/check.h"
#include "base/i18n/unicodestring.h"
#include "base/memory/ptr_util.h"
#include "base/not_fatal_until.h"

namespace {

// Maximum length of the string to match to avoid causing an icu::RegexMatcher
// stack overflow. (crbug.com/1198219)
constexpr int kMaxStringLength = 5000;

}  // namespace

namespace autofill {

std::unique_ptr<const icu::RegexPattern> CompileRegex(
    std::u16string_view regex) {
  const icu::UnicodeString icu_regex(false, regex.data(), regex.length());
  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::RegexPattern> regex_pattern = base::WrapUnique(
      icu::RegexPattern::compile(icu_regex, UREGEX_CASE_INSENSITIVE, status));
  DCHECK(U_SUCCESS(status));
  return regex_pattern;
}

bool MatchesRegex(std::u16string_view input,
                  const icu::RegexPattern& regex_pattern,
                  std::vector<std::u16string>* groups) {
  if (input.size() > kMaxStringLength)
    return false;

  UErrorCode status = U_ZERO_ERROR;
  // `icu_input` must outlive `regex_matcher` because it holds a reference to
  // it.
  icu::UnicodeString icu_input(false, input.data(), input.length());
  std::unique_ptr<icu::RegexMatcher> regex_matcher =
      base::WrapUnique(regex_pattern.matcher(icu_input, status));
  UBool matched = regex_matcher->find(0, status);
  DCHECK(U_SUCCESS(status));

  if (matched && groups) {
    int32_t matched_groups = regex_matcher->groupCount();
    groups->resize(matched_groups + 1);
    for (int32_t i = 0; i < matched_groups + 1; ++i) {
      icu::UnicodeString match_unicode = regex_matcher->group(i, status);
      DCHECK(U_SUCCESS(status));
      (*groups)[i] = base::i18n::UnicodeStringToString16(match_unicode);
    }
  }
  return matched;
}

AutofillRegexCache::AutofillRegexCache(ThreadSafe thread_safe)
    : thread_safe_(thread_safe) {
  if (!thread_safe_)
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

AutofillRegexCache::~AutofillRegexCache() {
  if (!thread_safe_)
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

const icu::RegexPattern* AutofillRegexCache::GetRegexPattern(
    std::u16string_view regex) {
  auto GetOrCreate = [&]() {
    auto it = cache_.find(regex);
    if (it == cache_.end()) {
      bool success;
      std::tie(it, success) =
          cache_.emplace(std::u16string(regex), CompileRegex(regex));
      DCHECK(success);
    }
    CHECK(it != cache_.end(), base::NotFatalUntil::M130);
    DCHECK(it->second.get());
    return it->second.get();
  };
  if (!thread_safe_) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return GetOrCreate();
  }
  base::AutoLock lock(lock_);
  return GetOrCreate();
}

}  // namespace autofill
