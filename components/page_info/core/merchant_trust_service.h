// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_INFO_CORE_MERCHANT_TRUST_SERVICE_H_
#define COMPONENTS_PAGE_INFO_CORE_MERCHANT_TRUST_SERVICE_H_

#include <optional>
#include <string>

#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/page_info/core/page_info_types.h"
#include "components/page_info/core/proto/merchant_trust_metadata.pb.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/origin.h"

class GURL;
class TemplateURLService;

namespace page_info {
namespace proto {
class MerchantTrustSignalsV3;
}

// Provides merchant information for a web site.
class MerchantTrustService : public KeyedService {
 public:
  using DecisionAndMetadata =
      std::pair<optimization_guide::OptimizationGuideDecision,
                std::optional<page_info::proto::MerchantTrustSignalsV3>>;

  explicit MerchantTrustService(
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider,
      bool is_off_the_record,
      PrefService* prefs);
  ~MerchantTrustService() override;

  MerchantTrustService(const MerchantTrustService&) = delete;
  MerchantTrustService& operator=(const MerchantTrustService&) = delete;

  // Returns merchant trust information for the website with |url|.
  virtual std::optional<page_info::MerchantData>
  GetMerchantTrustInfo(const GURL& url, ukm::SourceId source_id) const;

 private:
  const raw_ptr<optimization_guide::OptimizationGuideDecider>
      optimization_guide_decider_;
  const bool is_off_the_record_;
  const raw_ptr<PrefService> prefs_;

  // Virtual for tests.
  virtual bool IsOptimizationGuideAllowed() const;
  virtual optimization_guide::OptimizationGuideDecision CanApplyOptimization(
      const GURL& url,
      optimization_guide::OptimizationMetadata* optimization_metadata) const;

  std::optional<page_info::MerchantData> GetMerchantDataFromProto(
      const std::optional<proto::MerchantTrustSignalsV3>& metadata) const;
};

}  // namespace page_info

#endif  // COMPONENTS_PAGE_INFO_CORE_MERCHANT_TRUST_SERVICE_H_
