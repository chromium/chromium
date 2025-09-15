// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_MANAGER_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_MANAGER_IMPL_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_manager.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"

namespace one_time_tokens {
struct OtpFetchReply;
}  // namespace one_time_tokens

namespace autofill {

// This class triggers the fetching of OTPs from the `SmsOtpBackend` as soon
// as `OnFieldTypesDetermined()` is notified about the classification of
// OTP fields. OTPs are fetched only once per instantiation. This is ok
// if the `OtpManagerImpl` is recreated on each frame navigation.
//
// One instance per frame, owned by the BrowserAutofillManager.
class OtpManagerImpl : public OtpManager, public AutofillManager::Observer {
 public:
  using GetOtpSuggestionsCallback =
      base::OnceCallback<void(std::vector<std::string>)>;

  OtpManagerImpl(AutofillManager* autofill_manager,
                 one_time_tokens::SmsOtpBackend* sms_otp_backend);
  OtpManagerImpl(const OtpManagerImpl&) = delete;
  OtpManagerImpl& operator=(const OtpManagerImpl&) = delete;
  ~OtpManagerImpl() override;

  void GetOtpSuggestions(GetOtpSuggestionsCallback callback) override;

  // AutofillManager::Observer:
  void OnFieldTypesDetermined(
      AutofillManager& manager,
      FormGlobalId form,
      AutofillManager::Observer::FieldTypeSource source) override;

 private:
  // Handler for when the SMS backend returns OTPs.
  void OnOtpRetrievalComplete(const one_time_tokens::OtpFetchReply& reply);

  // May be nullptr on platforms that don't support SMS OTP fetching.
  raw_ptr<one_time_tokens::SmsOtpBackend> sms_otp_backend_ = nullptr;

  // Set to true the first time the SMS backend is asked for an OTP retrieval.
  bool sms_otp_retrieval_was_ever_started_ = false;
  // Set to true while waiting for the response of the SMS backend.
  bool sms_otp_retrieval_in_progress_ = false;

  // Fetched OTP values. Most recent entry last.
  // TODO(crbug.com/415273270) Expire old tokens after some time.
  std::vector<one_time_tokens::OneTimeToken> otp_suggestions_;

  // Only the last call from the UI to generate suggestions is retained as such
  // a callback corresponds to the desire to show an autofill dropdown. A new
  // call to `GetOtpSuggestions()` invalidates the previous call.
  std::optional<GetOtpSuggestionsCallback>
      last_pending_get_suggestions_callback_;

  base::ScopedObservation<AutofillManager, AutofillManager::Observer>
      autofill_manager_observation_{this};

  base::WeakPtrFactory<OtpManagerImpl> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_MANAGER_IMPL_H_
