// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_coordinator.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/containers/map_util.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/autofill_driver.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/gmail_otp_source.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/sms_otp_source.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/logging/log_macros.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "components/one_time_tokens/core/browser/one_time_token_type.h"
#include "components/one_time_tokens/core/common/one_time_token_switches.h"

namespace autofill {

namespace {

std::string GetCommandLineMockOtpValue() {
  return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      one_time_tokens::switches::kMockOtpValue);
}

}  // namespace

OtpCoordinator::OtpCoordinator(AutofillClient* client)
    : client_(CHECK_DEREF(client)), security_gateway_(*client_) {
  if (!GetCommandLineMockOtpValue().empty()) {
    return;
  }
  if (auto* sms_otp_service = client_->GetSmsOtpService()) {
    auto sms_source = std::make_unique<SmsOtpSource>(sms_otp_service);
    sms_source->SetObserver(this);
    sources_.push_back(std::move(sms_source));
  }
  if (base::FeatureList::IsEnabled(
          features::kAutofillShowGmailOtpSuggestions)) {
    auto gmail_source = std::make_unique<GmailOtpSource>(*client_);
    gmail_source->SetObserver(this);
    sources_.push_back(std::move(gmail_source));
  }
}

OtpCoordinator::~OtpCoordinator() = default;

void OtpCoordinator::StartListeningForOtp(const OtpTarget& target) {
  if (!IsTargetActiveAndEligible(target)) {
    return;
  }

  auto [it, processing_new_target] =
      deliveries_.try_emplace(target.field, target);
  if (processing_new_target) {
    target_field_identifier_.OnOtpFieldsDetected();
    const SiteFrame site_frame = GetSiteFrame(target);
    if (phishing_blocked_site_frames_.contains(site_frame)) {
      it->second.BlockOnPhishing();
      return;
    }
    for (const OtpTarget& eligible :
         target_field_identifier_.GetEligibleTargets()) {
      if (GetSiteFrame(eligible) != site_frame) {
        continue;
      }
      if (std::optional<OtpCandidate> available =
              GetAvailableCandidate(eligible.field)) {
        if (it->second.Offer(*available)) {
          CHECK_DEREF(FindObserverForFrame(target.form_owner))
              .OnOtpAvailable(OtpSuggestionData(available->token));
        }
        break;
      }
    }
  }

  for (auto& source : sources_) {
    if (processing_new_target) {
      for (OtpCandidate& candidate : source->GetAvailable(target)) {
        OnSourceCandidateAvailable(target, std::move(candidate));
      }
    }
    source->Start(target);
  }
}

std::vector<one_time_tokens::OneTimeToken> OtpCoordinator::GetAvailableTokens(
    const OtpTarget& target) const {
  if (!IsTargetActiveAndEligible(target)) {
    return {};
  }

  if (std::string mock_otp = GetCommandLineMockOtpValue(); !mock_otp.empty()) {
    LOG_AF(client_->GetCurrentLogManager())
        << LoggingScope::kOneTimeTokens
        << "Using mock OTP value from command line switch.";
    return {one_time_tokens::OneTimeToken(
        one_time_tokens::OneTimeTokenType::kSmsOtp, std::move(mock_otp),
        base::TimeTicks::Now())};
  }

  std::optional<OtpCandidate> candidate = GetAvailableCandidate(target.field);
  if (!candidate.has_value() ||
      !security_gateway_.EvaluateReactiveServing(target, *candidate).allowed) {
    return {};
  }

  LOG_AF(client_->GetCurrentLogManager())
      << LoggingScope::kOneTimeTokens
      << "Delivering OTP suggestion to UI. Token length: "
      << candidate->token.value().size() << " (value omitted for privacy).";
  return {std::move(candidate->token)};
}

void OtpCoordinator::RegisterObserver(Observer* observer) {
  observers_.AddObserver(observer);
  target_field_identifier_.RegisterManager(
      observer->GetBrowserAutofillManager());
}

void OtpCoordinator::UnregisterObserver(Observer* observer) {
  BrowserAutofillManager& manager = observer->GetBrowserAutofillManager();
  observers_.RemoveObserver(observer);
  if (std::ranges::none_of(observers_, [&manager](const Observer& remaining) {
        return &remaining.GetBrowserAutofillManager() == &manager;
      })) {
    target_field_identifier_.UnregisterManager(manager);
  }
}

