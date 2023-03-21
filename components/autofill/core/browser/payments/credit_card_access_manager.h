// Copyright 2019 The Chromium Authors
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

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/metrics/form_events/credit_card_form_event_logger.h"
#include "components/autofill/core/browser/payments/credit_card_cvc_authenticator.h"
#include "components/autofill/core/browser/payments/credit_card_otp_authenticator.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/payments/wait_for_signal_or_timeout.h"
#include "components/autofill/core/browser/personal_data_manager.h"

#if !BUILDFLAG(IS_IOS)
#include "components/autofill/core/browser/payments/credit_card_fido_authenticator.h"
#endif

namespace autofill {

class BrowserAutofillManager;
enum class WebauthnDialogCallbackType;

namespace autofill_metrics {
class AutofillMetricsBaseTest;
}

// Flow type denotes which card unmask authentication method was used.
// TODO(crbug/1300959): Deprecate kCvcThenFido, kCvcFallbackFromFido, and
// kOtpFallbackFromFido.
enum class UnmaskAuthFlowType {
  kNone = 0,
  // Only CVC prompt was shown.
  kCvc = 1,
  // Only WebAuthn prompt was shown.
  kFido = 2,
  // CVC authentication was required in addition to WebAuthn.
  kCvcThenFido = 3,
  // FIDO authentication failed and fell back to CVC authentication.
  kCvcFallbackFromFido = 4,
  // OTP authentication was offered.
  kOtp = 5,
  // FIDO authentication failed and fell back to OTP authentication.
  kOtpFallbackFromFido = 6,
  kMaxValue = kOtpFallbackFromFido,
};

// TODO(crbug.com/1249665): Remove this. This was added and never used.
// The result of the attempt to fetch full information for a credit card.
enum class CreditCardFetchResult {
  kNone = 0,
  // The attempt succeeded retrieving the full information of a credit card.
  kSuccess = 1,
  // The attempt failed due to a transient error.
  kTransientError = 2,
  // The attempt failed due to a permanent error.
  kPermanentError = 3,
  kMaxValue = kPermanentError,
};

struct CachedServerCardInfo {
 public:
  // An unmasked CreditCard.
  CreditCard card;

  std::u16string cvc;

