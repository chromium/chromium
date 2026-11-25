// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/address_hierarchy_constraints_cache.h"

#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/address_hierarchy_constraints.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

// Tests that repeated lookups for the same country return the same cached
// `AddressHierarchyConstraints` instance, custom-hierarchy countries get
// distinct entries, and countries without a custom hierarchy share the legacy
// `"XX"` entry.
TEST(AddressHierarchyConstraintsCacheTest, ReusesCachedEntries) {
  AddressHierarchyConstraintsCache& cache =
      AddressHierarchyConstraintsCache::GetInstance();

  const AddressHierarchyConstraints& us_first =
      cache.Get(AddressCountryCode("US"));
  const AddressHierarchyConstraints& us_second =
      cache.Get(AddressCountryCode("US"));
  EXPECT_EQ(&us_first, &us_second);

  const AddressHierarchyConstraints& mx = cache.Get(AddressCountryCode("MX"));
  EXPECT_NE(&us_first, &mx);

  // Countries without an active custom hierarchy normalize to "XX" and share
  // one entry.
  const AddressHierarchyConstraints& empty = cache.Get(AddressCountryCode(""));
  const AddressHierarchyConstraints& gb = cache.Get(AddressCountryCode("GB"));
  const AddressHierarchyConstraints& xx = cache.Get(AddressCountryCode("XX"));
  EXPECT_EQ(&empty, &gb);
  EXPECT_EQ(&gb, &xx);
  EXPECT_NE(&xx, &us_first);

  // Verify legacy ("XX") vs custom ("MX") address line incompatibilities.
  EXPECT_FALSE(xx.incompatible_field_types(ADDRESS_HOME_LINE2)
                   .contains(ADDRESS_HOME_STREET_NAME));
  EXPECT_TRUE(mx.incompatible_field_types(ADDRESS_HOME_LINE2)
                  .contains(ADDRESS_HOME_STREET_NAME));
}

// Tests that toggling a `HierarchyFeature` flag
// (`kAutofillSupportSplitZipCode`) partitions cached entries for the same
// country and reuses each flag state's entry on subsequent lookups.
TEST(AddressHierarchyConstraintsCacheTest, RespectsHierarchyFeatureFlag) {
  AddressHierarchyConstraintsCache& cache =
      AddressHierarchyConstraintsCache::GetInstance();

  base::test::ScopedFeatureList disable_split_zip;
  disable_split_zip.InitAndDisableFeature(
      features::kAutofillSupportSplitZipCode);

  const AddressHierarchyConstraints& us_without_split_zip =
      cache.Get(AddressCountryCode("US"));
  EXPECT_FALSE(us_without_split_zip.supported_field_types().contains(
      ADDRESS_HOME_ZIP_PREFIX));

  {
    base::test::ScopedFeatureList enable_split_zip(
        features::kAutofillSupportSplitZipCode);

    const AddressHierarchyConstraints& us_with_split_zip =
        cache.Get(AddressCountryCode("US"));
    EXPECT_NE(&us_without_split_zip, &us_with_split_zip);
    EXPECT_TRUE(us_with_split_zip.supported_field_types().contains(
        ADDRESS_HOME_ZIP_PREFIX));
  }

  EXPECT_EQ(&us_without_split_zip, &cache.Get(AddressCountryCode("US")));
}

// Tests that toggling a per-country model launch flag
// (`kAutofillUseINAddressModel`) switches that country between the shared
// legacy `"XX"` entry and its dedicated custom-hierarchy entry without
// affecting other countries.
TEST(AddressHierarchyConstraintsCacheTest, RespectsCountryModelFeatureFlag) {
  AddressHierarchyConstraintsCache& cache =
      AddressHierarchyConstraintsCache::GetInstance();

  base::test::ScopedFeatureList disable_in_model;
  disable_in_model.InitAndDisableFeature(features::kAutofillUseINAddressModel);
  const AddressHierarchyConstraints& xx_entry =
      cache.Get(AddressCountryCode("XX"));
  EXPECT_EQ(&xx_entry, &cache.Get(AddressCountryCode("IN")));

  {
    base::test::ScopedFeatureList enable_in_model(
        features::kAutofillUseINAddressModel);
    const AddressHierarchyConstraints& in_entry =
        cache.Get(AddressCountryCode("IN"));
    EXPECT_NE(&xx_entry, &in_entry);
    EXPECT_TRUE(in_entry.supported_field_types().contains(
        ADDRESS_HOME_DEPENDENT_LOCALITY_AND_LANDMARK));
    EXPECT_EQ(&xx_entry, &cache.Get(AddressCountryCode("XX")));
    EXPECT_EQ(&xx_entry, &cache.Get(AddressCountryCode("GB")));
  }

  EXPECT_EQ(&xx_entry, &cache.Get(AddressCountryCode("IN")));
}

}  // namespace
}  // namespace autofill
