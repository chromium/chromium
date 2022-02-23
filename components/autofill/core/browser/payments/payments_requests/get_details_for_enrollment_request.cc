// Copyright 2022 The Chromium Authors. All rights reserved.
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
    kUploadCardBillableServiceNumber;

// The billable service number for the request if the enrollment happens after a
// server card retrieval or in the settings page.
const int kDownstreamEnrollBillableServiceNumber =
    kUnmaskCardBillableServiceNumber;

}  // namespace

GetDetailsForEnrollmentRequest::GetDetailsForEnrollmentRequest(
    const PaymentsClient::GetDetailsForEnrollmentRequestDetails&
        request_details,
    base::OnceCallback<
        void(AutofillClient::PaymentsRpcResult,
             const PaymentsClient::GetDetailsForEnrollmentResponseDetails&)>
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
  base::Value request_dict(base::Value::Type::DICTIONARY);

  base::Value context(base::Value::Type::DICTIONARY);
  context.SetKey("language_code", base::Value(request_details_.app_locale));
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
      NOTREACHED();
      break;
  }
  context.SetKey("billable_service", base::Value(billable_service_number));
  if (request_details_.billing_customer_number != 0) {
    context.SetKey("customer_context",
                   BuildCustomerContextDictionary(
                       request_details_.billing_customer_number));
  }
  request_dict.SetKey("context", std::move(context));

  request_dict.SetKey(
      "instrument_id",
      base::Value(base::NumberToString(request_details_.instrument_id)));

  if (!request_details_.risk_data.empty()) {
    request_dict.SetKey("risk_data_encoded",
                        BuildRiskDictionary(request_details_.risk_data));
  }

  std::string request_content;
  base::JSONWriter::Write(request_dict, &request_content);
  VLOG(3) << "GetDetailsForEnrollmentRequest request body: " << request_content;
  return request_content;
}

void GetDetailsForEnrollmentRequest::ParseResponse(
    const base::Value& response) {
  const base::Value* google_legal_message = response.FindKeyOfType(
      "google_legal_message", base::Value::Type::DICTIONARY);
  if (google_legal_message) {
    LegalMessageLine::Parse(*google_legal_message,
                            &response_details_.google_legal_message,
                            /*escape_apostrophes=*/true);
  }

  const base::Value* external_legal_message = response.FindKeyOfType(
      "external_legal_message", base::Value::Type::DICTIONARY);
  if (external_legal_message) {
    LegalMessageLine::Parse(*external_legal_message,
                            &response_details_.issuer_legal_message,
                            /*escape_apostrophes=*/true);
  }

  const auto* context_token = response.FindStringKey("context_token");
  response_details_.vcn_context_token =
      context_token ? *context_token : std::string();
}

bool GetDetailsForEnrollmentRequest::IsResponseComplete() {
  return !response_details_.vcn_context_token.empty() &&
         !response_details_.google_legal_message.empty();
}

void GetDetailsForEnrollmentRequest::RespondToDelegate(
    AutofillClient::PaymentsRpcResult result) {
  std::move(callback_).Run(result, response_details_);
}

}  // namespace autofill::payments
