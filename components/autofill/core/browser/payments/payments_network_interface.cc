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
#include "components/autofill/core/browser/metrics/payments/credit_card_save_metrics.h"
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
namespace {

using PaymentsRpcResult = PaymentsAutofillClient::PaymentsRpcResult;

}  // namespace

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
    base::OnceCallback<void(PaymentsRpcResult, UnmaskDetails&)> callback,
    const std::string& app_locale) {
  IssueRequest(std::make_unique<GetUnmaskDetailsRequest>(
      std::move(callback), app_locale,
      account_info_getter_->IsSyncFeatureEnabledForPaymentsServerMetrics()));
}

void PaymentsNetworkInterface::UnmaskCard(
    const UnmaskRequestDetails& request_details,
    base::OnceCallback<void(PaymentsRpcResult, const UnmaskResponseDetails&)>
        callback) {
  IssueRequest(std::make_unique<UnmaskCardRequest>(
      request_details,
      account_info_getter_->IsSyncFeatureEnabledForPaymentsServerMetrics(),
      std::move(callback)));
}

void PaymentsNetworkInterface::UnmaskIban(
    const UnmaskIbanRequestDetails& request_details,
    base::OnceCallback<void(PaymentsRpcResult, const std::u16string&)>
        callback) {
  IssueRequest(std::make_unique<UnmaskIbanRequest>(
      request_details,
      account_info_getter_->IsSyncFeatureEnabledForPaymentsServerMetrics(),
      std::move(callback)));
}

void PaymentsNetworkInterface::OptChange(
    const OptChangeRequestDetails request_details,
    base::OnceCallback<void(PaymentsRpcResult, OptChangeResponseDetails&)>
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
    GetCardUploadDetailsCallback callback,
    const int billable_service_number,
    const int64_t billing_customer_number,
    UploadCardSource upload_card_source) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  GetCardUploadDetailsCallback callback_with_latency_metrics = base::BindOnce(
      [](GetCardUploadDetailsCallback callback, base::TimeTicks start_time,
         PaymentsRpcResult result, const std::u16string& context_token,
         std::unique_ptr<base::Value::Dict> legal_message,
         std::vector<std::pair<int, int>> supported_card_bin_ranges) {
        autofill_metrics::LogGetCardUploadDetailsRequestLatencyMetric(
            base::TimeTicks::Now() - start_time,
            result == PaymentsRpcResult::kSuccess);
        std::move(callback).Run(std::move(result), context_token,
                                std::move(legal_message),
                                std::move(supported_card_bin_ranges));
      },
      std::move(callback), start_time);

  IssueRequest(std::make_unique<GetCardUploadDetailsRequest>(
      addresses, detected_values, client_behavior_signals,
      account_info_getter_->IsSyncFeatureEnabledForPaymentsServerMetrics(),
      app_locale, std::move(callback_with_latency_metrics),
      billable_service_number, billing_customer_number, upload_card_source));
}

void PaymentsNetworkInterface::UploadCard(
    const UploadCardRequestDetails& request_details,
    base::OnceCallback<void(PaymentsRpcResult,
                            const UploadCardResponseDetails&)> callback) {
  IssueRequest(std::make_unique<UploadCardRequest>(
      request_details,
      account_info_getter_->IsSyncFeatureEnabledForPaymentsServerMetrics(),
      std::move(callback)));
}

void PaymentsNetworkInterface::GetIbanUploadDetails(
    const std::string& app_locale,
    int64_t billing_customer_number,
    const std::string& country_code,
    base::OnceCallback<void(PaymentsRpcResult,
                            const std::u16string& validation_regex,
                            const std::u16string& context_token,
                            std::unique_ptr<base::Value::Dict>)> callback) {
  IssueRequest(std::make_unique<GetIbanUploadDetailsRequest>(
      account_info_getter_->IsSyncFeatureEnabledForPaymentsServerMetrics(),
      app_locale, billing_customer_number, country_code, std::move(callback)));
}

void PaymentsNetworkInterface::UploadIban(
    const UploadIbanRequestDetails& details,
    base::OnceCallback<void(PaymentsRpcResult)> callback) {
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
    base::OnceCallback<void(PaymentsRpcResult, const std::string&)> callback) {
  IssueRequest(std::make_unique<SelectChallengeOptionRequest>(
      request_details, std::move(callback)));
}

void PaymentsNetworkInterface::GetVirtualCardEnrollmentDetails(
    const GetDetailsForEnrollmentRequestDetails& request_details,
    base::OnceCallback<void(PaymentsRpcResult,
                            const GetDetailsForEnrollmentResponseDetails&)>
        callback) {
  IssueRequest(std::make_unique<GetDetailsForEnrollmentRequest>(
      request_details, std::move(callback)));
}

void PaymentsNetworkInterface::UpdateVirtualCardEnrollment(
    const UpdateVirtualCardEnrollmentRequestDetails& request_details,
    base::OnceCallback<void(PaymentsRpcResult)> callback) {
  IssueRequest(std::make_unique<UpdateVirtualCardEnrollmentRequest>(
      request_details, std::move(callback)));
}

}  // namespace autofill::payments
