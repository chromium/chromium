// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_manager_impl.h"

#include <algorithm>

#include "base/containers/to_vector.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/one_time_tokens/core/browser/sms_otp_backend.h"
#include "components/password_manager/core/browser/features/password_features.h"

namespace autofill {

namespace {
std::vector<std::string> OtpsToSuggestionStrings(
    const std::vector<one_time_tokens::OneTimeToken>& otp_values) {
  return base::ToVector(otp_values, &one_time_tokens::OneTimeToken::value);
}
}  // namespace

OtpManagerImpl::OtpManagerImpl(AutofillManager* autofill_manager,
                               one_time_tokens::SmsOtpBackend* sms_otp_backend)
    : sms_otp_backend_(sms_otp_backend) {
  if (autofill_manager) {
    autofill_manager_observation_.Observe(autofill_manager);
  }

  // TODO(crbug.com/415273270) This is just a hack to prepopulate the OTPs in
  // case no real backend is triggered. The feature definition should migrate to
  // autofill.
  if (base::FeatureList::IsEnabled(
          password_manager::features::kDebugUiForOtps)) {
    otp_suggestions_ = {one_time_tokens::OneTimeToken(
        // TODO(crbug.com/41527327) kSmsOtp is just a dummy value at the
        // moment. It's unclear if otp_source_ will remain in the current form.
        // Depending on that we may want to fix this or not.
        one_time_tokens::OneTimeTokenType::kSmsOtp, "Identified OTP field.",
        base::Time::Now())};
  }
}

OtpManagerImpl::~OtpManagerImpl() = default;

void OtpManagerImpl::GetOtpSuggestions(
    OtpManagerImpl::GetOtpSuggestionsCallback callback) {
  if (!sms_otp_retrieval_in_progress_) {
    std::move(callback).Run(OtpsToSuggestionStrings(otp_suggestions_));
  } else {
    last_pending_get_suggestions_callback_ = std::move(callback);
  }
}

void OtpManagerImpl::OnFieldTypesDetermined(
    AutofillManager& manager,
    FormGlobalId form_id,
    AutofillManager::Observer::FieldTypeSource source) {
  // On non-android platforms and in tests the backend may be not initialized.
  if (!sms_otp_backend_) {
    return;
  }
  // The first time an OTP field is detected, Chrome sends a request that is
  // valid for 5 minutes. Therefore, we don't send multiple requests. There is
  // no concept of failing to retrieve OTPs (having no OTP permission is
  // currently out of scope). We don't try again after 5 minutes because the OTP
  // is probably too old by that time.
  // TODO(crbug.com/415273270) This will be replaced by a subscription
  // mechanism.
  if (sms_otp_retrieval_was_ever_started_) {
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

  sms_otp_retrieval_was_ever_started_ = true;
  sms_otp_retrieval_in_progress_ = true;
  sms_otp_backend_->RetrieveSmsOtp(base::BindOnce(
      &OtpManagerImpl::OnOtpRetrievalComplete, weak_ptr_factory_.GetWeakPtr()));
}

void OtpManagerImpl::OnOtpRetrievalComplete(
    const one_time_tokens::OtpFetchReply& reply) {
  sms_otp_retrieval_in_progress_ = false;
  if (reply.otp_value.has_value() && !reply.otp_value->value().empty()) {
    // If the same token was retrieved before, remove it.
    std::erase_if(otp_suggestions_,
                  [&reply](const one_time_tokens::OneTimeToken& token) {
                    return token.value() == reply.otp_value.value().value();
                  });
    otp_suggestions_.push_back(reply.otp_value.value());
  }

  // Process the last pending callbacks from the UI to provide suggestions.
  if (last_pending_get_suggestions_callback_.has_value()) {
    std::move(*last_pending_get_suggestions_callback_)
        .Run(OtpsToSuggestionStrings(otp_suggestions_));
  }

  // TODO(crbug.com/415272524): Record metrics on how often the retrieval
  // succeeds or fails, in combination with the OTP source.
}

}  // namespace autofill