  // Number of times this card was accessed from the cache.
  int cache_uses = 0;
};

// Manages logic for accessing credit cards either stored locally or stored
// with Google Payments. Owned by BrowserAutofillManager.
class CreditCardAccessManager : public CreditCardCvcAuthenticator::Requester,
#if !BUILDFLAG(IS_IOS)
                                public CreditCardFidoAuthenticator::Requester,
#endif
                                public CreditCardOtpAuthenticator::Requester {
 public:
  class Accessor {
   public:
    virtual ~Accessor() = default;
    virtual void OnCreditCardFetched(CreditCardFetchResult result,
                                     const CreditCard* credit_card,
                                     const std::u16string& cvc) = 0;
  };

  CreditCardAccessManager(AutofillDriver* driver,
                          AutofillClient* client,
                          PersonalDataManager* personal_data_manager,
                          autofill_metrics::CreditCardFormEventLogger*
                              credit_card_form_event_logger);

  CreditCardAccessManager(const CreditCardAccessManager&) = delete;
  CreditCardAccessManager& operator=(const CreditCardAccessManager&) = delete;

  ~CreditCardAccessManager() override;

  // Logs information about current credit card data.
  void UpdateCreditCardFormEventLogger();
  // Returns true when deletion is allowed. Only local cards can be deleted.
  bool DeleteCard(const CreditCard* card);
  // Returns true if the |card| is deletable. Fills out
  // |title| and |body| with relevant user-facing text.
  bool GetDeletionConfirmationText(const CreditCard* card,
                                   std::u16string* title,
                                   std::u16string* body);

  // Returns false only if some form of authentication is still in progress.
  bool ShouldClearPreviewedForm();

  // Makes a call to Google Payments to retrieve authentication details.
  void PrepareToFetchCreditCard();

  // Calls |accessor->OnCreditCardFetched()| once credit card is fetched.
  virtual void FetchCreditCard(const CreditCard* card,
                               base::WeakPtr<Accessor> accessor);

  // If |opt_in| = true, opts the user into using FIDO authentication for card
  // unmasking. Otherwise, opts the user out. If |creation_options| is set,
  // WebAuthn registration prompt will be invoked to create a new credential.
  void FIDOAuthOptChange(bool opt_in);

  // Makes a call to FIDOAuthOptChange() with |opt_in|.
  // TODO(crbug/949269): Add a rate limiter to counter spam clicking.
  void OnSettingsPageFIDOAuthToggled(bool opt_in);

  // Resets the rate limiter for fetching unmask deatils. Used with
  // PostTaskWithDelay() with a timeout.
  void SignalCanFetchUnmaskDetails();

  // Caches CreditCard and corresponding CVC for unmasked card so that
  // card info can later be filled without attempting to auth again.
  // TODO(crbug/1069929): Add browsertests for this.
  void CacheUnmaskedCardInfo(const CreditCard& card, const std::u16string& cvc);

  // Return the info for the server cards present in the
  // |unamsked_cards_cache_|.
  std::vector<const CachedServerCardInfo*> GetCachedUnmaskedCards() const;

  // Returns true if a |unmasked_cards_cache| contains an entry for the card.
  bool IsCardPresentInUnmaskedCache(const CreditCard& card) const;

#if !BUILDFLAG(IS_IOS)
  CreditCardFidoAuthenticator* GetOrCreateFidoAuthenticator();
#endif

  // CreditCardCvcAuthenticator::Requester:
  void OnCvcAuthenticationComplete(
      const CreditCardCvcAuthenticator::CvcAuthenticationResponse& response)
      override;

  // CreditCardOtpAuthenticator::Requester:
  void OnOtpAuthenticationComplete(
      const CreditCardOtpAuthenticator::OtpAuthenticationResponse& response)
      override;

  void SetUnmaskDetailsRequestInProgressForTesting(
      bool unmask_details_request_in_progress) {
    unmask_details_request_in_progress_ = unmask_details_request_in_progress;
  }

  bool ShouldOfferFidoOptInDialogForTesting(
      const CreditCardCvcAuthenticator::CvcAuthenticationResponse& response) {
    return ShouldOfferFidoOptInDialog(response);
  }

#if BUILDFLAG(IS_ANDROID)
  bool ShouldOfferFidoAuthForTesting() { return ShouldOfferFidoAuth(); }
#endif

 private:
  // TODO(crbug.com/1249665): Remove FRIEND and change everything to _ForTesting
  // or public.
  FRIEND_TEST_ALL_PREFIXES(CreditCardAccessManagerBrowserTest,
                           NavigateFromPage_UnmaskedCardCacheResets);
  FRIEND_TEST_ALL_PREFIXES(CreditCardAccessManagerTest,
                           PreflightCallRateLimited);
  FRIEND_TEST_ALL_PREFIXES(CreditCardAccessManagerTest,
                           UnmaskAuthFlowEvent_AlsoLogsVirtualCardSubhistogram);
  FRIEND_TEST_ALL_PREFIXES(CreditCardAccessManagerTest,
                           RiskBasedVirtualCardUnmasking_Success);
  FRIEND_TEST_ALL_PREFIXES(
      CreditCardAccessManagerTest,
      RiskBasedVirtualCardUnmasking_AuthenticationRequired_OtpOnly);
  FRIEND_TEST_ALL_PREFIXES(
      CreditCardAccessManagerTest,
      RiskBasedVirtualCardUnmasking_AuthenticationRequired_FidoAndOtp_PrefersFido);
  FRIEND_TEST_ALL_PREFIXES(
      CreditCardAccessManagerTest,
      RiskBasedVirtualCardUnmasking_AuthenticationRequired_FidoAndOtp_FidoNotOptedIn);
  FRIEND_TEST_ALL_PREFIXES(
      CreditCardAccessManagerTest,
      RiskBasedVirtualCardUnmasking_AuthenticationRequired_FidoOnly);
  FRIEND_TEST_ALL_PREFIXES(
      CreditCardAccessManagerTest,
      RiskBasedVirtualCardUnmasking_AuthenticationRequired_FidoAndOtp_FidoFailedFallBackToOtp);
  FRIEND_TEST_ALL_PREFIXES(
      CreditCardAccessManagerTest,
      RiskBasedVirtualCardUnmasking_AuthenticationRequired_FidoOnly_FidoNotOptedIn);
  FRIEND_TEST_ALL_PREFIXES(
      CreditCardAccessManagerTest,
      RiskBasedVirtualCardUnmasking_CreditCardAccessManagerReset_TriggersOtpAuthenticatorResetOnFlowCancelled);
  FRIEND_TEST_ALL_PREFIXES(
      CreditCardAccessManagerTest,
      RiskBasedVirtualCardUnmasking_Failure_MerchantOptedOut);
  FRIEND_TEST_ALL_PREFIXES(
      CreditCardAccessManagerTest,
      RiskBasedVirtualCardUnmasking_Failure_NoOptionReturned);
  FRIEND_TEST_ALL_PREFIXES(
      CreditCardAccessManagerTest,
      RiskBasedVirtualCardUnmasking_Failure_VirtualCardRetrievalError);
  FRIEND_TEST_ALL_PREFIXES(CreditCardAccessManagerTest,
                           RiskBasedVirtualCardUnmasking_FlowCancelled);
  friend class autofill_metrics::AutofillMetricsBaseTest;
  friend class CreditCardAccessManagerTest;

#if !BUILDFLAG(IS_IOS)
  void set_fido_authenticator_for_testing(
      std::unique_ptr<CreditCardFidoAuthenticator> fido_authenticator) {
    fido_authenticator_ = std::move(fido_authenticator);
  }
#endif

#if defined(UNIT_TEST)
  // Mocks that a virtual card was selected, so unit tests that don't run the
  // actual Autofill suggestions dropdown UI can still follow their remaining
  // steps under the guise of doing it for a virtual card.
  void set_virtual_card_suggestion_selected_on_form_event_logger_for_testing() {
    form_event_logger_->set_latest_selected_card_was_virtual_card_for_testing(
        /*latest_selected_card_was_virtual_card=*/true);
  }
#endif

  // Returns whether or not unmasked card cache is empty. Exposed for testing.
  bool UnmaskedCardCacheIsEmpty();

  // Invoked from CreditCardFidoAuthenticator::IsUserVerifiable().
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

  // Determines what type of authentication is required. |fido_auth_enabled|
  // suggests whether the server has offered FIDO auth as an option.
  void GetAuthenticationType(bool fido_auth_enabled);
  void GetAuthenticationTypeForVirtualCard(bool fido_auth_enabled);
  void GetAuthenticationTypeForMaskedServerCard(bool fido_auth_enabled);

  // Starts the authentication process and delegates the task to authenticators
  // based on the `unmask_auth_flow_type`. Also logs authentication type if
  // FIDO auth was suggested.
  void Authenticate(UnmaskAuthFlowType unmask_auth_flow_type);

#if BUILDFLAG(IS_ANDROID)
  bool ShouldOfferFidoAuth() const override;
  bool UserOptedInToFidoFromSettingsPageOnMobile() const override;
#endif

#if !BUILDFLAG(IS_IOS)
  // CreditCardFidoAuthenticator::Requester:
  void OnFIDOAuthenticationComplete(
      const CreditCardFidoAuthenticator::FidoAuthenticationResponse& response)
      override;
  void OnFidoAuthorizationComplete(bool did_succeed) override;
#endif

  bool is_authentication_in_progress() {
    return is_authentication_in_progress_;
  }

  // Returns whether the user has opted in to FIDO auth.
  bool IsUserOptedInToFidoAuth();

  // Returns whether FIDO auth is enabled. |fido_auth_offered| indicates whether
  // Payments server has offered FIDO auth as an option.
  bool IsFidoAuthEnabled(bool fido_auth_offered);

  // Returns true if |unmask_details_| is set and the card selected is listed as
  // FIDO eligible.
  bool IsSelectedCardFidoAuthorized();

  // Returns true if there should be an immediate response after a CVC
  // Authentication. The cases where we would have an immediate response is if
  // there is no need for FIDO authentication after a successful CVC
  // authentication, or the CVC authentication was unsuccessful. The result of
  // this function and ShouldRegisterCardWithFido() are mutually exclusive (can
  // not both be true).
  bool ShouldRespondImmediately(
      const CreditCardCvcAuthenticator::CvcAuthenticationResponse& response);

  // Returns true if FIDO registration should occur. The result of this function
  // and ShouldRespondImmediately() are mutually exclusive (can not both be
  // true).
  bool ShouldRegisterCardWithFido(
      const CreditCardCvcAuthenticator::CvcAuthenticationResponse& response);

  // Returns true if the we can offer FIDO opt-in for the user. In the
  // downstream flow, after we offer FIDO opt-in, if the user accepts we might
  // also offer FIDO authentication for the downstreamed card so that the FIDO
  // registration flow is complete.
  bool ShouldOfferFidoOptInDialog(
      const CreditCardCvcAuthenticator::CvcAuthenticationResponse& response);

  // TODO(crbug.com/991037): Move this function under the build flags after the
  // refactoring is done. Offer the option to use WebAuthn for authenticating
  // future card unmasking.
  void ShowWebauthnOfferDialog(std::string card_authorization_token);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // After card verification starts, shows the verify pending dialog if WebAuthn
  // is enabled, indicating some verification steps are in progress.
  void ShowVerifyPendingDialog();

  // Invokes the corresponding callback on different user's responses on either
  // the Webauthn offer dialog or verify pending dialog.
  void HandleDialogUserResponse(WebauthnDialogCallbackType type);
#endif

  // Returns the key for the given card to be used for inserting or querying the
  // `unmasked_card_cache_`.
  std::string GetKeyForUnmaskedCardsCache(const CreditCard& card) const;

  // Helper function to fetch masked server cards.
  void FetchMaskedServerCard();

  // Helper function to fetch virtual cards.
  void FetchVirtualCard();

  // Callback function invoked when risk data is fetched.
  void OnDidGetUnmaskRiskData(const std::string& risk_data);

  // Callback function invoked when an unmask response for a virtual card has
  // been received.
  void OnVirtualCardUnmaskResponseReceived(
      AutofillClient::PaymentsRpcResult result,
      payments::PaymentsClient::UnmaskResponseDetails& response_details);

  // Invoked when CreditCardAccessManager stops waiting for UnmaskDetails to
  // return. If OnDidGetUnmaskDetails() has been invoked,
  // |get_unmask_details_returned| should be set to true.
  void OnStopWaitingForUnmaskDetails(bool get_unmask_details_returned);

  // Callback function invoked when the user has accepted the authentication
  // selection dialog and chosen an auth method to use.
  void OnUserAcceptedAuthenticationSelectionDialog(
      const std::string& selected_challenge_option_id);

  // Callback function invoked when the user has cancelled the virtual card
  // unmasking.
  void OnVirtualCardUnmaskCancelled();

  // Reset all the member variables of |this| and restore initial states.
  void Reset();

  // Handles the FIDO opt-in status change.
  void HandleFidoOptInStatusChange();

  // Shows the authenticator selection dialog for users to confirm their choice
  // of authentication method.
  void ShowUnmaskAuthenticatorSelectionDialog();

  // Returns a pointer to the card unmask challenge option returned from the
  // server that has an id which matches the passed in `challenge_id`. If no
  // challenge option is found, this function will return nullptr.
  CardUnmaskChallengeOption* GetCardUnmaskChallengeOptionForChallengeId(
      const std::string& challenge_id);

  // `record_type` is the credit card record type of the card we are about to
  // unmask. This function returns true if we should log server card unmask
  // attempt metrics for `record_type`, and false otherwise. These metrics are
  // logged if we are attempting to unmask a card that has its information
  // stored in the server, such as a virtual card or a masked server card.
  bool ShouldLogServerCardUnmaskAttemptMetrics(
      CreditCard::RecordType record_type);

  // The current form of authentication in progress.
  UnmaskAuthFlowType unmask_auth_flow_type_ = UnmaskAuthFlowType::kNone;

  // Is set to true only when waiting for the callback to
  // OnCvcAuthenticationComplete() to be executed.
  bool is_authentication_in_progress_ = false;

  // The associated autofill driver. Weak reference.
  const raw_ptr<AutofillDriver> driver_;

  // The associated autofill client. Weak reference.
  const raw_ptr<AutofillClient> client_;

  // Client to interact with Payments servers.
  raw_ptr<payments::PaymentsClient> payments_client_;

  // The personal data manager, used to save and load personal data to/from the
  // web database.
  // Weak reference.
  // May be NULL. NULL indicates OTR.
  raw_ptr<PersonalDataManager> personal_data_manager_;

  // For logging metrics.
  raw_ptr<autofill_metrics::CreditCardFormEventLogger, DanglingUntriaged>
      form_event_logger_;

  // Timestamp used for preflight call metrics.
  absl::optional<base::TimeTicks> preflight_call_timestamp_;

  // Timestamp used for user-perceived latency metrics.
  absl::optional<base::TimeTicks>
      card_selected_without_unmask_details_timestamp_;

  // Timestamp for when fido_authenticator_->IsUserVerifiable() is called.
  absl::optional<base::TimeTicks> is_user_verifiable_called_timestamp_;

#if !BUILDFLAG(IS_IOS)
  std::unique_ptr<CreditCardFidoAuthenticator> fido_authenticator_;

  // User opt in/out intention when local pref and payments mismatch.
  UserOptInIntention opt_in_intention_ = UserOptInIntention::kUnspecified;
#endif

  // Struct to store necessary information to start an authentication. It is
  // populated before an authentication is offered. It includes suggested
  // authentication methods and other information to facilitate card unmasking.
  payments::PaymentsClient::UnmaskDetails unmask_details_;

  // Structs to store information passed to and fetched from the server for
  // virtual card unmasking.
  payments::PaymentsClient::UnmaskRequestDetails
      virtual_card_unmask_request_details_;
  payments::PaymentsClient::UnmaskResponseDetails
      virtual_card_unmask_response_details_;

  // Resets when PrepareToFetchCreditCard() is called, if not already reset.
  // Signaled when OnDidGetUnmaskDetails() is called or after timeout.
  // Authenticate() is called when signaled.
  WaitForSignalOrTimeout ready_to_start_authentication_;

  // Required to avoid any unnecessary calls to Payments servers by signifying
  // when preflight unmask details calls should be made. Initial state is true,
  // and is set to false when PrepareToFetchCreditCard() is called. Reset to
  // true after an authentication is complete or after a timeout.
  // GetUnmaskDetailsIfUserIsVerifiable() is not called unless this is true.
  bool can_fetch_unmask_details_ = true;

  // The credit card being accessed.
  std::unique_ptr<CreditCard> card_;

  // When authorizing a new card, the CVC will be temporarily stored after the
  // first CVC check, and then will be used to fill the form after FIDO
  // authentication is complete.
  std::u16string cvc_ = std::u16string();

  // Set to true only if user has a verifying platform authenticator.
  // e.g. Touch/Face ID, Windows Hello, Android fingerprint, etc., is available
  // and enabled.
  absl::optional<bool> is_user_verifiable_;

  // True only if currently waiting on unmask details. This avoids making
  // unnecessary calls to payments.
  bool unmask_details_request_in_progress_ = false;

  // The object attempting to access a card.
  base::WeakPtr<Accessor> accessor_;

  // Used only in virtual card authentication to differentiate between
  // authentication methods. Set when a challenge option is selected, and we are
  // about to render the specific challenge option's input dialog.
  raw_ptr<CardUnmaskChallengeOption> selected_challenge_option_ = nullptr;

  // Cached data of cards which have been unmasked. This is cleared upon page
  // navigation. Map key is the card's server_id.
  std::unordered_map<std::string, CachedServerCardInfo> unmasked_card_cache_;

  base::WeakPtrFactory<CreditCardAccessManager> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_ACCESS_MANAGER_H_
