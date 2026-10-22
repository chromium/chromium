// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_manager_impl.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/affiliations/core/browser/affiliation_service.h"
#include "components/affiliations/core/browser/domain_matching/domain_relation_checker.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_trigger_source.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_field_detector.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_metrics_tracker.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_phish_guard_delegate.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/logging/log_buffer.h"
#include "components/autofill/core/common/logging/log_macros.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/one_time_tokens/core/browser/gmail_otp_backend.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "components/one_time_tokens/core/browser/one_time_token_type.h"
#include "components/one_time_tokens/core/browser/sms_otp_service.h"
#include "components/one_time_tokens/core/browser/util/expiring_subscription.h"
#include "components/one_time_tokens/core/common/one_time_token_switches.h"

using one_time_tokens::ExpiringSubscriptionHandle;
using one_time_tokens::OneTimeToken;
using one_time_tokens::OneTimeTokenRetrievalError;
using one_time_tokens::OneTimeTokenType;
using one_time_tokens::SmsOtpService;

namespace autofill {

namespace {

std::string GetMockOtpValue() {
  return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      one_time_tokens::switches::kMockOtpValue);
}

}  // namespace

OtpManagerImpl::OtpManagerImpl(BrowserAutofillManager& owner,
                               SmsOtpService* sms_otp_service)
    : owner_(owner), sms_otp_service_(sms_otp_service) {
  autofill_manager_observation_.Observe(&owner);
  if (PrefService* prefs = owner_->client().GetPrefs()) {
    pref_change_registrar_.Init(prefs);
    pref_change_registrar_.Add(
        prefs::kAutofillGmailOtpFillingEnabled,
        base::BindRepeating(
            &OtpManagerImpl::OnGmailOtpFillingEnabledPrefChanged,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

OtpManagerImpl::~OtpManagerImpl() {
  if (last_pending_get_suggestions_callback_) {
    std::move(last_pending_get_suggestions_callback_).Run({});
  }
}

void OtpManagerImpl::GetOtpSuggestions(
    const FormStructure& form,
    const FormFieldData& field,
    OtpManagerImpl::GetOtpSuggestionsCallback callback) {
  if (!sms_otp_service_ || owner_->driver().IsEmbedded() ||
      !OtpFieldDetector::IsOtpField(form, field) ||
      !OtpFieldDetector::IsOtpForm(form)) {
    std::move(callback).Run({});
    return;
  }

  std::string mock_otp = GetMockOtpValue();
  if (!mock_otp.empty()) {
    LOG_AF(owner_->client().GetCurrentLogManager())
        << LoggingScope::kOneTimeTokens
        << "Using mock OTP value from command line switch.";
    std::move(callback).Run(
        {OneTimeToken(OneTimeTokenType::kSmsOtp, std::move(mock_otp),
                      base::TimeTicks::Now())});
    return;
  }

  if (last_pending_get_suggestions_callback_) {
    std::move(last_pending_get_suggestions_callback_).Run({});
  }
  is_gmail_phish_guard_pending_ = false;
  last_pending_frame_token_ = field.host_frame();
  last_pending_get_suggestions_callback_ = std::move(callback);

  // This queries OTPs from the backend and calls `OnOneTimeTokenReceived` to
  // deliver the OTP to `last_pending_get_suggestions_callback_`.
  GetRecentOtpsAndRenewSubscription();
}

void OtpManagerImpl::GetRecentOtpsAndRenewSubscription() {
  CHECK(sms_otp_service_);

  one_time_tokens::GmailOtpBackend* gmail_otp_backend =
      owner_->client().GetGmailOtpBackend();

  const base::Time expiration_time =
      base::Time::Now() + kGmailOtpTickleSubscriptionDuration;
  if (gmail_otp_tickle_subscription_.IsAlive()) {
    gmail_otp_tickle_subscription_.SetExpirationTime(expiration_time);
  } else if (gmail_otp_backend) {
    gmail_otp_tickle_subscription_ = gmail_otp_backend->SubscribeToTickles(
        expiration_time, base::BindRepeating(&OtpManagerImpl::OnTickleReceived,
                                             weak_ptr_factory_.GetWeakPtr()));
  }

  if (UserOptedIntoGmailOtpFilling()) {
    if (std::optional<OneTimeToken> token = SelectMostRecentToken();
        token && !token->value().empty()) {
      LOG_AF(owner_->client().GetCurrentLogManager())
          << LoggingScope::kOneTimeTokens
          << "Delivering cached Gmail OTP suggestion.";
      OnOneTimeTokenReceived(std::move(*token));
    } else {
      MaybeRetrieveGmailOtp();
    }
  }

  // Gmail OTPs are retrieved from the `GmailOtpBackend` via
  // `GmailOtpRetriever` (verifying domain affiliation against the frame
  // origin). Therefore, only SMS OTPs are taken from the `SmsOtpService`
  // and any unverified Gmail OTPs returned by it are dropped.
  // This may call OnOneTimeTokenReceived() zero times, but ...
  sms_otp_service_->GetRecentOneTimeTokens(base::BindRepeating(
      [](base::WeakPtr<OtpManagerImpl> self,
         base::expected<OneTimeToken, OneTimeTokenRetrievalError> result) {
        if (self && (!result.has_value() ||
                     result->type() == OneTimeTokenType::kSmsOtp)) {
          self->OnOneTimeTokenReceived(std::move(result));
        }
      },
      weak_ptr_factory_.GetWeakPtr()));

  // ... this guarantees at least one call of OnOneTimeTokenReceived().
  if (sms_otp_subscription_.IsAlive()) {
    sms_otp_subscription_.SetExpirationTime(base::Time::Now() +
                                            kSmsOtpSubscriptionDuration);
  } else {
    sms_otp_subscription_ = sms_otp_service_->Subscribe(
        base::Time::Now() + kSmsOtpSubscriptionDuration,
        /*callback=*/
        base::BindRepeating(&OtpManagerImpl::OnOneTimeTokenReceived,
                            weak_ptr_factory_.GetWeakPtr()),
        /*expiration_callback=*/
        base::BindOnce(
            [](base::WeakPtr<OtpManagerImpl> self) {
              if (!self) {
                return;
              }
              CHECK(!self->sms_otp_subscription_.IsAlive());
              self->OnOneTimeTokenReceived(base::unexpected(
                  OneTimeTokenRetrievalError::kSubscriptionExpired));
            },
            weak_ptr_factory_.GetWeakPtr()));
  }
}

void OtpManagerImpl::MaybeRetrieveGmailOtp() {
  // If a Gmail OTP retrieval is already in flight (e.g. initiated upon tickle
  // arrival while the field was focused), do not start a concurrent one. The
  // in-flight retriever will deliver the token to any pending suggestions
  // callback upon completion.
  if (gmail_otp_retriever_) {
    return;
  }

  affiliations::AffiliationService* affiliation_service =
      owner_->client().GetAffiliationService();
  one_time_tokens::GmailOtpBackend* gmail_otp_backend =
      owner_->client().GetGmailOtpBackend();
  if (std::optional<OneTimeToken> token = SelectMostRecentToken();
      affiliation_service && gmail_otp_backend &&
      (!token || token->value().empty()) && UserOptedIntoGmailOtpFilling()) {
    auto checker = std::make_unique<affiliations::DomainRelationChecker>(
        *affiliation_service);
    const url::Origin frame_origin =
        owner_->client().GetLastCommittedPrimaryMainFrameOrigin();
    // TODO(crbug.com/571474857): Clarify security requirements for login flow
    // classification in non-agentic OTP autofill.
    gmail_otp_retriever_ = one_time_tokens::GmailOtpRetriever::CreateAndStart(
        *gmail_otp_backend, std::move(checker), frame_origin,
        /*is_login_flow=*/false,
        base::BindOnce(&OtpManagerImpl::OnGmailOtpRetrieved,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void OtpManagerImpl::OnGmailOtpFillingEnabledPrefChanged() {
  if (!sms_otp_service_ || owner_->driver().IsEmbedded() ||
      !owner_->driver().IsActive() || !UserOptedIntoGmailOtpFilling() ||
      !IsOtpFieldDetected() || AnyOtpFieldContainsTypedInput()) {
    return;
  }
  bool has_otp_form_in_frame = false;
  owner_->ForEachCachedForm(
      [&has_otp_form_in_frame](const FormStructure& form) {
        if (has_otp_form_in_frame) {
          return;
        }
        if (OtpFieldDetector::IsOtpForm(form)) {
          has_otp_form_in_frame = true;
        }
      });
  if (!has_otp_form_in_frame) {
    return;
  }
  MaybeRetrieveGmailOtp();
}

void OtpManagerImpl::OnFieldTypesDetermined(
    AutofillManager& manager,
    FormGlobalId form_id,
    AutofillManager::Observer::FieldTypeSource source,
    bool small_forms_were_parsed) {
  // On non-Android platforms and in tests the backend may be not initialized.
  // Furthermore, do not retrieve or subscribe to OTPs for embedded frame trees
  // (such as fenced frames or GuestViews).
  if (!sms_otp_service_ || manager.driver().IsEmbedded()) {
    return;
  }

  const FormStructure* form = owner_->FindCachedFormById(form_id);
  if (!form || !OtpFieldDetector::IsOtpForm(*form)) {
    return;
  }

  if (!GetMockOtpValue().empty()) {
    return;
  }

  std::vector<FieldGlobalId> otp_field_ids;
  for (const auto& field : form->fields()) {
    if (OtpFieldDetector::IsOtpField(*form, *field)) {
      otp_field_ids.push_back(field->global_id());
    }
  }

  LOG_AF(owner_->client().GetCurrentLogManager())
      << LoggingScope::kOneTimeTokens << "OTP field detected in web form."
      << Br{} << "Form ID: " << form_id;

  if (OtpMetricsTracker* tracker = owner_->client().GetOtpMetricsTracker()) {
    tracker->OnOtpFieldDetected(form_id, std::move(otp_field_ids), *owner_);
  }

  GetRecentOtpsAndRenewSubscription();
}

// This is a workaround to prevent the Keyboard Accessory from popping up when
// an OTP arrives and the keyboard is hidden.
// TODO(crbug.com/451991285): Remove this method once we switch to using
// observers instead of delaying the callback.
void OtpManagerImpl::OnBeforeFocusOnFormField(AutofillManager& manager,
                                              FormGlobalId form,
                                              FieldGlobalId field) {
  currently_focused_form_id_ = form;
  if (currently_focused_field_id_ != field) {
    currently_focused_field_id_ = field;
    // Reset deduplication state when focus changes so the same OTP can be
    // suggested again if the user focuses another OTP field or re-focuses this
    // one.
    last_triggered_otp_value_.clear();
  }

  if (last_pending_get_suggestions_callback_) {
    is_gmail_phish_guard_pending_ = false;
    // Post the callback asynchronously to prevent re-entrancy when notifying
    // `Observer::OnAfterAskForValuesToFill` from inside this
    // `Observer::OnBeforeFocusOnFormField` notification loop.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(last_pending_get_suggestions_callback_),
                       std::vector<OneTimeToken>{}));
  }
}

// This is a workaround to prevent the Keyboard Accessory from popping up when
// an OTP arrives and the keyboard is hidden.
// TODO(crbug.com/451991285): Remove this method once we switch to using
// observers instead of delaying the callback.
void OtpManagerImpl::OnBeforeFocusOnNonFormField(AutofillManager& manager) {
  currently_focused_form_id_.reset();
  currently_focused_field_id_.reset();
  last_triggered_otp_value_.clear();

  if (last_pending_get_suggestions_callback_) {
    is_gmail_phish_guard_pending_ = false;
    // Post the callback asynchronously to prevent re-entrancy when notifying
    // `Observer::OnAfterAskForValuesToFill` from inside this
    // `Observer::OnBeforeFocusOnNonFormField` notification loop.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(last_pending_get_suggestions_callback_),
                       std::vector<OneTimeToken>{}));
  }
}

// Verifies preconditions (OTP field presence, user consent, no existing input,
// etc.) and starts an asynchronous Gmail OTP retrieval for the target field's
// origin.
void OtpManagerImpl::OnTickleReceived() {
  LOG_AF(owner_->client().GetCurrentLogManager())
      << LoggingScope::kOneTimeTokens << "Gmail OTP tickle received.";
  if (!IsOtpFieldDetected()) {
    LOG_AF(owner_->client().GetCurrentLogManager())
        << LoggingScope::kOneTimeTokens
        << "OTP tickle received but no OTP field detected on page. Skipping "
           "payload fetch.";
    return;
  }
  if (AnyOtpFieldContainsTypedInput()) {
    LOG_AF(owner_->client().GetCurrentLogManager())
        << LoggingScope::kOneTimeTokens
        << "OTP tickle received but OTP field already contains user typed "
           "input. Skipping payload fetch.";
    return;
  }
  if (!UserOptedIntoGmailOtpFilling()) {
    LOG_AF(owner_->client().GetCurrentLogManager())
        << LoggingScope::kOneTimeTokens
        << "OTP tickle received but user consent preference is disabled. "
           "Skipping payload fetch.";
    return;
  }
  const AutofillField* target_field = GetTargetOtpField();
  if (!target_field) {
    LOG_AF(owner_->client().GetCurrentLogManager())
        << LoggingScope::kOneTimeTokens
        << "OTP tickle received but no target OTP field found. "
           "Skipping payload fetch.";
    return;
  }
  if (gmail_otp_retriever_) {
    LOG_AF(owner_->client().GetCurrentLogManager())
        << LoggingScope::kOneTimeTokens
        << "GmailOtpRetriever is already in flight. Skipping token retrieval "
           "on tickle.";
    return;
  }

  affiliations::AffiliationService* affiliation_service =
      owner_->client().GetAffiliationService();
  one_time_tokens::GmailOtpBackend* gmail_otp_backend =
      owner_->client().GetGmailOtpBackend();
  if (!affiliation_service || !gmail_otp_backend) {
    LOG_AF(owner_->client().GetCurrentLogManager())
        << LoggingScope::kOneTimeTokens
        << "OTP tickle received but AffiliationService or GmailOtpBackend is "
           "missing. Skipping payload fetch.";
    return;
  }

  LOG_AF(owner_->client().GetCurrentLogManager())
      << LoggingScope::kOneTimeTokens
      << "Starting GmailOtpRetriever on tickle arrival.";
  auto checker = std::make_unique<affiliations::DomainRelationChecker>(
      *affiliation_service);
  const url::Origin frame_origin =
      target_field->origin().opaque()
          ? owner_->client().GetLastCommittedPrimaryMainFrameOrigin()
          : target_field->origin();
  // TODO(crbug.com/571474857): Clarify security requirements for login flow
  // classification in non-agentic OTP autofill.
  gmail_otp_retriever_ = one_time_tokens::GmailOtpRetriever::CreateAndStart(
      *gmail_otp_backend, std::move(checker), frame_origin,
      /*is_login_flow=*/false,
      base::BindOnce(&OtpManagerImpl::OnGmailOtpRetrieved,
                     weak_ptr_factory_.GetWeakPtr()));
}

void OtpManagerImpl::OnGmailOtpRetrieved(
    base::expected<one_time_tokens::GmailOtpRetriever::Result,
                   OneTimeTokenRetrievalError> result) {
  if (gmail_otp_retriever_) {
    // OnGmailOtpRetrieved is invoked as a callback by gmail_otp_retriever_
    // which means that gmail_otp_retriever_ is still on call stack at this
    // point.
    // Its deletion has to happen later, so we have to post a task instead of
    // deleting it right here.
    base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(gmail_otp_retriever_));
  }
  // TODO(crbug.com/556170395): figure out correct behaviour for cases when OTP
  // values are absent, or fetching resulted in an error. Running the callback
  // right here has a potential of breaking the case when the page is actually
  // waiting for an SMS OTP which ends up arriving a bit later.
  if (!result) {
    LOG_AF(owner_->client().GetCurrentLogManager())
        << LoggingScope::kOneTimeTokens
        << "OTP retrieval resulted in a failure. Error code: "
        << std::to_underlying(result.error());
    return;
  }
  if (result->token.value().empty()) {
    LOG_AF(owner_->client().GetCurrentLogManager())
        << LoggingScope::kOneTimeTokens
        << "Retrieved an empty OTP value, no suggestions will be generated.";
    return;
  }

  if (UserOptedIntoGmailOtpFilling()) {
    last_retrieved_gmail_otp_ = result->token;
  }
  OnOneTimeTokenReceived(std::move(result->token));
}

void OtpManagerImpl::OnOneTimeTokenReceived(
    base::expected<OneTimeToken, OneTimeTokenRetrievalError> token_or_error) {
  // Errors can originate from both the SMS and the Gmail OTP flows. Like SMS
  // OTPs, they must not consume `last_pending_get_suggestions_callback_` while
  // a PhishGuard check for a Gmail OTP is pending. Otherwise, the callback is
  // run with empty suggestions.
  if (!token_or_error) {
    if (last_pending_get_suggestions_callback_ &&
        !is_gmail_phish_guard_pending_) {
      std::move(last_pending_get_suggestions_callback_).Run({});
    }
    return;
  }

  OneTimeToken& token = *token_or_error;
  const bool is_gmail_otp = token.type() == OneTimeTokenType::kGmail;
  if (!is_gmail_otp && (!last_pending_get_suggestions_callback_ ||
                        is_gmail_phish_guard_pending_)) {
    return;
  }

  // Do not process or deliver Gmail OTPs (including previously cached tokens)
  // if the user has not opted into Gmail OTP filling or opted out after a token
  // was cached.
  if (is_gmail_otp && !UserOptedIntoGmailOtpFilling()) {
    last_retrieved_gmail_otp_.reset();
    if (last_pending_get_suggestions_callback_) {
      is_gmail_phish_guard_pending_ = false;
      std::move(last_pending_get_suggestions_callback_).Run({});
    }
    return;
  }

  LocalFrameToken frame_token = last_pending_frame_token_;
  if (!last_pending_get_suggestions_callback_) {
    const AutofillField* target_field = GetTargetOtpField();
    if (!target_field) {
      return;
    }
    frame_token = target_field->host_frame();
  }

  if (!token.value().empty()) {
    owner_->GetOtpFormEventLogger().OnOtpAvailable();
  }

  base::OnceCallback<void(OneTimeTokensPhishGuardVerdict)> show_suggestions;
  switch (token.type()) {
    case OneTimeTokenType::kGmail:
      show_suggestions = base::BindOnce(
          &OtpManagerImpl::MaybeShowOtpSuggestionsForGmail,
          weak_ptr_factory_.GetWeakPtr(), std::move(token), frame_token);
      break;
    case OneTimeTokenType::kSmsOtp:
      show_suggestions = base::BindOnce(
          &OtpManagerImpl::MaybeShowOtpSuggestionsForSms,
          weak_ptr_factory_.GetWeakPtr(), std::move(token), frame_token);
      break;
  }

  // We run PhishGuard check to make sure OTPs are not shown to users on
  // potential phishing sites.
  if (OtpPhishGuardDelegate* delegate =
          owner_->client().GetOtpPhishGuardDelegate()) {
    if (is_gmail_otp && last_pending_get_suggestions_callback_) {
      is_gmail_phish_guard_pending_ = true;
    }
    base::TimeTicks start_time = base::TimeTicks::Now();
    base::UmaHistogramBoolean(
        "Autofill.OneTimeTokens.PhishGuard.CheckPerformed", true);
    LOG_AF(owner_->client().GetCurrentLogManager())
        << LoggingScope::kOneTimeTokens
        << "PhishGuard check initiated for OTP token delivery.";
    delegate->StartOtpPhishGuardCheck(
        frame_token,
        base::BindOnce(
            [](base::WeakPtr<OtpManagerImpl> self,
               base::OnceCallback<void(OneTimeTokensPhishGuardVerdict)>
                   show_suggestions,
               base::TimeTicks start_time, bool is_phishing_site) {
              if (!self) {
                return;
              }
              base::UmaHistogramTimes(
                  "Autofill.OneTimeTokens.PhishGuard.Latency",
                  base::TimeTicks::Now() - start_time);
              std::move(show_suggestions)
                  .Run(is_phishing_site
                           ? OneTimeTokensPhishGuardVerdict::kPhishing
                           : OneTimeTokensPhishGuardVerdict::kNotPhishing);
            },
            weak_ptr_factory_.GetWeakPtr(), std::move(show_suggestions),
            start_time));
  } else {
    base::UmaHistogramBoolean(
        "Autofill.OneTimeTokens.PhishGuard.CheckPerformed", false);
    std::move(show_suggestions).Run(OneTimeTokensPhishGuardVerdict::kUnknown);
  }
}

void OtpManagerImpl::MaybeShowOtpSuggestionsForSms(
    OneTimeToken token,
    LocalFrameToken frame_token,
    OneTimeTokensPhishGuardVerdict verdict) {
  LOG_AF(owner_->client().GetCurrentLogManager())
      << LoggingScope::kOneTimeTokens
      << "PhishGuard check completed with verdict: " << verdict;

  base::UmaHistogramEnumeration("Autofill.OneTimeTokens.PhishGuard.Verdict",
                                verdict);

  if (!last_pending_get_suggestions_callback_) {
    LOG_AF(owner_->client().GetCurrentLogManager())
        << LoggingScope::kOneTimeTokens
        << "No pending callback, skipping further processing.";
    return;
  }

  // If a new suggestion request was started for a different frame while this
  // PhishGuard check was in flight, ignore this stale result so we neither
  // deliver an OTP to an unverified frame nor cancel the new frame's pending
  // callback.
  if (last_pending_frame_token_ != frame_token) {
    return;
  }

  std::vector<OneTimeToken> suggestions;
  if (!token.value().empty()) {
    suggestions.push_back(std::move(token));
  }

  if (IsOtpDeliveryBlocked()) {
    LOG_AF(owner_->client().GetCurrentLogManager())
        << LoggingScope::kOneTimeTokens << LogMessage::kSuggestionSuppressed
        << "Reason: OTP delivery is blocked due to the WebOTP API.";
    suggestions.clear();
  } else if (verdict == OneTimeTokensPhishGuardVerdict::kPhishing) {
    LOG_AF(owner_->client().GetCurrentLogManager())
        << LoggingScope::kOneTimeTokens << LogMessage::kSuggestionSuppressed
        << "Reason: PhishGuard verdict is phishing.";
    suggestions.clear();
  } else if (!suggestions.empty()) {
    LOG_AF(owner_->client().GetCurrentLogManager())
        << LoggingScope::kOneTimeTokens
        << "Delivering OTP suggestion to UI. Token length: "
        << suggestions[0].value().size() << " (value omitted for privacy).";
  }

  std::move(last_pending_get_suggestions_callback_).Run(std::move(suggestions));
}

void OtpManagerImpl::MaybeShowOtpSuggestionsForGmail(
    OneTimeToken token,
    LocalFrameToken frame_token,
    OneTimeTokensPhishGuardVerdict verdict) {
  LOG_AF(owner_->client().GetCurrentLogManager())
      << LoggingScope::kOneTimeTokens
      << "PhishGuard check completed with verdict: " << verdict;

  base::UmaHistogramEnumeration("Autofill.OneTimeTokens.PhishGuard.Verdict",
                                verdict);

  // Reactive path: a UI suggestion callback is currently pending.
  if (last_pending_get_suggestions_callback_) {
    // If a new suggestion request was started for a different frame while this
    // PhishGuard check was in flight, ignore this stale result so we neither
    // deliver an OTP to an unverified frame nor cancel the new frame's pending
    // callback.
    if (last_pending_frame_token_ != frame_token) {
      return;
    }
    is_gmail_phish_guard_pending_ = false;
    std::vector<OneTimeToken> suggestions;
    if (verdict == OneTimeTokensPhishGuardVerdict::kPhishing) {
      last_retrieved_gmail_otp_.reset();
      LOG_AF(owner_->client().GetCurrentLogManager())
          << LoggingScope::kOneTimeTokens << LogMessage::kSuggestionSuppressed
          << "Reason: PhishGuard verdict is phishing.";
    } else if (!token.value().empty()) {
      LOG_AF(owner_->client().GetCurrentLogManager())
          << LoggingScope::kOneTimeTokens
          << "Delivering OTP suggestion to UI. Token length: "
          << token.value().size() << " (value omitted for privacy).";
      last_triggered_otp_value_ = token.value();
      suggestions.push_back(std::move(token));
    }
    std::move(last_pending_get_suggestions_callback_)
        .Run(std::move(suggestions));
    return;
  }

  // Proactive path: no UI callback is pending, so trigger suggestions via the
  // renderer if the OTP and target field are eligible.
  if (verdict == OneTimeTokensPhishGuardVerdict::kPhishing) {
    last_retrieved_gmail_otp_.reset();
    LOG_AF(owner_->client().GetCurrentLogManager())
        << LoggingScope::kOneTimeTokens << LogMessage::kSuggestionSuppressed
        << "Reason: PhishGuard verdict is phishing.";
    return;
  }

  if (token.value().empty()) {
    return;
  }

  const AutofillField* target_field = GetTargetOtpField();
  if (!target_field || target_field->host_frame() != frame_token) {
    LOG_AF(owner_->client().GetCurrentLogManager())
        << LoggingScope::kOneTimeTokens
        << "Target OTP field is missing or frame has changed. "
           "Skipping proactive suggestion trigger.";
    return;
  }

  if (AnyOtpFieldContainsTypedInput()) {
    LOG_AF(owner_->client().GetCurrentLogManager())
        << LoggingScope::kOneTimeTokens
        << "OTP field contains user input. Skipping proactive suggestion "
           "trigger.";
    return;
  }

  if (token.value() == last_triggered_otp_value_) {
    LOG_AF(owner_->client().GetCurrentLogManager())
        << LoggingScope::kOneTimeTokens
        << "OTP token matches the last triggered value. Skipping duplicate "
           "trigger.";
    return;
  }

  last_triggered_otp_value_ = std::move(token).value();
  LOG_AF(owner_->client().GetCurrentLogManager())
      << LoggingScope::kOneTimeTokens
      << "Proactively triggering suggestions via renderer for OTP field.";
  owner_->driver().RendererShouldTriggerSuggestions(
      target_field->global_id(),
      AutofillSuggestionTriggerSource::kGmailOneTimePasswordAvailable);
}

const AutofillField* OtpManagerImpl::GetTargetOtpField() const {
  if (currently_focused_field_id_) {
    const FormStructure* form = nullptr;
    const AutofillField* field = nullptr;
    if (currently_focused_form_id_) {
      AutofillManager::FormAndField form_and_field = owner_->FindFormAndField(
          *currently_focused_form_id_, *currently_focused_field_id_);
      form = form_and_field.form_structure;
      field = form_and_field.autofill_field;
    }
    if (!field) {
      // AutofillManager provides an overload `FindCachedFormById(const
      // FieldGlobalId&)` that searches cached forms for the one containing the
      // given field ID.
      form = owner_->FindCachedFormById(*currently_focused_field_id_);
      if (form) {
        field = form->GetFieldById(*currently_focused_field_id_);
      }
    }
    return (form && field && OtpFieldDetector::IsOtpForm(*form) &&
            OtpFieldDetector::IsOtpField(*form, *field))
               ? field
               : nullptr;
  }

  const AutofillField* target_field = nullptr;
  owner_->ForEachCachedForm([&target_field](const FormStructure& form) {
    if (target_field || !OtpFieldDetector::IsOtpForm(form)) {
      return;
    }
    for (const std::unique_ptr<AutofillField>& field : form.fields()) {
      if (OtpFieldDetector::IsOtpField(form, *field)) {
        target_field = field.get();
        return;
      }
    }
  });
  return target_field;
}

bool OtpManagerImpl::IsOtpDeliveryBlocked() {
  return owner_->client().DocumentUsedWebOTP();
}

bool OtpManagerImpl::IsOtpFieldDetected() const {
  OtpFieldDetector* detector = owner_->client().GetOtpFieldDetector();
  return detector && detector->IsOtpFieldPresent();
}

bool OtpManagerImpl::AnyOtpFieldContainsTypedInput() const {
  bool has_typed_input = false;
  owner_->ForEachCachedForm([&has_typed_input](const FormStructure& form) {
    if (has_typed_input) {
      return;
    }
    has_typed_input =
        std::ranges::any_of(form.fields(), [&form](const auto& field) {
          return OtpFieldDetector::IsOtpField(form, *field) &&
                 field->all_modifiers().contains(FieldModifier::kUser);
        });
  });
  return has_typed_input;
}

bool OtpManagerImpl::UserOptedIntoGmailOtpFilling() const {
  PrefService* prefs = owner_->client().GetPrefs();
  return prefs && prefs::IsAutofillGmailOtpFillingEnabled(prefs);
}

std::optional<OneTimeToken> OtpManagerImpl::SelectMostRecentToken() const {
  if (!last_retrieved_gmail_otp_ ||
      last_retrieved_gmail_otp_->on_device_arrival_time().is_null() ||
      base::TimeTicks::Now() -
              last_retrieved_gmail_otp_->on_device_arrival_time() >
          one_time_tokens::kCacheDurationForOldTokens) {
    return std::nullopt;
  }
  return last_retrieved_gmail_otp_;
}

LogBuffer& operator<<(LogBuffer& buffer,
                      OneTimeTokensPhishGuardVerdict verdict) {
  switch (verdict) {
    case OneTimeTokensPhishGuardVerdict::kUnknown:
      return buffer << "kUnknown";
    case OneTimeTokensPhishGuardVerdict::kPhishing:
      return buffer << "kPhishing";
    case OneTimeTokensPhishGuardVerdict::kNotPhishing:
      return buffer << "kNotPhishing";
  }
  NOTREACHED();
}

}  // namespace autofill
