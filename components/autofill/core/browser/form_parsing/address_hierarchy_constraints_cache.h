// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_ADDRESS_HIERARCHY_CONSTRAINTS_CACHE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_ADDRESS_HIERARCHY_CONSTRAINTS_CACHE_H_

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/common/dense_set.h"

namespace autofill {

class AddressHierarchyConstraints;

// References to `base::Feature`s that affect `AddressHierarchyConstraints`
// across countries. Per-country model launch flags checked by
// `i18n_model_definition::IsCustomHierarchyAvailableForCountry()` are already
// reflected by normalizing countries without an active custom model to `"XX"`.
// Renumbering existing items of this enum is safe because they are only used
// for in-memory cache keys and not persisted across sessions or server-side.
enum class HierarchyFeature : uint8_t {
  // This entry only exists to ensure that the enum never becomes empty as
  // features are added and removed.
  kUnusedDummyFeature = 0,
  kAutofillSupportSplitZipCode = 1,
  kMaxValue = kAutofillSupportSplitZipCode,
};

// Process-lifetime singleton cache of immutable `AddressHierarchyConstraints`
// instances across `AddressFieldParserNG` parse runs.
//
// Map lookups and lazy creation are guarded by `base::Lock` because heuristic
// parsing runs on background sequences. Once created, entries are retained for
// the life of the process without eviction and read via non-owning reference
// without further locking.
class AddressHierarchyConstraintsCache {
 public:
  static AddressHierarchyConstraintsCache& GetInstance();

  AddressHierarchyConstraintsCache(const AddressHierarchyConstraintsCache&) =
      delete;
  AddressHierarchyConstraintsCache& operator=(
      const AddressHierarchyConstraintsCache&) = delete;
  ~AddressHierarchyConstraintsCache() = delete;

  // Returns the cached `AddressHierarchyConstraints` for `country_code` and the
  // currently active `HierarchyFeature`s. Countries without an active custom
  // hierarchy normalize to `"XX"`.
  // This function is thread-safe, and the returned object is immutable and safe
  // to read concurrently without locking.
  const AddressHierarchyConstraints& Get(AddressCountryCode country_code);

 private:
  friend class base::NoDestructor<AddressHierarchyConstraintsCache>;

  using CacheKey = std::pair<AddressCountryCode, DenseSet<HierarchyFeature>>;

  AddressHierarchyConstraintsCache();

  base::Lock lock_;
  base::flat_map<CacheKey, std::unique_ptr<const AddressHierarchyConstraints>>
      cache_ GUARDED_BY(lock_);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_ADDRESS_HIERARCHY_CONSTRAINTS_CACHE_H_
