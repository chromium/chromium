// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_MANAGER_IMPL_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_MANAGER_IMPL_TEST_API_H_

#include <optional>

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_manager_impl.h"

namespace autofill {

class AutofillField;

// Test API for `OtpManagerImpl`.
class OtpManagerImplTestApi {
 public:
  static constexpr base::TimeDelta kSmsOtpSubscriptionDuration =
      OtpManagerImpl::kSmsOtpSubscriptionDuration;

  static constexpr base::TimeDelta kGmailOtpTickleSubscriptionDuration =
      OtpManagerImpl::kGmailOtpTickleSubscriptionDuration;

  explicit OtpManagerImplTestApi(OtpManagerImpl& manager) : manager_(manager) {}

  std::optional<one_time_tokens::OneTimeToken> SelectMostRecentToken(
      std::optional<one_time_tokens::OneTimeTokenType> type =
          std::nullopt) const {
    return manager_->SelectMostRecentToken(type);
  }

  const one_time_tokens::ExpiringSubscription& gmail_otp_tickle_subscription()
      const {
    return manager_->gmail_otp_tickle_subscription_;
  }

  bool IsOtpFieldDetected() const { return manager_->IsOtpFieldDetected(); }

  bool AnyOtpFieldContainsTypedInput() const {
    return manager_->AnyOtpFieldContainsTypedInput();
  }

  bool UserOptedIntoGmailOtpFilling() const {
    return manager_->UserOptedIntoGmailOtpFilling();
  }

  bool has_log_subscription() const { return !!manager_->log_subscription_; }

  std::optional<FormGlobalId> currently_focused_form_id() const {
    return manager_->currently_focused_form_id_;
  }

  std::optional<FieldGlobalId> currently_focused_field_id() const {
    return manager_->currently_focused_field_id_;
  }

  const AutofillField* GetFocusedOtpField() const {
    return manager_->GetFocusedOtpField();
  }

 private:
  raw_ref<OtpManagerImpl> manager_;
};

inline OtpManagerImplTestApi test_api(OtpManagerImpl& manager) {
  return OtpManagerImplTestApi(manager);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_MANAGER_IMPL_TEST_API_H_
