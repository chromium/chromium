// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_PROMO_REGISTRY_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_PROMO_REGISTRY_H_

#include <map>
#include <utility>

#include "chrome/browser/ui/user_education/feature_promo_specification.h"

class BrowserView;

namespace base {
struct Feature;
}

namespace views {
class View;
}

// Stores parameters for in-product help promos. For each registered
// IPH, has the bubble parameters and a method for getting an anchor
// view for a given BrowserView. Promos should be registered here when
// feasible.
class FeaturePromoRegistry {
 public:
  FeaturePromoRegistry();
  ~FeaturePromoRegistry();

  static FeaturePromoRegistry* GetInstance();

  // Returns the FeaturePromoSpecification to start an IPH for
  // the given feature. |iph_feature| is the feature to show for.
  // |browser_view| is the window it should show in.
  //
  // The params must be used immediately since it contains a View
  // pointer that may become stale. This may return nothing in which
  // case the promo shouldn't show.
  const FeaturePromoSpecification* GetParamsForFeature(
      const base::Feature& iph_feature);

  // Registers a feature promo.
  //
  // Prefer putting these calls in the body of RegisterKnownFeatures()
  // when possible.
  void RegisterFeature(FeaturePromoSpecification spec);

  void ClearFeaturesForTesting();
  void ReinitializeForTesting();

 private:
  // To avoid sprinkling RegisterFeature() calls throughout the Top
  // Chrome codebase, you can put your call in here.
  void RegisterKnownFeatures();

  std::map<const base::Feature*, FeaturePromoSpecification> feature_promo_data_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_PROMO_REGISTRY_H_
