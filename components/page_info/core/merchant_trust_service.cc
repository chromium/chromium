// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_info/core/merchant_trust_service.h"

#include <optional>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "components/commerce/core/proto/merchant_trust.pb.h"
#include "components/optimization_guide/core/hints_processing_util.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/page_info/core/features.h"
#include "components/page_info/core/merchant_trust_validation.h"
#include "components/page_info/core/page_info_types.h"
#include "components/page_info/core/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "net/base/url_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace {

std::string GetUserActionString(
    page_info::MerchantTrustInteraction interaction) {
  switch (interaction) {
    case page_info::MerchantTrustInteraction::kPageInfoRowShown:
      return "PageInfoRowSeen";
    case page_info::MerchantTrustInteraction::kBubbleOpenedFromPageInfo:
      return "BubbleOpenedFromPageInfo";
    case page_info::MerchantTrustInteraction::kSidePanelOpened:
      return "SidePanelOpened";
    case page_info::MerchantTrustInteraction::kBubbleClosed:
      return "BubbleClosed";
    case page_info::MerchantTrustInteraction::kSidePanelClosed:
      return "SidePanelClosed";
    case page_info::MerchantTrustInteraction::kBubbleOpenedFromLocationBarChip:
      return "BubbleOpenedFromLocationBarChip";
    case page_info::MerchantTrustInteraction::
        kSidePanelOpenedOnSameTabNavigation:
      return "SidePanelOpenedOnSameTabNavigation";
    case page_info::MerchantTrustInteraction::
        kSidePanelClosedOnSameTabNavigation:
      return "SidePanelClosedOnSameTabNavigation";
  }
}

std::string GetFamiliarityString(page_info::MerchantFamiliarity familiarity) {
  switch (familiarity) {
    case page_info::MerchantFamiliarity::kFamiliar:
      return "FamiliarSite";
    case page_info::MerchantFamiliarity::kNotFamiliar:
      return "UnfamiliarSite";
  }
}

}  // namespace

namespace page_info {
using OptimizationGuideDecision = optimization_guide::OptimizationGuideDecision;
using MerchantTrustStatus = merchant_trust_validation::MerchantTrustStatus;

namespace {

std::optional<page_info::MerchantData> GetSampleData() {
  page_info::MerchantData merchant_data;

  merchant_data.star_rating = 4.5;
  merchant_data.count_rating = 100;
  merchant_data.page_url = GURL(
      "https://customerreviews.google.com/v/merchant?q=amazon.com&c=US&gl=US");
  merchant_data.reviews_summary =
      "This is a test summary for the merchant trust side panel.";
  return merchant_data;
}
}  // namespace

// static
void MerchantTrustService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterTimePref(prefs::kMerchantTrustUiLastInteractionTime,
                             base::Time());
  registry->RegisterTimePref(prefs::kMerchantTrustPageInfoLastOpenTime,
                             base::Time());
}

MerchantTrustService::MerchantTrustService(
    std::unique_ptr<Delegate> delegate,
    optimization_guide::OptimizationGuideDecider* optimization_guide_decider,
    bool is_off_the_record,
    PrefService* prefs)
    : delegate_(std::move(delegate)),
      optimization_guide_decider_(optimization_guide_decider),
      is_off_the_record_(is_off_the_record),
      prefs_(prefs),
      weak_ptr_factory_(this) {
  if (optimization_guide_decider_) {
    optimization_guide_decider_->RegisterOptimizationTypes(
        {optimization_guide::proto::MERCHANT_TRUST_SIGNALS_V2});
  }
}

void MerchantTrustService::GetMerchantTrustInfo(
    const GURL& url,
    MerchantDataCallback callback) const {
  if (!optimization_guide::IsValidURLForURLKeyedHint(url)) {
    std::move(callback).Run(url, std::nullopt);
    return;
  }

  if (!IsOptimizationGuideAllowed()) {
    std::move(callback).Run(url, std::nullopt);
    return;
  }

  optimization_guide_decider_->CanApplyOptimization(
      url, optimization_guide::proto::MERCHANT_TRUST_SIGNALS_V2,
      base::BindOnce(&MerchantTrustService::OnCanApplyOptimizationComplete,
                     weak_ptr_factory_.GetWeakPtr(), url, std::move(callback)));
}

