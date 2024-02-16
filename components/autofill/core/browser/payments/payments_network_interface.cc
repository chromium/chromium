// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_network_interface.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_data_model.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/account_info_getter.h"
#include "components/autofill/core/browser/payments/client_behavior_constants.h"
#include "components/autofill/core/browser/payments/payments_requests/get_card_upload_details_request.h"
#include "components/autofill/core/browser/payments/payments_requests/get_details_for_enrollment_request.h"
#include "components/autofill/core/browser/payments/payments_requests/get_iban_upload_details_request.h"
#include "components/autofill/core/browser/payments/payments_requests/get_unmask_details_request.h"
#include "components/autofill/core/browser/payments/payments_requests/opt_change_request.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"
#include "components/autofill/core/browser/payments/payments_requests/select_challenge_option_request.h"
#include "components/autofill/core/browser/payments/payments_requests/unmask_card_request.h"
#include "components/autofill/core/browser/payments/payments_requests/unmask_iban_request.h"
#include "components/autofill/core/browser/payments/payments_requests/update_virtual_card_enrollment_request.h"
#include "components/autofill/core/browser/payments/payments_requests/upload_card_request.h"
#include "components/autofill/core/browser/payments/payments_requests/upload_iban_request.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#include "components/autofill/core/browser/payments/local_card_migration_manager.h"
#include "components/autofill/core/browser/payments/payments_requests/migrate_cards_request.h"
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

