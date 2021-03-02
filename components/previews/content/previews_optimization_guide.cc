// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/previews_optimization_guide.h"

#include <utility>

#include "components/optimization_guide/content/browser/optimization_guide_decider.h"
#include "components/optimization_guide/core/hints_processing_util.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/previews/content/previews_user_data.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_switches.h"
#include "content/public/browser/navigation_handle.h"

namespace previews {

namespace {

// Returns false if the optimization guide decision is false. If unknown, it
// depends on the preview type whether the preview should be applied. Currently,
// only DeferAllScript may be applied in the unknown case.
bool ShouldApplyPreviewWithDecision(
    PreviewsType type,
    optimization_guide::OptimizationGuideDecision decision) {
  switch (decision) {
    case optimization_guide::OptimizationGuideDecision::kFalse:
      return false;
    case optimization_guide::OptimizationGuideDecision::kTrue:
      return true;
    case optimization_guide::OptimizationGuideDecision::kUnknown:
      return type == PreviewsType::DEFER_ALL_SCRIPT
                 ? params::ApplyDeferWhenOptimizationGuideDecisionUnknown()
                 : false;
  }
}

// The max size of the cache holding painful page load decisions by the
// navigation ID of the navigation handle.
size_t kDefaultPainfulPageLoadDecisionsCacheSize = 10;

// Returns base::nullopt if |previews_type| can't be converted.
base::Optional<optimization_guide::proto::OptimizationType>
ConvertPreviewsTypeToOptimizationType(PreviewsType previews_type) {
  switch (previews_type) {
    case PreviewsType::UNSPECIFIED:
      return optimization_guide::proto::TYPE_UNSPECIFIED;
    case PreviewsType::DEFER_ALL_SCRIPT:
      return optimization_guide::proto::DEFER_ALL_SCRIPT;
    default:
      return base::nullopt;
  }
}

// Returns the optimization types to register with the Optimization Guide
// Decider based on which Previews are enabled for the session.
base::flat_set<optimization_guide::proto::OptimizationType>
GetOptimizationTypesToRegister() {
  base::flat_set<optimization_guide::proto::OptimizationType>
      optimization_types;

  if (params::IsDeferAllScriptPreviewsEnabled())
    optimization_types.insert(optimization_guide::proto::DEFER_ALL_SCRIPT);

  return optimization_types;
}


}  // namespace

PreviewsOptimizationGuide::PreviewsOptimizationGuide(
    optimization_guide::OptimizationGuideDecider* optimization_guide_decider)
    : optimization_guide_decider_(optimization_guide_decider),
      painful_page_load_decisions_(kDefaultPainfulPageLoadDecisionsCacheSize),
      registered_optimization_types_(GetOptimizationTypesToRegister()) {
  DCHECK(optimization_guide_decider_);

  optimization_guide_decider_->RegisterOptimizationTypes(
      std::vector<optimization_guide::proto::OptimizationType>(
          registered_optimization_types_.begin(),
          registered_optimization_types_.end()));
  optimization_guide_decider_->RegisterOptimizationTargets(
      {optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});
}

PreviewsOptimizationGuide::~PreviewsOptimizationGuide() = default;

void PreviewsOptimizationGuide::StartCheckingIfShouldShowPreview(
    content::NavigationHandle* navigation_handle) {
  if (params::OverrideShouldShowPreviewCheck()) {
    // We are not going to use the decision from |optimization_guide_decider_|,
    // so just return.
    return;
  }

  if (painful_page_load_decisions_.Get(navigation_handle->GetNavigationId()) !=
      painful_page_load_decisions_.end()) {
    // We have either already evaluated the model or have kicked off a model
    // evaluation, so just return.
    return;
  }

  painful_page_load_decisions_.Put(
      navigation_handle->GetNavigationId(),
      optimization_guide::OptimizationGuideDecision::kUnknown);
  optimization_guide_decider_->ShouldTargetNavigationAsync(
      navigation_handle,
      optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      /*client_model_features=*/{},
      base::BindOnce(&PreviewsOptimizationGuide::OnPainfulPageLoadDecision,
                     weak_ptr_factory_.GetWeakPtr(),
                     navigation_handle->GetNavigationId()));
}

void PreviewsOptimizationGuide::OnPainfulPageLoadDecision(
    int64_t navigation_id,
    optimization_guide::OptimizationGuideDecision decision) {
  painful_page_load_decisions_.Put(navigation_id, decision);
}

bool PreviewsOptimizationGuide::ShouldShowPreview(
    content::NavigationHandle* navigation_handle) {
  // See if we should override the optimization guide and always show a preview.
  if (params::OverrideShouldShowPreviewCheck())
    return true;

  auto ppd_iter =
      painful_page_load_decisions_.Get(navigation_handle->GetNavigationId());
  return ppd_iter != painful_page_load_decisions_.end() &&
         ppd_iter->second ==
             optimization_guide::OptimizationGuideDecision::kTrue;
}

bool PreviewsOptimizationGuide::CanApplyPreview(
    PreviewsUserData* previews_data,
    content::NavigationHandle* navigation_handle,
    PreviewsType type) {
  base::Optional<optimization_guide::proto::OptimizationType>
      optimization_type = ConvertPreviewsTypeToOptimizationType(type);
  if (!optimization_type.has_value())
    return false;

  // See if we can apply the optimization. Note that
  // |optimization_guide_decider_| also ensures that the current browser
  // conditions match a painful page load as a prerequisite for returning true.
  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_guide::OptimizationGuideDecision decision =
      optimization_guide_decider_->CanApplyOptimization(
          navigation_handle->GetURL(), *optimization_type,
          &optimization_metadata);

  if (!ShouldApplyPreviewWithDecision(type, decision))
    return false;


  return true;
}

}  // namespace previews
