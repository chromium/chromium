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
#include "base/synchronization/lock.h"
#include "third_party/icu/source/i18n/unicode/regex.h"

namespace {

// Maximum length of the string to match to avoid causing an icu::RegexMatcher
// stack overflow. (crbug.com/1198219)
constexpr int kMaxStringLength = 5000;

// A thread-local class that serves as a cache of compiled regex patterns.
//
// The regexp state can be accessed from multiple threads in single process
// mode, and this class offers per-thread instance instead of per-process
// singleton instance (https://crbug.com/812182).
class AutofillRegexes {
 public:
  AutofillRegexes() = default;

  AutofillRegexes(const AutofillRegexes&) = delete;
  AutofillRegexes& operator=(const AutofillRegexes&) = delete;

  // Returns the compiled regex matcher corresponding to |pattern|.
  icu::RegexMatcher* GetMatcher(const base::StringPiece16& pattern);

 private:
  ~AutofillRegexes() = default;

  // Maps patterns to their corresponding regex matchers.
  std::map<std::u16string, std::unique_ptr<icu::RegexMatcher>, std::less<>>
      matchers_;
};

icu::RegexMatcher* AutofillRegexes::GetMatcher(
    const base::StringPiece16& pattern) {
  auto it = matchers_.find(pattern);
  if (it == matchers_.end()) {
    const icu::UnicodeString icu_pattern(false, pattern.data(),
                                         pattern.length());

    UErrorCode status = U_ZERO_ERROR;
    auto matcher = std::make_unique<icu::RegexMatcher>(
        icu_pattern, UREGEX_CASE_INSENSITIVE, status);
    DCHECK(U_SUCCESS(status));

    auto result = matchers_.insert(std::make_pair(pattern, std::move(matcher)));
    DCHECK(result.second);
    it = result.first;
  }
  return it->second.get();
}

}  // namespace

namespace autofill {

bool MatchesPattern(const base::StringPiece16& input,
                    const base::StringPiece16& pattern,
                    std::u16string* match,
                    int32_t group_to_be_captured) {
  if (input.size() > kMaxStringLength)
    return false;

  static base::NoDestructor<AutofillRegexes> g_autofill_regexes;
  static base::NoDestructor<base::Lock> g_lock;
  base::AutoLock lock(*g_lock);

  icu::RegexMatcher* matcher = g_autofill_regexes->GetMatcher(pattern);
  icu::UnicodeString icu_input(false, input.data(), input.length());
  matcher->reset(icu_input);

  UErrorCode status = U_ZERO_ERROR;
  UBool matched = matcher->find(0, status);
  DCHECK(U_SUCCESS(status));

  if (matched && match) {
    icu::UnicodeString match_unicode =
        matcher->group(group_to_be_captured, status);
    DCHECK(U_SUCCESS(status));
    *match = base::i18n::UnicodeStringToString16(match_unicode);
  }

  return matched;
}

}  // namespace autofill
