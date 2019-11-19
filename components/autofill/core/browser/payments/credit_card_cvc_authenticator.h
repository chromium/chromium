// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_CVC_AUTHENTICATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_CVC_AUTHENTICATOR_H_

#include <memory>

#include "base/strings/string16.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/card_unmask_delegate.h"
#include "components/autofill/core/browser/payments/full_card_request.h"

namespace autofill {

// Authenticates credit card unmasking through CVC verification.
class CreditCardCVCAuthenticator
    : public payments::FullCardRequest::ResultDelegate,
      public payments::FullCardRequest::UIDelegate {
 public:
  struct CVCAuthenticationResponse {
    CVCAuthenticationResponse();
    ~CVCAuthenticationResponse();

    CVCAuthenticationResponse& with_did_succeed(bool b) {
      did_succeed = b;
      return *this;
    }
    // Data pointed to by |c| must outlive this object.
    CVCAuthenticationResponse& with_card(const CreditCard* c) {
      card = c;
      return *this;
    }
    CVCAuthenticationResponse& with_cvc(const base::string16 s) {
      cvc = base::string16(s);
      return *this;
    }
    CVCAuthenticationResponse& with_creation_options(
        base::Optional<base::Value> v) {
      creation_options = std::move(v);
      return *this;
    }
    CVCAuthenticationResponse& with_request_options(
        base::Optional<base::Value> v) {
      request_options = std::move(v);
      return *this;
    }
    CVCAuthenticationResponse& with_card_authorization_token(std::string s) {
      card_authorization_token = s;
      return *this;
    }
    bool did_succeed = false;
    const CreditCard* card = nullptr;
    base::string16 cvc = base::string16();
    base::Optional<base::Value> creation_options = base::nullopt;
    base::Optional<base::Value> request_options = base::nullopt;
    std::string card_authorization_token = std::string();
  };
  class Requester {
   public:
    virtual ~Requester() {}
    virtual void OnCVCAuthenticationComplete(
        const CVCAuthenticationResponse& response) = 0;
  };
  explicit CreditCardCVCAuthenticator(AutofillClient* client);
  ~CreditCardCVCAuthenticator() override;

  // Authentication
  void Authenticate(const CreditCard* card,
                    base::WeakPtr<Requester> requester,
                    PersonalDataManager* personal_data_manager,
                    const base::TimeTicks& form_parsed_timestamp);

  // payments::FullCardRequest::ResultDelegate
  void OnFullCardRequestSucceeded(
      const payments::FullCardRequest& full_card_request,
      const CreditCard& card,
      const base::string16& cvc) override;
  void OnFullCardRequestFailed() override;

  // payments::FullCardRequest::UIDelegate
  void ShowUnmaskPrompt(const CreditCard& card,
                        AutofillClient::UnmaskCardReason reason,
                        base::WeakPtr<CardUnmaskDelegate> delegate) override;
  void OnUnmaskVerificationResult(
      AutofillClient::PaymentsRpcResult result) override;

  payments::FullCardRequest* GetFullCardRequest();

  base::WeakPtr<payments::FullCardRequest::UIDelegate>
  GetAsFullCardRequestUIDelegate();

 private:
  friend class AutofillAssistantTest;
  friend class AutofillManagerTest;
  friend class AutofillMetricsTest;
  friend class CreditCardAccessManagerTest;
  friend class CreditCardCVCAuthenticatorTest;

  // The associated autofill client. Weak reference.
  AutofillClient* const client_;

  // Responsible for getting the full card details, including the PAN and the
  // CVC.
  std::unique_ptr<payments::FullCardRequest> full_card_request_;

  // Weak pointer to object that is requesting authentication.
  base::WeakPtr<Requester> requester_;

  base::WeakPtrFactory<CreditCardCVCAuthenticator> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CreditCardCVCAuthenticator);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_CVC_AUTHENTICATOR_H_
