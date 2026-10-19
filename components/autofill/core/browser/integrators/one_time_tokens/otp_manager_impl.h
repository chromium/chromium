// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_MANAGER_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_MANAGER_IMPL_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_manager.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/one_time_tokens/core/browser/gmail_otp_retriever.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "components/one_time_tokens/core/browser/sms_otp_service.h"
#include "components/one_time_tokens/core/browser/util/expiring_subscription.h"
#include "components/prefs/pref_change_registrar.h"
#include "url/origin.h"

namespace autofill {

class AutofillField;
class BrowserAutofillManager;
class FormFieldData;
class FormStructure;
class LogBuffer;

// Used for histograms. Do not reorder.
enum class OneTimeTokensPhishGuardVerdict {
  kUnknown = 0,
  kPhishing = 1,
  kNotPhishing = 2,
  kMaxValue = kNotPhishing,
};

LogBuffer& operator<<(LogBuffer& buffer,
                      OneTimeTokensPhishGuardVerdict verdict);

// This class triggers the fetching of SMS OTPs from the `SmsOtpService`
// and of Gmail OTPs from the `GmailOtpBackend` as soon as
// `OnFieldTypesDetermined()` is notified about the classification of OTP
// fields. OTPs are fetched only once per instantiation. This is ok if the
// `OtpManagerImpl` is recreated on each frame navigation.
//
// One instance per frame, owned by the BrowserAutofillManager.
class OtpManagerImpl : public OtpManager, public AutofillManager::Observer {
 public:
  OtpManagerImpl(BrowserAutofillManager& owner,
                 one_time_tokens::SmsOtpService* sms_otp_service);
  OtpManagerImpl(const OtpManagerImpl&) = delete;
  OtpManagerImpl& operator=(const OtpManagerImpl&) = delete;
  ~OtpManagerImpl() override;

  // OtpManager:
  // Evaluates in-memory cached Gmail OTPs for immediate delivery (subject
  // to PhishGuard verification), queries recent OTPs from the backend, and
  // renews subscriptions for incoming OTPs.
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

 private:
  friend class OtpManagerImplTestApi;

  // The duration for which `OtpManagerImpl` will wait for an incoming OTP
  // coming from an SMS message.
  static constexpr base::TimeDelta kSmsOtpSubscriptionDuration =
      base::Minutes(1);

  // The duration for which `OtpManagerImpl` will wait for a notification
  // about an incoming OTP in the user's Gmail inbox. The actual OTP fetch
  // will happen as part of a `gmail_otp_retriever_` call.
  static constexpr base::TimeDelta kGmailOtpTickleSubscriptionDuration =
      base::Minutes(5);

  // Returns `last_retrieved_gmail_otp_` if it is present and not expired, or
  // `std::nullopt` otherwise.
  std::optional<one_time_tokens::OneTimeToken> SelectMostRecentToken() const;
  // Fetches recent OTPs and creates or renewes a subscription. Any OTPs
  // discovered in this process are reported to `OnOneTimeTokenReceived`.
  // This calls OnOneTimeTokenReceived() at least one time.
  void GetRecentOtpsAndRenewSubscription();

  // Starts `gmail_otp_retriever_` if no Gmail OTP is currently cached and the
  // user has opted into Gmail OTP filling.
  void MaybeRetrieveGmailOtp();

  // Called when `prefs::kAutofillGmailOtpFillingEnabled` changes.
  void OnGmailOtpFillingEnabledPrefChanged();

  // Called when the `GmailOtpBackend` notifies about an incoming Gmail OTP
  // tickle push notification.
  //
  // Verifies preconditions before initiating retrieval:
  // - An eligible OTP field is detected on the page.
  // - No OTP field already contains user-typed input.
  // - The user has opted into Gmail OTP filling.
  // - No Gmail OTP retrieval is already in flight.
  // - Both `AffiliationService` and `GmailOtpBackend` are available.
  //
  // If all preconditions are satisfied, starts an asynchronous retrieval via
  // `GmailOtpRetriever` using the target OTP field's frame origin (falling back
  // to the primary main frame origin if opaque) to fetch the token payload,
  // which invokes `OnGmailOtpRetrieved` upon completion.
  void OnTickleReceived();

  // Callback for `one_time_tokens::GmailOtpRetriever`.
  void OnGmailOtpRetrieved(
      base::expected<one_time_tokens::GmailOtpRetriever::Result,
                     one_time_tokens::OneTimeTokenRetrievalError> result);