void OtpCoordinator::OnSourceCandidateAvailable(
    std::optional<OtpTarget> source_target,
    base::expected<OtpCandidate, one_time_tokens::OneTimeTokenRetrievalError>
        candidate_or_error) {
  if (!candidate_or_error.has_value()) {
    return;
  }

  // TODO(crbug.com/415273270): Enforce non-empty token values as an invariant
  // in `OneTimeToken` and `OtpCandidate`.
  OtpCandidate candidate = std::move(*candidate_or_error);
  std::vector<OtpTarget> targets_to_trigger_security_gateway;

  for (const OtpTarget& target :
       target_field_identifier_.GetEligibleTargets()) {
    if (source_target.has_value() && target.field != source_target->field) {
      continue;
    }
    const SiteFrame site_frame = GetSiteFrame(target);
    if (const OtpTokenIdentity* in_flight =
            base::FindOrNull(in_flight_validations_, site_frame);
        in_flight && !candidate.identity.IsStrictlyNewerThan(*in_flight)) {
      continue;
    }
    const OtpDelivery* delivery = base::FindOrNull(deliveries_, target.field);
    if (!delivery || !delivery->CanAccept(candidate)) {
      continue;
    }
    in_flight_validations_.insert_or_assign(site_frame, candidate.identity);
    targets_to_trigger_security_gateway.push_back(target);
  }

  for (const OtpTarget& target : targets_to_trigger_security_gateway) {
    security_gateway_.ValidateDelivery(
        target, candidate,
        base::BindOnce(&OtpCoordinator::OnDeliveryValidated,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

// static
OtpCoordinator::SiteFrame OtpCoordinator::GetSiteFrame(
    const OtpTarget& target) {
  return {target.origin, target.frame};
}

void OtpCoordinator::OnDeliveryValidated(OtpTarget target,
                                         OtpCandidate candidate,
                                         OtpDeliveryPolicy::Decision decision) {
  // TODO(crbug.com/415273270): Track a per-`SiteFrame` generation counter
  // bumped on `OnFrameBecameInactive` so an in-flight verdict for a previous
  // document in the same frame is ignored.
  const SiteFrame site_frame = GetSiteFrame(target);
  const OtpTokenIdentity* in_flight =
      base::FindOrNull(in_flight_validations_, site_frame);
  if (!in_flight) {
    return;
  }
  if (*in_flight == candidate.identity) {
    in_flight_validations_.erase(site_frame);
  }

  if (decision.reason == OtpDeliveryPolicy::SkipReason::kPhishingVerdict) {
    in_flight_validations_.erase(site_frame);
    phishing_blocked_site_frames_.insert(site_frame);
    for (auto& [field_id, delivery] : deliveries_) {
      if (GetSiteFrame(delivery.target()) == site_frame) {
        delivery.BlockOnPhishing();
      }
    }
    for (auto& source : sources_) {
      source->ClearVerifiedCacheForOrigin(target.origin);
    }
    return;
  }

  if (!decision.allowed) {
    return;
  }

  base::flat_set<LocalFrameToken> notified_form_owners;

  for (const OtpTarget& field_target :
       target_field_identifier_.GetEligibleTargets()) {
    OtpDelivery* delivery = base::FindOrNull(deliveries_, field_target.field);
    if (GetSiteFrame(field_target) != site_frame || !delivery ||
        !delivery->Offer(candidate)) {
      continue;
    }
    notified_form_owners.insert(field_target.form_owner);
  }

  for (const LocalFrameToken& form_owner : notified_form_owners) {
    CHECK_DEREF(FindObserverForFrame(form_owner))
        .OnOtpAvailable(OtpSuggestionData(candidate.token));
  }
}

std::optional<OtpCandidate> OtpCoordinator::GetAvailableCandidate(
    FieldGlobalId field) const {
  const OtpDelivery* delivery = base::FindOrNull(deliveries_, field);
  return delivery ? delivery->GetAvailableCandidate() : std::nullopt;
}

OtpCoordinator::Observer* OtpCoordinator::FindObserverForFrame(
    LocalFrameToken frame) {
  auto it = std::ranges::find(observers_, frame, [](const Observer& observer) {
    return observer.GetBrowserAutofillManager().driver().GetFrameToken();
  });
  return it != observers_.end() ? &*it : nullptr;
}

bool OtpCoordinator::IsTargetActiveAndEligible(const OtpTarget& target) const {
  return std::ranges::contains(target_field_identifier_.GetEligibleTargets(),
                               target);
}

}  // namespace autofill
