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

#include "base/check.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/containers/map_util.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
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
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
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

// static
OtpCoordinator::SiteFrame OtpCoordinator::GetSiteFrame(
    const OtpTarget& target) {
  return {target.origin, target.frame};
}

// static
bool OtpCoordinator::IsUsedTokenExpired(const UsedToken& used,
                                        base::TimeTicks now) {
  const base::TimeTicks ref_time =
      std::max(used.identity.on_device_arrival, used.recorded_at);
  return now - ref_time > used.identity.CacheDuration();
}

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
  bool offered_from_peer = false;
  if (processing_new_target) {
    target_field_identifier_.OnOtpFieldsDetected();
    const SiteFrame site_frame = GetSiteFrame(target);
    if (phishing_blocked_site_frames_.contains(site_frame)) {
      it->second.BlockOnPhishing();
      return;
    }
    std::optional<OtpCandidate> peer_candidate;
    bool peer_in_form_already_has_candidate = false;
    std::optional<FieldGlobalId> focused_field_in_form;
    for (const OtpTarget& eligible :
         target_field_identifier_.GetEligibleTargets()) {
      if (eligible.form == target.form &&
          target_field_identifier_.IsFieldFocused(eligible.field)) {
        focused_field_in_form = eligible.field;
      }
      if (GetSiteFrame(eligible) != site_frame) {
        continue;
      }
      if (std::optional<OtpCandidate> available =
              GetAvailableCandidate(eligible.field);
          available.has_value() &&
          !IsCandidateSuppressedByUsedTokens(target, *available)) {
        if (!peer_candidate.has_value()) {
          peer_candidate = available;
        }
        if (eligible.form == target.form) {
          peer_in_form_already_has_candidate = true;
        }
      }
    }
    if (peer_candidate.has_value() && it->second.Offer(*peer_candidate)) {
      offered_from_peer = true;
      if (!peer_in_form_already_has_candidate) {
        CHECK_DEREF(FindObserverForFrame(target.form_owner))
            .OnOtpAvailable(OtpSuggestionData(peer_candidate->token));
      }
      const bool should_try_proactive =
          focused_field_in_form.has_value()
              ? (*focused_field_in_form == target.field)
              : !peer_in_form_already_has_candidate;
      if (should_try_proactive) {
        TryProactiveTrigger(target, *peer_candidate);
      }
    }
  }

  for (auto& source : sources_) {
    if (processing_new_target && !offered_from_peer) {
      for (OtpCandidate& candidate : source->GetAvailable(target)) {
        OnSourceCandidateAvailable(target, std::move(candidate));
      }
    }
    source->Start(target);
  }
}

void OtpCoordinator::StartListeningForEligibleTargets() {
  for (const OtpTarget& target :
       target_field_identifier_.GetEligibleTargets()) {
    StartListeningForOtp(target);
  }
}

