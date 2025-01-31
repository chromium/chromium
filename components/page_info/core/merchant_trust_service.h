// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_INFO_CORE_MERCHANT_TRUST_SERVICE_H_
#define COMPONENTS_PAGE_INFO_CORE_MERCHANT_TRUST_SERVICE_H_

#include <optional>
#include <string>

#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "components/commerce/core/proto/merchant_trust.pb.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/page_info/core/page_info_types.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/origin.h"

class GURL;
class TemplateURLService;

namespace optimization_guide {
class OptimizationGuideDecider;
class OptimizationMetadata;
}  // namespace optimization_guide

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace page_info {

enum class MerchantFamiliarity {
  kNotFamiliar,
  kFamiliar,
  kMaxValue = kFamiliar
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(MerchantTrustInteraction)
enum class MerchantTrustInteraction {
  kPageInfoRowShown = 0,
  kBubbleOpenedFromPageInfo = 1,
  kSidePanelOpened = 2,
  kBubbleClosed = 3,
  kSidePanelClosed = 4,
  kBubbleOpenedFromLocationBarChip = 5,
  kSidePanelOpenedOnSameTabNavigation = 6,
  kSidePanelClosedOnSameTabNavigation = 7,
  kMaxValue = kSidePanelClosedOnSameTabNavigation
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/security/enums.xml:MerchantTrustInteraction)

static constexpr double kMerchantFamiliarityThreshold = 5;

// Provides merchant information for a web site.
class MerchantTrustService : public KeyedService {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  class Delegate {
   public:
    // Launches the evaluation survey based on the experiment state.
    virtual void ShowEvaluationSurvey() = 0;

    virtual double GetSiteEngagementScore(const GURL url) = 0;

    virtual ~Delegate() = default;
  };

  using DecisionAndMetadata =
      std::pair<optimization_guide::OptimizationGuideDecision,
                std::optional<commerce::MerchantTrustSignalsV2>>;

  MerchantTrustService(
      std::unique_ptr<Delegate> delegate,
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider,
      bool is_off_the_record,
      PrefService* prefs);
  ~MerchantTrustService() override;

  MerchantTrustService(const MerchantTrustService&) = delete;
  MerchantTrustService& operator=(const MerchantTrustService&) = delete;

  // Asynchronously fetches merchant trust information for the given URL.
  virtual void GetMerchantTrustInfo(const GURL& url,
                            MerchantDataCallback callback) const;

  // Attempt to show an evaluation survey if the conditions apply. It will show
  // either a control or an experiment survey depending on the feature state.
  virtual void MaybeShowEvaluationSurvey();

  virtual void RecordMerchantTrustInteraction(
      const GURL& url,
      MerchantTrustInteraction interaction) const;

  void RecordMerchantTrustUkm(ukm::SourceId source_id,
                              MerchantTrustInteraction interaction) const;

  void SetClockForTesting(base::Clock* clock) { clock_ = clock; }

 private:
  std::unique_ptr<Delegate> delegate_;
  const raw_ptr<optimization_guide::OptimizationGuideDecider>
      optimization_guide_decider_;
  const bool is_off_the_record_;
  const raw_ptr<PrefService> prefs_;
  raw_ptr<base::Clock> clock_ = base::DefaultClock::GetInstance();

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

  // Whether the evaluation survey should be shown based on how long ago user
  // interacted with the feature.
  bool CanShowEvaluationSurvey();

  void RecordEngagementScore(const GURL& url,
                             MerchantTrustInteraction interaction) const;
};

}  // namespace page_info

#endif  // COMPONENTS_PAGE_INFO_CORE_MERCHANT_TRUST_SERVICE_H_
