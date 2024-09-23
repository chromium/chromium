// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_PAYMENTS_REQUEST_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_PAYMENTS_REQUEST_H_

#include <string>

#include "base/values.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/client_behavior_constants.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/common/autofill_payments_features.h"

namespace autofill::payments {

// Shared class for the various Payments request types.
class PaymentsRequest {
 public:
  PaymentsRequest();
  virtual ~PaymentsRequest();

  // Returns the URL path for this type of request.
  virtual std::string GetRequestUrlPath() = 0;

  // Returns the content type that should be used in the HTTP request.
  virtual std::string GetRequestContentType() = 0;

  // Returns the content that should be provided in the HTTP request.
  virtual std::string GetRequestContent() = 0;

  // Parses the required elements of the HTTP response.
  virtual void ParseResponse(const base::Value::Dict& response) = 0;

  // Returns true if all of the required elements were successfully retrieved by
  // a call to ParseResponse.
  virtual bool IsResponseComplete() = 0;

  // Invokes the appropriate callback in the delegate based on what type of
  // request this is.
  virtual void RespondToDelegate(
      PaymentsAutofillClient::PaymentsRpcResult result) = 0;

  // Returns true if the response indicates that we received an error that is
  // retryable, false otherwise. If this returns true, the PaymentsRpcResult for
  // the request will be kTryAgainFailure.
  virtual bool IsRetryableFailure(const std::string& error_code);

  // Returns the name of the request that should be recorded in histograms. The
  // default implementation is an empty string, and the PaymentsRequest
  // constructor will check that if GetTimeout returns a non-null value then
  // GetHistogramName must return a non-empty string.
  //
  // If a subclass overrides this, it must add the histogram name to the
  // Autofill.PaymentsRequestType variants in histograms.xml.
  virtual std::string GetHistogramName() const;

  // Returns the client-side timeout that should be set on the network request,
  // if any. If a Payments API call reaches this timeout, it will be closed from
  // the client side. Note that this may not stop the underlying API call from
  // succeeding - it may still complete on the server side.
  //
  // The default implementation has no client-side timeout.
  //
  // If a subclass overrides this, it must also override GetHistogramName.
  virtual std::optional<base::TimeDelta> GetTimeout() const;

 protected:
  // Shared helper function to build the risk data sent in the request.
  base::Value::Dict BuildRiskDictionary(const std::string& encoded_risk_data);

  // Shared helper function to build the customer context sent in the request.
  base::Value::Dict BuildCustomerContextDictionary(
      int64_t external_customer_id);

  // Shared helper function that builds the Chrome user context which is then
  // set in the payment requests.
  base::Value::Dict BuildChromeUserContext(
      const std::vector<ClientBehaviorConstants>& client_behavior_signals,
      bool full_sync_enabled);

  // Shared helper functoin that returns a dictionary with the structure
  // expected by Payments RPCs, containing each of the fields in |profile|,
  // formatted according to |app_locale|. If |include_non_location_data| is
  // false, the name and phone number in |profile| are not included.
  base::Value::Dict BuildAddressDictionary(const AutofillProfile& profile,
                                           const std::string& app_locale,
                                           bool include_non_location_data);

  // Shared helper function that returns a dictionary of the credit card with
  // the structure expected by Payments RPCs, containing expiration month,
  // expiration year and cardholder name (if any) fields in |credit_card|,
  // formatted according to |app_locale|. |pan_field_name| is the field name for
  // the encrypted pan. We use each credit card's guid as the unique id.
  base::Value::Dict BuildCreditCardDictionary(
      const CreditCard& credit_card,
      const std::string& app_locale,
      const std::string& pan_field_name);

  // Shared helper functions for string operations.
  static void AppendStringIfNotEmpty(const AutofillProfile& profile,
                                     const FieldType& type,
                                     const std::string& app_locale,
                                     base::Value::List& list);
  static void SetStringIfNotEmpty(const AutofillDataModel& profile,
                                  const FieldType& type,
                                  const std::string& app_locale,
                                  const std::string& path,
                                  base::Value::Dict& dictionary);
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_PAYMENTS_REQUEST_H_
