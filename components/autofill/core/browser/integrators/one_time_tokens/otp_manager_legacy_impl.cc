// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_manager_legacy_impl.h"

#include <memory>
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
#include "components/autofill/core/browser/autofill_field.h"
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
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/logging/log_buffer.h"
#include "components/autofill/core/common/logging/log_macros.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_log_sink.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "components/one_time_tokens/core/browser/one_time_token_service.h"
#include "components/one_time_tokens/core/browser/util/expiring_subscription.h"
#include "components/one_time_tokens/core/common/one_time_token_switches.h"

using one_time_tokens::ExpiringSubscriptionHandle;
using one_time_tokens::OneTimeToken;
using one_time_tokens::OneTimeTokenRetrievalError;
using one_time_tokens::OneTimeTokenService;
using one_time_tokens::OneTimeTokenSource;

namespace autofill {

namespace {

std::string GetMockOtpValue() {
  return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      one_time_tokens::switches::kMockOtpValue);
}

}  // namespace

OtpManagerLegacyImpl::OtpManagerLegacyImpl(
    BrowserAutofillManager& owner,
    OneTimeTokenService* one_time_token_service)
    : owner_(owner), one_time_token_service_(one_time_token_service) {
  autofill_manager_observation_.Observe(&owner);
  if (one_time_token_service_ && owner_->driver().GetParent() == nullptr &&
      !owner_->driver().IsEmbedded() && one_time_token_service_->log_sink()) {
    log_subscription_ = one_time_token_service_->log_sink()->AddLogHandler(
        base::BindRepeating(&OtpManagerLegacyImpl::OnLogMessage,
                            weak_ptr_factory_.GetWeakPtr()));
  }
}

OtpManagerLegacyImpl::~OtpManagerLegacyImpl() {
  if (last_pending_get_suggestions_callback_) {
    std::move(last_pending_get_suggestions_callback_).Run({});
  }
}

void OtpManagerLegacyImpl::GetOtpSuggestions(
    const FormStructure& form,
    const FormFieldData& field,
    OtpManagerLegacyImpl::GetOtpSuggestionsCallback callback) {
  if (!one_time_token_service_ || owner_->driver().IsEmbedded() ||
      field.origin().opaque() || !OtpFieldDetector::IsOtpForm(form)) {
    std::move(callback).Run({});
    return;
  }

  std::string mock_otp = GetMockOtpValue();
  if (!mock_otp.empty()) {
    LOG_AF(owner_->client().GetCurrentLogManager())
        << LoggingScope::kOneTimeTokens
        << "Using mock OTP value from command line switch.";
    std::move(callback).Run({std::move(mock_otp)});
    return;
  }

  if (last_pending_get_suggestions_callback_) {
    std::move(last_pending_get_suggestions_callback_).Run({});
  }
  last_pending_frame_token_ = field.host_frame();
  last_pending_get_suggestions_callback_ = std::move(callback);

  // This queries OTPs from the backend and calls `OnOneTimeTokenReceived` to
  // deliver the OTP to `last_pending_get_suggestions_callback_`.
  GetRecentOtpsAndRenewSubscription();
}

void OtpManagerLegacyImpl::GetRecentOtpsAndRenewSubscription() {
  CHECK(one_time_token_service_);

  // This may call OnOneTimeTokenReceived() zero times, but ...
  one_time_token_service_->GetRecentOneTimeTokens(
      base::BindRepeating(&OtpManagerLegacyImpl::OnOneTimeTokenReceived,
                          weak_ptr_factory_.GetWeakPtr()));

  // ... this guarantees at least one call of OnOneTimeTokenReceived().
  if (sms_otp_subscription_.IsAlive()) {
    sms_otp_subscription_.SetExpirationTime(base::Time::Now() +
                                            kSmsOtpSubscriptionDuration);
  } else {
    sms_otp_subscription_ = one_time_token_service_->Subscribe(
        OneTimeTokenSource::kOnDeviceSms,
        base::Time::Now() + kSmsOtpSubscriptionDuration,
        /*callback=*/
        base::BindRepeating(&OtpManagerLegacyImpl::OnOneTimeTokenReceived,
                            weak_ptr_factory_.GetWeakPtr()),
        /*expiration_callback=*/
        base::BindOnce(
            [](base::WeakPtr<OtpManagerLegacyImpl> self) {
              if (!self) {
                return;
              }
              CHECK(!self->sms_otp_subscription_.IsAlive());
              self->OnOneTimeTokenReceived(
                  OneTimeTokenSource::kOnDeviceSms,
                  base::unexpected(
                      OneTimeTokenRetrievalError::kSubscriptionExpired));
            },
            weak_ptr_factory_.GetWeakPtr()));
  }
}

