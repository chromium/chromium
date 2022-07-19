// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_REGEXES_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_REGEXES_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/sequence_checker.h"
#include "base/strings/string_piece.h"
#include "base/synchronization/lock.h"
#include "base/types/strong_alias.h"
#include "third_party/icu/source/i18n/unicode/regex.h"

namespace autofill {

using ThreadSafe = base::StrongAlias<struct ThreadSafeTag, bool>;

// A thread-local class that serves as a cache of compiled regex patterns.
//
// The regexp state can be accessed from multiple threads in single process
// mode, and this class offers per-thread instance instead of per-process
// singleton instance (https://crbug.com/812182).
class AutofillRegexes {
 public:
  explicit AutofillRegexes(ThreadSafe thread_safe);
  ~AutofillRegexes();

  AutofillRegexes(const AutofillRegexes&) = delete;
  AutofillRegexes& operator=(const AutofillRegexes&) = delete;

  // Case-insensitive regular expression matching.
  // Returns true if `pattern` is found in `input`.
  // If `groups` is non-null, it gets resized and the found capture groups
  // are written into it.
  // Thread-safety depends on `thread_safe_`.
  bool MatchesPattern(const base::StringPiece16& input,
                      const base::StringPiece16& pattern,
                      std::vector<std::u16string>* groups = nullptr);

 private:
  // Returns the compiled regex matcher corresponding to `pattern`.
  icu::RegexMatcher* GetMatcher(const base::StringPiece16& pattern);

  // `MatchesPattern()` uses the lock if `thread_safe_`. Otherwise, it validates
  // the sequence.
  ThreadSafe thread_safe_{false};
  base::Lock lock_;
  SEQUENCE_CHECKER(sequence_checker_);

  // Maps patterns to their corresponding regex matchers.
  std::map<std::u16string, std::unique_ptr<icu::RegexMatcher>, std::less<>>
      matchers_;
};

// Calls MatchesPattern() for a global, thread-safe AutofillRegexes object.
bool MatchesPattern(const base::StringPiece16& input,
                    const base::StringPiece16& pattern,
                    std::vector<std::u16string>* groups = nullptr);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_REGEXES_H_