  // TODO(crbug.com/415273270): Update UI (dropdown or keyboard accessory) when
  // a new token is received.
  void OnOneTimeTokenReceived(
      base::expected<one_time_tokens::OneTimeToken,
                     one_time_tokens::OneTimeTokenRetrievalError>
          token_or_error);

  // Evaluates the PhishGuard `verdict` (or `kUnknown` if no check was
  // performed) and delivers or triggers OTP suggestions for SMS and Gmail
  // respectively.
  void MaybeShowOtpSuggestionsForSms(one_time_tokens::OneTimeToken token,
                                     LocalFrameToken frame_token,
                                     OneTimeTokensPhishGuardVerdict verdict);
  void MaybeShowOtpSuggestionsForGmail(one_time_tokens::OneTimeToken token,
                                       LocalFrameToken frame_token,
                                       OneTimeTokensPhishGuardVerdict verdict);

  // Returns the target OTP field:
  // - If a form field is currently focused, returns it if it is an eligible OTP
  //   field, or `nullptr` if another (non-OTP) field has focus.
  // - If no form field is currently focused, returns the first eligible OTP
  //   field in cached forms, or `nullptr` if none exists.
  const AutofillField* GetTargetOtpField() const;

  // Returns true if an OTP must not be delivered to the caller in an autofill
  // context, e.g., because the page called the WebOTP API.
  bool IsOtpDeliveryBlocked();

  // Checks whether an OTP field was detected in the document.
  bool IsOtpFieldDetected() const;

  // Checks whether a field which was detected as an OTP, already contains
  // some user input. We will not fill the value in such case.
  bool AnyOtpFieldContainsTypedInput() const;

  // Checks whether the user has opted into the GMail OTP filling.
  // The consent is stored in prefs::IsAutofillGmailOtpFillingEnabled.
  bool UserOptedIntoGmailOtpFilling() const;

  // The owning BrowserAutofillManager.
  raw_ref<BrowserAutofillManager> owner_;

  // May be nullptr on platforms that don't support SMS OTP fetching.
  raw_ptr<one_time_tokens::SmsOtpService> sms_otp_service_ = nullptr;

  // Manages retrieval of a Gmail OTP for the target origin, started either when
  // an OTP field is detected on the page or in response to an incoming Gmail
  // OTP tickle notification.
  std::unique_ptr<one_time_tokens::GmailOtpRetriever> gmail_otp_retriever_;

  // Subscription to the `SmsOtpService` for incoming SMS OTPs.
  one_time_tokens::ExpiringSubscription sms_otp_subscription_;
  // Subscription to the `GmailOtpBackend` for incoming Gmail OTP tickles.
  one_time_tokens::ExpiringSubscription gmail_otp_tickle_subscription_;

  // Observes `prefs::kAutofillGmailOtpFillingEnabled` so that when the user
  // accepts the opt-in bubble, `OtpManagerImpl` immediately retrieves the
  // pending Gmail OTP and triggers suggestions on the current page.
  PrefChangeRegistrar pref_change_registrar_;

  // Only the last call from the UI to generate suggestions is retained as such
  // a callback corresponds to the desire to show an autofill dropdown. A new
  // call to `GetOtpSuggestions()` invalidates the previous call.
  GetOtpSuggestionsCallback last_pending_get_suggestions_callback_;
  LocalFrameToken last_pending_frame_token_;

  // True while an asynchronous PhishGuard check is pending for a Gmail OTP
  // associated with `last_pending_get_suggestions_callback_`. Prevents
  // non-Gmail callbacks (such as synchronous SMS backend errors on Desktop)
  // from consuming `last_pending_get_suggestions_callback_` before the
  // PhishGuard check completes.
  bool is_gmail_phish_guard_pending_ = false;

  // Tracks the currently focused form and field, if any.
  std::optional<FormGlobalId> currently_focused_form_id_;
  std::optional<FieldGlobalId> currently_focused_field_id_;

  // Value of the last OTP that proactively triggered suggestions via the
  // renderer, used to avoid duplicate triggers.
  std::string last_triggered_otp_value_;

  // The most recent domain-verified Gmail OTP retrieved via
  // `gmail_otp_retriever_`, cached so that proactive renderer-triggered
  // `GetOtpSuggestions()` requests can synchronously serve it.
  std::optional<one_time_tokens::OneTimeToken> last_retrieved_gmail_otp_;

  base::ScopedObservation<BrowserAutofillManager, AutofillManager::Observer>
      autofill_manager_observation_{this};

  base::WeakPtrFactory<OtpManagerImpl> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_MANAGER_IMPL_H_
