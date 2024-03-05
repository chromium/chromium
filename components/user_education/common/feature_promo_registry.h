// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_REGISTRY_H_
#define COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_REGISTRY_H_

#include <map>

#include "base/containers/contains.h"
#include "base/containers/map_util.h"
#include "base/feature_list.h"
#include "components/user_education/common/feature_promo_specification.h"
#include "components/user_education/common/new_badge_specification.h"

namespace user_education {

// Stores parameters for in-product help promos. For each registered
// IPH, has the bubble parameters and a method for getting an anchor
// view for a given BrowserView. Promos should be registered here when
// feasible.
template <typename T>
class FeatureRegistry {
 public:
  // Data structure used to store the actual registry.
  using FeatureDataMap = std::map<const base::Feature*, T>;

  FeatureRegistry() = default;
  FeatureRegistry(FeatureRegistry&& other) noexcept = default;
  FeatureRegistry& operator=(FeatureRegistry&& other) noexcept = default;
  virtual ~FeatureRegistry() = default;

  // Determines whether or not a particular `feature` is registered.
  bool IsFeatureRegistered(const base::Feature& feature) const {
    return base::Contains(feature_data_, &feature);
  }

  // Returns the specification for the given `feature`, or null if not found.
  //
  // (Some configurations may be only conditionally registered based on feature
  // flags.)
  const T* GetParamsForFeature(const base::Feature& feature) const {
    return base::FindOrNull(feature_data_, &feature);
  }

  const FeatureDataMap& feature_data() const { return feature_data_; }

  void clear_features_for_testing() { feature_data_.clear(); }

 protected:
  // Maps a feature to a specification.
  void RegisterFeature(const base::Feature& feature, T spec) {
    const auto result = feature_data_.emplace(&feature, std::move(spec));
    DCHECK(result.second) << "Duplicate feature registered: " << feature.name;
  }

 private:
  FeatureDataMap feature_data_;
};

// Registry used for IPH.
class FeaturePromoRegistry : public FeatureRegistry<FeaturePromoSpecification> {
 public:
  FeaturePromoRegistry();
  FeaturePromoRegistry(FeaturePromoRegistry&& other) noexcept;
  FeaturePromoRegistry& operator=(FeaturePromoRegistry&& other) noexcept;
  ~FeaturePromoRegistry() override;

  // Registers `spec` with the feature from `spec`.
  void RegisterFeature(FeaturePromoSpecification spec);
};

// Registry used for "New" Badge.
class NewBadgeRegistry : public FeatureRegistry<NewBadgeSpecification> {
 public:
  NewBadgeRegistry();
  NewBadgeRegistry(NewBadgeRegistry&&) noexcept;
  NewBadgeRegistry& operator=(NewBadgeRegistry&& other) noexcept;
  ~NewBadgeRegistry() override;

  // Registers `spec` with the feature from `spec`.
  void RegisterFeature(NewBadgeSpecification spec);
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_REGISTRY_H_
