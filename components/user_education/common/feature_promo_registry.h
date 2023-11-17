// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_REGISTRY_H_
#define COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_REGISTRY_H_

#include <map>

#include "base/feature_list.h"
#include "components/user_education/common/feature_promo_specification.h"

namespace user_education {

// Stores parameters for in-product help promos. For each registered
// IPH, has the bubble parameters and a method for getting an anchor
// view for a given BrowserView. Promos should be registered here when
// feasible.
class FeaturePromoRegistry {
 public:
  FeaturePromoRegistry();
  FeaturePromoRegistry(FeaturePromoRegistry&& other) noexcept;
  FeaturePromoRegistry& operator=(FeaturePromoRegistry&& other) noexcept;
  ~FeaturePromoRegistry();

  // Determines whether or not a particular feature is registered.
  bool IsFeatureRegistered(const base::Feature& iph_feature) const;

  // Returns the FeaturePromoSpecification to start an IPH for
  // the given feature. |iph_feature| is the feature to show for.
  // |browser_view| is the window it should show in.
  //
  // The params must be used immediately since it contains a View
  // pointer that may become stale. This may return nothing in which
  // case the promo shouldn't show.
  const FeaturePromoSpecification* GetParamsForFeature(
      const base::Feature& iph_feature) const;

  // Registers a feature promo.
  //
  // Prefer putting these calls in the body of RegisterKnownFeatures()
  // when possible.
  void RegisterFeature(FeaturePromoSpecification spec);

  const std::map<const base::Feature*, FeaturePromoSpecification>&
  GetRegisteredFeaturePromoSpecifications() const {
    return feature_promo_data_;
  }

  void ClearFeaturesForTesting();

 private:
  std::map<const base::Feature*, FeaturePromoSpecification> feature_promo_data_;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_REGISTRY_H_