namespace autofill::payments {

PaymentsNetworkInterface::UnmaskDetails::UnmaskDetails() = default;

PaymentsNetworkInterface::UnmaskDetails::UnmaskDetails(const UnmaskDetails& other) {
  *this = other;
}

PaymentsNetworkInterface::UnmaskDetails::UnmaskDetails(UnmaskDetails&&) = default;

PaymentsNetworkInterface::UnmaskDetails& PaymentsNetworkInterface::UnmaskDetails::operator=(
    const PaymentsNetworkInterface::UnmaskDetails& other) {
  unmask_auth_method = other.unmask_auth_method;
  offer_fido_opt_in = other.offer_fido_opt_in;
  if (other.fido_request_options.has_value()) {
    fido_request_options = other.fido_request_options->Clone();
  } else {
    fido_request_options.reset();
  }
  fido_eligible_card_ids = other.fido_eligible_card_ids;
  return *this;
}

PaymentsNetworkInterface::UnmaskDetails& PaymentsNetworkInterface::UnmaskDetails::operator=(
    UnmaskDetails&&) = default;

PaymentsNetworkInterface::UnmaskDetails::~UnmaskDetails() = default;

PaymentsNetworkInterface::UnmaskRequestDetails::UnmaskRequestDetails() = default;

PaymentsNetworkInterface::UnmaskRequestDetails::UnmaskRequestDetails(
    const UnmaskRequestDetails& other) {
  *this = other;
}

PaymentsNetworkInterface::UnmaskRequestDetails::UnmaskRequestDetails(
    UnmaskRequestDetails&&) = default;

PaymentsNetworkInterface::UnmaskRequestDetails&
PaymentsNetworkInterface::UnmaskRequestDetails::operator=(
    const PaymentsNetworkInterface::UnmaskRequestDetails& other) {
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
  merchant_domain_for_footprints = other.merchant_domain_for_footprints;
  selected_challenge_option = other.selected_challenge_option;
  client_behavior_signals = other.client_behavior_signals;
  redirect_completion_proof = other.redirect_completion_proof;
  return *this;
}

PaymentsNetworkInterface::UnmaskRequestDetails&
PaymentsNetworkInterface::UnmaskRequestDetails::operator=(
    UnmaskRequestDetails&&) = default;

PaymentsNetworkInterface::UnmaskRequestDetails::~UnmaskRequestDetails() = default;

PaymentsNetworkInterface::UnmaskResponseDetails::UnmaskResponseDetails() = default;

PaymentsNetworkInterface::UnmaskResponseDetails::UnmaskResponseDetails(
    const UnmaskResponseDetails& other) {
  *this = other;
}

PaymentsNetworkInterface::UnmaskResponseDetails::UnmaskResponseDetails(
    UnmaskResponseDetails&&) = default;

PaymentsNetworkInterface::UnmaskResponseDetails&
PaymentsNetworkInterface::UnmaskResponseDetails::operator=(
    const UnmaskResponseDetails& other) {
  real_pan = other.real_pan;
  dcvv = other.dcvv;
  expiration_month = other.expiration_month;
  expiration_year = other.expiration_year;
  if (other.fido_request_options.has_value()) {
    fido_request_options = other.fido_request_options->Clone();
  } else {
    fido_request_options.reset();
  }
  card_authorization_token = other.card_authorization_token;
  card_unmask_challenge_options = other.card_unmask_challenge_options;
  context_token = other.context_token;
  flow_status = other.flow_status;
  card_type = other.card_type;
  autofill_error_dialog_context = other.autofill_error_dialog_context;
  return *this;
}

PaymentsNetworkInterface::UnmaskResponseDetails&
PaymentsNetworkInterface::UnmaskResponseDetails::operator=(UnmaskResponseDetails&&) =
    default;

PaymentsNetworkInterface::UnmaskResponseDetails::~UnmaskResponseDetails() = default;

PaymentsNetworkInterface::UnmaskIbanRequestDetails::UnmaskIbanRequestDetails() = default;
PaymentsNetworkInterface::UnmaskIbanRequestDetails::UnmaskIbanRequestDetails(
    const UnmaskIbanRequestDetails& other) = default;
PaymentsNetworkInterface::UnmaskIbanRequestDetails::~UnmaskIbanRequestDetails() = default;

PaymentsNetworkInterface::OptChangeRequestDetails::OptChangeRequestDetails() = default;
PaymentsNetworkInterface::OptChangeRequestDetails::OptChangeRequestDetails(
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
PaymentsNetworkInterface::OptChangeRequestDetails::~OptChangeRequestDetails() = default;

PaymentsNetworkInterface::OptChangeResponseDetails::OptChangeResponseDetails() = default;
PaymentsNetworkInterface::OptChangeResponseDetails::OptChangeResponseDetails(
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
PaymentsNetworkInterface::OptChangeResponseDetails::~OptChangeResponseDetails() = default;

PaymentsNetworkInterface::UploadCardRequestDetails::UploadCardRequestDetails() =
    default;
PaymentsNetworkInterface::UploadCardRequestDetails::UploadCardRequestDetails(
    const UploadCardRequestDetails& other) = default;
PaymentsNetworkInterface::UploadCardRequestDetails::
    ~UploadCardRequestDetails() = default;

PaymentsNetworkInterface::UploadIbanRequestDetails::UploadIbanRequestDetails() = default;
PaymentsNetworkInterface::UploadIbanRequestDetails::UploadIbanRequestDetails(
    const UploadIbanRequestDetails& other) = default;
PaymentsNetworkInterface::UploadIbanRequestDetails::~UploadIbanRequestDetails() = default;

PaymentsNetworkInterface::MigrationRequestDetails::MigrationRequestDetails() = default;
PaymentsNetworkInterface::MigrationRequestDetails::MigrationRequestDetails(
    const MigrationRequestDetails& other) = default;
PaymentsNetworkInterface::MigrationRequestDetails::~MigrationRequestDetails() = default;

PaymentsNetworkInterface::SelectChallengeOptionRequestDetails::
    SelectChallengeOptionRequestDetails() = default;
PaymentsNetworkInterface::SelectChallengeOptionRequestDetails::
    SelectChallengeOptionRequestDetails(
        const SelectChallengeOptionRequestDetails& other) = default;
PaymentsNetworkInterface::SelectChallengeOptionRequestDetails::
    ~SelectChallengeOptionRequestDetails() = default;

PaymentsNetworkInterface::GetDetailsForEnrollmentRequestDetails::
    GetDetailsForEnrollmentRequestDetails() = default;
PaymentsNetworkInterface::GetDetailsForEnrollmentRequestDetails::
    GetDetailsForEnrollmentRequestDetails(
        const GetDetailsForEnrollmentRequestDetails& other) = default;
PaymentsNetworkInterface::GetDetailsForEnrollmentRequestDetails::
    ~GetDetailsForEnrollmentRequestDetails() = default;

PaymentsNetworkInterface::GetDetailsForEnrollmentResponseDetails::
    GetDetailsForEnrollmentResponseDetails() = default;
PaymentsNetworkInterface::GetDetailsForEnrollmentResponseDetails::
    GetDetailsForEnrollmentResponseDetails(
        const GetDetailsForEnrollmentResponseDetails& other) = default;
PaymentsNetworkInterface::GetDetailsForEnrollmentResponseDetails::
    ~GetDetailsForEnrollmentResponseDetails() = default;

PaymentsNetworkInterface::UploadCardResponseDetails::UploadCardResponseDetails() =
    default;
PaymentsNetworkInterface::UploadCardResponseDetails::~UploadCardResponseDetails() =
    default;

PaymentsNetworkInterface::UpdateVirtualCardEnrollmentRequestDetails::
    UpdateVirtualCardEnrollmentRequestDetails() = default;
PaymentsNetworkInterface::UpdateVirtualCardEnrollmentRequestDetails::
    UpdateVirtualCardEnrollmentRequestDetails(
        const UpdateVirtualCardEnrollmentRequestDetails&) = default;
PaymentsNetworkInterface::UpdateVirtualCardEnrollmentRequestDetails&
PaymentsNetworkInterface::UpdateVirtualCardEnrollmentRequestDetails::operator=(
    const UpdateVirtualCardEnrollmentRequestDetails&) = default;
PaymentsNetworkInterface::UpdateVirtualCardEnrollmentRequestDetails::
    ~UpdateVirtualCardEnrollmentRequestDetails() = default;

PaymentsNetworkInterface::PaymentsNetworkInterface(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    AccountInfoGetter* account_info_getter,
    bool is_off_the_record)
    : PaymentsNetworkInterfaceBase(url_loader_factory,
                                   identity_manager,
                                   account_info_getter,
                                   is_off_the_record) {}

PaymentsNetworkInterface::~PaymentsNetworkInterface() = default;

void PaymentsNetworkInterface::Prepare() {
  if (access_token_.empty())
    StartTokenFetch(false);
}

void PaymentsNetworkInterface::GetUnmaskDetails(
    base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                            PaymentsNetworkInterface::UnmaskDetails&)> callback,
    const std::string& app_locale) {
  IssueRequest(std::make_unique<GetUnmaskDetailsRequest>(
      std::move(callback), app_locale,
      account_info_getter_->IsSyncFeatureEnabledForPaymentsServerMetrics()));
}

void PaymentsNetworkInterface::UnmaskCard(
    const PaymentsNetworkInterface::UnmaskRequestDetails& request_details,
    base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                            PaymentsNetworkInterface::UnmaskResponseDetails&)> callback) {
  IssueRequest(std::make_unique<UnmaskCardRequest>(
      request_details,
      account_info_getter_->IsSyncFeatureEnabledForPaymentsServerMetrics(),
      std::move(callback)));
}

void PaymentsNetworkInterface::UnmaskIban(
    const UnmaskIbanRequestDetails& request_details,
    base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                            const std::u16string&)> callback) {
  IssueRequest(std::make_unique<UnmaskIbanRequest>(
      request_details,
      account_info_getter_->IsSyncFeatureEnabledForPaymentsServerMetrics(),
      std::move(callback)));
}