void OtpCoordinator::StopListeningForForm(
    FormGlobalId form_id,
    const base::flat_set<FieldGlobalId>& except) {
  DropDeliveries([&](const OtpDelivery& delivery) {
    return delivery.target().form == form_id &&
           !except.contains(delivery.target().field);
  });
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
      IsCandidateSuppressedByUsedTokens(target, *candidate) ||
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

void OtpCoordinator::OnFrameBecameInactive(LocalFrameToken frame) {
  base::EraseIf(phishing_blocked_site_frames_,
                [&](const SiteFrame& sf) { return sf.second == frame; });
  target_field_identifier_.OnFrameBecameInactive(frame);
  DropDeliveries([&](const OtpDelivery& delivery) {
    return delivery.target().frame == frame ||
           delivery.target().form_owner == frame;
  });
}

void OtpCoordinator::OnFocusChanged(LocalFrameToken frame,
                                    std::optional<FormGlobalId> form_id,
                                    std::optional<FieldGlobalId> field_id) {
  target_field_identifier_.OnFocusChanged(frame, form_id, field_id);
}

void OtpCoordinator::OnTabVisibilityChanged(bool is_visible) {
  if (!std::exchange(is_tab_visible_, is_visible) && is_visible &&
      is_window_focused_) {
    RetryDeferredProactiveTriggers();
  }
}

void OtpCoordinator::OnWindowFocusChanged(bool is_focused) {
  if (!std::exchange(is_window_focused_, is_focused) && is_focused &&
      is_tab_visible_) {
    RetryDeferredProactiveTriggers();
  }
}

void OtpCoordinator::OnTextFieldValueChanged() {
  target_field_identifier_.OnTextFieldValueChanged();
}

void OtpCoordinator::OnOtpSuggestionsShown(FieldGlobalId field,
                                           const std::string& shown_value) {
  const OtpDelivery* delivery = base::FindOrNull(deliveries_, field);
  if (std::optional<OtpCandidate> candidate = GetAvailableCandidate(field);
      delivery && candidate.has_value() &&
      candidate->identity.value == shown_value) {
    RecordFormSuggestionsShown(delivery->target().form, candidate->identity);
  }
}

void OtpCoordinator::OnOtpFilled(const OtpTarget& target,
                                 const std::string& filled_value) {
  CHECK(!filled_value.empty());

  std::vector<OtpTokenIdentity> identities_to_consume;
  auto add_identity = [&](const OtpTokenIdentity& identity) {
    if (!std::ranges::contains(identities_to_consume, identity)) {
      identities_to_consume.push_back(identity);
    }
  };
  for (auto& [field_id, delivery] : deliveries_) {
    if (delivery.target().origin != target.origin) {
      continue;
    }
    const std::optional<OtpTokenIdentity>& newest =
        delivery.GetNewestIdentity();
    if (newest.has_value() && newest->value == filled_value) {
      add_identity(*newest);
      delivery.MarkConsumed();
      suggestion_dropdown_dispatcher_.ClearField(field_id);
    }
  }

  for (auto& source : sources_) {
    for (const OtpCandidate& candidate : source->GetAvailable(target)) {
      if (candidate.identity.value == filled_value) {
        add_identity(candidate.identity);
      }
    }
  }

  for (auto& source : sources_) {
    for (const OtpTokenIdentity& identity : identities_to_consume) {
      source->MarkConsumed(identity);
    }
  }

  if (identities_to_consume.empty()) {
    // A null arrival time blocks tokens with the same value without blocking
    // newer tokens.
    identities_to_consume.emplace_back(
        filled_value, /*sender=*/std::nullopt,
        /*email_received_timestamp=*/std::nullopt,
        /*on_device_arrival=*/base::TimeTicks());
  }

  const base::TimeTicks now = base::TimeTicks::Now();
  base::EraseIf(used_tokens_per_origin_, [&](auto& entry) {
    std::erase_if(entry.second, [&](const UsedToken& used) {
      return IsUsedTokenExpired(used, now);
    });
    return entry.second.empty();
  });
  std::vector<UsedToken>& used_tokens = used_tokens_per_origin_[target.origin];
  for (const OtpTokenIdentity& identity : identities_to_consume) {
    if (auto it =
            std::ranges::find(used_tokens, identity, &UsedToken::identity);
        it != used_tokens.end()) {
      it->recorded_at = now;
    } else {
      used_tokens.push_back(UsedToken{.identity = identity, .recorded_at = now});
    }
  }
}

void OtpCoordinator::OnOtpFormSubmitted(const FormData& form) {
  std::vector<std::pair<OtpTarget, std::u16string>> filled_fields;
  for (const FormFieldData& field : form.fields()) {
    if (const OtpDelivery* delivery =
            base::FindOrNull(deliveries_, field.global_id());
        delivery && !field.value().empty()) {
      filled_fields.emplace_back(delivery->target(), field.value());
    }
  }
  if (filled_fields.size() > 1u &&
      std::ranges::all_of(filled_fields, [](const auto& entry) {
        return entry.second.size() == 1u;
      })) {
    std::u16string combined;
    for (const auto& [_, value] : filled_fields) {
      combined += value;
    }
    OnOtpFilled(filled_fields.front().first, base::UTF16ToUTF8(combined));
  } else {
    for (const auto& [target, value] : filled_fields) {
      OnOtpFilled(target, base::UTF16ToUTF8(value));
    }
  }
  StopListeningForForm(form.global_id());
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
    if (IsCandidateSuppressedByUsedTokens(target, candidate) || !delivery ||
        !delivery->CanAccept(candidate)) {
      continue;
    }
    in_flight_validations_.insert_or_assign(site_frame, candidate.identity);
    targets_to_trigger_security_gateway.push_back(target);
  }

  for (const OtpTarget& target : targets_to_trigger_security_gateway) {
    security_gateway_.ValidateDelivery(
        target, candidate,
        base::BindOnce(&OtpCoordinator::OnDeliveryValidated,
                       weak_ptr_factory_.GetWeakPtr(), listening_generation_));
  }
}

