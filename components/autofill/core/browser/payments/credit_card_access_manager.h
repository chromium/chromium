// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_ACCESS_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_ACCESS_MANAGER_H_

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/strings/string16.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/cancelable_task_tracker.h"
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
enum class WebauthnDialogCallbackType;

// Flow type denotes which card unmask authentication method was used.
enum class UnmaskAuthFlowType {
  kNone = 0,
  // Only CVC prompt was shown.
  kCvc = 1,
  // Only WebAuthn prompt was shown.
  kFido = 2,
  // CVC authentication was required in addition to WebAuthn.
  kCvcThenFido = 3,
  // WebAuthn prompt failed and fell back to CVC prompt.
  kCvcFallbackFromFido = 4,
};

struct CachedServerCardInfo {
 public:
  // An unmasked CreditCard.
  CreditCard card;

  base::string16 cvc;

  // Number of times this card was accessed from the cache.
  int cache_uses = 0;
};

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

  CreditCardAccessManager(
      AutofillDriver* driver,
      AutofillClient* client,
      PersonalDataManager* personal_data_manager,
      CreditCardFormEventLogger* credit_card_form_event_logger);
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

  // Resets the rate limiter for fetching unmask deatils. Used with
  // PostTaskWithDelay() with a timeout, and also called by AutofillDriver on
  // page refresh.
  void SignalCanFetchUnmaskDetails();

  // Caches CreditCard and corresponding CVC for unmasked card so that
  // card info can later be filled without attempting to auth again.
  // TODO(crbug/1069929): Add browsertests for this.
  void CacheUnmaskedCardInfo(const CreditCard& card, const base::string16& cvc);

  CreditCardCVCAuthenticator* GetOrCreateCVCAuthenticator();

#if !defined(OS_IOS)
  CreditCardFIDOAuthenticator* GetOrCreateFIDOAuthenticator();
#endif

 private:
  FRIEND_TEST_ALL_PREFIXES(CreditCardAccessManagerBrowserTest,
                           NavigateFromPage_UnmaskedCardCacheResets);
  FRIEND_TEST_ALL_PREFIXES(CreditCardAccessManagerTest,
                           PreflightCallRateLimited);
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

  // Returns whether or not unmasked card cache is empty. Exposed for testing.
  bool UnmaskedCardCacheIsEmpty();

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
  void OnDidGetUnmaskDetails(
      AutofillClient::PaymentsRpcResult result,
      payments::PaymentsClient::UnmaskDetails& unmask_details);

  // Determines what form of authentication is required.
  UnmaskAuthFlowType GetAuthenticationType(bool get_unmask_details_returned);

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
#if defined(OS_ANDROID)
  bool ShouldOfferFidoAuth() const override;
  bool UserOptedInToFidoFromSettingsPageOnMobile() const override;
#endif

#if !defined(OS_IOS)
  // CreditCardFIDOAuthenticator::Requester:
  void OnFIDOAuthenticationComplete(
      bool did_succeed,
      const CreditCard* card = nullptr,
      const base::string16& cvc = base::string16()) override;
  void OnFidoAuthorizationComplete(bool did_succeed) override;
#endif

  bool is_authentication_in_progress() {
    return is_authentication_in_progress_;
  }

  // Returns true only if |credit_card| is a local card.
  bool IsLocalCard(const CreditCard* credit_card);

  // If true, FetchCreditCard() should wait for OnDidGetUnmaskDetails() to begin
  // authentication. If false, FetchCreditCard() can begin authentication
  // immediately.
  bool IsFidoAuthenticationEnabled();

  // Returns true if |unmask_details_| is set and the card selected is listed as
  // FIDO eligible.
  bool IsSelectedCardFidoAuthorized();

  // TODO(crbug.com/991037): Move this function under the build flags after the
  // refactoring is done.
  // Offer the option to use WebAuthn for authenticating future card unmasking.
  void ShowWebauthnOfferDialog(std::string card_authorization_token);

