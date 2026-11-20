// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_distiller/core/eligibility/eligibility_decider.h"

#include <string_view>
#include <utility>

#include "base/containers/fixed_flat_set.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/values.h"
#include "components/google/core/common/google_util.h"
#include "components/optimization_guide/core/hints/optimization_guide_decider.h"
#include "components/optimization_guide/core/hints/optimization_metadata.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/web_distiller/core/extraction_utils.h"

namespace web_distiller {

namespace {

// Known Google websites that don't support Reading Mode.
constexpr auto kGoogleWorkspaceBlocklist =
    base::MakeFixedFlatSet<std::string_view>(
        {"assistant.google.com", "calendar.google.com", "docs.google.com",
         "drive.google.com", "mail.google.com", "photos.google.com"});

}  // namespace

ReaderModeEligibility EligibilityFromScriptValue(const base::Value* value) {
  if (!value || !value->is_bool()) {
    return ReaderModeEligibility::kMalformedResponse;
  }
  return value->GetBool()
             ? ReaderModeEligibility::kReaderModeEligible
             : ReaderModeEligibility::kReaderModeNotEligibleContentAndLength;
}

ReaderModeEligibility EligibilityFromOptimizationGuideDecision(
    optimization_guide::OptimizationGuideDecision decision) {
  switch (decision) {
    case optimization_guide::OptimizationGuideDecision::kTrue:
      return ReaderModeEligibility::kReaderModeEligible;
    case optimization_guide::OptimizationGuideDecision::kFalse:
      return ReaderModeEligibility::
          kReaderModeNotEligibleOptimizationGuideIneligible;
    case optimization_guide::OptimizationGuideDecision::kUnknown:
      return ReaderModeEligibility::
          kReaderModeNotEligibleOptimizationGuideUnknown;
  }
}

bool IsUrlEligible(const GURL& url) {
  return url.SchemeIsHTTPOrHTTPS() && !google_util::IsGoogleHomePageUrl(url) &&
         !google_util::IsGoogleSearchUrl(url) &&
         !google_util::IsYoutubeDomainUrl(
             url, google_util::ALLOW_SUBDOMAIN,
             google_util::ALLOW_NON_STANDARD_PORTS) &&
         !kGoogleWorkspaceBlocklist.contains(url.host());
}

EligibilityDecider::EligibilityDecider(
    Delegate& delegate,
    optimization_guide::OptimizationGuideDecider* optimization_guide_decider,
    base::TimeDelta start_delay,
    ResultCallback on_result)
    : delegate_(delegate),
      optimization_guide_decider_(optimization_guide_decider),
      start_delay_(start_delay),
      on_result_(std::move(on_result)) {}

EligibilityDecider::~EligibilityDecider() = default;

void EligibilityDecider::Start(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Cancel();
  start_timer_.Start(FROM_HERE, start_delay_,
                     base::BindOnce(&EligibilityDecider::EvaluateReadability,
                                    weak_ptr_factory_.GetWeakPtr(), url,
                                    current_decision_id_));
}

bool EligibilityDecider::Cancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ++current_decision_id_;
  if (!start_timer_.IsRunning()) {
    return false;
  }
  start_timer_.Stop();
  return true;
}

void EligibilityDecider::EvaluateReadability(const GURL& url, int decision_id) {
  if (!url.EqualsIgnoringRef(delegate_->GetLastCommittedURL())) {
    return;
  }
  if (!delegate_->IsContentHTML() || !IsUrlEligible(url)) {
    ReportResult(url, decision_id, std::nullopt);
    return;
  }

  delegate_->ExecuteScript(
      GetDistillerTriggeringScript(),
      base::BindOnce(&EligibilityDecider::OnScriptResult,
                     weak_ptr_factory_.GetWeakPtr(), url, decision_id));
}

void EligibilityDecider::OnScriptResult(const GURL& url,
                                        int decision_id,
                                        const base::Value* value) {
  ReaderModeEligibility result = EligibilityFromScriptValue(value);
  if (result == ReaderModeEligibility::kReaderModeEligible &&
      optimization_guide_decider_) {
    if (!IsCurrentDecision(decision_id) ||
        !url.EqualsIgnoringRef(delegate_->GetLastCommittedURL())) {
      return;
    }
    optimization_guide_decider_->CanApplyOptimization(
        url, optimization_guide::proto::READER_MODE_ELIGIBLE,
        base::BindOnce(&EligibilityDecider::OnOptimizationGuideDecision,
                       weak_ptr_factory_.GetWeakPtr(), url, decision_id));
    return;
  }
  ReportResult(url, decision_id, result);
}

void EligibilityDecider::OnOptimizationGuideDecision(
    const GURL& url,
    int decision_id,
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  ReportResult(url, decision_id,
               EligibilityFromOptimizationGuideDecision(decision));
}

void EligibilityDecider::ReportResult(
    const GURL& url,
    int decision_id,
    std::optional<ReaderModeEligibility> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool is_current_url = IsCurrentDecision(decision_id) &&
                        url.EqualsIgnoringRef(delegate_->GetLastCommittedURL());
  on_result_.Run(url, result, is_current_url);
}

bool EligibilityDecider::IsCurrentDecision(int decision_id) const {
  return decision_id == current_decision_id_;
}

}  // namespace web_distiller