void PaymentsNetworkInterface::OptChange(
    const OptChangeRequestDetails request_details,
    base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                            PaymentsNetworkInterface::OptChangeResponseDetails&)>
        callback) {
  IssueRequest(std::make_unique<OptChangeRequest>(
      request_details, std::move(callback),
      account_info_getter_->IsSyncFeatureEnabledForPaymentsServerMetrics()));
}

void PaymentsNetworkInterface::GetCardUploadDetails(
    const std::vector<AutofillProfile>& addresses,
    const int detected_values,
    const std::vector<ClientBehaviorConstants>& client_behavior_signals,
    const std::string& app_locale,
    base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                            const std::u16string&,
                            std::unique_ptr<base::Value::Dict>,
                            std::vector<std::pair<int, int>>)> callback,
    const int billable_service_number,
    const int64_t billing_customer_number,
    UploadCardSource upload_card_source) {
  IssueRequest(std::make_unique<GetCardUploadDetailsRequest>(
      addresses, detected_values, client_behavior_signals,
      account_info_getter_->IsSyncFeatureEnabledForPaymentsServerMetrics(),
      app_locale, std::move(callback), billable_service_number,
      billing_customer_number, upload_card_source));
}

void PaymentsNetworkInterface::UploadCard(
    const PaymentsNetworkInterface::UploadCardRequestDetails& request_details,
    base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                            const UploadCardResponseDetails&)> callback) {
  IssueRequest(std::make_unique<UploadCardRequest>(
      request_details,
      account_info_getter_->IsSyncFeatureEnabledForPaymentsServerMetrics(),
      std::move(callback)));
}

