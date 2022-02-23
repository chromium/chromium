// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/content/metrics/commerce_metrics_tab_helper.h"

#include "base/bind.h"
#include "components/commerce/core/metrics/metrics_utils.h"
#include "components/optimization_guide/content/browser/optimization_guide_decider.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace commerce::metrics {

CommerceMetricsTabHelper::CommerceMetricsTabHelper(
    content::WebContents* content,
    optimization_guide::OptimizationGuideDecider* optimization_guide,
    PrefService* pref_service,
    bool is_off_the_record)
    : content::WebContentsObserver(content),
      content::WebContentsUserData<CommerceMetricsTabHelper>(*content),
      optimization_guide_(optimization_guide),
      pref_service_(pref_service),
      is_off_the_record_(is_off_the_record),
      weak_ptr_factory_(this) {
  // In tests |optimization_guide_| can be null.
  if (optimization_guide_) {
    std::vector<optimization_guide::proto::OptimizationType> types;
    types.push_back(
        optimization_guide::proto::OptimizationType::PRICE_TRACKING);
    optimization_guide_->RegisterOptimizationTypes(types);
  }
}

CommerceMetricsTabHelper::~CommerceMetricsTabHelper() = default;

void CommerceMetricsTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument() ||
      !navigation_handle->IsInPrimaryMainFrame() || !optimization_guide_) {
    return;
  }

  optimization_guide_->CanApplyOptimizationAsync(
      navigation_handle,
      optimization_guide::proto::OptimizationType::PRICE_TRACKING,
      base::BindOnce(&CommerceMetricsTabHelper::OnOptimizationGuideResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CommerceMetricsTabHelper::OnOptimizationGuideResult(
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  RecordPDPStateForNavigation(decision, metadata, pref_service_,
                              is_off_the_record_);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CommerceMetricsTabHelper);

}  // namespace commerce::metrics
