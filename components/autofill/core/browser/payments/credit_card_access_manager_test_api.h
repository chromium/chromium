// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_ACCESS_MANAGER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_ACCESS_MANAGER_TEST_API_H_

#include "base/check_deref.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/wait_for_signal_or_timeout.h"

namespace autofill {

class CreditCardAccessManagerTestApi {
 public:
  explicit CreditCardAccessManagerTestApi(
      CreditCardAccessManager* credit_card_access_manager)
      : credit_card_access_manager_(CHECK_DEREF(credit_card_access_manager)) {}
  CreditCardAccessManagerTestApi(const CreditCardAccessManagerTestApi&) =
      delete;
  CreditCardAccessManagerTestApi& operator=(
      const CreditCardAccessManagerTestApi&) = delete;
  ~CreditCardAccessManagerTestApi() = default;

  bool ShouldOfferFidoOptInDialog(
      const CreditCardCvcAuthenticator::CvcAuthenticationResponse& response) {
    return credit_card_access_manager_->ShouldOfferFidoOptInDialog(response);
  }

#if BUILDFLAG(IS_ANDROID)
  bool ShouldOfferFidoAuth() {
    return credit_card_access_manager_->ShouldOfferFidoAuth();
  }
#endif

  void OnVcn3dsAuthenticationComplete(
      payments::PaymentsWindowManager::Vcn3dsAuthenticationResponse response) {
    credit_card_access_manager_->OnVcn3dsAuthenticationComplete(
        std::move(response));
  }

  bool UnmaskedCardCacheIsEmpty() {
    return credit_card_access_manager_->UnmaskedCardCacheIsEmpty();
  }

  void set_is_user_verifiable(std::optional<bool> is_user_verifiable) {
    credit_card_access_manager_->is_user_verifiable_ =
        std::move(is_user_verifiable);
  }

  std::optional<bool> is_user_verifiable() {
    return credit_card_access_manager_->is_user_verifiable_;
  }

  void set_can_fetch_unmask_details(bool can_fetch_unmask_details) {
    credit_card_access_manager_->can_fetch_unmask_details_ =
        can_fetch_unmask_details;
  }
  bool can_fetch_unmask_details() {
    return credit_card_access_manager_->can_fetch_unmask_details_;
  }

#if !BUILDFLAG(IS_IOS)
  void OnFIDOAuthenticationComplete(
      const CreditCardFidoAuthenticator::FidoAuthenticationResponse& response) {
    credit_card_access_manager_->OnFIDOAuthenticationComplete(response);
  }
  void set_fido_authenticator(
      std::unique_ptr<CreditCardFidoAuthenticator> fido_authenticator) {
    credit_card_access_manager_->fido_authenticator_ =
        std::move(fido_authenticator);
  }
#endif  // !BUILDFLAG(IS_IOS)

  void OnVirtualCardUnmaskCancelled() {
    credit_card_access_manager_->OnVirtualCardUnmaskCancelled();
  }

  void set_is_authentication_in_progress(bool is_authentication_in_progress) {
    credit_card_access_manager_->is_authentication_in_progress_ =
        is_authentication_in_progress;
  }
  bool is_authentication_in_progress() {
    return credit_card_access_manager_->is_authentication_in_progress_;
  }

  void set_unmask_details_request_in_progress(
      bool unmask_details_request_in_progress) {
    credit_card_access_manager_->unmask_details_request_in_progress_ =
        unmask_details_request_in_progress;
  }
  bool unmask_details_request_in_progress() {
    return credit_card_access_manager_->unmask_details_request_in_progress_;
  }

  void OnDidGetUnmaskDetails(
      payments::PaymentsAutofillClient::PaymentsRpcResult result,
      payments::PaymentsNetworkInterface::UnmaskDetails& unmask_details) {
    credit_card_access_manager_->OnDidGetUnmaskDetails(result, unmask_details);
  }

  WaitForSignalOrTimeout& ready_to_start_authentication() {
    return credit_card_access_manager_->ready_to_start_authentication_;
  }

  UnmaskAuthFlowType unmask_auth_flow_type() {
    return credit_card_access_manager_->unmask_auth_flow_type_;
  }

  void set_unmask_auth_flow_type(UnmaskAuthFlowType unmask_auth_flow_type) {
    credit_card_access_manager_->unmask_auth_flow_type_ = unmask_auth_flow_type;
  }

  void OnUserAcceptedAuthenticationSelectionDialog(
      const std::string& selected_challenge_option_id) {
    credit_card_access_manager_->OnUserAcceptedAuthenticationSelectionDialog(
        selected_challenge_option_id);
  }

 private:
  const raw_ref<CreditCardAccessManager> credit_card_access_manager_;
};

inline CreditCardAccessManagerTestApi test_api(
    CreditCardAccessManager& manager) {
  return CreditCardAccessManagerTestApi(&manager);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_ACCESS_MANAGER_TEST_API_H_