void MerchantTrustService::MaybeShowEvaluationSurvey() {
  if (CanShowEvaluationSurvey()) {
    delegate_->ShowEvaluationSurvey();
  }
}

void MerchantTrustService::OnCanApplyOptimizationComplete(
    const GURL& url,
    MerchantDataCallback callback,
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) const {
  if (decision != optimization_guide::OptimizationGuideDecision::kUnknown) {
    std::optional<commerce::MerchantTrustSignalsV2> merchant_trust_metadata =
        metadata.ParsedMetadata<commerce::MerchantTrustSignalsV2>();
    std::move(callback).Run(url,
                            GetMerchantDataFromProto(merchant_trust_metadata));
    return;
  }

  if (kMerchantTrustEnabledWithSampleData.Get()) {
    std::move(callback).Run(url, GetSampleData());
    return;
  }
  std::move(callback).Run(url, std::nullopt);
}

MerchantTrustService::~MerchantTrustService() = default;

bool MerchantTrustService::IsOptimizationGuideAllowed() const {
  return optimization_guide::IsUserPermittedToFetchFromRemoteOptimizationGuide(
      is_off_the_record_, prefs_);
}

std::optional<page_info::MerchantData>
MerchantTrustService::GetMerchantDataFromProto(
    const std::optional<commerce::MerchantTrustSignalsV2>& metadata) const {
  auto status = merchant_trust_validation::ValidateProto(metadata);
  base::UmaHistogramEnumeration("Security.PageInfo.MerchantTrustStatus",
                                status);
  auto enabled_without_summary =
      IsMerchantTrustWithoutSummaryEnabled() &&
      status == MerchantTrustStatus::kValidWithMissingReviewsSummary;

  if (status != MerchantTrustStatus::kValid && !enabled_without_summary) {
    return std::nullopt;
  }

  std::optional<page_info::MerchantData> merchant_data;

  commerce::MerchantTrustSignalsV2 merchant_proto = metadata.value();
  if (metadata.has_value() && merchant_proto.IsInitialized()) {
    merchant_data.emplace();

    if (merchant_proto.has_merchant_star_rating()) {
      merchant_data->star_rating = merchant_proto.merchant_star_rating();
    }

    if (merchant_proto.has_merchant_count_rating()) {
      merchant_data->count_rating = merchant_proto.merchant_count_rating();
    }

    if (merchant_proto.has_merchant_details_page_url()) {
      merchant_data->page_url =
          GURL(merchant_proto.merchant_details_page_url());
    }

    if (merchant_proto.has_shopper_voice_summary()) {
      merchant_data->reviews_summary = merchant_proto.shopper_voice_summary();
    }
  }

  return merchant_data;
}

bool MerchantTrustService::CanShowEvaluationSurvey() {
  if (base::FeatureList::IsEnabled(
          page_info::kMerchantTrustEvaluationControlSurvey)) {
    base::Time last_shown =
        prefs_->GetTime(prefs::kMerchantTrustPageInfoLastOpenTime);

    base::TimeDelta last_shown_delta = clock_->Now() - last_shown;
    return last_shown_delta >=
               kMerchantTrustEvaluationControlMinTimeToShowSurvey.Get() &&
           last_shown_delta <=
               kMerchantTrustEvaluationControlMaxTimeToShowSurvey.Get();
  }

  if (base::FeatureList::IsEnabled(
          page_info::kMerchantTrustEvaluationExperimentSurvey)) {
    base::Time last_shown =
        prefs_->GetTime(prefs::kMerchantTrustUiLastInteractionTime);

    base::TimeDelta last_shown_delta = clock_->Now() - last_shown;
    return last_shown_delta >=
               kMerchantTrustEvaluationExperimentMinTimeToShowSurvey.Get() &&
           last_shown_delta <=
               kMerchantTrustEvaluationExperimentMaxTimeToShowSurvey.Get();
  }

  return false;
}

