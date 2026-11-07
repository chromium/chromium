// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_coordinator.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/flat_set.h"
#include "base/containers/map_util.h"
#include "base/feature_list.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/autofill_driver.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/gmail_otp_source.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/sms_otp_source.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"

namespace autofill {

OtpCoordinator::OtpCoordinator(AutofillClient* client)
    : client_(CHECK_DEREF(client)) {
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
  base::flat_set<LocalFrameToken> notified_form_owners;

  for (const OtpTarget& target :
       target_field_identifier_.GetEligibleTargets()) {
    if (source_target.has_value() && target.field != source_target->field) {
      continue;
    }
    OtpDelivery* delivery = base::FindOrNull(deliveries_, target.field);
    if (!delivery || !delivery->Offer(candidate)) {
      continue;
    }
    notified_form_owners.insert(target.form_owner);
  }

  for (const LocalFrameToken& form_owner : notified_form_owners) {
    CHECK_DEREF(FindObserverForFrame(form_owner))
        .OnOtpAvailable(OtpSuggestionData(candidate.token));
  }
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
