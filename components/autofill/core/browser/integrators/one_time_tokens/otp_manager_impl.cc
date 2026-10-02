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
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/expected.h"
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
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_log_sink.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "components/one_time_tokens/core/browser/one_time_token_service.h"
#include "components/one_time_tokens/core/browser/one_time_token_type.h"
#include "components/one_time_tokens/core/browser/util/expiring_subscription.h"
#include "components/one_time_tokens/core/common/one_time_token_switches.h"

using one_time_tokens::ExpiringSubscriptionHandle;
using one_time_tokens::OneTimeToken;
using one_time_tokens::OneTimeTokenRetrievalError;
using one_time_tokens::OneTimeTokenService;
using one_time_tokens::OneTimeTokenSource;
using one_time_tokens::OneTimeTokenType;

namespace autofill {

namespace {

std::string GetMockOtpValue() {
  return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      one_time_tokens::switches::kMockOtpValue);
}

}  // namespace

OtpManagerImpl::OtpManagerImpl(BrowserAutofillManager& owner,
                               OneTimeTokenService* one_time_token_service)
    : owner_(owner), one_time_token_service_(one_time_token_service) {
  autofill_manager_observation_.Observe(&owner);
  if (one_time_token_service_) {
    if (owner_->driver().GetParent() == nullptr &&
        !owner_->driver().IsEmbedded()) {
      if (one_time_token_service_->log_sink()) {
        log_subscription_ = one_time_token_service_->log_sink()->AddLogHandler(
            base::BindRepeating(&OtpManagerImpl::OnLogMessage,
                                weak_ptr_factory_.GetWeakPtr()));
      }
    }
    gmail_otp_tickle_subscription_ =
        one_time_token_service_->SubscribeToTickles(
            OneTimeTokenSource::kGmail,
            base::Time::Now() + kGmailOtpTickleSubscriptionDuration,
            base::BindRepeating(&OtpManagerImpl::OnTickleReceived,
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

  if (UserOptedIntoGmailOtpFilling()) {
    if (std::optional<OneTimeToken> token = SelectMostRecentToken();
        token && token->type() == OneTimeTokenType::kGmail &&
        !token->value().empty()) {
      LOG_AF(owner_->client().GetCurrentLogManager())
          << LoggingScope::kOneTimeTokens
          << "Evaluating cached Gmail OTP suggestion for delivery.";
      OnOneTimeTokenReceived(OneTimeTokenSource::kGmail, std::move(*token));
    }
  }

  // This queries OTPs from the backend and calls `OnOneTimeTokenReceived` to
  // deliver the OTP to `last_pending_get_suggestions_callback_`.
  GetRecentOtpsAndRenewSubscription();
}

void OtpManagerImpl::GetRecentOtpsAndRenewSubscription() {
  CHECK(one_time_token_service_);

  // This may call OnOneTimeTokenReceived() zero times, but ...
  one_time_token_service_->GetRecentOneTimeTokens(base::BindRepeating(
      &OtpManagerImpl::OnOneTimeTokenReceived, weak_ptr_factory_.GetWeakPtr()));

  // ... this guarantees at least one call of OnOneTimeTokenReceived().
  if (sms_otp_subscription_.IsAlive()) {
    sms_otp_subscription_.SetExpirationTime(base::Time::Now() +
                                            kSmsOtpSubscriptionDuration);
  } else {
    sms_otp_subscription_ = one_time_token_service_->Subscribe(
        OneTimeTokenSource::kOnDeviceSms,
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
              self->OnOneTimeTokenReceived(
                  OneTimeTokenSource::kOnDeviceSms,
                  base::unexpected(
                      OneTimeTokenRetrievalError::kSubscriptionExpired));
            },
            weak_ptr_factory_.GetWeakPtr()));
  }

  if (gmail_otp_tickle_subscription_.IsAlive()) {
    gmail_otp_tickle_subscription_.SetExpirationTime(
        base::Time::Now() + kGmailOtpTickleSubscriptionDuration);
  } else {
    gmail_otp_tickle_subscription_ =
        one_time_token_service_->SubscribeToTickles(
            OneTimeTokenSource::kGmail,
            base::Time::Now() + kGmailOtpTickleSubscriptionDuration,
            base::BindRepeating(&OtpManagerImpl::OnTickleReceived,
                                weak_ptr_factory_.GetWeakPtr()));
  }
}

void OtpManagerImpl::OnFieldTypesDetermined(
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
    // Post the callback asynchronously to prevent re-entrancy when notifying
    // `Observer::OnAfterAskForValuesToFill` from inside this
    // `Observer::OnBeforeFocusOnFormField` notification loop.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(last_pending_get_suggestions_callback_),
                       std::vector<std::string>{}));
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
    // Post the callback asynchronously to prevent re-entrancy when notifying
    // `Observer::OnAfterAskForValuesToFill` from inside this
    // `Observer::OnBeforeFocusOnNonFormField` notification loop.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(last_pending_get_suggestions_callback_),
                       std::vector<std::string>{}));
  }
}

