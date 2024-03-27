// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_REGEXES_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_REGEXES_H_

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/types/strong_alias.h"
#include "third_party/icu/source/i18n/unicode/regex.h"

namespace autofill {

using ThreadSafe = base::StrongAlias<struct ThreadSafeTag, bool>;

// Compiles a case-insensitive regular expression.
//
// The return icu::RegexPattern is thread-safe (because it's const and icu
// guarantees thread-safety of the const functions). In particularly this
// includes icu::RegexPattern::matcher().
//
// May also be used to initialize `static base::NoDestructor<icu::RegexPattern>`
// function-scope variables.
std::unique_ptr<const icu::RegexPattern> CompileRegex(
    std::u16string_view regex);

// Returns true if `regex` is found in `input`.
// If `groups` is non-null, it gets resized and the found capture groups
// are written into it.
// Thread-safe.
bool MatchesRegex(std::u16string_view input,
                  const icu::RegexPattern& regex_pattern,
                  std::vector<std::u16string>* groups = nullptr);

// Calls MatchesRegex() after compiling the `regex` on the first call and
// retrieving it from a static variable in subsequent calls.
//
// This function is thread-safe.
template <const char16_t regex[]>
bool MatchesRegex(std::u16string_view input,
                  std::vector<std::u16string>* groups = nullptr) {
  static base::NoDestructor<std::unique_ptr<const icu::RegexPattern>>
      regex_pattern(CompileRegex(regex));
  return MatchesRegex(input, **regex_pattern, groups);
}

// A cache of compiled regex patterns. It can be configured to be thread-safe
// (using a mutex) or not (in which case it uses a sequence checker).
class AutofillRegexCache {
 public:
  explicit AutofillRegexCache(ThreadSafe thread_safe);
  ~AutofillRegexCache();

  AutofillRegexCache(const AutofillRegexCache&) = delete;
  AutofillRegexCache& operator=(const AutofillRegexCache&) = delete;

  // Returns the compiled regex corresponding to `regex`.
  // This function is thread-safe if `thread_safe_`.
  // The returned object is thread-safe in any case (because it's const).
  // Although the returned pointer is guaranteed to be non-nullptr, we do not
  // return a reference to avoid accidental copies.
  const icu::RegexPattern* GetRegexPattern(std::u16string_view regex);

 private:
  // `MatchesPattern()` uses the lock if `thread_safe_`. Otherwise, it validates
  // the sequence.
  ThreadSafe thread_safe_{false};
  base::Lock lock_;
  SEQUENCE_CHECKER(sequence_checker_);

  // Maps regex strings to their corresponding compiled regex patterns.
  std::
      map<std::u16string, std::unique_ptr<const icu::RegexPattern>, std::less<>>
          cache_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_REGEXES_H_
