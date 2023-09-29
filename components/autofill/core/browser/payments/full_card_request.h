// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_FULL_CARD_REQUEST_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_FULL_CARD_REQUEST_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/payments/card_unmask_delegate.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_options.h"
#include "url/origin.h"

namespace autofill {

class BrowserAutofillManagerTest;
class AutofillMetricsTest;
class CreditCardAccessManagerTest;
class CreditCardCvcAuthenticatorTest;
class CreditCard;
class PersonalDataManager;

namespace autofill_metrics {
class AutofillMetricsBaseTest;
}

namespace payments {

// Retrieves the full card details, including the pan and the cvc.
// TODO(crbug/1061638): Refactor to use base::WaitableEvent where possible.
class FullCardRequest final : public CardUnmaskDelegate {
 public:
  // The type of failure.
  enum FailureType {
    UNKNOWN,

    // The user closed the prompt. The following scenarios are possible:
    // 1) The user declined to enter their CVC and closed the prompt.
    // 2) The user provided their CVC, got auth declined and then closed the
    //    prompt without attempting a second time.
    // 3) The user provided their CVC and closed the prompt before waiting for
    //    the result.
    PROMPT_CLOSED,

    // The card could not be looked up due to card auth declined or failed.
    VERIFICATION_DECLINED,

    // The request failed due to transient failures when retrieving virtual card
    // information.
    VIRTUAL_CARD_RETRIEVAL_TRANSIENT_FAILURE,

    // The request failed due to permanent failures when retrieving virtual card
    // information.
    VIRTUAL_CARD_RETRIEVAL_PERMANENT_FAILURE,

    // The request failed for technical reasons, such as a closing page or lack
    // of network connection.
    GENERIC_FAILURE
  };

  // The interface for receiving the full card details.
  class ResultDelegate {
   public:
    virtual ~ResultDelegate() = default;
    virtual void OnFullCardRequestSucceeded(
        const payments::FullCardRequest& full_card_request,
        const CreditCard& card,
        const std::u16string& cvc) = 0;
    virtual void OnFullCardRequestFailed(CreditCard::RecordType card_type,
                                         FailureType failure_type) = 0;
  };

  // The delegate responsible for displaying the unmask prompt UI.
  class UIDelegate {
   public:
    virtual ~UIDelegate() = default;
    virtual void ShowUnmaskPrompt(
        const CreditCard& card,
        const CardUnmaskPromptOptions& card_unmask_prompt_options,
        base::WeakPtr<CardUnmaskDelegate> delegate) = 0;
    virtual void OnUnmaskVerificationResult(
        AutofillClient::PaymentsRpcResult result) = 0;

#if BUILDFLAG(IS_ANDROID)
    // Returns whether or not the user, while on the CVC prompt, should be
    // offered to switch to FIDO authentication for card unmasking. This will
    // always be false for Desktop since FIDO authentication is offered as a
    // separate prompt after the CVC prompt. On Android, however, this may be
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

  // The parameters should outlive the FullCardRequest.
  FullCardRequest(AutofillClient* autofill_client,
                  payments::PaymentsClient* payments_client,
                  PersonalDataManager* personal_data_manager);

  FullCardRequest(const FullCardRequest&) = delete;
  FullCardRequest& operator=(const FullCardRequest&) = delete;

  ~FullCardRequest();

  // Retrieves the pan for |card| after querying the user for CVC and invokes
  // Delegate::OnFullCardRequestSucceeded() or
  // Delegate::OnFullCardRequestFailed(). Only one request should be active at a
  // time.
  //
  // If the card is local, has a non-empty GUID, and the user has updated its
  // expiration date, then this function will write the new information to
  // autofill table on disk.
  void GetFullCard(const CreditCard& card,
                   AutofillClient::UnmaskCardReason reason,
                   base::WeakPtr<ResultDelegate> result_delegate,
                   base::WeakPtr<UIDelegate> ui_delegate,
                   const url::Origin& merchant_domain_for_footprints);

  // Refer to the comment above `GetFullCard()` for the high level overview of
  // how this function works. The additional fields in this function are
  // Virtual Card specific fields that are required in the UnmaskCardRequest for
  // unmasking a Virtual Card via CVC authentication.
  void GetFullVirtualCardViaCVC(
      const CreditCard& card,
      AutofillClient::UnmaskCardReason reason,
      base::WeakPtr<ResultDelegate> result_delegate,
      base::WeakPtr<UIDelegate> ui_delegate,
      const GURL& last_committed_primary_main_frame_origin,
      const std::string& vcn_context_token,
      const CardUnmaskChallengeOption& selected_challenge_option,
      const url::Origin& merchant_domain_for_footprints);

