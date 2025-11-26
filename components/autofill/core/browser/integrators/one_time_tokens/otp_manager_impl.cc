// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_manager_impl.h"

#include <algorithm>

#include "base/containers/to_vector.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_phish_guard_delegate.h"
#include "components/password_manager/core/browser/features/password_features.h"

using one_time_tokens::ExpiringSubscriptionHandle;
using one_time_tokens::OneTimeToken;
using one_time_tokens::OneTimeTokenRetrievalError;
using one_time_tokens::OneTimeTokenService;
using one_time_tokens::OneTimeTokenSource;
using one_time_tokens::OneTimeTokenType;

namespace autofill {

namespace {
constexpr base::TimeDelta kSubscriptionDuration = base::Minutes(1);
}  // namespace

OtpManagerImpl::OtpManagerImpl(BrowserAutofillManager& owner,
                               OneTimeTokenService* one_time_token_service)
    : owner_(owner), one_time_token_services_(one_time_token_service) {
  autofill_manager_observation_.Observe(&owner);
}

OtpManagerImpl::~OtpManagerImpl() = default;

void OtpManagerImpl::GetOtpSuggestions(
    OtpManagerImpl::GetOtpSuggestionsCallback callback) {
  // TODO(crbug.com/415273270) This is just a hack to prepopulate the OTPs in
  // case no real backend is triggered. The feature definition should migrate to
  // autofill.
  if (base::FeatureList::IsEnabled(
          password_manager::features::kDebugUiForOtps)) {
    std::move(callback).Run({"Identified OTP field."});
    return;
  }

  last_pending_get_suggestions_callback_ = std::move(callback);

  // This queries OTPs from the backend and calls `OnOneTimeTokenReceived` to
  // deliver the OTP to `last_pending_get_suggestions_callback_`.
  GetRecentOtpsAndRenewSubscription();
}

void OtpManagerImpl::GetRecentOtpsAndRenewSubscription() {
  if (!one_time_token_services_) {
    return;
  }

  one_time_token_services_->GetRecentOneTimeTokens(base::BindRepeating(
      &OtpManagerImpl::OnOneTimeTokenReceived, weak_ptr_factory_.GetWeakPtr()));

  if (subscription_.IsAlive()) {
    subscription_.SetExpirationTime(base::Time::Now() + kSubscriptionDuration);
    return;
  }

  subscription_ = one_time_token_services_->Subscribe(
      base::Time::Now() + kSubscriptionDuration,
      base::BindRepeating(&OtpManagerImpl::OnOneTimeTokenReceived,
                          weak_ptr_factory_.GetWeakPtr()));
}

void OtpManagerImpl::OnFieldTypesDetermined(
    AutofillManager& manager,
    FormGlobalId form_id,
    AutofillManager::Observer::FieldTypeSource source) {
  // On non-android platforms and in tests the backend may be not initialized.
  if (!one_time_token_services_) {
    return;
  }

  const autofill::FormStructure* form = manager.FindCachedFormById(form_id);
  if (!form) {
    return;
  }

  const bool form_contains_otp_field = std::ranges::any_of(
      form->fields(), [](const std::unique_ptr<AutofillField>& field) {
        return field->Type().GetTypes().contains(ONE_TIME_CODE);
      });
  if (!form_contains_otp_field) {
    return;
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
  if (!last_pending_get_suggestions_callback_.is_null()) {
    std::move(last_pending_get_suggestions_callback_).Run({});
  }
}

// This is a workaround to prevent the Keyboard Accessory from popping up when
// an OTP arrives and the keyboard is hidden.
// TODO(crbug.com/451991285): Remove this method once we switch to using
// observers instead of delaying the callback.
void OtpManagerImpl::OnBeforeFocusOnNonFormField(AutofillManager& manager) {
  if (!last_pending_get_suggestions_callback_.is_null()) {
    std::move(last_pending_get_suggestions_callback_).Run({});
  }
}

void OtpManagerImpl::OnOneTimeTokenReceived(
    OneTimeTokenSource backend_type,
    base::expected<OneTimeToken, OneTimeTokenRetrievalError> token_or_error) {
  // If token_or_error holds an error, run the callback with empty otp value.
  if (!token_or_error.has_value()) {
    if (!last_pending_get_suggestions_callback_.is_null()) {
      std::move(last_pending_get_suggestions_callback_).Run({});
    }
    return;
  }

  // If we are here, token_or_error holds a OneTimeToken, we check if the
  // callback is valid.
  if (!last_pending_get_suggestions_callback_) {
    return;
  }

  OneTimeToken& token = token_or_error.value();

  // We run PhishGuard check to make sure OTPs are not shown to users on
  // potential phishing sites.
  if (OtpPhishGuardDelegate* delegate =
          owner_->client().GetOtpPhishGuardDelegate()) {
    phish_guard_check_start_time_ = base::TimeTicks::Now();
    base::UmaHistogramBoolean(
        "Autofill.OneTimeTokens.PhishGuard.CheckPerformed", true);
    delegate->StartOtpPhishGuardCheck(
        owner_->client().GetLastCommittedPrimaryMainFrameURL(),
        base::BindOnce(
            [](base::WeakPtr<OtpManagerImpl> self, OneTimeToken token,
               bool is_phishing_site) {
              if (self) {
                self->MaybeShowOtpSuggestions(
                    std::move(token),
                    is_phishing_site
                        ? OneTimeTokensPhishGuardVerdict::kPhishing
                        : OneTimeTokensPhishGuardVerdict::kNotPhishing);
              }
            },
            weak_ptr_factory_.GetWeakPtr(), std::move(token)));
  } else {
    base::UmaHistogramBoolean(
        "Autofill.OneTimeTokens.PhishGuard.CheckPerformed", false);
    MaybeShowOtpSuggestions(std::move(token),
                            OneTimeTokensPhishGuardVerdict::kUnknown);
  }
}

void OtpManagerImpl::MaybeShowOtpSuggestions(
    OneTimeToken token,
    OneTimeTokensPhishGuardVerdict verdict) {
  if (!phish_guard_check_start_time_.is_null()) {
    base::UmaHistogramTimes(
        "Autofill.OneTimeTokens.PhishGuard.Latency",
        base::TimeTicks::Now() - phish_guard_check_start_time_);
  }

  base::UmaHistogramEnumeration("Autofill.OneTimeTokens.PhishGuard.Verdict",
                                verdict);

  if (!last_pending_get_suggestions_callback_) {
    return;
  }

  std::vector<std::string> suggestions;
  if (!token.value().empty()) {
    suggestions.emplace_back(std::move(token).value());
  }

  if (IsOtpDeliveryBlocked() ||
      verdict == OneTimeTokensPhishGuardVerdict::kPhishing) {
    suggestions.clear();
  }

  owner_->GetOtpFormEventLogger().OnOtpAvailable();
  std::move(last_pending_get_suggestions_callback_).Run(std::move(suggestions));
}

bool OtpManagerImpl::IsOtpDeliveryBlocked() {
  return owner_->client().DocumentUsedWebOTP();
}

}  // namespace autofill
