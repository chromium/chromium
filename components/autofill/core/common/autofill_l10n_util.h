// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_L10N_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_L10N_UTIL_H_

#include <memory>
#include <string>

#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/i18n/unicode/coll.h"

namespace autofill {
namespace l10n {

// Obtains the ICU Collator for this locale. If unsuccessful, attempts to return
// the ICU collator for the English locale. If unsuccessful, returns null.
std::unique_ptr<icu::Collator> GetCollatorForLocale(const icu::Locale& locale);

// Assists with locale-aware case insensitive string comparisons.
// The `collator_` member is initialized in the constructor, which triggers the
// loading of locale-specific rules. While these rules are cached, loading them
// for the first time can be slow. Avoid adding this class as member variable to
// other classes for this reason. See e.g. crbug.com/1410875.
class CaseInsensitiveCompare {
 public:
  CaseInsensitiveCompare();
  // Used for testing.
  explicit CaseInsensitiveCompare(const icu::Locale& locale);

  CaseInsensitiveCompare(const CaseInsensitiveCompare&) = delete;
  CaseInsensitiveCompare& operator=(const CaseInsensitiveCompare&) = delete;

  ~CaseInsensitiveCompare();

  bool StringsEqual(const std::u16string& lhs, const std::u16string& rhs) const;

 private:
  std::unique_ptr<icu::Collator> collator_;
};

}  // namespace l10n
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_L10N_UTIL_H_
