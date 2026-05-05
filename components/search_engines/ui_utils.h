// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_UI_UTILS_H_
#define COMPONENTS_SEARCH_ENGINES_UI_UTILS_H_

#include "base/containers/flat_map.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "third_party/icu/source/i18n/unicode/coll.h"

class TemplateURL;
struct TemplateURLData;

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
  std::string GetShortNameSortKeyForTesting(
      const std::u16string& short_name) const;

 private:
  std::unique_ptr<icu::Collator> collator_;
};

// Comparator function to sort `TemplateURL`s by putting first engines in order
// defined by the `prepopulated_engines`, and then following sorting logic of
class OrderTemplateUrlsByPrepopulatedAndManagedAndAlphabetically {
 public:
  explicit OrderTemplateUrlsByPrepopulatedAndManagedAndAlphabetically(
      std::vector<std::unique_ptr<TemplateURLData>> prepopulated_engines);

  OrderTemplateUrlsByPrepopulatedAndManagedAndAlphabetically(
      const OrderTemplateUrlsByPrepopulatedAndManagedAndAlphabetically& other);
  ~OrderTemplateUrlsByPrepopulatedAndManagedAndAlphabetically();

  bool operator()(const TemplateURL* lhs, const TemplateURL* rhs) const;

 private:
  std::string GetShortNameSortKey(const std::u16string& short_name) const;

  std::unique_ptr<icu::Collator> collator_;
  // Maps an engine's `prepopulate_id` to its rank in the `prepopulated_engines`
  // order.
  base::flat_map<int, size_t> prepopulated_ranks_;
};

template_url_starter_pack_data::StarterPackIdSet GetDisabledStarterPackIds(
    bool ai_mode_enabled,
    bool gemini_enabled);

}  // namespace internal
#endif  // COMPONENTS_SEARCH_ENGINES_UI_UTILS_H_
