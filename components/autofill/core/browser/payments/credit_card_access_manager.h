// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_ACCESS_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_ACCESS_MANAGER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/metrics/form_events/credit_card_form_event_logger.h"
#include "components/autofill/core/browser/payments/credit_card_cvc_authenticator.h"
#include "components/autofill/core/browser/payments/credit_card_otp_authenticator.h"
#include "components/autofill/core/browser/payments/credit_card_risk_based_authenticator.h"
#include "components/autofill/core/browser/payments/mandatory_reauth_manager.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_window_manager.h"
#include "components/autofill/core/browser/payments/wait_for_signal_or_timeout.h"
#include "components/autofill/core/browser/personal_data_manager.h"

#if !BUILDFLAG(IS_IOS)
#include "components/autofill/core/browser/payments/credit_card_fido_authenticator.h"
#endif

namespace autofill {

class AutofillClient;
enum class WebauthnDialogCallbackType;

// Flow type denotes which card unmask authentication method was used.
// TODO(crbug.com/40216473): Deprecate kCvcThenFido, kCvcFallbackFromFido, and
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
  // VCN 3DS was the only challenge option returned.
  kThreeDomainSecure = 7,
  // VCN 3DS was one of the challenge options returned in the challenge
  // selection dialog, and user selected the 3DS challenge option.
  kThreeDomainSecureConsentAlreadyGiven = 8,
  kMaxValue = kThreeDomainSecureConsentAlreadyGiven,
};

// TODO(crbug.com/40197696): Remove this. This was added and never used.
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

// TODO(crbug.com/40927041): Remove CVC from CachedServerCardInfo.
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
class CreditCardAccessManager
    : public CreditCardCvcAuthenticator::Requester,
#if !BUILDFLAG(IS_IOS)
      public CreditCardFidoAuthenticator::Requester,
