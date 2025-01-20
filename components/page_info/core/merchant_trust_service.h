// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_INFO_CORE_MERCHANT_TRUST_SERVICE_H_
#define COMPONENTS_PAGE_INFO_CORE_MERCHANT_TRUST_SERVICE_H_

#include <optional>
#include <string>

#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "components/commerce/core/proto/merchant_trust.pb.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/page_info/core/page_info_types.h"
#include "url/origin.h"

class GURL;
class TemplateURLService;

namespace optimization_guide {
class OptimizationGuideDecider;
class OptimizationMetadata;
}  // namespace optimization_guide

namespace page_info {

// Provides merchant information for a web site.
class MerchantTrustService : public KeyedService {
 public:
  using DecisionAndMetadata =
      std::pair<optimization_guide::OptimizationGuideDecision,
                std::optional<commerce::MerchantTrustSignalsV2>>;

  explicit MerchantTrustService(
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider,
      bool is_off_the_record,
      PrefService* prefs);
  ~MerchantTrustService() override;

  MerchantTrustService(const MerchantTrustService&) = delete;
  MerchantTrustService& operator=(const MerchantTrustService&) = delete;

  // Asynchronously fetches merchant trust information for the given URL.
  virtual void GetMerchantTrustInfo(const GURL& url,
                            MerchantDataCallback callback) const;

 private:
  const raw_ptr<optimization_guide::OptimizationGuideDecider>
      optimization_guide_decider_;
  const bool is_off_the_record_;
  const raw_ptr<PrefService> prefs_;

  base::WeakPtrFactory<MerchantTrustService> weak_ptr_factory_;

  // Virtual for tests.
  virtual bool IsOptimizationGuideAllowed() const;

  // Handles the response for the optimization guide decider.
  void OnCanApplyOptimizationComplete(
      const GURL& url,
      MerchantDataCallback callback,
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata) const;

  std::optional<page_info::MerchantData> GetMerchantDataFromProto(
      const std::optional<commerce::MerchantTrustSignalsV2>& metadata) const;
};

}  // namespace page_info

#endif  // COMPONENTS_PAGE_INFO_CORE_MERCHANT_TRUST_SERVICE_H_