  // Retrieves the pan for `card` through a FIDO assertion and invokes
  // Delegate::OnFullCardRequestSucceeded() or
  // Delegate::OnFullCardRequestFailed(). Only one request should be active at a
  // time. `merchant_domain_for_footprints` is the full origin of the primary
  // frame where the unmasking happened this is used for personalization if the
  // user is not in incognito mode. `last_committed_primary_main_frame_origin`
  // is the full origin of the primary main frame where the card retrieval
  // happens. `context_token` is used for providing context of the request to
  // the server to link related requests.
  // `last_committed_primary_main_frame_origin` and `context_token` are
  // populated if the full card request is for a virtual card.
  //
  // If the card is local, has a non-empty GUID, and the user has updated its
  // expiration date, then this function will write the new information to
  // autofill table on disk.
  void GetFullCardViaFIDO(
      const CreditCard& card,
      AutofillClient::UnmaskCardReason reason,
      base::WeakPtr<ResultDelegate> result_delegate,
      base::Value::Dict fido_assertion_info,
      const url::Origin& merchant_domain_for_footprints,
      absl::optional<GURL> last_committed_primary_main_frame_origin =
          absl::nullopt,
      absl::optional<std::string> context_token = absl::nullopt);

  // Called by the payments client when a card has been unmasked.
  void OnDidGetRealPan(
      AutofillClient::PaymentsRpcResult result,
      payments::PaymentsClient::UnmaskResponseDetails& response_details);

  // Called when verification is cancelled. This is used only by
  // CreditCardFidoAuthenticator to cancel the flow for opted-in users.
  void OnFIDOVerificationCancelled();

  payments::PaymentsClient::UnmaskResponseDetails unmask_response_details()
      const {
    return unmask_response_details_;
  }

  payments::PaymentsClient::UnmaskRequestDetails*
  GetUnmaskRequestDetailsForTesting() const {
    return request_.get();
  }

  bool GetShouldUnmaskCardForTesting() const { return should_unmask_card_; }

 private:
  friend class autofill::BrowserAutofillManagerTest;
  friend class autofill::AutofillMetricsTest;
  friend class autofill::autofill_metrics::AutofillMetricsBaseTest;
  friend class autofill::CreditCardAccessManagerTest;
  friend class autofill::CreditCardCvcAuthenticatorTest;

  // Retrieves the pan for `card` and invokes
  // `Delegate::OnFullCardRequestSucceeded()` or
  // `Delegate::OnFullCardRequestFailed()`. Only one request should be active at
  // a time.
  //
  // If `ui_delegate` is set, then the user is queried for CVC.
  // Else if `fido_assertion_info` is a dictionary, FIDO verification is used.
  // `last_committed_primary_main_frame_origin` is the full origin of the
  // primary main frame on which the card is unmasked. `context_token` is used
  // for providing context of the request to the server to link related
  // requests. `selected_challenge_option` is the challenge option that was
  // selected for authentication when the user was challenged with several
  // authentication methods. `last_committed_primary_main_frame_origin`,
  // `context_token`, and `selected_challenge_option` need to be specified if
  // the full card request is for a virtual card.
  // `merchant_domain_for_footprints` is the full origin of the primary main
  // frame where the unmasking happened that is used for personalization if the
  // user is not in incognito mode.
  //
  // If the card is local, has a non-empty GUID, and the user has updated its
  // expiration date, then this function will write the new information to
  // autofill table on disk.
  void GetFullCardImpl(
      const CreditCard& card,
      AutofillClient::UnmaskCardReason reason,
      base::WeakPtr<ResultDelegate> result_delegate,
      base::WeakPtr<UIDelegate> ui_delegate,
      absl::optional<base::Value::Dict> fido_assertion_info,
      absl::optional<GURL> last_committed_primary_main_frame_origin,
      absl::optional<std::string> context_token,
      absl::optional<CardUnmaskChallengeOption> selected_challenge_option,
      const url::Origin& merchant_domain_for_footprints);

  // CardUnmaskDelegate:
  void OnUnmaskPromptAccepted(
      const UserProvidedUnmaskDetails& user_response) override;
  void OnUnmaskPromptClosed() override;
  bool ShouldOfferFidoAuth() const override;

  // Called by autofill client when the risk data has been loaded.
  void OnDidGetUnmaskRiskData(const std::string& risk_data);

  // Makes final preparations for the unmask request and calls
  // PaymentsClient::UnmaskCard().
  void SendUnmaskCardRequest();

  // Resets the state of the request.
  void Reset();

  // The associated autofill client.
  const raw_ref<AutofillClient> autofill_client_;

  // Responsible for unmasking a masked server card.
  const raw_ptr<payments::PaymentsClient> payments_client_;

  // Responsible for updating the server card on disk after it's been unmasked.
  const raw_ptr<PersonalDataManager> personal_data_manager_;

  // Receiver of the full PAN and CVC.
  base::WeakPtr<ResultDelegate> result_delegate_;

  // Delegate responsible for displaying the unmask prompt UI.
  base::WeakPtr<UIDelegate> ui_delegate_;

  // The pending request to get a card's full PAN and CVC.
  std::unique_ptr<payments::PaymentsClient::UnmaskRequestDetails> request_;

  // Whether the card unmask request should be sent to the payment server.
  bool should_unmask_card_;

  // The timestamp when the full PAN was requested from a server. For
  // histograms.
  base::TimeTicks real_pan_request_timestamp_;

  // Includes all details from GetRealPan response.
  payments::PaymentsClient::UnmaskResponseDetails unmask_response_details_;

  // Enables destroying FullCardRequest while CVC prompt is showing or a server
  // communication is pending.
  base::WeakPtrFactory<FullCardRequest> weak_ptr_factory_{this};
};

}  // namespace payments
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_FULL_CARD_REQUEST_H_