void MerchantTrustService::RecordMerchantTrustInteraction(
    const GURL& url,
    MerchantTrustInteraction interaction) const {
  MerchantFamiliarity merchant_familiarity =
      delegate_->GetSiteEngagementScore(url) >= kMerchantFamiliarityThreshold
          ? MerchantFamiliarity::kFamiliar
          : MerchantFamiliarity::kNotFamiliar;

  std::string histogram_name =
      base::StrCat({"Security.PageInfo.MerchantTrustInteraction.",
                    GetFamiliarityString(merchant_familiarity)});

  std::string user_action_string =
      base::StrCat({"MerchantTrust.", GetUserActionString(interaction), ".",
                    GetFamiliarityString(merchant_familiarity)});

  base::UmaHistogramEnumeration(histogram_name, interaction);
  base::RecordAction(base::UserMetricsAction(user_action_string.c_str()));
  RecordEngagementScore(url, interaction);
}

void MerchantTrustService::RecordMerchantTrustUkm(
    ukm::SourceId source_id,
    MerchantTrustInteraction interaction) const {
  if (source_id == ukm::kInvalidSourceId) {
    return;
  }

  switch (interaction) {
    case MerchantTrustInteraction::kPageInfoRowShown:
      ukm::builders::Shopping_MerchantTrust_RowSeen(source_id)
          .SetHasOccurred(true)
          .Record(ukm::UkmRecorder::Get());
      break;
    case MerchantTrustInteraction::kBubbleOpenedFromPageInfo:
    case MerchantTrustInteraction::kBubbleOpenedFromLocationBarChip:
      ukm::builders::Shopping_MerchantTrust_BubbleOpened(source_id)
          .SetHasOccurred(true)
          .Record(ukm::UkmRecorder::Get());
      break;
    case MerchantTrustInteraction::kSidePanelOpened:
      ukm::builders::Shopping_MerchantTrust_SidePanelOpened(source_id)
          .SetHasOccurred(true)
          .Record(ukm::UkmRecorder::Get());
      break;
    case MerchantTrustInteraction::kBubbleClosed:
    case MerchantTrustInteraction::kSidePanelClosed:
    case MerchantTrustInteraction::kSidePanelOpenedOnSameTabNavigation:
    case MerchantTrustInteraction::kSidePanelClosedOnSameTabNavigation:
      break;
  }
}

void MerchantTrustService::RecordEngagementScore(
    const GURL& url,
    MerchantTrustInteraction interaction) const {
  auto engagement_score = delegate_->GetSiteEngagementScore(url);
  switch (interaction) {
    case MerchantTrustInteraction::kPageInfoRowShown:
      UMA_HISTOGRAM_COUNTS_100(
          "Security.PageInfo.MerchantTrustEngagement.PageInfoRowShown",
          engagement_score);
      break;
    case MerchantTrustInteraction::kBubbleOpenedFromPageInfo:
    case MerchantTrustInteraction::kBubbleOpenedFromLocationBarChip:
      UMA_HISTOGRAM_COUNTS_100(
          "Security.PageInfo.MerchantTrustEngagement.BubbleOpened",
          engagement_score);
      break;
    case MerchantTrustInteraction::kSidePanelOpened:
      UMA_HISTOGRAM_COUNTS_100(
          "Security.PageInfo.MerchantTrustEngagement.SidePanelOpened",
          engagement_score);
      break;
    case MerchantTrustInteraction::kBubbleClosed:
    case MerchantTrustInteraction::kSidePanelClosed:
    case MerchantTrustInteraction::kSidePanelOpenedOnSameTabNavigation:
    case MerchantTrustInteraction::kSidePanelClosedOnSameTabNavigation:
      break;
  }
}

}  // namespace page_info
