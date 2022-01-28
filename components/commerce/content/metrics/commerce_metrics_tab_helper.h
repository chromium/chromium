// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CONTENT_METRICS_COMMERCE_METRICS_TAB_HELPER_H_
#define COMPONENTS_COMMERCE_CONTENT_METRICS_COMMERCE_METRICS_TAB_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class PrefService;

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace optimization_guide {
class OptimizationGuideDecider;
}  // namespace optimization_guide

namespace commerce::metrics {

class CommerceMetricsTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<CommerceMetricsTabHelper> {
 public:
  ~CommerceMetricsTabHelper() override;
  CommerceMetricsTabHelper(const CommerceMetricsTabHelper& other) = delete;
  CommerceMetricsTabHelper& operator=(const CommerceMetricsTabHelper& other) =
      delete;

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  friend class content::WebContentsUserData<CommerceMetricsTabHelper>;

  CommerceMetricsTabHelper(
      content::WebContents* contents,
      optimization_guide::OptimizationGuideDecider* optimization_guide,
      PrefService* pref_service,
      bool is_off_the_record);

  void OnOptimizationGuideResult(
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata);

  raw_ptr<optimization_guide::OptimizationGuideDecider> optimization_guide_;

  raw_ptr<PrefService> pref_service_;

  bool is_off_the_record_;

  base::WeakPtrFactory<CommerceMetricsTabHelper> weak_ptr_factory_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace commerce::metrics

#endif  // COMPONENTS_COMMERCE_CONTENT_METRICS_COMMERCE_METRICS_TAB_HELPER_H_