void PaymentsNetworkInterface::GetIbanUploadDetails(
    const std::string& app_locale,
    int64_t billing_customer_number,
    int billable_service_number,
    base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                            const std::u16string&,
                            std::unique_ptr<base::Value::Dict>)> callback) {
  IssueRequest(std::make_unique<GetIbanUploadDetailsRequest>(
      account_info_getter_->IsSyncFeatureEnabledForPaymentsServerMetrics(),
      app_locale, billing_customer_number, billable_service_number,
      std::move(callback)));
}

void PaymentsNetworkInterface::UploadIban(
    const UploadIbanRequestDetails& details,
    base::OnceCallback<void(AutofillClient::PaymentsRpcResult)> callback) {
  IssueRequest(std::make_unique<UploadIbanRequest>(
      details,
      account_info_getter_->IsSyncFeatureEnabledForPaymentsServerMetrics(),
      std::move(callback)));
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
void PaymentsNetworkInterface::MigrateCards(
    const MigrationRequestDetails& request_details,
    const std::vector<MigratableCreditCard>& migratable_credit_cards,
    MigrateCardsCallback callback) {
  IssueRequest(std::make_unique<MigrateCardsRequest>(
      request_details, migratable_credit_cards,
      account_info_getter_->IsSyncFeatureEnabledForPaymentsServerMetrics(),
      std::move(callback)));
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

void PaymentsNetworkInterface::SelectChallengeOption(
    const SelectChallengeOptionRequestDetails& request_details,
    base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                            const std::string&)> callback) {
  IssueRequest(std::make_unique<SelectChallengeOptionRequest>(
      request_details, std::move(callback)));
}

void PaymentsNetworkInterface::GetVirtualCardEnrollmentDetails(
    const GetDetailsForEnrollmentRequestDetails& request_details,
    base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                            const PaymentsNetworkInterface::
                                GetDetailsForEnrollmentResponseDetails&)>
        callback) {
  IssueRequest(std::make_unique<GetDetailsForEnrollmentRequest>(
      request_details, std::move(callback)));
}

void PaymentsNetworkInterface::UpdateVirtualCardEnrollment(
    const UpdateVirtualCardEnrollmentRequestDetails& request_details,
    base::OnceCallback<void(AutofillClient::PaymentsRpcResult)> callback) {
  IssueRequest(std::make_unique<UpdateVirtualCardEnrollmentRequest>(
      request_details, std::move(callback)));
}

}  // namespace autofill::payments