void OtpCoordinator::OnDeliveryValidated(uint64_t generation,
                                         OtpTarget target,
                                         OtpCandidate candidate,
                                         OtpDeliveryPolicy::Decision decision) {
  // TODO(crbug.com/415273270): Track a per-`SiteFrame` generation counter
  // bumped on `OnFrameBecameInactive` so an in-flight verdict for a previous
  // document in the same frame is ignored.
  if (generation != listening_generation_) {
    return;
  }
  const SiteFrame site_frame = GetSiteFrame(target);
  const OtpTokenIdentity* in_flight =
      base::FindOrNull(in_flight_validations_, site_frame);
  if (!in_flight) {
    return;
  }
  if (*in_flight == candidate.identity) {
    in_flight_validations_.erase(site_frame);
  }
  if (!HasActiveDeliveryForSiteFrame(site_frame)) {
    return;
  }

  if (decision.reason == OtpDeliveryPolicy::SkipReason::kPhishingVerdict) {
    in_flight_validations_.erase(site_frame);
    phishing_blocked_site_frames_.insert(site_frame);
    for (auto& [field_id, delivery] : deliveries_) {
      if (GetSiteFrame(delivery.target()) == site_frame) {
        delivery.BlockOnPhishing();
        suggestion_dropdown_dispatcher_.ClearField(field_id);
      }
    }
    for (auto& source : sources_) {
      source->ClearVerifiedCacheForOrigin(target.origin);
    }
    return;
  }

  if (!decision.allowed ||
      IsCandidateSuppressedByUsedTokens(target, candidate)) {
    return;
  }

  base::flat_set<LocalFrameToken> notified_form_owners;
  std::optional<OtpTarget> proactive_target;

  for (const OtpTarget& field_target :
       target_field_identifier_.GetEligibleTargets()) {
    OtpDelivery* delivery = base::FindOrNull(deliveries_, field_target.field);
    if (!delivery || GetSiteFrame(field_target) != site_frame ||
        !delivery->Offer(candidate)) {
      continue;
    }
    notified_form_owners.insert(field_target.form_owner);
    if (!proactive_target.has_value() ||
        target_field_identifier_.IsFieldFocused(field_target.field)) {
      proactive_target = field_target;
    }
  }

  for (const LocalFrameToken& form_owner : notified_form_owners) {
    CHECK_DEREF(FindObserverForFrame(form_owner))
        .OnOtpAvailable(OtpSuggestionData(candidate.token));
  }

  if (proactive_target.has_value()) {
    TryProactiveTrigger(*proactive_target, candidate);
  }
}

void OtpCoordinator::TryProactiveTrigger(const OtpTarget& target,
                                         const OtpCandidate& candidate) {
  const bool can_show_on_screen_now = is_tab_visible_ && is_window_focused_;
  if (suggestion_dropdown_dispatcher_.TriggerProactive(
          target, candidate, can_show_on_screen_now,
          [this, &candidate](const OtpTarget& t) {
            if (!security_gateway_
                     .EvaluateProactiveTrigger(
                         t, candidate,
                         OtpSecurityGateway::ProactiveTriggerContext{
                             .target_field_exists =
                                 IsTargetActiveAndEligible(t),
                             // Frame activity is already verified by
                             // `IsTargetActiveAndEligible(t)` via
                             // `GetEligibleTargets()`.
                             .frame_active = true,
                             .is_otp_field_focused =
                                 target_field_identifier_.IsFieldFocused(
                                     t.field),
                             .has_non_otp_focus_anywhere =
                                 target_field_identifier_
                                     .HasNonOtpFocusAnywhere(),
                             .any_otp_field_has_typed_input =
                                 target_field_identifier_
                                     .AnyOtpFieldContainsTypedInput(),
                             .typed_in_any_field_after_detection =
                                 target_field_identifier_
                                     .TypedInAnyFieldAfterOtpDetection(),
                         })
                     .allowed) {
              return false;
            }
            const std::optional<OtpTarget> designated_target =
                target_field_identifier_.GetTarget();
            if (designated_target.has_value() &&
                target_field_identifier_.IsFieldFocused(
                    designated_target->field) &&
                designated_target->field != t.field) {
              return false;
            }
            LOG_AF(client_->GetCurrentLogManager())
                << LoggingScope::kOneTimeTokens
                << "Proactively triggering suggestions via renderer for OTP "
                   "field.";
            CHECK_DEREF(FindObserverForFrame(t.form_owner))
                .TriggerSuggestions(t.field);
            return true;
          })) {
    RecordFormSuggestionsShown(target.form, candidate.identity);
  }
}