#if !defined(OS_ANDROID) && !defined(OS_IOS)
  // After card verification starts, shows the verify pending dialog if WebAuthn
  // is enabled, indicating some verification steps are in progress.
  void ShowVerifyPendingDialog();

  // Invokes the corresponding callback on different user's responses on either
  // the Webauthn offer dialog or verify pending dialog.
  void HandleDialogUserResponse(WebauthnDialogCallbackType type);
#endif

  // Additionlly authorizes the card with FIDO. It also delays the form filling.
  // It should only be called when registering a new card or opting-in from
  // Android.
  void AdditionallyPerformFidoAuth(
      const CreditCardCVCAuthenticator::CVCAuthenticationResponse& response,
      base::Value request_options);

  // The current form of authentication in progress.
  UnmaskAuthFlowType unmask_auth_flow_type_ = UnmaskAuthFlowType::kNone;

  // Is set to true only when waiting for the callback to
  // OnCVCAuthenticationComplete() to be executed.
  bool is_authentication_in_progress_ = false;

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

  // For logging metrics.
  CreditCardFormEventLogger* form_event_logger_;

  // Timestamp used for preflight call metrics.
  base::TimeTicks preflight_call_timestamp_;

  // Timestamp used for user-perceived latency metrics.
  base::Optional<base::TimeTicks>
      card_selected_without_unmask_details_timestamp_ = base::nullopt;

  // Meant for histograms recorded in FullCardRequest.
  base::TimeTicks form_parsed_timestamp_;

  // Timestamp for when fido_authenticator_->IsUserVerifiable() is called.
  base::Optional<base::TimeTicks> is_user_verifiable_called_timestamp_ =
      base::nullopt;

  // Authenticators for card unmasking.
  std::unique_ptr<CreditCardCVCAuthenticator> cvc_authenticator_;
#if !defined(OS_IOS)
  std::unique_ptr<CreditCardFIDOAuthenticator> fido_authenticator_;

  // User opt in/out intention when local pref and payments mismatch.
  UserOptInIntention opt_in_intention_ = UserOptInIntention::kUnspecified;
#endif

  // Suggested authentication method and other information to facilitate card
  // unmasking.
  payments::PaymentsClient::UnmaskDetails unmask_details_;

  // Resets when PrepareToFetchCreditCard() is called, if not already reset.
  // Signaled when OnDidGetUnmaskDetails() is called or after timeout.
  // Authenticate() is called when signaled.
  base::WaitableEvent ready_to_start_authentication_;

  // Tracks the Authenticate() task that is signaled by
  // |ready_to_start_authentication_|, allowing it to be canceled if necessary.
  base::CancelableTaskTracker cancelable_authenticate_task_tracker_;

  // Required to avoid any unnecessary preflight calls to Payments servers.
  // Initial state is signaled. Resets when PrepareToFetchCreditCard() is
  // called. Signaled after an authentication is complete or after a timeout.
  // GetUnmaskDetailsIfUserIsVerifiable() is not called unless this is signaled.
  base::WaitableEvent can_fetch_unmask_details_;

  // The credit card being accessed.
  std::unique_ptr<CreditCard> card_;

  // When authorizing a new card, the CVC will be temporarily stored after the
  // first CVC check, and then will be used to fill the form after FIDO
  // authentication is complete.
  base::string16 cvc_ = base::string16();

  // Set to true only if user has a verifying platform authenticator.
  // e.g. Touch/Face ID, Windows Hello, Android fingerprint, etc., is available
  // and enabled.
  base::Optional<bool> is_user_verifiable_;

  // True only if currently waiting on unmask details. This avoids making
  // unnecessary calls to payments.
  bool unmask_details_request_in_progress_ = false;

  // The object attempting to access a card.
  base::WeakPtr<Accessor> accessor_;

  // Cached data of cards which have been unmasked. This is cleared upon page
  // navigation. Map key is the card's server_id.
  std::unordered_map<std::string, CachedServerCardInfo> unmasked_card_cache_;

  base::WeakPtrFactory<CreditCardAccessManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CreditCardAccessManager);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_ACCESS_MANAGER_H_
