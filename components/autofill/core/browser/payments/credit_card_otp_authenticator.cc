// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/credit_card_otp_authenticator.h"

namespace autofill {

CreditCardOtpAuthenticator::OtpAuthenticationResponse::
    OtpAuthenticationResponse() = default;
CreditCardOtpAuthenticator::OtpAuthenticationResponse::
    ~OtpAuthenticationResponse() = default;

CreditCardOtpAuthenticator::CreditCardOtpAuthenticator(AutofillClient* client)
    : autofill_client_(client), payments_client_(client->GetPaymentsClient()) {}

CreditCardOtpAuthenticator::~CreditCardOtpAuthenticator() = default;

void CreditCardOtpAuthenticator::Authenticate(
    const CreditCard* card,
    base::WeakPtr<Requester> requester,
    const std::string& context_token,
    int64_t billing_customer_number) {
  if (!card) {
    return requester->OnOtpAuthenticationComplete(
        OtpAuthenticationResponse().with_did_succeed(false));
  }

  requester_ = requester;

  absl::optional<GURL> last_committed_url_origin;
  if (autofill_client_->GetLastCommittedURL().is_valid()) {
    last_committed_url_origin =
        autofill_client_->GetLastCommittedURL().GetOrigin();
  }

  DCHECK(payments_client_);
  payments_client_->Prepare();

  request_ = std::make_unique<payments::PaymentsClient::UnmaskRequestDetails>();
  request_->card = *card;
  request_->reason = AutofillClient::UNMASK_FOR_AUTOFILL;
  request_->last_committed_url_origin = last_committed_url_origin;
  request_->billing_customer_number = billing_customer_number;
  request_->context_token = context_token_;

  // TODO(crbug.com/1243475): Show otp authentication dialog with masked phone
  // number and otp digits. Then hold request before otp value is populated.

  // TODO(crbug.com/1243475): Explore the possibility of sending one
  // LoadRiskData request per session.
  autofill_client_->LoadRiskData(
      base::BindOnce(&CreditCardOtpAuthenticator::OnDidGetUnmaskRiskData,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CreditCardOtpAuthenticator::OnDidGetRealPan(
    AutofillClient::PaymentsRpcResult result,
    payments::PaymentsClient::UnmaskResponseDetails& response_details) {
  // TODO(crbug.com/1243475): Add latency logging.
  if (result == AutofillClient::SUCCESS) {
    if (response_details.card_type !=
        AutofillClient::PaymentsRpcCardType::VIRTUAL_CARD) {
      // Currently we offer OTP authentication only for virtual cards.
      NOTREACHED();
      requester_->OnOtpAuthenticationComplete(
          OtpAuthenticationResponse().with_did_succeed(false));
      return;
    }

    // The following prerequisites should be ensured in the PaymentsClient.
    DCHECK(!response_details.real_pan.empty());
    DCHECK(!response_details.dcvv.empty());
    DCHECK(!response_details.expiration_month.empty());
    DCHECK(!response_details.expiration_year.empty());

    request_->card.SetNumber(base::UTF8ToUTF16(response_details.real_pan));
    request_->card.set_record_type(CreditCard::VIRTUAL_CARD);
    request_->card.SetExpirationMonthFromString(
        base::UTF8ToUTF16(response_details.expiration_month),
        /*app_locale=*/std::string());
    request_->card.SetExpirationYearFromString(
        base::UTF8ToUTF16(response_details.expiration_year));

    auto response = OtpAuthenticationResponse().with_did_succeed(true);
    response.card = &(request_->card);
    response.cvc = base::UTF8ToUTF16(response_details.dcvv);
    requester_->OnOtpAuthenticationComplete(response);
    return;
  }
  // TODO(crbug.com/1243475): Add error handling.
  requester_->OnOtpAuthenticationComplete(
      OtpAuthenticationResponse().with_did_succeed(false));
}

void CreditCardOtpAuthenticator::OnDidGetUnmaskRiskData(
    const std::string& risk_data) {
  request_->risk_data = risk_data;
  if (!otp_.empty())
    SendUnmaskCardRequest();
}

void CreditCardOtpAuthenticator::SendUnmaskCardRequest() {
  payments_client_->UnmaskCard(
      *request_, base::BindOnce(&CreditCardOtpAuthenticator::OnDidGetRealPan,
                                weak_ptr_factory_.GetWeakPtr()));
}

void CreditCardOtpAuthenticator::Reset() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  payments_client_->CancelRequest();
  request_.reset();
  otp_ = std::u16string();
  context_token_ = std::string();
}

}  // namespace autofill
