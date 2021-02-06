// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_CLIENT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_CLIENT_H_

#include <set>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/card_unmask_delegate.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace signin {
class IdentityManager;
}  // namespace signin

namespace network {
struct ResourceRequest;
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace autofill {

class AccountInfoGetter;
class MigratableCreditCard;

namespace payments {

// Callback type for MigrateCards callback. |result| is the Payments Rpc result.
// |save_result| is an unordered_map parsed from the response whose key is the
// unique id (guid) for each card and value is the server save result string.
// |display_text| is the returned tip from Payments to show on the UI.
typedef base::OnceCallback<void(
    AutofillClient::PaymentsRpcResult result,
    std::unique_ptr<std::unordered_map<std::string, std::string>> save_result,
    const std::string& display_text)>
    MigrateCardsCallback;

// Billable service number is defined in Payments server to distinguish
// different requests.
const int kUnmaskCardBillableServiceNumber = 70154;
const int kUploadCardBillableServiceNumber = 70073;
const int kMigrateCardsBillableServiceNumber = 70264;

class PaymentsRequest;

// PaymentsClient issues Payments RPCs and manages responses and failure
// conditions. Only one request may be active at a time. Initiating a new
// request will cancel a pending request.
// Tests are located in
// src/components/autofill/content/browser/payments/payments_client_unittest.cc.
class PaymentsClient {
 public:
  // The names of the fields used to send non-location elements as part of an
  // address. Used in the implementation and in tests which verify that these
  // values are set or not at appropriate times.
  static const char kRecipientName[];
  static const char kPhoneNumber[];

  // Details for card unmasking, such as the suggested method of authentication,
  // along with any information required to facilitate the authentication.
  struct UnmaskDetails {
    UnmaskDetails();
    ~UnmaskDetails();
    UnmaskDetails& operator=(const UnmaskDetails& other);

    // The type of authentication method suggested for card unmask.
    AutofillClient::UnmaskAuthMethod unmask_auth_method =
        AutofillClient::UnmaskAuthMethod::UNKNOWN;
    // Set to true if the user should be offered opt-in for FIDO Authentication.
    bool offer_fido_opt_in = false;
    // Public Key Credential Request Options required for authentication.
    // https://www.w3.org/TR/webauthn/#dictdef-publickeycredentialrequestoptions
    base::Optional<base::Value> fido_request_options = base::nullopt;
    // Set of credit cards ids that are eligible for FIDO Authentication.
    std::set<std::string> fido_eligible_card_ids;
  };

  // A collection of the information required to make a credit card unmask
  // request.
  struct UnmaskRequestDetails {
    UnmaskRequestDetails();
    UnmaskRequestDetails(const UnmaskRequestDetails& other);
    ~UnmaskRequestDetails();

    int64_t billing_customer_number = 0;
    AutofillClient::UnmaskCardReason reason;
    CreditCard card;
    std::string risk_data;
    CardUnmaskDelegate::UserProvidedUnmaskDetails user_response;
    base::Optional<base::Value> fido_assertion_info = base::nullopt;
  };

  // Information retrieved from an UnmaskRequest.
  struct UnmaskResponseDetails {
    UnmaskResponseDetails();
    UnmaskResponseDetails(const UnmaskResponseDetails& other);
    ~UnmaskResponseDetails();
    UnmaskResponseDetails& operator=(const UnmaskResponseDetails& other);

    UnmaskResponseDetails& with_real_pan(std::string r) {
      real_pan = r;
      return *this;
    }

    UnmaskResponseDetails& with_dcvv(std::string d) {
      dcvv = d;
      return *this;
    }

    std::string real_pan;
    std::string dcvv;
    // Challenge required for enrolling user into FIDO authentication for future
    // card unmasking.
    base::Optional<base::Value> fido_creation_options = base::nullopt;
    // Challenge required for authorizing user for FIDO authentication for
    // future card unmasking.
    base::Optional<base::Value> fido_request_options = base::nullopt;
    // An opaque token used to logically chain consecutive UnmaskCard and
    // OptChange calls together.
    std::string card_authorization_token = std::string();
  };

  // Information required to either opt-in or opt-out a user for FIDO
  // Authentication.
  struct OptChangeRequestDetails {
    OptChangeRequestDetails();
    OptChangeRequestDetails(const OptChangeRequestDetails& other);
    ~OptChangeRequestDetails();

    std::string app_locale;

    // The reason for making the request.
    enum Reason {
      // Unknown default.
      UNKNOWN_REASON = 0,
      // The user wants to enable FIDO authentication for card unmasking.
      ENABLE_FIDO_AUTH = 1,
      // The user wants to disable FIDO authentication for card unmasking.
      DISABLE_FIDO_AUTH = 2,
      // The user is authorizing a new card for future FIDO authentication
      // unmasking.
      ADD_CARD_FOR_FIDO_AUTH = 3,
    };

    // Reason for the request.
    Reason reason;
    // Signature required for enrolling user into FIDO authentication for future
    // card unmasking.
    base::Optional<base::Value> fido_authenticator_response = base::nullopt;
    // An opaque token used to logically chain consecutive UnmaskCard and
    // OptChange calls together.
    std::string card_authorization_token = std::string();
  };

  // Information retrieved from an OptChange request.
  struct OptChangeResponseDetails {
    OptChangeResponseDetails();
    OptChangeResponseDetails(const OptChangeResponseDetails& other);
    ~OptChangeResponseDetails();

    // Unset if response failed. True if user is opted-in for FIDO
    // authentication for card unmasking. False otherwise.
    base::Optional<bool> user_is_opted_in;
    // Challenge required for enrolling user into FIDO authentication for future
    // card unmasking.
    base::Optional<base::Value> fido_creation_options = base::nullopt;
    // Challenge required for authorizing user for FIDO authentication for
    // future card unmasking.
    base::Optional<base::Value> fido_request_options = base::nullopt;
  };

  // A collection of the information required to make a credit card upload
  // request.
  struct UploadRequestDetails {
    UploadRequestDetails();
    UploadRequestDetails(const UploadRequestDetails& other);
    ~UploadRequestDetails();

    int64_t billing_customer_number = 0;
    int detected_values;
    CreditCard card;
    base::string16 cvc;
    std::vector<AutofillProfile> profiles;
    base::string16 context_token;
    std::string risk_data;
    std::string app_locale;
    std::vector<const char*> active_experiments;
  };

  // A collection of the information required to make local credit cards
  // migration request.
  struct MigrationRequestDetails {
    MigrationRequestDetails();
    MigrationRequestDetails(const MigrationRequestDetails& other);
    ~MigrationRequestDetails();

    int64_t billing_customer_number = 0;
    base::string16 context_token;
    std::string risk_data;
    std::string app_locale;
  };

  // An enum set in the GetUploadDetailsRequest indicating the source of the
  // request when uploading a card to Google Payments. It should stay consistent
  // with the same enum in Google Payments server code.
  enum UploadCardSource {
    // Source unknown.
    UNKNOWN_UPLOAD_CARD_SOURCE,
    // Single card is being uploaded from the normal credit card offer-to-save
    // prompt during a checkout flow.
    UPSTREAM_CHECKOUT_FLOW,
    // Single card is being uploaded from chrome://settings/payments.
    UPSTREAM_SETTINGS_PAGE,
    // Single card is being uploaded after being scanned by OCR.
    UPSTREAM_CARD_OCR,
    // 1+ cards are being uploaded from a migration request that started during
    // a checkout flow.
    LOCAL_CARD_MIGRATION_CHECKOUT_FLOW,
    // 1+ cards are being uploaded from a migration request that was initiated
    // from chrome://settings/payments.
    LOCAL_CARD_MIGRATION_SETTINGS_PAGE,
  };

  // |url_loader_factory| is reference counted so it has no lifetime or
  // ownership requirements. |identity_manager| and |account_info_getter| must
  // all outlive |this|. Either delegate might be nullptr. |is_off_the_record|
  // denotes incognito mode.
  PaymentsClient(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* const identity_manager,
      AccountInfoGetter* const account_info_getter,
      bool is_off_the_record = false);

  virtual ~PaymentsClient();

  // Starts fetching the OAuth2 token in anticipation of future Payments
  // requests. Called as an optimization, but not strictly necessary. Should
  // *not* be called in advance of GetUploadDetails or UploadCard because
  // identifying information should not be sent until the user has explicitly
  // accepted an upload prompt.
  void Prepare();

  // The user has interacted with a credit card form and may attempt to unmask a
  // card. This request returns what method of authentication is suggested,
  // along with any information to facilitate the authentication.
  virtual void GetUnmaskDetails(
      base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                              PaymentsClient::UnmaskDetails&)> callback,
      const std::string& app_locale);

  // The user has attempted to unmask a card with the given cvc.
  virtual void UnmaskCard(
      const UnmaskRequestDetails& request_details,
      base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                              UnmaskResponseDetails&)> callback);

  // Opts-in or opts-out the user to use FIDO authentication for card unmasking
  // on this device.
  void OptChange(
      const OptChangeRequestDetails request_details,
      base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                              PaymentsClient::OptChangeResponseDetails&)>
          callback);

