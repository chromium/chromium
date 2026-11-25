// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/address_hierarchy_constraints_cache.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/containers/map_util.h"
#include "base/feature.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/synchronization/lock.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_i18n_api.h"
#include "components/autofill/core/browser/form_parsing/address_hierarchy_constraints.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/dense_set.h"

namespace autofill {

namespace {

// Maps `HierarchyFeature`s to their corresponding `base::Feature`s.
const base::Feature* GetFeatureOfHierarchyFeature(HierarchyFeature feature) {
  switch (feature) {
    case HierarchyFeature::kUnusedDummyFeature:
      return nullptr;
    case HierarchyFeature::kAutofillSupportSplitZipCode:
      return &features::kAutofillSupportSplitZipCode;
  }
  NOTREACHED();
}

// Returns a `DenseSet` containing all `HierarchyFeature`s whose corresponding
// `base::Feature` is enabled.
DenseSet<HierarchyFeature> GetActiveHierarchyFeatures() {
  DenseSet<HierarchyFeature> active_features;
  for (HierarchyFeature hierarchy_feature : DenseSet<HierarchyFeature>::all()) {
    const base::Feature* base_feature =
        GetFeatureOfHierarchyFeature(hierarchy_feature);
    if (base_feature && base::FeatureList::IsEnabled(*base_feature)) {
      active_features.insert(hierarchy_feature);
    }
  }
  return active_features;
}

}  // namespace

// static
AddressHierarchyConstraintsCache&
AddressHierarchyConstraintsCache::GetInstance() {
  static base::NoDestructor<AddressHierarchyConstraintsCache> instance;
  return *instance;
}

AddressHierarchyConstraintsCache::AddressHierarchyConstraintsCache() = default;

const AddressHierarchyConstraints& AddressHierarchyConstraintsCache::Get(
    AddressCountryCode country_code) {
  AddressCountryCode normalized_country =
      i18n_model_definition::IsCustomHierarchyAvailableForCountry(country_code)
          ? std::move(country_code)
          : i18n_model_definition::kLegacyHierarchyCountryCode;
  const CacheKey key(std::move(normalized_country),
                     GetActiveHierarchyFeatures());

  base::AutoLock lock(lock_);
  const std::unique_ptr<const AddressHierarchyConstraints>& entry =
      base::GetOrCreate(cache_, key, [&key] {
        return std::make_unique<const AddressHierarchyConstraints>(key.first);
      });
  CHECK(entry);
  return *entry;
}

}  // namespace autofill
