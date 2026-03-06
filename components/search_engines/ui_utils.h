// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_UI_UTILS_H_
#define COMPONENTS_SEARCH_ENGINES_UI_UTILS_H_

#include "third_party/icu/source/i18n/unicode/coll.h"

class TemplateURL;

namespace internal {

// Comparator function to sort `TemplateURL`s by group (either created by the
// SiteSearchSettings policy, or not created by policy) and alphabetically
// inside each group.
//
// Alphabetical comparison is case-insensitive according to the current locale.
// In case of loading errors for ICU, fallback to regular string comparison.
class OrderTemplateUrlsByManagedAndAlphabetically {
 public:
  OrderTemplateUrlsByManagedAndAlphabetically();
  OrderTemplateUrlsByManagedAndAlphabetically(
      const OrderTemplateUrlsByManagedAndAlphabetically& other);
  ~OrderTemplateUrlsByManagedAndAlphabetically();

  bool operator()(const TemplateURL* lhs, const TemplateURL* rhs) const;

  // Exposed for testing
  std::string GetShortNameSortKey(const std::u16string& short_name) const;

 private:
  std::unique_ptr<icu::Collator> collator_;
};

}  // namespace internal

#endif  // COMPONENTS_SEARCH_ENGINES_UI_UTILS_H_