void OtpCoordinator::RetryDeferredProactiveTriggers() {
  suggestion_dropdown_dispatcher_.RetryPendingTriggers(
      [this](const OtpTarget& target) {
        OtpTarget retry_target = target;
        if (const std::optional<OtpTarget> designated =
                target_field_identifier_.GetTarget();
            designated.has_value() &&
            GetSiteFrame(*designated) == GetSiteFrame(target) &&
            (target_field_identifier_.IsFieldFocused(designated->field) ||
             !IsTargetActiveAndEligible(target))) {
          retry_target = *designated;
        }
        if (std::optional<OtpCandidate> candidate =
                GetAvailableCandidate(retry_target.field);
            candidate.has_value() &&
            !IsCandidateSuppressedByUsedTokens(retry_target, *candidate)) {
          TryProactiveTrigger(retry_target, *candidate);
        }
      });
}

void OtpCoordinator::RecordFormSuggestionsShown(
    FormGlobalId form_id,
    const OtpTokenIdentity& identity) {
  for (const OtpTarget& target :
       target_field_identifier_.GetEligibleTargets()) {
    if (target.form == form_id) {
      suggestion_dropdown_dispatcher_.RecordShown(target.field, identity);
    }
  }
}

void OtpCoordinator::DropDeliveries(
    base::FunctionRef<bool(const OtpDelivery&)> pred) {
  base::flat_set<url::Origin> affected_origins;
  base::EraseIf(deliveries_, [&](const auto& entry) {
    if (!pred(entry.second)) {
      return false;
    }
    affected_origins.insert(entry.second.target().origin);
    suggestion_dropdown_dispatcher_.ClearField(entry.first);
    return true;
  });
  for (const url::Origin& origin : affected_origins) {
    StopSourcesIfNoRemainingTargets(origin);
  }
}

void OtpCoordinator::StopSourcesIfNoRemainingTargets(
    const url::Origin& origin) {
  base::EraseIf(in_flight_validations_, [&](const auto& entry) {
    return !HasActiveDeliveryForSiteFrame(entry.first);
  });

  if (std::ranges::none_of(deliveries_, [&](const auto& entry) {
        return entry.second.target().origin == origin;
      })) {
    for (auto& source : sources_) {
      source->StopListeningForOrigin(origin);
    }
  }

  if (deliveries_.empty()) {
    ++listening_generation_;
    target_field_identifier_.OnOtpFieldsCleared();
    for (auto& source : sources_) {
      source->Stop();
    }
  }
}

bool OtpCoordinator::IsCandidateSuppressedByUsedTokens(
    const OtpTarget& target,
    const OtpCandidate& candidate) const {
  const base::TimeTicks now = base::TimeTicks::Now();
  const std::vector<UsedToken>* used_tokens =
      base::FindOrNull(used_tokens_per_origin_, target.origin);
  return used_tokens &&
         std::ranges::any_of(*used_tokens, [&](const UsedToken& used) {
           if (IsUsedTokenExpired(used, now)) {
             return false;
           }
           if (candidate.identity.value == used.identity.value) {
             return true;
           }
           return !used.identity.on_device_arrival.is_null() &&
                  !candidate.identity.IsStrictlyNewerThan(used.identity);
         });
}

std::optional<OtpCandidate> OtpCoordinator::GetAvailableCandidate(
    FieldGlobalId field) const {
  const OtpDelivery* delivery = base::FindOrNull(deliveries_, field);
  return delivery ? delivery->GetAvailableCandidate() : std::nullopt;
}

bool OtpCoordinator::HasActiveDeliveryForSiteFrame(
    const SiteFrame& site_frame) const {
  return std::ranges::any_of(deliveries_, [&](const auto& entry) {
    return GetSiteFrame(entry.second.target()) == site_frame;
  });
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
