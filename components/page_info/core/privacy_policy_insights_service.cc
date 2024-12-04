// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_info/core/privacy_policy_insights_service.h"

#include <optional>

#include "base/feature_list.h"
#include "components/optimization_guide/core/hints_processing_util.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/page_info/core/features.h"
#include "components/page_info/core/proto/privacy_policy_annotation_metadata.pb.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace page_info {

PrivacyPolicyInsightsService::PrivacyPolicyInsightsService(
    optimization_guide::OptimizationGuideDecider* optimization_guide_decider,
    bool is_off_the_record,
    PrefService* prefs)
    : optimization_guide_decider_(optimization_guide_decider),
      is_off_the_record_(is_off_the_record),
      prefs_(prefs) {
  if (optimization_guide_decider_) {
    optimization_guide_decider_->RegisterOptimizationTypes(
        {optimization_guide::proto::PRIVACY_POLICY_ANNOTATION});
  }
}

std::optional<page_info::proto::PrivacyPolicyAnnotation>
PrivacyPolicyInsightsService::GetPrivacyPolicyAnnotation(
    const GURL& url,
    ukm::SourceId source_id) const {
  if (!optimization_guide::IsValidURLForURLKeyedHint(url)) {
    return std::nullopt;
  }

  if (!IsOptimizationGuideAllowed()) {
    return std::nullopt;
  }
  // TODO(crbug.com/381393483): Call the optimization guide decider with the new
  // optimization type. Returning a dummy response for now.
  page_info::proto::PrivacyPolicyAnnotation privacy_policy_annotation;
  privacy_policy_annotation.set_is_privacy_policy(true);
  privacy_policy_annotation.set_canonical_url(
      "https://www.foo.com/policies/privacy/");

  // TODO(crbug.com/381392563): Validation of the response.

  // TODO(crbug.com/381392564): Add a UKM metric for the privacy policy annotation.

  return privacy_policy_annotation;
}

PrivacyPolicyInsightsService::~PrivacyPolicyInsightsService() = default;

bool PrivacyPolicyInsightsService::IsOptimizationGuideAllowed() const {
  return optimization_guide::IsUserPermittedToFetchFromRemoteOptimizationGuide(
      is_off_the_record_, prefs_);
}

}  // namespace page_info
