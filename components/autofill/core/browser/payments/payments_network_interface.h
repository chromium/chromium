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
#include "components/autofill/core/browser/payments/client_behavior_constants.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_network_interface_base.h"
#include "components/autofill/core/browser/payments/payments_request_details.h"
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

namespace payments {

using GetCardUploadDetailsCallback = base::OnceCallback<void(
    PaymentsAutofillClient::PaymentsRpcResult result,
    const std::u16string& context_token,
    std::unique_ptr<base::Value::Dict> legal_message,
    std::vector<std::pair<int, int>> supported_card_bin_ranges)>;

// PaymentsNetworkInterface issues Payments RPCs and manages responses and failure
// conditions. Only one request may be active at a time. Initiating a new
// request will cancel a pending request.
// Tests are located in
// src/components/autofill/content/browser/payments/payments_network_interface_unittest.cc.
class PaymentsNetworkInterface : public PaymentsNetworkInterfaceBase {
 public:
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
      base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                              const UploadCardResponseDetails&)> callback);

  // Determine if the user meets the Payments service conditions for upload.
  // The service uses `app_locale` and `billing_customer_number` to determine
  // which legal message to display. `country_code` is the first two characters
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
                              const GetDetailsForEnrollmentResponseDetails&)>
          callback);

  // The user has chosen to change the virtual-card enrollment of a credit card.
  // Send the necessary information for the server to identify the credit card
  // for which virtual-card enrollment will be updated, as well as metadata so
  // that the server understands the context for the request.
  virtual void UpdateVirtualCardEnrollment(
      const UpdateVirtualCardEnrollmentRequestDetails& request_details,
      base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult)>
          callback);

  // Determine if the user meets the conditions to link a BNPL partner, such as
  // Affirm. The `request_details` contains `issuer_id`, `app_locale` and
  // `billing_customer_number` that are used to determine the appropriate legal
  // message to display.
  // The callback function is triggered when the server responds. This function
  // receives the result of the response. Both the context token and legal
  // message are always returned in the callback upon a successful response.
  virtual void GetDetailsForCreateBnplPaymentInstrument(
      const GetDetailsForCreateBnplPaymentInstrumentRequestDetails&
          request_details,
      base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult result,
                              std::string context_token,
                              LegalMessageLines legal_message)> callback);

  // The user has indicated that they would like to create a BNPL payment
  // instrument. This request will fail server side if a successful call to
  // `GetDetailsForCreateBnplPaymentInstrument` has not already been made.
  // `request_details` contains all necessary information to build a
  // `CreateBnplPaymentInstrumentRequest`. `callback` is the callback function
  // that is triggered when a response is received from the server.
  virtual void CreateBnplPaymentInstrument(
      const CreateBnplPaymentInstrumentRequestDetails& request_details,
      base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult result,
                              std::string instrument_id)> callback);

  // Get the BNPL VCN details.
  virtual void GetBnplPaymentInstrumentForFetchingVcn(
      GetBnplPaymentInstrumentForFetchingVcnRequestDetails request_details,
      base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                              const BnplFetchVcnResponseDetails&)> callback);

  // Get the BNPL redirect url details.
  virtual void GetBnplPaymentInstrumentForFetchingUrl(
      GetBnplPaymentInstrumentForFetchingUrlRequestDetails request_details,
      base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                              const BnplFetchUrlResponseDetails&)> callback);

 private:
  friend class PaymentsNetworkInterfaceTest;
};

}  // namespace payments
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_NETWORK_INTERFACE_H_
