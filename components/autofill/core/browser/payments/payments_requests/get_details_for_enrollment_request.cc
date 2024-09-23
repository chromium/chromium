// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/get_details_for_enrollment_request.h"

#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"

namespace autofill::payments {

namespace {

// The path that the request will be sent to.
const char kGetDetailsForEnrollmentRequestPath[] =
    "payments/apis/virtualcardservice/getdetailsforenroll";

// The billable service number for the request if the enrollment happens after
// a local card upload.
const int kUpstreamEnrollBillableServiceNumber =
    kUploadPaymentMethodBillableServiceNumber;

// The billable service number for the request if the enrollment happens after a
// server card retrieval or in the settings page.
const int kDownstreamEnrollBillableServiceNumber =
    kUnmaskPaymentMethodBillableServiceNumber;

}  // namespace

GetDetailsForEnrollmentRequest::GetDetailsForEnrollmentRequest(
    const PaymentsNetworkInterface::GetDetailsForEnrollmentRequestDetails&
        request_details,
    base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                            const PaymentsNetworkInterface::
                                GetDetailsForEnrollmentResponseDetails&)>
        callback)
    : request_details_(request_details), callback_(std::move(callback)) {}

GetDetailsForEnrollmentRequest::~GetDetailsForEnrollmentRequest() = default;

std::string GetDetailsForEnrollmentRequest::GetRequestUrlPath() {
  return kGetDetailsForEnrollmentRequestPath;
}

std::string GetDetailsForEnrollmentRequest::GetRequestContentType() {
  return "application/json";
}

std::string GetDetailsForEnrollmentRequest::GetRequestContent() {
  base::Value::Dict request_dict;

  base::Value::Dict context;
  context.Set("language_code", request_details_.app_locale);
  int billable_service_number = 0;
  switch (request_details_.source) {
    case VirtualCardEnrollmentSource::kUpstream:
      billable_service_number = kUpstreamEnrollBillableServiceNumber;
      break;
    case VirtualCardEnrollmentSource::kDownstream:
    case VirtualCardEnrollmentSource::kSettingsPage:
      billable_service_number = kDownstreamEnrollBillableServiceNumber;
      break;
    case VirtualCardEnrollmentSource::kNone:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  context.Set("billable_service", billable_service_number);
  if (request_details_.billing_customer_number != 0) {
    context.Set("customer_context",
                BuildCustomerContextDictionary(
                    request_details_.billing_customer_number));
  }
  request_dict.Set("context", std::move(context));

  request_dict.Set("instrument_id",
                   base::NumberToString(request_details_.instrument_id));

  if (!request_details_.risk_data.empty()) {
    request_dict.Set("risk_data_encoded",
                     BuildRiskDictionary(request_details_.risk_data));
  }

  switch (request_details_.source) {
    case VirtualCardEnrollmentSource::kUpstream:
      request_dict.Set("channel_type", "CHROME_UPSTREAM");
      break;
    case VirtualCardEnrollmentSource::kDownstream:
    case VirtualCardEnrollmentSource::kSettingsPage:
      request_dict.Set("channel_type", "CHROME_DOWNSTREAM");
      break;
    case VirtualCardEnrollmentSource::kNone:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  std::string request_content;
  base::JSONWriter::Write(request_dict, &request_content);
  VLOG(3) << "GetDetailsForEnrollmentRequest request body: " << request_content;
  return request_content;
}

void GetDetailsForEnrollmentRequest::ParseResponse(
    const base::Value::Dict& response) {
  const base::Value::Dict* google_legal_message =
      response.FindDict("google_legal_message");
  if (google_legal_message) {
    LegalMessageLine::Parse(*google_legal_message,
                            &response_details_.google_legal_message,
                            /*escape_apostrophes=*/true);
  }

  const base::Value::Dict* external_legal_message =
      response.FindDict("external_legal_message");
  if (external_legal_message) {
    LegalMessageLine::Parse(*external_legal_message,
                            &response_details_.issuer_legal_message,
                            /*escape_apostrophes=*/true);
  }

  const auto* context_token = response.FindString("context_token");
  response_details_.vcn_context_token =
      context_token ? *context_token : std::string();
}

bool GetDetailsForEnrollmentRequest::IsResponseComplete() {
  return !response_details_.vcn_context_token.empty() &&
         !response_details_.google_legal_message.empty();
}

void GetDetailsForEnrollmentRequest::RespondToDelegate(
    PaymentsAutofillClient::PaymentsRpcResult result) {
  std::move(callback_).Run(result, response_details_);
}

}  // namespace autofill::payments
