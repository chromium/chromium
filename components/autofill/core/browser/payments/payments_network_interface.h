// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_NETWORK_INTERFACE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_NETWORK_INTERFACE_H_

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#include "components/autofill/core/browser/payments/card_unmask_delegate.h"
#include "components/autofill/core/browser/payments/client_behavior_constants.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_network_interface_base.h"
#include "components/autofill/core/browser/payments/payments_window_manager.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_flow.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "url/origin.h"

namespace signin {
class IdentityManager;
}  // namespace signin

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace autofill {

class AccountInfoGetter;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
class MigratableCreditCard;
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

namespace payments {

using GetCardUploadDetailsCallback = base::OnceCallback<void(
    PaymentsAutofillClient::PaymentsRpcResult result,
    const std::u16string& context_token,
    std::unique_ptr<base::Value::Dict> legal_message,
    std::vector<std::pair<int, int>> supported_card_bin_ranges)>;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Callback type for MigrateCards callback. |result| is the Payments Rpc result.
// |save_result| is an unordered_map parsed from the response whose key is the
// unique id (guid) for each card and value is the server save result string.
// |display_text| is the returned tip from Payments to show on the UI.
typedef base::OnceCallback<void(
    PaymentsAutofillClient::PaymentsRpcResult result,
    std::unique_ptr<std::unordered_map<std::string, std::string>> save_result,
    const std::string& display_text)>
    MigrateCardsCallback;
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// Billable service number is defined in Payments server to distinguish
// different requests.
inline constexpr int kUnmaskPaymentMethodBillableServiceNumber = 70154;
inline constexpr int kUploadPaymentMethodBillableServiceNumber = 70073;
inline constexpr int kMigrateCardsBillableServiceNumber = 70264;

// PaymentsNetworkInterface issues Payments RPCs and manages responses and failure
// conditions. Only one request may be active at a time. Initiating a new
// request will cancel a pending request.
// Tests are located in
// src/components/autofill/content/browser/payments/payments_network_interface_unittest.cc.
class PaymentsNetworkInterface : public PaymentsNetworkInterfaceBase {
 public:
  // The names of the fields used to send non-location elements as part of an
  // address. Used in the implementation and in tests which verify that these
  // values are set or not at appropriate times.
  static constexpr char kRecipientName[] = "recipient_name";
  static constexpr char kPhoneNumber[] = "phone_number";

  // Details for card unmasking, such as the suggested method of authentication,
  // along with any information required to facilitate the authentication.
  struct UnmaskDetails {
    UnmaskDetails();
    UnmaskDetails(const UnmaskDetails&);
    UnmaskDetails(UnmaskDetails&&);
    UnmaskDetails& operator=(const UnmaskDetails&);
    UnmaskDetails& operator=(UnmaskDetails&&);
    ~UnmaskDetails();

    // The type of authentication method suggested for card unmask.
    PaymentsAutofillClient::UnmaskAuthMethod unmask_auth_method =
        PaymentsAutofillClient::UnmaskAuthMethod::kUnknown;
    // Set to true if the user should be offered opt-in for FIDO Authentication.
    bool offer_fido_opt_in = false;
    // Public Key Credential Request Options required for authentication.
    // https://www.w3.org/TR/webauthn/#dictdef-publickeycredentialrequestoptions
    base::Value::Dict fido_request_options;
    // Set of credit cards ids that are eligible for FIDO Authentication.
    std::set<std::string> fido_eligible_card_ids;
  };

  // A collection of the information required to make a credit card unmask
  // request.
  struct UnmaskRequestDetails {
    UnmaskRequestDetails();
    UnmaskRequestDetails(const UnmaskRequestDetails& other);
    UnmaskRequestDetails(UnmaskRequestDetails&&);
    UnmaskRequestDetails& operator=(const UnmaskRequestDetails& other);
    UnmaskRequestDetails& operator=(UnmaskRequestDetails&&);
    ~UnmaskRequestDetails();

    int64_t billing_customer_number = 0;
    CreditCard card;
    std::string risk_data;
    CardUnmaskDelegate::UserProvidedUnmaskDetails user_response;
    std::optional<base::Value::Dict> fido_assertion_info;
    std::u16string otp;
    // An opaque token used to chain consecutive payments requests together.
    std::string context_token;
    // The origin of the primary main frame where the unmasking happened.
    // Should be populated when the unmasking is for a virtual-card.
    // TODO(crbug.com/325465172): Convert this to an std::optional<url::Origin>.
    std::optional<GURL> last_committed_primary_main_frame_origin;
    // The selected challenge option. Should be populated when we are doing CVC
    // unmasking for a virtual card.
    std::optional<CardUnmaskChallengeOption> selected_challenge_option;
    // A vector of signals used to share client behavior with the Payments
    // server.
    std::vector<ClientBehaviorConstants> client_behavior_signals;
    // The token received in the final redirect of a PaymentsWindowManager flow,
    // which is the only scenario where this field should be populated.
    PaymentsWindowManager::RedirectCompletionResult redirect_completion_result;
  };

  // Information retrieved from an UnmaskRequest.
  struct UnmaskResponseDetails {
    UnmaskResponseDetails();
    UnmaskResponseDetails(const UnmaskResponseDetails& other);
    UnmaskResponseDetails(UnmaskResponseDetails&&);
    UnmaskResponseDetails& operator=(const UnmaskResponseDetails& other);
    UnmaskResponseDetails& operator=(UnmaskResponseDetails&&);
    ~UnmaskResponseDetails();

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
    // The expiration month of the card. It falls in between 1 - 12. Should be
    // populated when the card is a virtual-card which does not necessarily have
    // the same expiration date as its related actual card.
    std::string expiration_month;
    // The four-digit expiration year of the card. Should be populated when the
    // card is a virtual-card which does not necessarily have the same
    // expiration date as its related actual card.
    std::string expiration_year;
    // Challenge required for authorizing user for FIDO authentication for
    // future card unmasking.
    base::Value::Dict fido_request_options;
    // An opaque token used to logically chain consecutive UnmaskCard and
    // OptChange calls together.
    std::string card_authorization_token;
    // Available card unmask challenge options.
    std::vector<CardUnmaskChallengeOption> card_unmask_challenge_options;
    // An opaque token used to chain consecutive payments requests together.
    // Client should not update or modify this token.
    std::string context_token;
    // An intermediate status in cases other than immediate success or failure.
    std::string flow_status;

    // The type of the returned credit card.
    PaymentsAutofillClient::PaymentsRpcCardType card_type =
        PaymentsAutofillClient::PaymentsRpcCardType::kUnknown;

    // Context for the error dialog that is returned from the Payments server.
    // If present, that means this response was an error, and these fields
    // should be used for the autofill error dialog as they will provide detail
    // into the specific error that occurred.
    std::optional<AutofillErrorDialogContext> autofill_error_dialog_context;
  };

  // A collection of information required to make an unmask IBAN request.
  struct UnmaskIbanRequestDetails {
    UnmaskIbanRequestDetails();
    UnmaskIbanRequestDetails(const UnmaskIbanRequestDetails& other);
    ~UnmaskIbanRequestDetails();

    int billable_service_number = 0;
    int64_t billing_customer_number = 0;
    int64_t instrument_id;
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
    std::optional<base::Value::Dict> fido_authenticator_response;
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
    std::optional<bool> user_is_opted_in;
    // Challenge required for enrolling user into FIDO authentication for future
    // card unmasking.
    std::optional<base::Value::Dict> fido_creation_options;
    // Challenge required for authorizing user for FIDO authentication for
    // future card unmasking.
    std::optional<base::Value::Dict> fido_request_options;
  };

  // A collection of the information required to make local credit cards
  // migration request.
  struct MigrationRequestDetails {
    MigrationRequestDetails();
    MigrationRequestDetails(const MigrationRequestDetails& other);
    ~MigrationRequestDetails();

    int64_t billing_customer_number = 0;
    std::u16string context_token;
    std::string risk_data;
    std::string app_locale;
  };

  // A collection of the information required to make select challenge option
  // request.
  struct SelectChallengeOptionRequestDetails {
    SelectChallengeOptionRequestDetails();
    SelectChallengeOptionRequestDetails(
        const SelectChallengeOptionRequestDetails& other);
    ~SelectChallengeOptionRequestDetails();

    CardUnmaskChallengeOption selected_challenge_option;
    // An opaque token used to chain consecutive payments requests together.
    std::string context_token;
    int64_t billing_customer_number = 0;
  };

  // A collection of information needed for the
  // UpdateVirtualCardEnrollmentRequest.
  struct UpdateVirtualCardEnrollmentRequestDetails {
    UpdateVirtualCardEnrollmentRequestDetails();
    UpdateVirtualCardEnrollmentRequestDetails(
        const UpdateVirtualCardEnrollmentRequestDetails&);
    UpdateVirtualCardEnrollmentRequestDetails& operator=(
        const UpdateVirtualCardEnrollmentRequestDetails&);
    ~UpdateVirtualCardEnrollmentRequestDetails();
    // Denotes the source that the corresponding
    // UpdateVirtualCardEnrollmentRequest for this
    // UpdateVirtualCardEnrollmentRequestDetails originated from, i.e., a
    // |virtual_card_enrollment_source| of kUpstream means the request happens
    // after a user saved a card in the upstream flow.
    VirtualCardEnrollmentSource virtual_card_enrollment_source =
        VirtualCardEnrollmentSource::kNone;
    // Denotes the type of this specific UpdateVirtualCardEnrollmentRequest,
    // i.e., a type of VirtualCardEnrollmentRequestType::kEnroll would mean this
    // is an enroll request.
    VirtualCardEnrollmentRequestType virtual_card_enrollment_request_type =
        VirtualCardEnrollmentRequestType::kNone;
    // The billing customer number for the account this request is sent to. If
    // |billing_customer_number| is non-zero, it means the user has a Google
    // Payments account.
    int64_t billing_customer_number = 0;
    // Populated if it is an unenroll request. |instrument_id| lets the server
    // know which card to unenroll from VCN.
    std::optional<int64_t> instrument_id;
    // Populated if it is an enroll request. Based on the |vcn_context_token|
    // the server is able to retrieve the instrument id, and using
    // |vcn_context_token| for enroll allows the server to link a
    // GetDetailsForEnroll call with the corresponding Enroll call.
    std::optional<std::string> vcn_context_token;
  };

  // The struct to hold all detailed information to construct a
  // GetDetailsForEnrollmentRequest.
  struct GetDetailsForEnrollmentRequestDetails {
    GetDetailsForEnrollmentRequestDetails();
    GetDetailsForEnrollmentRequestDetails(
        const GetDetailsForEnrollmentRequestDetails& other);
    ~GetDetailsForEnrollmentRequestDetails();

    // The type of the enrollment this request is for.
    VirtualCardEnrollmentSource source = VirtualCardEnrollmentSource::kNone;

    // |instrument_id| is used by the server to identify a specific card to get
    // details for.
    int64_t instrument_id = 0;

    // The billing customer number of the account this request is sent to.
    int64_t billing_customer_number = 0;

    // |risk_data| contains some fingerprint data for the user and the device.
    std::string risk_data;

    // |app_locale| is the Chrome locale.
    std::string app_locale;
  };

  // A collection of information received in the response for a
  // GetDetailsForEnrollRequest.
  struct GetDetailsForEnrollmentResponseDetails {
    GetDetailsForEnrollmentResponseDetails();
    GetDetailsForEnrollmentResponseDetails(
        const GetDetailsForEnrollmentResponseDetails& other);
    ~GetDetailsForEnrollmentResponseDetails();
    // |vcn_context_token| is used in the sequential Enroll call, where it
    // allows the server to get the instrument id for this |vcn_context_token|
    // and link this specific GetDetailsForEnroll call with its corresponding
    // enroll call.
    std::string vcn_context_token;
    // Google's legal message lines in the virtual-card enroll flow for this
    // specific card based on |vcn_context_token|.
    LegalMessageLines google_legal_message;
    // The issuer's legal message lines in the virtual-card enroll flow for this
    // specific card based on |vcn_context_token|.
    LegalMessageLines issuer_legal_message;
  };

  // A collection of the information required to make a credit card upload
  // request.
  struct UploadCardRequestDetails {
    UploadCardRequestDetails();
    UploadCardRequestDetails(const UploadCardRequestDetails& other);
    ~UploadCardRequestDetails();

    int64_t billing_customer_number = 0;
    int detected_values;
    CreditCard card;
    std::u16string cvc;
    std::vector<AutofillProfile> profiles;
    std::u16string context_token;
    std::string risk_data;
    std::string app_locale;
    std::vector<ClientBehaviorConstants> client_behavior_signals;
  };

  // A collection of information required to make an IBAN upload request.
  struct UploadIbanRequestDetails {
    UploadIbanRequestDetails();
    UploadIbanRequestDetails(const UploadIbanRequestDetails& other);
    ~UploadIbanRequestDetails();

    std::string app_locale;
    int billable_service_number = 0;
    int64_t billing_customer_number = 0;
    std::u16string context_token;
    std::u16string value;
    std::u16string nickname;
  };

  // An enum set in the GetCardUploadDetailsRequest indicating the source of the
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

  // A collection of information received in the response for an
  // UploadCardRequest.
  struct UploadCardResponseDetails {
    UploadCardResponseDetails();
    UploadCardResponseDetails(const UploadCardResponseDetails&);
    UploadCardResponseDetails(UploadCardResponseDetails&&);
    UploadCardResponseDetails& operator=(const UploadCardResponseDetails&);
    UploadCardResponseDetails& operator=(UploadCardResponseDetails&&);
    ~UploadCardResponseDetails();
    // |instrument_id| is used by the server as an identifier for the card that
    // was uploaded. Currently, we have it in the UploadCardResponseDetails so
    // that we can send it in the GetDetailsForEnrollRequest in the virtual card
    // enrollment flow. Will only not be populated in the case of an imperfect
    // conversion from string to int64_t, or if the server does not return an
    // instrument id.
    std::optional<int64_t> instrument_id;
    // |virtual_card_enrollment_state| is used to determine whether we want to
    // pursue further action with the credit card that was uploaded regarding
    // virtual card enrollment. For example, if the state is
    // kUnenrolledAndEligible we might offer the user the option to enroll the
    // card that was uploaded into virtual card.
    CreditCard::VirtualCardEnrollmentState virtual_card_enrollment_state =
        CreditCard::VirtualCardEnrollmentState::kUnspecified;
    // |card_art_url| is the mapping that would be used by PersonalDataManager
    // to try to get the card art for the credit card that was uploaded. It is
    // used in flows where after uploading a card we want to display its card
    // art. Since chrome sync does not instantly sync the card art with the url,
    // the actual card art image might not always be present. Flows that use
    // |card_art_url| need to make sure they handle the case where the image has
    // not been synced yet. For virtual card eligible cards this should not be
    // empty. If using this field use DCHECKs to ensure it is populated.
    GURL card_art_url;
    // If the uploaded card is VCN eligible,
    // |get_details_for_enrollment_response_details| will be populated so that
    // we can display the virtual card enrollment bubble without needing to do
    // another GetDetailsForEnroll network call.
    std::optional<GetDetailsForEnrollmentResponseDetails>
        get_details_for_enrollment_response_details = std::nullopt;
  };

  // |url_loader_factory| is reference counted so it has no lifetime or
  // ownership requirements. |identity_manager| and |account_info_getter| must
  // all outlive |this|. Either delegate might be nullptr. |is_off_the_record|
  // denotes incognito mode.
  PaymentsNetworkInterface(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      AccountInfoGetter* account_info_getter,
      bool is_off_the_record = false);

  PaymentsNetworkInterface(const PaymentsNetworkInterface&) = delete;
  PaymentsNetworkInterface& operator=(const PaymentsNetworkInterface&) = delete;

  ~PaymentsNetworkInterface() override;

  // Starts fetching the OAuth2 token in anticipation of future Payments
  // requests. Called as an optimization, but not strictly necessary.
  void Prepare();

  // The user has interacted with a credit card form and may attempt to unmask a
  // card. This request returns what method of authentication is suggested,
  // along with any information to facilitate the authentication.
  virtual void GetUnmaskDetails(
      base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                              UnmaskDetails&)> callback,
      const std::string& app_locale);

  // The user has attempted to unmask a card with the given cvc.
  virtual void UnmaskCard(
      const UnmaskRequestDetails& request_details,
      base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                              const UnmaskResponseDetails&)> callback);

  // Triggers a request to the Payments server to unmask an IBAN. `callback` is
  // the callback function that is triggered when a response is received from
  // the server and the full IBAN value is returned via callback.
  virtual void UnmaskIban(
      const UnmaskIbanRequestDetails& request_details,
      base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                              const std::u16string&)> callback);

  // Opts-in or opts-out the user to use FIDO authentication for card unmasking
  // on this device.
  void OptChange(
      const OptChangeRequestDetails request_details,
      base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                              OptChangeResponseDetails&)> callback);

  // Determine if the user meets the Payments service's conditions for upload.
  // The service uses |addresses| (from which names and phone numbers are
  // removed) and |app_locale| and |billing_customer_number| to determine which
  // legal message to display. |detected_values| is a bitmask of
  // CreditCardSaveManager::DetectedValue values that relays what data is
  // actually available for upload in order to make more informed upload
  // decisions. |callback| is the callback function when get response from
  // server. |billable_service_number| is used to set the billable service
  // number in the GetCardUploadDetails request. If the conditions are met, the
  // legal message will be returned via |callback|. |client_behavior_signals| is
  // used by Payments server to track Chrome behaviors. |upload_card_source| is
  // used by Payments server metrics to track the source of the request.
  virtual void GetCardUploadDetails(
      const std::vector<AutofillProfile>& addresses,
      const int detected_values,
      const std::vector<ClientBehaviorConstants>& client_behavior_signals,
      const std::string& app_locale,
      GetCardUploadDetailsCallback callback,
      const int billable_service_number,
      const int64_t billing_customer_number,
      UploadCardSource upload_card_source =
          UploadCardSource::UNKNOWN_UPLOAD_CARD_SOURCE);

  // The user has indicated that they would like to upload a card with the given
  // cvc. This request will fail server-side if a successful call to
  // GetCardUploadDetails has not already been made.
  virtual void UploadCard(
      const UploadCardRequestDetails& details,
      base::OnceCallback<
          void(PaymentsAutofillClient::PaymentsRpcResult,
               const PaymentsNetworkInterface::UploadCardResponseDetails&)>
          callback);

  // Determine if the user meets the Payments service conditions for upload.
  // The service uses `app_locale` and `billing_customer_number` to determine
  // which legal message to display. `billable_service_number` is defined in
  // the Payments server to distinguish different requests and is set in the
  // GetIbanUploadDetails request. `country_code` is the first two characters
  // of the IBAN, representing its country of origin. `callback` is the
  // callback function that is triggered when a response is received from the
  // server, and the callback is triggered with that response's result. The
  // `validation_regex` is used to validate whether the given IBAN can be saved
  // to the server. The legal message will always be returned upon a successful
  // response via `callback`. A successful response does not guarantee that the
  // legal message is valid, callers should parse the legal message and use it
  // to decide if IBAN upload save should be offered.
  virtual void GetIbanUploadDetails(
      const std::string& app_locale,
      int64_t billing_customer_number,
      int billable_service_number,
      const std::string& country_code,
      base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult result,
                              const std::u16string& validation_regex,
                              const std::u16string& context_token,
                              std::unique_ptr<base::Value::Dict>)> callback);

  // The user has indicated that they would like to upload an IBAN. This request
  // will fail server-side if a successful call to GetIbanUploadDetails has not
  // already been made. `details` contains all necessary information to build
  // an `UploadIbanRequest`. `callback` is the callback function that is
  // triggered when a response is received from the server.
  virtual void UploadIban(
      const UploadIbanRequestDetails& details,
      base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult)>
          callback);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // The user has indicated that they would like to migrate their local credit
  // cards. This request will fail server-side if a successful call to
  // GetCardUploadDetails has not already been made.
  virtual void MigrateCards(
      const MigrationRequestDetails& details,
      const std::vector<MigratableCreditCard>& migratable_credit_cards,
      MigrateCardsCallback callback);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

  // The user has chosen one of the available challenge options. Send the
  // selected challenge option to server to continue the unmask flow.
  virtual void SelectChallengeOption(
      const SelectChallengeOptionRequestDetails& details,
      base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                              const std::string&)> callback);

  // Retrieve information necessary for the enrollment from the server. This is
  // invoked before we show the bubble to request user consent for the
  // enrollment.
  virtual void GetVirtualCardEnrollmentDetails(
      const GetDetailsForEnrollmentRequestDetails& request_details,
      base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                              const PaymentsNetworkInterface::
                                  GetDetailsForEnrollmentResponseDetails&)>
          callback);

  // The user has chosen to change the virtual-card enrollment of a credit card.
  // Send the necessary information for the server to identify the credit card
  // for which virtual-card enrollment will be updated, as well as metadata so
  // that the server understands the context for the request.
  virtual void UpdateVirtualCardEnrollment(
      const UpdateVirtualCardEnrollmentRequestDetails& request_details,
      base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult)>
          callback);

 private:
  friend class PaymentsNetworkInterfaceTest;
};

}  // namespace payments
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_NETWORK_INTERFACE_H_