  // Determine if the user meets the Payments service's conditions for upload.
  // The service uses |addresses| (from which names and phone numbers are
  // removed) and |app_locale| to determine which legal message to display.
  // |detected_values| is a bitmask of CreditCardSaveManager::DetectedValue
  // values that relays what data is actually available for upload in order to
  // make more informed upload decisions. |callback| is the callback function
  // when get response from server. |billable_service_number| is used to set the
  // billable service number in the GetUploadDetails request. If the conditions
  // are met, the legal message will be returned via |callback|.
  // |active_experiments| is used by Payments server to track requests that were
  // triggered by enabled features. |upload_card_source| is used by Payments
  // server metrics to track the source of the request.
  virtual void GetUploadDetails(
      const std::vector<AutofillProfile>& addresses,
      const int detected_values,
      const std::vector<const char*>& active_experiments,
      const std::string& app_locale,
      base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                              const base::string16&,
                              std::unique_ptr<base::Value>,
                              std::vector<std::pair<int, int>>)> callback,
      const int billable_service_number,
      UploadCardSource upload_card_source =
          UploadCardSource::UNKNOWN_UPLOAD_CARD_SOURCE);

  // The user has indicated that they would like to upload a card with the given
  // cvc. This request will fail server-side if a successful call to
  // GetUploadDetails has not already been made.
  virtual void UploadCard(
      const UploadRequestDetails& details,
      base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                              const std::string&)> callback);

  // The user has indicated that they would like to migrate their local credit
  // cards. This request will fail server-side if a successful call to
  // GetUploadDetails has not already been made.
  virtual void MigrateCards(
      const MigrationRequestDetails& details,
      const std::vector<MigratableCreditCard>& migratable_credit_cards,
      MigrateCardsCallback callback);

  // Cancels and clears the current |request_|.
  void CancelRequest();

  // Exposed for testing.
  void set_url_loader_factory_for_testing(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  bool is_off_the_record() { return is_off_the_record_; }

 private:
  friend class PaymentsClientTest;

  // Initiates a Payments request using the state in |request|. If
  // |authenticate| is true, ensures that an OAuth token is avialble first.
  // Takes ownership of |request|.
  void IssueRequest(std::unique_ptr<PaymentsRequest> request,
                    bool authenticate);

  // Creates |resource_request_| to be used later in StartRequest().
  void InitializeResourceRequest();

  // Callback from |simple_url_loader_|.
  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body);
  void OnSimpleLoaderCompleteInternal(int response_code,
                                      const std::string& data);

  // Callback that handles a completed access token request.
  void AccessTokenFetchFinished(GoogleServiceAuthError error,
                                signin::AccessTokenInfo access_token_info);

  // Handles a completed access token request in the case of failure.
  void AccessTokenError(const GoogleServiceAuthError& error);

  // Initiates a new OAuth2 token request.
  void StartTokenFetch(bool invalidate_old);

  // Adds the token to |simple_url_loader_| and starts the request.
  void SetOAuth2TokenAndStartRequest();

  // Creates |simple_url_loader_| and calls it to start the request.
  void StartRequest();

  // The URL loader factory for the request.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Provided in constructor; not owned by PaymentsClient.
  signin::IdentityManager* const identity_manager_;

  // Provided in constructor; not owned by PaymentsClient.
  AccountInfoGetter* const account_info_getter_;

  // The current request.
  std::unique_ptr<PaymentsRequest> request_;

  // The resource request being used to issue the current request.
  std::unique_ptr<network::ResourceRequest> resource_request_;

  // The URL loader being used to issue the current request.
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  // The OAuth2 token fetcher for any account.
  std::unique_ptr<signin::AccessTokenFetcher> token_fetcher_;

  // The OAuth2 token, or empty if not fetched.
  std::string access_token_;

  // Denotes incognito mode.
  bool is_off_the_record_;

  // True if |request_| has already retried due to a 401 response from the
  // server.
  bool has_retried_authorization_;

  base::WeakPtrFactory<PaymentsClient> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PaymentsClient);
};

}  // namespace payments
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_CLIENT_H_
