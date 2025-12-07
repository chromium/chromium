// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_MANAGER_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_MANAGER_IMPL_H_

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/metrics/otp_form_event_logger.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_manager.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_service.h"

namespace autofill {

class BrowserAutofillManager;

// Used for histograms. Do not reorder.
enum class OneTimeTokensPhishGuardVerdict {
  kUnknown = 0,
  kPhishing = 1,
  kNotPhishing = 2,
  kMaxValue = kNotPhishing,
};

// This class triggers the fetching of OTPs from the `OneTimeTokenService` as
// soon as `OnFieldTypesDetermined()` is notified about the classification of
// OTP fields. OTPs are fetched only once per instantiation. This is ok
// if the `OtpManagerImpl` is recreated on each frame navigation.
//
// One instance per frame, owned by the BrowserAutofillManager.
class OtpManagerImpl : public OtpManager, public AutofillManager::Observer {
 public:
  OtpManagerImpl(BrowserAutofillManager& owner,
                 one_time_tokens::OneTimeTokenService* one_time_token_service);
  OtpManagerImpl(const OtpManagerImpl&) = delete;
  OtpManagerImpl& operator=(const OtpManagerImpl&) = delete;
  ~OtpManagerImpl() override;

  // OtpManager:
  // Returns any cached OTPs (if they exist) and renews a subscription so that
  // incoming OTPs can be reported.
  void GetOtpSuggestions(GetOtpSuggestionsCallback callback) override;

  // AutofillManager::Observer:
  void OnFieldTypesDetermined(
      AutofillManager& manager,
      FormGlobalId form,
      AutofillManager::Observer::FieldTypeSource source) override;
  void OnBeforeFocusOnFormField(AutofillManager& manager,
                                FormGlobalId form,
                                FieldGlobalId field) override;
  void OnBeforeFocusOnNonFormField(AutofillManager& manager) override;

 private:
  // Fetches recent OTPs and creates or renewes a subscription. Any OTPs
  // discovered in this process are reported to `OnOneTimeTokenReceived`.
  void GetRecentOtpsAndRenewSubscription();

  // TODO(crbug.com/415273270): Update UI (dropdown or keyboard accessory) when
  // a new token is received.
  void OnOneTimeTokenReceived(
      one_time_tokens::OneTimeTokenSource,
      base::expected<one_time_tokens::OneTimeToken,
                     one_time_tokens::OneTimeTokenRetrievalError>
          token_or_error);

  // Callback for `OtpPhishGuardDelegate::StartOtpPhishGuardCheck`.
  void MaybeShowOtpSuggestions(one_time_tokens::OneTimeToken token,
                               OneTimeTokensPhishGuardVerdict verdict);

  // Returns true if an OTP must not be delivered to the caller in an autofill
  // context, e.g., because the page called the WebOTP API.
  bool IsOtpDeliveryBlocked();

  // The owning BrowserAutofillManager.
  raw_ref<BrowserAutofillManager> owner_;

  // May be nullptr on platforms that don't support SMS OTP fetching.
  raw_ptr<one_time_tokens::OneTimeTokenService> one_time_token_services_ =
      nullptr;

  // Subscription to a `OneTimetokenService`.
  one_time_tokens::ExpiringSubscription subscription_;

  // Only the last call from the UI to generate suggestions is retained as such
  // a callback corresponds to the desire to show an autofill dropdown. A new
  // call to `GetOtpSuggestions()` invalidates the previous call.
  GetOtpSuggestionsCallback last_pending_get_suggestions_callback_;

  // The time when the phish guard check was started.
  base::TimeTicks phish_guard_check_start_time_;

  // The last received OTP. This is used to store the OTP between the phishing
  // check and the actual display of the suggestions.
  std::optional<one_time_tokens::OneTimeToken> last_received_otp_;

  base::ScopedObservation<BrowserAutofillManager, AutofillManager::Observer>
      autofill_manager_observation_{this};

  base::WeakPtrFactory<OtpManagerImpl> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_MANAGER_IMPL_H_