void OtpManagerLegacyImpl::OnFieldTypesDetermined(
    AutofillManager& manager,
    FormGlobalId form_id,
    AutofillManager::Observer::FieldTypeSource source,
    bool small_forms_were_parsed) {
  // On non-Android platforms and in tests the backend may be not initialized.
  // Furthermore, do not retrieve or subscribe to OTPs for embedded frame trees
  // (such as fenced frames or GuestViews).
  if (!one_time_token_service_ || manager.driver().IsEmbedded()) {
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
    if (field->Type().GetTypes().contains(ONE_TIME_CODE)) {
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
void OtpManagerLegacyImpl::OnBeforeFocusOnFormField(AutofillManager& manager,
                                                    FormGlobalId form,
                                                    FieldGlobalId field) {
  if (!last_pending_get_suggestions_callback_) {
    return;
  }
  // Post the callback asynchronously to prevent re-entrancy when notifying
  // `Observer::OnAfterAskForValuesToFill` from inside this
  // `Observer::OnBeforeFocusOnFormField` notification loop.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(last_pending_get_suggestions_callback_),
                     std::vector<std::string>{}));
}

// This is a workaround to prevent the Keyboard Accessory from popping up when
// an OTP arrives and the keyboard is hidden.
// TODO(crbug.com/451991285): Remove this method once we switch to using
// observers instead of delaying the callback.
void OtpManagerLegacyImpl::OnBeforeFocusOnNonFormField(
    AutofillManager& manager) {
  if (!last_pending_get_suggestions_callback_) {
    return;
  }
  // Post the callback asynchronously to prevent re-entrancy when notifying
  // `Observer::OnAfterAskForValuesToFill` from inside this
  // `Observer::OnBeforeFocusOnNonFormField` notification loop.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(last_pending_get_suggestions_callback_),
                     std::vector<std::string>{}));
}

void OtpManagerLegacyImpl::OnOneTimeTokenReceived(
    OneTimeTokenSource backend_type,
    base::expected<OneTimeToken, OneTimeTokenRetrievalError> token_or_error) {
  switch (backend_type) {
    case OneTimeTokenSource::kOnDeviceSms:
      break;
    case OneTimeTokenSource::kGmail:
      // `GmailOtpBackend` may be rolled out independently of
      // `kAutofillShowGmailOtpSuggestions` (e.g., for Actor or pre-launch
      // metrics), in which case `GetRecentOneTimeTokens()` can emit cached
      // Gmail tokens. Ignore them in the legacy SMS-only manager.
      return;
    case OneTimeTokenSource::kUnknown:
      NOTREACHED();
  }

  if (!last_pending_get_suggestions_callback_) {
    return;
  }

  // If token_or_error holds an error, run the callback with empty otp value.
  if (!token_or_error.has_value()) {
    std::move(last_pending_get_suggestions_callback_).Run({});
    return;
  }

  OneTimeToken& token = *token_or_error;
  if (!token.value().empty()) {
    owner_->GetOtpFormEventLogger().OnOtpAvailable();
  }

  base::OnceCallback<void(OneTimeTokensPhishGuardVerdict)> show_suggestions =
      base::BindOnce(&OtpManagerLegacyImpl::MaybeShowOtpSuggestionsForSms,
                     weak_ptr_factory_.GetWeakPtr(), std::move(token));

  // We run PhishGuard check to make sure OTPs are not shown to users on
  // potential phishing sites.
  if (OtpPhishGuardDelegate* delegate =
          owner_->client().GetOtpPhishGuardDelegate()) {
    base::TimeTicks start_time = base::TimeTicks::Now();
    base::UmaHistogramBoolean(
        "Autofill.OneTimeTokens.PhishGuard.CheckPerformed", true);
    LOG_AF(owner_->client().GetCurrentLogManager())
        << LoggingScope::kOneTimeTokens
        << "PhishGuard check initiated for OTP token delivery.";
    delegate->StartOtpPhishGuardCheck(
        last_pending_frame_token_,
        base::BindOnce(
            [](base::WeakPtr<OtpManagerLegacyImpl> self,
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

void OtpManagerLegacyImpl::MaybeShowOtpSuggestionsForSms(
    OneTimeToken token,
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

  std::vector<std::string> suggestions;
  if (!token.value().empty()) {
    suggestions.emplace_back(std::move(token).value());
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
        << suggestions[0].size() << " (value omitted for privacy).";
  }

  std::move(last_pending_get_suggestions_callback_).Run(std::move(suggestions));
}

bool OtpManagerLegacyImpl::IsOtpDeliveryBlocked() {
  return owner_->client().DocumentUsedWebOTP();
}

void OtpManagerLegacyImpl::OnLogMessage(std::string_view message) {
  LOG_AF(owner_->client().GetCurrentLogManager())
      << LoggingScope::kOneTimeTokens << message;
}

}  // namespace autofill