void OtpManagerImpl::OnTickleReceived(OneTimeTokenSource source) {
  LOG_AF(owner_->client().GetCurrentLogManager())
      << LoggingScope::kOneTimeTokens
      << "Tickle received for source: " << static_cast<int>(source);
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
}

void OtpManagerImpl::OnOneTimeTokenReceived(
    OneTimeTokenSource backend_type,
    base::expected<OneTimeToken, OneTimeTokenRetrievalError> token_or_error) {
  if (backend_type != OneTimeTokenSource::kGmail &&
      !last_pending_get_suggestions_callback_) {
    return;
  }

  // Do not process or deliver Gmail OTPs (including previously cached tokens)
  // if the user has not opted into Gmail OTP filling or opted out after a token
  // was cached.
  if (backend_type == OneTimeTokenSource::kGmail &&
      !UserOptedIntoGmailOtpFilling()) {
    if (last_pending_get_suggestions_callback_) {
      std::move(last_pending_get_suggestions_callback_).Run({});
    }
    return;
  }

  // If token_or_error holds an error, run the callback with empty otp value.
  if (!token_or_error.has_value()) {
    if (last_pending_get_suggestions_callback_) {
      std::move(last_pending_get_suggestions_callback_).Run({});
    }
    return;
  }

  LocalFrameToken frame_token = last_pending_frame_token_;
  if (!last_pending_get_suggestions_callback_) {
    // TODO(crbug.com/556170646): Also support proactive triggering when the OTP
    // field is not currently focused in a follow-up CL.
    const AutofillField* target_field = GetFocusedOtpField();
    if (!target_field) {
      return;
    }
    frame_token = target_field->host_frame();
  }

  OneTimeToken& token = *token_or_error;
  if (!token.value().empty()) {
    owner_->GetOtpFormEventLogger().OnOtpAvailable();
  }

  base::OnceCallback<void(OneTimeTokensPhishGuardVerdict)> show_suggestions;
  switch (backend_type) {
    case OneTimeTokenSource::kGmail:
      show_suggestions = base::BindOnce(
          &OtpManagerImpl::MaybeShowOtpSuggestionsForGmail,
          weak_ptr_factory_.GetWeakPtr(), std::move(token), frame_token);
      break;
    case OneTimeTokenSource::kOnDeviceSms:
      show_suggestions =
          base::BindOnce(&OtpManagerImpl::MaybeShowOtpSuggestionsForSms,
                         weak_ptr_factory_.GetWeakPtr(), std::move(token));
      break;
    case OneTimeTokenSource::kUnknown:
      NOTREACHED();
  }

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
    std::vector<std::string> suggestions;
    if (verdict == OneTimeTokensPhishGuardVerdict::kPhishing) {
      LOG_AF(owner_->client().GetCurrentLogManager())
          << LoggingScope::kOneTimeTokens << LogMessage::kSuggestionSuppressed
          << "Reason: PhishGuard verdict is phishing.";
    } else if (!token.value().empty()) {
      LOG_AF(owner_->client().GetCurrentLogManager())
          << LoggingScope::kOneTimeTokens
          << "Delivering OTP suggestion to UI. Token length: "
          << token.value().size() << " (value omitted for privacy).";
      last_triggered_otp_value_ = token.value();
      suggestions.emplace_back(std::move(token).value());
    }
    std::move(last_pending_get_suggestions_callback_)
        .Run(std::move(suggestions));
    return;
  }

  // Proactive path: no UI callback is pending, so trigger suggestions via the
  // renderer if the OTP and target field are eligible.
  if (verdict == OneTimeTokensPhishGuardVerdict::kPhishing) {
    LOG_AF(owner_->client().GetCurrentLogManager())
        << LoggingScope::kOneTimeTokens << LogMessage::kSuggestionSuppressed
        << "Reason: PhishGuard verdict is phishing.";
    return;
  }

  if (token.value().empty()) {
    return;
  }

  // TODO(crbug.com/556170646): Also support proactive triggering when the OTP
  // field is not currently focused in a follow-up CL.
  const AutofillField* target_field = GetFocusedOtpField();
  if (!target_field || target_field->host_frame() != frame_token) {
    LOG_AF(owner_->client().GetCurrentLogManager())
        << LoggingScope::kOneTimeTokens
        << "Focused field is not the expected OTP field or frame has changed. "
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

const AutofillField* OtpManagerImpl::GetFocusedOtpField() const {
  // TODO(crbug.com/556170646): Rework this method to return a target
  // `ONE_TIME_CODE` field from cached forms even when no OTP field is currently
  // focused, so proactive Gmail OTP suggestions can be triggered without
  // requiring the OTP field to have focus.
  if (!currently_focused_field_id_.has_value()) {
    return nullptr;
  }
  const AutofillField* field = nullptr;
  if (currently_focused_form_id_.has_value()) {
    field = owner_
                ->FindFormAndField(*currently_focused_form_id_,
                                   *currently_focused_field_id_)
                .autofill_field;
  }
  if (!field) {
    // AutofillManager provides an overload `FindCachedFormById(const
    // FieldGlobalId&)` that searches cached forms for the one containing the
    // given field ID.
    if (const FormStructure* form =
            owner_->FindCachedFormById(*currently_focused_field_id_)) {
      field = form->GetFieldById(*currently_focused_field_id_);
    }
  }
  return field && field->Type().GetTypes().contains(ONE_TIME_CODE) ? field
                                                                   : nullptr;
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
    has_typed_input = std::ranges::any_of(form.fields(), [](const auto& field) {
      return field->Type().GetTypes().contains(ONE_TIME_CODE) &&
             field->all_modifiers().contains(FieldModifier::kUser);
    });
  });
  return has_typed_input;
}

