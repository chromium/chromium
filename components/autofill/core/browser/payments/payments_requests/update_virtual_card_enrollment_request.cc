// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/update_virtual_card_enrollment_request.h"

#include <string>

#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_flow.h"

namespace autofill {
namespace payments {

namespace {
const char kEnrollRequestPath[] = "payments/apis/virtualcardservice/enroll";
const char kUnenrollRequestPath[] = "payments/apis/virtualcardservice/unenroll";
}  // namespace

UpdateVirtualCardEnrollmentRequest::UpdateVirtualCardEnrollmentRequest(
    const PaymentsNetworkInterface::UpdateVirtualCardEnrollmentRequestDetails&
        request_details,
    base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult)>
        callback)
    : request_details_(request_details), callback_(std::move(callback)) {}

UpdateVirtualCardEnrollmentRequest::~UpdateVirtualCardEnrollmentRequest() =
    default;

std::string UpdateVirtualCardEnrollmentRequest::GetRequestUrlPath() {
  return request_details_.virtual_card_enrollment_request_type ==
                 VirtualCardEnrollmentRequestType::kEnroll
             ? kEnrollRequestPath
             : kUnenrollRequestPath;
}

std::string UpdateVirtualCardEnrollmentRequest::GetRequestContentType() {
  return "application/json";
}

std::string UpdateVirtualCardEnrollmentRequest::GetRequestContent() {
  base::Value::Dict request_dict;

  switch (request_details_.virtual_card_enrollment_request_type) {
    case VirtualCardEnrollmentRequestType::kEnroll:
      BuildEnrollRequestDictionary(&request_dict);
      break;
    case VirtualCardEnrollmentRequestType::kUnenroll:
      BuildUnenrollRequestDictionary(&request_dict);
      break;
    case VirtualCardEnrollmentRequestType::kNone:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  std::string request_content;
  base::JSONWriter::Write(request_dict, &request_content);
  VLOG(3) << "UpdateVirtualCardEnrollmentRequest Body: " << request_content;
  return request_content;
}

void UpdateVirtualCardEnrollmentRequest::ParseResponse(
    const base::Value::Dict& response) {
  // Only enroll requests have a response to parse, unenroll request responses
  // are empty except for possible errors which are parsed in
  // PaymentsNetworkInterface.
  if (request_details_.virtual_card_enrollment_request_type ==
      VirtualCardEnrollmentRequestType::kEnroll) {
    auto* enroll_result = response.FindString("enroll_result");
    if (enroll_result) {
      enroll_result_ = *enroll_result;
    }
  }
}

bool UpdateVirtualCardEnrollmentRequest::IsResponseComplete() {
  switch (request_details_.virtual_card_enrollment_request_type) {
    case VirtualCardEnrollmentRequestType::kEnroll:
      // If it is an enroll request, we know the response is complete if the
      // response has an enroll result that is ENROLL_SUCCESS, as that is the
      // only field in an enroll response other than the possible error.
      return enroll_result_.has_value() && enroll_result_ == "ENROLL_SUCCESS";
    case VirtualCardEnrollmentRequestType::kUnenroll:
      // Unenroll responses are empty except for having an error. In
      // PaymentsNetworkInterface, if the response has an error it will be
      // handled before we check IsResponseComplete(), so if we ever reach this
      // branch we know the response completed successfully as there is no
      // error. Thus, we always return true.
      return true;
    case VirtualCardEnrollmentRequestType::kNone:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

void UpdateVirtualCardEnrollmentRequest::RespondToDelegate(
    PaymentsAutofillClient::PaymentsRpcResult result) {
  std::move(callback_).Run(result);
}

std::string UpdateVirtualCardEnrollmentRequest::GetHistogramName() const {
  switch (request_details_.virtual_card_enrollment_request_type) {
    case VirtualCardEnrollmentRequestType::kEnroll:
      return "UpdateVirtualCardEnrollment_Enroll";
    default:
      NOTREACHED();
  }
}

std::optional<base::TimeDelta> UpdateVirtualCardEnrollmentRequest::GetTimeout()
    const {
  if (request_details_.virtual_card_enrollment_request_type !=
      VirtualCardEnrollmentRequestType::kEnroll) {
    return std::nullopt;
  }

  if (!base::FeatureList::IsEnabled(
          features::kAutofillVcnEnrollRequestTimeout)) {
    return std::nullopt;
  }

  return base::Milliseconds(
      features::kAutofillVcnEnrollRequestTimeoutMilliseconds.Get());
}

void UpdateVirtualCardEnrollmentRequest::BuildEnrollRequestDictionary(
    base::Value::Dict* request_dict) {
  DCHECK(request_details_.virtual_card_enrollment_request_type ==
         VirtualCardEnrollmentRequestType::kEnroll);

  // If it is an enroll request, we should always have a context token from the
  // previous GetDetailsForEnroll request and an instrument id.
  DCHECK(request_details_.vcn_context_token.has_value() &&
         request_details_.instrument_id.has_value());

  // Builds the context and channel_type for this enroll request.
  base::Value::Dict context;
  switch (request_details_.virtual_card_enrollment_source) {
    case VirtualCardEnrollmentSource::kUpstream:
      context.Set("billable_service",
                  kUploadPaymentMethodBillableServiceNumber);
      request_dict->Set("channel_type", "CHROME_UPSTREAM");
      break;
    case VirtualCardEnrollmentSource::kDownstream:
      // Downstream enroll is treated the same as settings page enroll because
      // chrome client should already have a card synced from the server.
      // Fall-through.
    case VirtualCardEnrollmentSource::kSettingsPage:
      context.Set("billable_service",
                  kUnmaskPaymentMethodBillableServiceNumber);
      request_dict->Set("channel_type", "CHROME_DOWNSTREAM");
      break;
    case VirtualCardEnrollmentSource::kNone:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  if (request_details_.billing_customer_number != 0) {
    context.Set("customer_context",
                BuildCustomerContextDictionary(
                    request_details_.billing_customer_number));
  }
  request_dict->Set("context", std::move(context));

  // Sets the virtual_card_enrollment_flow field in this enroll request which
  // lets the server know whether the enrollment is happening with ToS or not.
  // Chrome client requests will always be ENROLL_WITH_TOS. This field is
  // necessary because virtual card enroll through other platforms enrolls
  // without ToS, for example Web Push Provisioning.
  request_dict->Set("virtual_card_enrollment_flow", "ENROLL_WITH_TOS");

  // Sets the instrument_id field in this enroll request.
  request_dict->Set(
      "instrument_id",
      base::NumberToString(request_details_.instrument_id.value()));

  // Sets the context_token field in this enroll request which is used by the
  // server to link this enroll request to the previous
  // GetDetailsForEnrollRequest, as well as to retrieve the specific credit card
  // to enroll.
  request_dict->Set("context_token",
                    request_details_.vcn_context_token.value());
}

void UpdateVirtualCardEnrollmentRequest::BuildUnenrollRequestDictionary(
    base::Value::Dict* request_dict) {
  DCHECK(request_details_.virtual_card_enrollment_request_type ==
         VirtualCardEnrollmentRequestType::kUnenroll);

  // If it is an unenroll request, we should always have an instrument id and we
  // should not have a context token set in |request_details_|.
  DCHECK(request_details_.instrument_id.has_value() &&
         !request_details_.vcn_context_token.has_value());

  // Builds the context for this unenroll request with the billable service
  // number and the billing customer number if present.
  base::Value::Dict context;
  if (request_details_.billing_customer_number != 0) {
    context.Set("customer_context",
                BuildCustomerContextDictionary(
                    request_details_.billing_customer_number));
  }
  context.Set("billable_service", kUnmaskPaymentMethodBillableServiceNumber);
  request_dict->Set("context", std::move(context));

  // Sets the instrument_id field in this unenroll request which is used by
  // the server to get the appropriate credit card to unenroll.
  request_dict->Set(
      "instrument_id",
      base::NumberToString(request_details_.instrument_id.value()));
}

}  // namespace payments
}  // namespace autofill
