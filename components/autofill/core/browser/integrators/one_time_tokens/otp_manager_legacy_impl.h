// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_MANAGER_LEGACY_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_MANAGER_LEGACY_IMPL_H_

#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_manager.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_manager_impl.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "components/one_time_tokens/core/browser/one_time_token_service.h"
#include "components/one_time_tokens/core/browser/util/expiring_subscription.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill {

class BrowserAutofillManager;
class FormFieldData;
class FormStructure;

// This class triggers the fetching of OTPs from the `OneTimeTokenService` as
// soon as `OnFieldTypesDetermined()` is notified about the classification of
// OTP fields. OTPs are fetched only once per instantiation. This is ok
// if the `OtpManagerLegacyImpl` is recreated on each frame navigation.
//
// One instance per frame, owned by the BrowserAutofillManager.
class OtpManagerLegacyImpl : public OtpManager,
                             public AutofillManager::Observer {
 public:
  OtpManagerLegacyImpl(
      BrowserAutofillManager& owner,
      one_time_tokens::OneTimeTokenService* one_time_token_service);
  OtpManagerLegacyImpl(const OtpManagerLegacyImpl&) = delete;
  OtpManagerLegacyImpl& operator=(const OtpManagerLegacyImpl&) = delete;
  ~OtpManagerLegacyImpl() override;

  // OtpManager:
  // Queries recent OTPs from the backend and renews subscriptions for incoming
  // SMS OTPs.
  void GetOtpSuggestions(const FormStructure& form,
                         const FormFieldData& field,
                         GetOtpSuggestionsCallback callback) override;

  // AutofillManager::Observer:
  void OnFieldTypesDetermined(AutofillManager& manager,
                              FormGlobalId form,
                              AutofillManager::Observer::FieldTypeSource source,
                              bool small_forms_were_parsed) override;
  void OnBeforeFocusOnFormField(AutofillManager& manager,
                                FormGlobalId form,
                                FieldGlobalId field) override;
  void OnBeforeFocusOnNonFormField(AutofillManager& manager) override;

  // Callback handler for `log_subscription_`.
  void OnLogMessage(std::string_view message);

 private:
  friend class OtpManagerLegacyImplTestApi;

  // The duration for which `OtpManagerLegacyImpl` will wait for an incoming OTP
  // coming from an SMS message.
  static constexpr base::TimeDelta kSmsOtpSubscriptionDuration =
      base::Minutes(1);

  // Fetches recent OTPs and creates or renewes a subscription. Any OTPs
  // discovered in this process are reported to `OnOneTimeTokenReceived`.
  // This calls OnOneTimeTokenReceived() at least one time.
  void GetRecentOtpsAndRenewSubscription();

  // TODO(crbug.com/415273270): Update UI (dropdown or keyboard accessory) when
  // a new token is received.
  void OnOneTimeTokenReceived(
      one_time_tokens::OneTimeTokenSource,
      base::expected<one_time_tokens::OneTimeToken,
                     one_time_tokens::OneTimeTokenRetrievalError>
          token_or_error);

  // Evaluates the PhishGuard `verdict` (or `kUnknown` if no check was
  // performed) and delivers OTP suggestions for SMS.
  void MaybeShowOtpSuggestionsForSms(one_time_tokens::OneTimeToken token,
                                     OneTimeTokensPhishGuardVerdict verdict);

  // Returns true if an OTP must not be delivered to the caller in an autofill
  // context, e.g., because the page called the WebOTP API.
  bool IsOtpDeliveryBlocked();

  // The owning BrowserAutofillManager.
  raw_ref<BrowserAutofillManager> owner_;

  // May be nullptr on platforms that don't support SMS OTP fetching.
  raw_ptr<one_time_tokens::OneTimeTokenService> one_time_token_service_ =
      nullptr;

  // Subscription to `OneTimeTokenService` for SMS OTPs.
  one_time_tokens::ExpiringSubscription sms_otp_subscription_;

  // Subscription to log events of `one_time_token_service_`.
  base::CallbackListSubscription log_subscription_;

  // Only the last call from the UI to generate suggestions is retained as such
  // a callback corresponds to the desire to show an autofill dropdown. A new
  // call to `GetOtpSuggestions()` invalidates the previous call.
  GetOtpSuggestionsCallback last_pending_get_suggestions_callback_;
  LocalFrameToken last_pending_frame_token_;

  base::ScopedObservation<BrowserAutofillManager, AutofillManager::Observer>
      autofill_manager_observation_{this};

  base::WeakPtrFactory<OtpManagerLegacyImpl> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_MANAGER_LEGACY_IMPL_H_
