// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUEST_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUEST_H_

#include <memory>

namespace autofill {

class AutofillClient;

namespace payments {

// Interface for the various Payments request types.
class PaymentsRequest {
 public:
  virtual ~PaymentsRequest() {}

  // Returns the URL path for this type of request.
  virtual std::string GetRequestUrlPath() = 0;

  // Returns the content type that should be used in the HTTP request.
  virtual std::string GetRequestContentType() = 0;

  // Returns the content that should be provided in the HTTP request.
  virtual std::string GetRequestContent() = 0;

  // Parses the required elements of the HTTP response.
  virtual void ParseResponse(const base::Value& response) = 0;

  // Returns true if all of the required elements were successfully retrieved by
  // a call to ParseResponse.
  virtual bool IsResponseComplete() = 0;

  // Invokes the appropriate callback in the delegate based on what type of
  // request this is.
  virtual void RespondToDelegate(AutofillClient::PaymentsRpcResult result) = 0;
};

}  // namespace payments
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUEST_H_
