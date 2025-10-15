// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_manager_impl.h"

#include <algorithm>

#include "base/containers/to_vector.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
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

void OtpManagerImpl::OnOneTimeTokenReceived(
    OneTimeTokenSource backend_type,
    std::variant<OneTimeToken, OneTimeTokenRetrievalError> token_or_error) {
  // TODO(crbug.com/415272524): Record metrics on how often the retrieval
  // succeeds or fails, in combination with the OTP source.
  if (std::holds_alternative<OneTimeTokenRetrievalError>(token_or_error)) {
    if (!last_pending_get_suggestions_callback_.is_null()) {
      std::move(last_pending_get_suggestions_callback_).Run({});
    }
    return;
  }

  const OneTimeToken& token = std::get<OneTimeToken>(token_or_error);

  std::vector<std::string> suggestions;
  if (!token.value().empty()) {
    suggestions.emplace_back(token.value());
  }

  if (IsOtpDeliveryBlocked()) {
    suggestions.clear();
  }

  if (!last_pending_get_suggestions_callback_.is_null()) {
    if (owner_->GetMetricState().has_value()) {
      owner_->GetMetricState()->otp_form_event_logger.OnOtpAvailable();
    }
    std::move(last_pending_get_suggestions_callback_).Run(suggestions);
  }
}

bool OtpManagerImpl::IsOtpDeliveryBlocked() {
  return owner_->client().DocumentUsedWebOTP();
}

}  // namespace autofill