#endif
      public CreditCardOtpAuthenticator::Requester,
      public CreditCardRiskBasedAuthenticator::Requester {
 public:
  using OnCreditCardFetchedCallback =
      base::OnceCallback<void(CreditCardFetchResult, const CreditCard*)>;
  using OtpAuthenticationResponse =
      CreditCardOtpAuthenticator::OtpAuthenticationResponse;

  CreditCardAccessManager(AutofillManager* manager,
                          autofill_metrics::CreditCardFormEventLogger*
                              credit_card_form_event_logger);

  CreditCardAccessManager(const CreditCardAccessManager&) = delete;
  CreditCardAccessManager& operator=(const CreditCardAccessManager&) = delete;

  ~CreditCardAccessManager() override;

  // Logs information about current credit card data.
  void UpdateCreditCardFormEventLogger();

  // Returns false only if some form of authentication is still in progress.
  bool ShouldClearPreviewedForm();

  // Makes a call to Google Payments to retrieve authentication details.
  virtual void PrepareToFetchCreditCard();

  // `on_credit_card_fetched` is run once `card` is fetched.
  virtual void FetchCreditCard(
      const CreditCard* card,
      OnCreditCardFetchedCallback on_credit_card_fetched);

  // Checks whether we should offer risk-based authentication for masked server
  // card retrieval.
  bool IsMaskedServerCardRiskBasedAuthAvailable() const;

  // If |opt_in| = true, opts the user into using FIDO authentication for card
  // unmasking. Otherwise, opts the user out. If |creation_options| is set,
  // WebAuthn registration prompt will be invoked to create a new credential.
  void FIDOAuthOptChange(bool opt_in);

  // Makes a call to FIDOAuthOptChange() with |opt_in|.
  // TODO(crbug.com/40621544): Add a rate limiter to counter spam clicking.
  void OnSettingsPageFIDOAuthToggled(bool opt_in);

  // Resets the rate limiter for fetching unmask deatils. Used with
  // PostTaskWithDelay() with a timeout.
  void SignalCanFetchUnmaskDetails();

  // Caches CreditCard and corresponding CVC for unmasked card so that
  // card info can later be filled without attempting to auth again.
  // TODO(crbug.com/40126138): Add browsertests for this.
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

  // CreditCardRiskBasedAuthenticator::Requester:
  void OnRiskBasedAuthenticationResponseReceived(
      const CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse&
          response) override;
  void OnVirtualCardRiskBasedAuthenticationResponseReceived(
      payments::PaymentsAutofillClient::PaymentsRpcResult result,
      const payments::PaymentsNetworkInterface::UnmaskResponseDetails&
          response_details) override;

 private:
  friend class CreditCardAccessManagerTestApi;

  AutofillClient& autofill_client() { return manager_->client(); }

  payments::PaymentsAutofillClient& payments_autofill_client() {
    return *autofill_client().GetPaymentsAutofillClient();
  }

  PersonalDataManager& personal_data_manager() {
    return *autofill_client().GetPersonalDataManager();
  }

  PaymentsDataManager& payments_data_manager() {
    return personal_data_manager().payments_data_manager();
  }

  base::WeakPtr<CreditCardAccessManager> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Returns whether or not unmasked card cache is empty. Exposed for
  // testing.
  bool UnmaskedCardCacheIsEmpty();

  // Invoked from CreditCardFidoAuthenticator::IsUserVerifiable().
  // |is_user_verifiable| is set to true only if user has a verifying platform
  // authenticator. e.g. Touch/Face ID, Windows Hello, Android fingerprint,
  // etc., is available and enabled. If set to true, then an Unmask Details
  // request will be sent to Google Payments.
  void GetUnmaskDetailsIfUserIsVerifiable(bool is_user_verifiable);

  // Log success metrics based on `unmask_auth_flow_type` if user passed
  // authentication, as well as fill the form.
  void LogMetricsAndFillFormForServerUnmaskFlows(
      UnmaskAuthFlowType unmask_auth_flow_type);

  // Sets |unmask_details_|. May be ignored if response is too late and user is
  // not opted-in for FIDO auth, or if user does not select a card.
  void OnDidGetUnmaskDetails(
      payments::PaymentsAutofillClient::PaymentsRpcResult result,
      payments::PaymentsNetworkInterface::UnmaskDetails& unmask_details);

  // Determines what type of authentication is required. `fido_auth_enabled`
  // suggests whether the server has offered FIDO auth as an option.
  void StartAuthenticationFlow(bool fido_auth_enabled);
  void StartAuthenticationFlowForVirtualCard(bool fido_auth_enabled);
  void StartAuthenticationFlowForMaskedServerCard(bool fido_auth_enabled);

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

  // TODO(crbug.com/40639086): Move this function under the build flags after
  // the refactoring is done. Offer the option to use WebAuthn for
  // authenticating future card unmasking.
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

  // Helper function to fetch local cards.
  void FetchLocalCard();

  // Checks if Mandatory Re-auth is needed after the card has been returned. If
  // needed, starts the device authentication flow before filling the form.
  // Otherwise, directly fills the form.
  void OnNonInteractiveAuthenticationSuccess(
      CreditCard::RecordType record_type);

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

  // Starts the device authentication flow during a payments autofill form fill.
  // `OnDeviceAuthenticationResponseForFilling()` will be invoked when we
  // receive a response from the device authentication.
  // `on_credit_card_fetched_callback_` will be used to handle the response of
  // the authentication, and possibly fill the card into the form. `card` is the
  // card that needs to be filled. This function should only be called on
  // platforms where DeviceAuthenticator is present.
  // TODO(crbug.com/40268876): Move authentication logic for re-auth into
  // MandatoryReauthManager.
  void StartDeviceAuthenticationForFilling(const CreditCard* card);

  // Callback function invoked when we receive a response from a mandatory
  // re-auth authentication in a flow where we might fill the card after the
  // response. If it is successful, we will fill `card` into the form using
  // `accessor`, otherwise we will handle the error. `successful_auth` is true
  // if the authentication was successful, false otherwise. Pass
  // `authenticate_method` for logging purpose.
  // TODO(crbug.com/40268876): Move authentication logic for re-auth into
  // MandatoryReauthManager.
  void OnDeviceAuthenticationResponseForFilling(
      payments::MandatoryReauthAuthenticationMethod authentication_method,
      const CreditCard* card,
      bool successful_auth);

  // Notifies the class that triggered card unmasking that the unmasking flow
  // has completed. This method is run after a VCN 3DS authentication has
  // completed.
  void OnVcn3dsAuthenticationComplete(
      payments::PaymentsWindowManager::Vcn3dsAuthenticationResponse response);

  // The current form of authentication in progress.
  UnmaskAuthFlowType unmask_auth_flow_type_ = UnmaskAuthFlowType::kNone;

  // Is set to true only when waiting for the callback to
  // OnCvcAuthenticationComplete() to be executed.
  bool is_authentication_in_progress_ = false;

  // The owning AutofillManager.
  const raw_ref<AutofillManager> manager_;

  // For logging metrics.
  const raw_ptr<autofill_metrics::CreditCardFormEventLogger> form_event_logger_;

  // Timestamp used for preflight call metrics.
  std::optional<base::TimeTicks> preflight_call_timestamp_;

  // Timestamp used for user-perceived latency metrics.
  std::optional<base::TimeTicks>
      card_selected_without_unmask_details_timestamp_;

  // Timestamp for when fido_authenticator_->IsUserVerifiable() is called.
  std::optional<base::TimeTicks> is_user_verifiable_called_timestamp_;

#if !BUILDFLAG(IS_IOS)
  std::unique_ptr<CreditCardFidoAuthenticator> fido_authenticator_;

  // User opt in/out intention when local pref and payments mismatch.
  UserOptInIntention opt_in_intention_ = UserOptInIntention::kUnspecified;
#endif

  // Struct to store necessary information to start an authentication. It is
  // populated before an authentication is offered. It includes suggested
  // authentication methods and other information to facilitate card unmasking.
  payments::PaymentsNetworkInterface::UnmaskDetails unmask_details_;

  // Structs to store information passed to and fetched from the server for
  // virtual card unmasking.
  payments::PaymentsNetworkInterface::UnmaskRequestDetails
      virtual_card_unmask_request_details_;
  payments::PaymentsNetworkInterface::UnmaskResponseDetails
      virtual_card_unmask_response_details_;

  // Struct to store response returned by CreditCardRiskBasedAuthenticator.
  CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse
      risk_based_authentication_response_;

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
  // It will be set when user preview or select the card. Before authentication,
  // the card is the masked server card which is retrieved from webdatabase.
  // After FIDO, CVC, OTP authentication, it will be override by a new card
  // constructed by the server response.
  std::unique_ptr<CreditCard> card_;

  // Set to true only if user has a verifying platform authenticator.
  // e.g. Touch/Face ID, Windows Hello, Android fingerprint, etc., is available
  // and enabled.
  std::optional<bool> is_user_verifiable_;

  // True only if currently waiting on unmask details. This avoids making
  // unnecessary calls to payments.
  bool unmask_details_request_in_progress_ = false;

  // Callback to notify the caller of the access manager when fetching the
  // card has finished. Only has a meaningful value when an authentication is in
  // progress.
  OnCreditCardFetchedCallback on_credit_card_fetched_callback_;

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
