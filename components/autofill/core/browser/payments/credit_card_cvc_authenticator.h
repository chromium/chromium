// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_CVC_AUTHENTICATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_CVC_AUTHENTICATOR_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/card_unmask_delegate.h"
#include "components/autofill/core/browser/payments/full_card_request.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_options.h"

namespace autofill {

class AutofillClient;

namespace autofill_metrics {
class AutofillMetricsBaseTest;
}

// Authenticates credit card unmasking through CVC verification.
class CreditCardCvcAuthenticator
    : public payments::FullCardRequest::ResultDelegate,
      public payments::FullCardRequest::UIDelegate {
 public:
  struct CvcAuthenticationResponse {
    CvcAuthenticationResponse();
    ~CvcAuthenticationResponse();

    CvcAuthenticationResponse& with_did_succeed(bool b) {
      did_succeed = b;
      return *this;
    }
    // Data pointed to by |c| must outlive this object.
    CvcAuthenticationResponse& with_card(const CreditCard* c) {
      card = c;
      return *this;
    }
    CvcAuthenticationResponse& with_cvc(const std::u16string s) {
      cvc = std::u16string(s);
      return *this;
    }
    CvcAuthenticationResponse& with_request_options(base::Value::Dict v) {
      request_options = std::move(v);
      return *this;
    }
    CvcAuthenticationResponse& with_card_authorization_token(std::string s) {
      card_authorization_token = s;
      return *this;
    }
    bool did_succeed = false;
    raw_ptr<const CreditCard> card = nullptr;
    // TODO(crbug.com/40927733): Remove CVC.
    std::u16string cvc;
    base::Value::Dict request_options;
    std::string card_authorization_token;
  };
  class Requester {
   public:
    virtual ~Requester() = default;
    virtual void OnCvcAuthenticationComplete(
        const CvcAuthenticationResponse& response) = 0;

#if BUILDFLAG(IS_ANDROID)
    // Returns whether or not the user, while on the CVC prompt, should be
    // offered to switch to FIDO authentication for card unmasking. This will
    // always be false for Desktop since FIDO authentication is offered as a
    // separate prompt after the CVC prompt. On Android, however, this is
    // offered through a checkbox on the CVC prompt. This feature does not yet
    // exist on iOS.
    virtual bool ShouldOfferFidoAuth() const = 0;

    // This returns true only on Android when the user previously opted-in for
    // FIDO authentication through the settings page and this is the first card
    // downstream since. In this case, the opt-in checkbox is not shown and the
    // opt-in request is sent.
    virtual bool UserOptedInToFidoFromSettingsPageOnMobile() const = 0;
#endif
  };
  explicit CreditCardCvcAuthenticator(AutofillClient* client);

  CreditCardCvcAuthenticator(const CreditCardCvcAuthenticator&) = delete;
  CreditCardCvcAuthenticator& operator=(const CreditCardCvcAuthenticator&) =
      delete;

  ~CreditCardCvcAuthenticator() override;

  // Authentication
  void Authenticate(const CreditCard& card,
                    base::WeakPtr<Requester> requester,
                    PersonalDataManager* personal_data_manager,
                    std::optional<std::string> vcn_context_token = std::nullopt,
                    std::optional<CardUnmaskChallengeOption>
                        selected_challenge_option = std::nullopt);

  // payments::FullCardRequest::ResultDelegate
  void OnFullCardRequestSucceeded(
      const payments::FullCardRequest& full_card_request,
      const CreditCard& card,
      const std::u16string& cvc) override;
  void OnFullCardRequestFailed(
      CreditCard::RecordType card_type,
      payments::FullCardRequest::FailureType failure_type) override;

  // payments::FullCardRequest::UIDelegate
  void ShowUnmaskPrompt(
      const CreditCard& card,
      const CardUnmaskPromptOptions& card_unmask_prompt_options,
      base::WeakPtr<CardUnmaskDelegate> delegate) override;
  void OnUnmaskVerificationResult(
      payments::PaymentsAutofillClient::PaymentsRpcResult result) override;
#if BUILDFLAG(IS_ANDROID)
  bool ShouldOfferFidoAuth() const override;
  bool UserOptedInToFidoFromSettingsPageOnMobile() const override;
#endif

  payments::FullCardRequest* GetFullCardRequest();

  base::WeakPtr<payments::FullCardRequest::UIDelegate>
  GetAsFullCardRequestUIDelegate();

 private:
  friend class BrowserAutofillManagerTest;
  friend class AutofillMetricsTest;
  friend class autofill_metrics::AutofillMetricsBaseTest;
  friend class CreditCardAccessManagerTestBase;
  friend class CreditCardCvcAuthenticatorTest;
  friend class FormFillerTest;

  // The associated autofill client. Weak reference.
  const raw_ptr<AutofillClient> client_;

  // Responsible for getting the full card details, including the PAN and the
  // CVC.
  std::unique_ptr<payments::FullCardRequest> full_card_request_;

  // Weak pointer to object that is requesting authentication.
  base::WeakPtr<Requester> requester_;

  base::WeakPtrFactory<CreditCardCvcAuthenticator> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_CVC_AUTHENTICATOR_H_
