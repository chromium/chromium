// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_PROMO_REGISTRY_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_PROMO_REGISTRY_H_

#include <map>

#include "base/callback.h"
#include "base/optional.h"
#include "chrome/browser/ui/views/user_education/feature_promo_bubble_params.h"

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
  using GetAnchorViewCallback =
      base::RepeatingCallback<views::View*(BrowserView*)>;

  FeaturePromoRegistry();
  ~FeaturePromoRegistry();

  static FeaturePromoRegistry* GetInstance();

  // Returns a complete FeaturePromoBubbleParams object to start IPH for
  // the given feature. |iph_feature| is the feature to show for.
  // |browser_view| is the window it should show in.
  //
  // The params must be used immediately since it contains a View
  // pointer that may become stale. This may return nothing in which
  // case the promo shouldn't show.
  base::Optional<FeaturePromoBubbleParams> GetParamsForFeature(
      const base::Feature& iph_feature,
      BrowserView* browser_view);

  // Registers a feature promo. |iph_feature| is the feature. |params|
  // are normal bubble params except the anchor_view member should be
  // null. |get_anchor_view_callback| specifies how to get the bubble's
  // anchor view for an arbitrary browser window.
  //
  // Prefer putting these calls in the body of RegisterKnownFeatures()
  // when possible.
  void RegisterFeature(const base::Feature& iph_feature,
                       const FeaturePromoBubbleParams& params,
                       GetAnchorViewCallback get_anchor_view_callback);

  void ClearFeaturesForTesting();
  void ReinitializeForTesting();

 private:
  // To avoid sprinkling RegisterFeature() calls throughout the Top
  // Chrome codebase, you can put your call in here.
  void RegisterKnownFeatures();

  struct FeaturePromoData {
    FeaturePromoData();
    FeaturePromoData(FeaturePromoData&&);
    ~FeaturePromoData();

    // The params for a promo, minus the anchor view.
    FeaturePromoBubbleParams params;

    GetAnchorViewCallback get_anchor_view_callback;
  };

  std::map<const base::Feature*, FeaturePromoData> feature_promo_data_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_PROMO_REGISTRY_H_