bool OtpManagerImpl::UserOptedIntoGmailOtpFilling() const {
  PrefService* prefs = owner_->client().GetPrefs();
  return prefs && prefs::IsAutofillGmailOtpFillingEnabled(prefs);
}

std::optional<OneTimeToken> OtpManagerImpl::SelectMostRecentToken(
    std::optional<OneTimeTokenType> type) const {
  if (!one_time_token_service_) {
    return std::nullopt;
  }
  base::TimeTicks now = base::TimeTicks::Now();
  std::vector<OneTimeToken> cached_tokens =
      one_time_token_service_->GetCachedOneTimeTokens();
  const OneTimeToken* most_recent = nullptr;
  for (const OneTimeToken& token : cached_tokens) {
    if ((type.has_value() && token.type() != *type) ||
        token.on_device_arrival_time().is_null() ||
        now - token.on_device_arrival_time() >
            one_time_tokens::kCacheDurationForOldTokens) {
      continue;
    }
    if (!most_recent || token.on_device_arrival_time() >
                            most_recent->on_device_arrival_time()) {
      most_recent = &token;
    }
  }
  return most_recent ? std::optional(*most_recent) : std::nullopt;
}

void OtpManagerImpl::OnLogMessage(std::string_view message) {
  LOG_AF(owner_->client().GetCurrentLogManager())
      << LoggingScope::kOneTimeTokens << message;
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
