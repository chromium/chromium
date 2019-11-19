// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_ACCESS_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_ACCESS_MANAGER_H_

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/string16.h"
#include "base/synchronization/waitable_event.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/metrics/credit_card_form_event_logger.h"
#include "components/autofill/core/browser/payments/credit_card_cvc_authenticator.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/personal_data_manager.h"

#if !defined(OS_IOS)
#include "components/autofill/core/browser/payments/credit_card_fido_authenticator.h"
#endif

namespace autofill {

class AutofillManager;

// Manages logic for accessing credit cards either stored locally or stored
// with Google Payments. Owned by AutofillManager.
#if defined(OS_IOS)
class CreditCardAccessManager : public CreditCardCVCAuthenticator::Requester {
#else
class CreditCardAccessManager : public CreditCardCVCAuthenticator::Requester,
                                public CreditCardFIDOAuthenticator::Requester {
#endif
 public:
  class Accessor {
   public:
    virtual ~Accessor() {}
    virtual void OnCreditCardFetched(
        bool did_succeed,
        const CreditCard* credit_card = nullptr,
        const base::string16& cvc = base::string16()) = 0;
  };

  explicit CreditCardAccessManager(AutofillDriver* driver,
                                   AutofillManager* autofill_manager);
  CreditCardAccessManager(
      AutofillDriver* driver,
      AutofillClient* client,
      PersonalDataManager* personal_data_manager,
      CreditCardFormEventLogger* credit_card_form_event_logger = nullptr);
  ~CreditCardAccessManager() override;

  // Logs information about current credit card data.
  void UpdateCreditCardFormEventLogger();
  // Returns all credit cards.
  std::vector<CreditCard*> GetCreditCards();
  // Returns credit cards in the order to be suggested to the user.
  std::vector<CreditCard*> GetCreditCardsToSuggest();
  // Returns true only if all cards are server cards.
  bool ShouldDisplayGPayLogo();
  // Returns true when deletion is allowed. Only local cards can be deleted.
  bool DeleteCard(const CreditCard* card);
  // Returns true if the |card| is deletable. Fills out
  // |title| and |body| with relevant user-facing text.
  bool GetDeletionConfirmationText(const CreditCard* card,
                                   base::string16* title,
                                   base::string16* body);

  // Returns false only if some form of authentication is still in progress.
  bool ShouldClearPreviewedForm();

  // Retrieves instance of CreditCard with given guid.
  CreditCard* GetCreditCard(std::string guid);

  // Makes a call to Google Payments to retrieve authentication details.
  void PrepareToFetchCreditCard();

  // Calls |accessor->OnCreditCardFetched()| once credit card is fetched.
  virtual void FetchCreditCard(
      const CreditCard* card,
      base::WeakPtr<Accessor> accessor,
      const base::TimeTicks& timestamp = base::TimeTicks());

  // If |opt_in| = true, opts the user into using FIDO authentication for card
  // unmasking. Otherwise, opts the user out. If |creation_options| is set,
  // WebAuthn registration prompt will be invoked to create a new credential.
  void FIDOAuthOptChange(bool opt_in);

  // Makes a call to FIDOAuthOptChange() with |opt_in|.
  // TODO(crbug/949269): Add a rate limiter to counter spam clicking.
  void OnSettingsPageFIDOAuthToggled(bool opt_in);

  CreditCardCVCAuthenticator* GetOrCreateCVCAuthenticator();

#if !defined(OS_IOS)
  CreditCardFIDOAuthenticator* GetOrCreateFIDOAuthenticator();
#endif

 private:
  friend class AutofillAssistantTest;
  friend class AutofillManagerTest;
  friend class AutofillMetricsTest;
  friend class CreditCardAccessManagerTest;

#if !defined(OS_IOS)
  void set_fido_authenticator_for_testing(
      std::unique_ptr<CreditCardFIDOAuthenticator> fido_authenticator) {
    fido_authenticator_ = std::move(fido_authenticator);
  }
#endif

  // Returns false if all suggested cards are local cards, otherwise true.
  bool ServerCardsAvailable();

  // Invoked from CreditCardFIDOAuthenticator::IsUserVerifiable().
  // |is_user_verifiable| is set to true only if user has a verifying platform
  // authenticator. e.g. Touch/Face ID, Windows Hello, Android fingerprint,
  // etc., is available and enabled. If set to true, then an Unmask Details
  // request will be sent to Google Payments.
  void GetUnmaskDetailsIfUserIsVerifiable(bool is_user_verifiable);

  // Sets |unmask_details_|. May be ignored if response is too late and user is
  // not opted-in for FIDO auth, or if user does not select a card.
  void OnDidGetUnmaskDetails(AutofillClient::PaymentsRpcResult result,
                             AutofillClient::UnmaskDetails& unmask_details);

  // If OnDidGetUnmaskDetails() was invoked by PaymentsClient, then
  // |get_unmask_details_returned| should be set to true. Based on the
  // contents of |unmask_details_|, either FIDO authentication or CVC
  // authentication will be prompted. If |get_unmask_details_returned| is false,
  // then only CVC authentication will be prompted.
  void Authenticate(bool get_unmask_details_returned = false);

  // CreditCardCVCAuthenticator::Requester:
  void OnCVCAuthenticationComplete(
      const CreditCardCVCAuthenticator::CVCAuthenticationResponse& response)
      override;

#if !defined(OS_IOS)
  // CreditCardFIDOAuthenticator::Requester:
  void OnFIDOAuthenticationComplete(bool did_succeed,
                                    const CreditCard* card = nullptr) override;
#endif

  bool is_authentication_in_progress() {
    return is_authentication_in_progress_;
  }

  // Returns true only if |credit_card| is a local card.
  bool IsLocalCard(const CreditCard* credit_card);

  // If true, FetchCreditCard() should wait for OnDidGetUnmaskDetails() to begin
  // authentication. If false, FetchCreditCard() can begin authentication
  // immediately.
  bool AuthenticationRequiresUnmaskDetails();

#if !defined(OS_ANDROID) && !defined(OS_IOS)
  // After card verification starts, shows the verify pending dialog if WebAuthn
  // is enabled, indicating some verification steps are in progress.
  void ShowVerifyPendingDialog();

  // The callback function invoked when the cancel button in the verify pending
  // dialog is clicked. Will cancel the attempt to fetch unmask details.
  void OnDidCancelCardVerification();
#endif

  // Is set to true only when waiting for the callback to
  // OnCVCAuthenticationComplete() to be executed.
  bool is_authentication_in_progress_ = false;

  // Set to true if the card selected needs to be authenticated through CVC
  // first, and then FIDO. This happens when a user is opted-in but has not
  // previously authenticated this card with CVC on this device.
  bool should_follow_up_cvc_with_fido_auth_ = false;

  // The associated autofill driver. Weak reference.
  AutofillDriver* const driver_;

  // The associated autofill client. Weak reference.
  AutofillClient* const client_;

  // Client to interact with Payments servers.
  payments::PaymentsClient* payments_client_;

  // The personal data manager, used to save and load personal data to/from the
  // web database.
  // Weak reference.
  // May be NULL. NULL indicates OTR.
  PersonalDataManager* personal_data_manager_;

  // For logging metrics. May be NULL for tests.
  CreditCardFormEventLogger* form_event_logger_;

  // Timestamp used for metrics.
  base::TimeTicks preflight_call_timestamp_;

  // Meant for histograms recorded in FullCardRequest.
  base::TimeTicks form_parsed_timestamp_;

  // Authenticators for card unmasking.
  std::unique_ptr<CreditCardCVCAuthenticator> cvc_authenticator_;
#if !defined(OS_IOS)
  std::unique_ptr<CreditCardFIDOAuthenticator> fido_authenticator_;
#endif

  // Suggested authentication method and other information to facilitate card
  // unmasking.
  AutofillClient::UnmaskDetails unmask_details_;

  // Resets when PrepareToFetchCreditCard() is called, if not already reset.
  // Signaled when OnDidGetUnmaskDetails() is called or after timeout.
  // Authenticate() is called when signaled.
  base::WaitableEvent ready_to_start_authentication_;

  // Required to avoid any unnecessary preflight calls to Payments servers.
  // Initial state is signaled. Resets when PrepareToFetchCreditCard() is
  // called. Signaled after an authentication is complete or after a timeout.
  // GetUnmaskDetailsIfUserIsVerifiable() is not called unless this is signaled.
  base::WaitableEvent can_fetch_unmask_details_;

  // The credit card being accessed.
  const CreditCard* card_;

  // Set to true only if user has a verifying platform authenticator.
  // e.g. Touch/Face ID, Windows Hello, Android fingerprint, etc., is available
  // and enabled.
  base::Optional<bool> is_user_verifiable_;

  // True only if currently waiting on unmask details. This avoids making
  // unnecessary calls to payments.
  bool unmask_details_request_in_progress_ = false;

  // The object attempting to access a card.
  base::WeakPtr<Accessor> accessor_;

  base::WeakPtrFactory<CreditCardAccessManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CreditCardAccessManager);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_ACCESS_MANAGER_H_
