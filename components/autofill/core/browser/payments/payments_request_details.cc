// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_request_details.h"

namespace autofill::payments {

UnmaskDetails::UnmaskDetails() = default;

UnmaskDetails::UnmaskDetails(const UnmaskDetails& other) {
  *this = other;
}

UnmaskDetails::UnmaskDetails(UnmaskDetails&&) = default;

UnmaskDetails& UnmaskDetails::operator=(const UnmaskDetails& other) {
  unmask_auth_method = other.unmask_auth_method;
  offer_fido_opt_in = other.offer_fido_opt_in;
  if (other.fido_request_options.empty()) {
    fido_request_options.clear();
  } else {
    fido_request_options = other.fido_request_options.Clone();
  }
  fido_eligible_card_ids = other.fido_eligible_card_ids;
  return *this;
}

UnmaskDetails& UnmaskDetails::operator=(UnmaskDetails&&) = default;

UnmaskDetails::~UnmaskDetails() = default;

UnmaskRequestDetails::UnmaskRequestDetails() = default;

UnmaskRequestDetails::UnmaskRequestDetails(const UnmaskRequestDetails& other) {
  *this = other;
}

UnmaskRequestDetails::UnmaskRequestDetails(UnmaskRequestDetails&&) = default;

UnmaskRequestDetails& UnmaskRequestDetails::operator=(
    const UnmaskRequestDetails& other) {
  billing_customer_number = other.billing_customer_number;
  card = other.card;
  risk_data = other.risk_data;
  user_response = other.user_response;
  if (other.fido_assertion_info.has_value()) {
    fido_assertion_info = other.fido_assertion_info->Clone();
  } else {
    fido_assertion_info.reset();
  }
  context_token = other.context_token;
  otp = other.otp;
  last_committed_primary_main_frame_origin =
      other.last_committed_primary_main_frame_origin;
  selected_challenge_option = other.selected_challenge_option;
  client_behavior_signals = other.client_behavior_signals;
  redirect_completion_result = other.redirect_completion_result;
  return *this;
}

UnmaskRequestDetails& UnmaskRequestDetails::operator=(UnmaskRequestDetails&&) =
    default;

UnmaskRequestDetails::~UnmaskRequestDetails() = default;

UnmaskResponseDetails::UnmaskResponseDetails() = default;

UnmaskResponseDetails::UnmaskResponseDetails(
    const UnmaskResponseDetails& other) {
  *this = other;
}

UnmaskResponseDetails::UnmaskResponseDetails(UnmaskResponseDetails&&) = default;

UnmaskResponseDetails& UnmaskResponseDetails::operator=(
    const UnmaskResponseDetails& other) {
  real_pan = other.real_pan;
  dcvv = other.dcvv;
  expiration_month = other.expiration_month;
  expiration_year = other.expiration_year;
  if (other.fido_request_options.empty()) {
    fido_request_options.clear();
  } else {
    fido_request_options = other.fido_request_options.Clone();
  }
  card_authorization_token = other.card_authorization_token;
  card_unmask_challenge_options = other.card_unmask_challenge_options;
  context_token = other.context_token;
  flow_status = other.flow_status;
  card_type = other.card_type;
  autofill_error_dialog_context = other.autofill_error_dialog_context;
  return *this;
}

UnmaskResponseDetails& UnmaskResponseDetails::operator=(
    UnmaskResponseDetails&&) = default;

UnmaskResponseDetails::~UnmaskResponseDetails() = default;

UnmaskIbanRequestDetails::UnmaskIbanRequestDetails() = default;
UnmaskIbanRequestDetails::UnmaskIbanRequestDetails(
    const UnmaskIbanRequestDetails& other) = default;
UnmaskIbanRequestDetails::~UnmaskIbanRequestDetails() = default;

OptChangeRequestDetails::OptChangeRequestDetails() = default;
OptChangeRequestDetails::OptChangeRequestDetails(
    const OptChangeRequestDetails& other) {
  app_locale = other.app_locale;
  reason = other.reason;
  if (other.fido_authenticator_response.has_value()) {
    fido_authenticator_response = other.fido_authenticator_response->Clone();
  } else {
    fido_authenticator_response.reset();
  }
  card_authorization_token = other.card_authorization_token;
}
OptChangeRequestDetails::~OptChangeRequestDetails() = default;

OptChangeResponseDetails::OptChangeResponseDetails() = default;
OptChangeResponseDetails::OptChangeResponseDetails(
    const OptChangeResponseDetails& other) {
  user_is_opted_in = other.user_is_opted_in;

  if (other.fido_creation_options.has_value()) {
    fido_creation_options = other.fido_creation_options->Clone();
  } else {
    fido_creation_options.reset();
  }
  if (other.fido_request_options.has_value()) {
    fido_request_options = other.fido_request_options->Clone();
  } else {
    fido_request_options.reset();
  }
}
OptChangeResponseDetails::~OptChangeResponseDetails() = default;

UploadCardRequestDetails::UploadCardRequestDetails() = default;
UploadCardRequestDetails::UploadCardRequestDetails(
    const UploadCardRequestDetails& other) = default;
UploadCardRequestDetails::~UploadCardRequestDetails() = default;

UploadIbanRequestDetails::UploadIbanRequestDetails() = default;
UploadIbanRequestDetails::UploadIbanRequestDetails(
    const UploadIbanRequestDetails& other) = default;
UploadIbanRequestDetails::~UploadIbanRequestDetails() = default;

MigrationRequestDetails::MigrationRequestDetails() = default;
MigrationRequestDetails::MigrationRequestDetails(
    const MigrationRequestDetails& other) = default;
MigrationRequestDetails::~MigrationRequestDetails() = default;

SelectChallengeOptionRequestDetails::SelectChallengeOptionRequestDetails() =
    default;
SelectChallengeOptionRequestDetails::SelectChallengeOptionRequestDetails(
    const SelectChallengeOptionRequestDetails& other) = default;
SelectChallengeOptionRequestDetails::~SelectChallengeOptionRequestDetails() =
    default;

GetDetailsForEnrollmentRequestDetails::GetDetailsForEnrollmentRequestDetails() =
    default;
GetDetailsForEnrollmentRequestDetails::GetDetailsForEnrollmentRequestDetails(
    const GetDetailsForEnrollmentRequestDetails& other) = default;
GetDetailsForEnrollmentRequestDetails::
    ~GetDetailsForEnrollmentRequestDetails() = default;

GetDetailsForEnrollmentResponseDetails::
    GetDetailsForEnrollmentResponseDetails() = default;
GetDetailsForEnrollmentResponseDetails::GetDetailsForEnrollmentResponseDetails(
    const GetDetailsForEnrollmentResponseDetails& other) = default;
GetDetailsForEnrollmentResponseDetails::
    ~GetDetailsForEnrollmentResponseDetails() = default;

UploadCardResponseDetails::UploadCardResponseDetails() = default;
UploadCardResponseDetails::UploadCardResponseDetails(
    const UploadCardResponseDetails&) = default;
UploadCardResponseDetails::UploadCardResponseDetails(
    UploadCardResponseDetails&&) = default;
UploadCardResponseDetails& UploadCardResponseDetails::operator=(
    const UploadCardResponseDetails&) = default;
UploadCardResponseDetails& UploadCardResponseDetails::operator=(
    UploadCardResponseDetails&&) = default;
UploadCardResponseDetails::~UploadCardResponseDetails() = default;

UpdateVirtualCardEnrollmentRequestDetails::
    UpdateVirtualCardEnrollmentRequestDetails() = default;
UpdateVirtualCardEnrollmentRequestDetails::
    UpdateVirtualCardEnrollmentRequestDetails(
        const UpdateVirtualCardEnrollmentRequestDetails&) = default;
UpdateVirtualCardEnrollmentRequestDetails&
UpdateVirtualCardEnrollmentRequestDetails::operator=(
    const UpdateVirtualCardEnrollmentRequestDetails&) = default;
UpdateVirtualCardEnrollmentRequestDetails::
    ~UpdateVirtualCardEnrollmentRequestDetails() = default;

}  // namespace autofill::payments
