// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_INFO_CORE_PRIVACY_POLICY_INSIGHTS_SERVICE_H_
#define COMPONENTS_PAGE_INFO_CORE_PRIVACY_POLICY_INSIGHTS_SERVICE_H_

#include <optional>
#include <string>

#include "base/containers/flat_set.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/hints/optimization_guide_decider.h"
#include "components/page_info/core/proto/privacy_policy_annotation_metadata.pb.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/origin.h"

class GURL;
class TemplateURLService;

namespace page_info {
namespace proto {
class PrivacyPolicyAnnotation;
}

// Provides privacy policy annotation information for a web site.
class PrivacyPolicyInsightsService : public KeyedService {
 public:
  explicit PrivacyPolicyInsightsService(
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider,
      bool is_off_the_record,
      PrefService* prefs);
  ~PrivacyPolicyInsightsService() override;

  PrivacyPolicyInsightsService(const PrivacyPolicyInsightsService&) = delete;
  PrivacyPolicyInsightsService& operator=(const PrivacyPolicyInsightsService&) = delete;

  // Returns privacy policy annotation information for the website with |url|.
  std::optional<page_info::proto::PrivacyPolicyAnnotation> GetPrivacyPolicyAnnotation(
      const GURL& url,
      ukm::SourceId source_id) const;

 private:
  const raw_ptr<optimization_guide::OptimizationGuideDecider>
      optimization_guide_decider_;
  const bool is_off_the_record_;
  const raw_ptr<PrefService> prefs_;

  // Virtual for tests.
  virtual bool IsOptimizationGuideAllowed() const;
};

}  // namespace page_info

#endif  // COMPONENTS_PAGE_INFO_CORE_PRIVACY_POLICY_INSIGHTS_SERVICE_H_
