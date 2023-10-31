// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_STORAGE_SERVICE_H_
#define COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_STORAGE_SERVICE_H_

#include <set>

#include "base/feature_list.h"
#include "components/user_education/common/feature_promo_data.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// Declare in the global namespace for test purposes.
class FeaturePromoStorageInteractiveTest;

namespace user_education {

// This service manages snooze and other display data for in-product help
// promos.
//
// It is an abstract base class in order to support multiple frameworks/
// platforms, and different data stores.
//
// Before showing an IPH, the IPH controller should ask if the IPH is blocked.
// The controller should also notify after the IPH is shown and after the user
// clicks the snooze/dismiss button.
class FeaturePromoStorageService {
 public:
  FeaturePromoStorageService();
  virtual ~FeaturePromoStorageService();

  // Disallow copy and assign.
  FeaturePromoStorageService(const FeaturePromoStorageService&) = delete;
  FeaturePromoStorageService& operator=(const FeaturePromoStorageService&) =
      delete;

  virtual absl::optional<FeaturePromoData> ReadPromoData(
      const base::Feature& iph_feature) const = 0;

  virtual void SavePromoData(const base::Feature& iph_feature,
                             const FeaturePromoData& promo_data) = 0;

  // Reset the state of |iph_feature|.
  virtual void Reset(const base::Feature& iph_feature) = 0;

  // Returns the set of apps that `iph_feature` has been shown for.
  std::set<std::string> GetShownForApps(const base::Feature& iph_feature) const;

  // Returns the count of previous snoozes for `iph_feature`.
  int GetSnoozeCount(const base::Feature& iph_feature) const;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_STORAGE_SERVICE_H_
