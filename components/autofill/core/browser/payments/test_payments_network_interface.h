// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_PAYMENTS_NETWORK_INTERFACE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_PAYMENTS_NETWORK_INTERFACE_H_

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/payments/payments_requests/update_virtual_card_enrollment_request.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace autofill::payments {

class TestPaymentsNetworkInterface : public payments::PaymentsNetworkInterface {
 public:
  TestPaymentsNetworkInterface(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_,
      signin::IdentityManager* identity_manager,
      PersonalDataManager* personal_data_manager);

  TestPaymentsNetworkInterface(const TestPaymentsNetworkInterface&) = delete;
  TestPaymentsNetworkInterface& operator=(const TestPaymentsNetworkInterface&) = delete;

  ~TestPaymentsNetworkInterface() override;

  void GetUnmaskDetails(
      base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                              PaymentsNetworkInterface::UnmaskDetails&)>
          callback,
      const std::string& app_locale) override;

  void UnmaskCard(
      const UnmaskRequestDetails& unmask_request_,
      base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                              const UnmaskResponseDetails&)> callback) override;

  void GetCardUploadDetails(
      const std::vector<AutofillProfile>& addresses,
      const int detected_values,
      const std::vector<ClientBehaviorConstants>& client_behavior_signals,
      const std::string& app_locale,
      base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                              const std::u16string&,
                              std::unique_ptr<base::Value::Dict>,
                              std::vector<std::pair<int, int>>)> callback,
      const int billable_service_number,
      const int64_t billing_customer_number,
      UploadCardSource upload_card_source =
          UploadCardSource::UNKNOWN_UPLOAD_CARD_SOURCE) override;

  void UploadCard(
      const payments::PaymentsNetworkInterface::UploadCardRequestDetails&
          request_details,
      base::OnceCallback<void(
          PaymentsAutofillClient::PaymentsRpcResult,
          const PaymentsNetworkInterface::UploadCardResponseDetails&)> callback)
      override;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  void MigrateCards(
      const MigrationRequestDetails& details,
      const std::vector<MigratableCreditCard>& migratable_credit_cards,
      MigrateCardsCallback callback) override;
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

  void SelectChallengeOption(
      const SelectChallengeOptionRequestDetails& details,
      base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                              const std::string&)> callback) override;

  void GetVirtualCardEnrollmentDetails(
      const GetDetailsForEnrollmentRequestDetails& request_details,
      base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                              const payments::PaymentsNetworkInterface::
                                  GetDetailsForEnrollmentResponseDetails&)>
          callback) override;

  void UpdateVirtualCardEnrollment(
      const UpdateVirtualCardEnrollmentRequestDetails& request_details,
      base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult)>
          callback) override;

  // Some metrics are affected by the latency of GetUnmaskDetails, so it is
  // useful to control whether or not GetUnmaskDetails() is responded to.
  void ShouldReturnUnmaskDetailsImmediately(bool should_return_unmask_details);

  void AllowFidoRegistration(bool offer_fido_opt_in = true);

  void AddFidoEligibleCard(std::string server_id,
                           std::string credential_id,
                           std::string relying_party_id);

  void SetFidoRequestOptionsInUnmaskDetails(std::string_view credential_id,
                                            std::string_view relying_party_id);

  void SetUploadCardResponseDetailsForUploadCard(
      const PaymentsNetworkInterface::UploadCardResponseDetails&
          upload_card_response_details);

  void SetSaveResultForCardsMigration(
      std::unique_ptr<std::unordered_map<std::string, std::string>>
          save_result);

  void SetSupportedBINRanges(std::vector<std::pair<int, int>> bin_ranges);

  void SetUseInvalidLegalMessageInGetUploadDetails(
      bool use_invalid_legal_message);

  void SetUseLegalMessageWithMultipleLinesInGetUploadDetails(
      bool use_legal_message_with_multiple_lines);

  void set_select_challenge_option_result(
      PaymentsAutofillClient::PaymentsRpcResult result) {
    select_challenge_option_result_ = result;
  }

  void set_update_virtual_card_enrollment_result(
      PaymentsAutofillClient::PaymentsRpcResult result) {
    update_virtual_card_enrollment_result_ = result;
  }

  payments::PaymentsNetworkInterface::UnmaskDetails* unmask_details() {
    return &unmask_details_;
  }
  const std::optional<payments::PaymentsNetworkInterface::UnmaskRequestDetails>&
  unmask_request() const {
    return unmask_request_;
  }
  const payments::PaymentsNetworkInterface::SelectChallengeOptionRequestDetails*
  select_challenge_option_request() {
    return &select_challenge_option_request_;
  }
  int detected_values_in_upload_details() const { return detected_values_; }
  const std::vector<AutofillProfile>& addresses_in_upload_details() const {
    return upload_details_addresses_;
  }
  const std::vector<AutofillProfile>& addresses_in_upload_card() const {
    return upload_card_addresses_;
  }
  const std::vector<ClientBehaviorConstants>&
  client_behavior_signals_in_request() const {
    return client_behavior_signals_;
  }
  int billable_service_number_in_request() const {
    return billable_service_number_;
  }
  int64_t billing_customer_number_in_request() const {
    return billing_customer_number_;
  }
  PaymentsNetworkInterface::UploadCardSource upload_card_source_in_request() const {
    return upload_card_source_;
  }

  const GetDetailsForEnrollmentRequestDetails&
  get_details_for_enrollment_request_details() {
    return get_details_for_enrollment_request_details_;
  }

  const UpdateVirtualCardEnrollmentRequestDetails&
  update_virtual_card_enrollment_request_details() {
    return update_virtual_card_enrollment_request_details_;
  }

 private:
  PaymentsNetworkInterface::UploadCardResponseDetails upload_card_response_details_;
  // Some metrics are affected by the latency of GetUnmaskDetails, so it is
  // useful to control whether or not GetUnmaskDetails() is responded to.
  bool should_return_unmask_details_ = true;
  payments::PaymentsNetworkInterface::UnmaskDetails unmask_details_;
  std::optional<payments::PaymentsNetworkInterface::UnmaskRequestDetails>
      unmask_request_;
  payments::PaymentsNetworkInterface::SelectChallengeOptionRequestDetails
      select_challenge_option_request_;
  std::vector<std::pair<int, int>> supported_card_bin_ranges_;
  std::vector<AutofillProfile> upload_details_addresses_;
  std::vector<AutofillProfile> upload_card_addresses_;
  int detected_values_;
  std::string pan_first_six_;
  std::vector<ClientBehaviorConstants> client_behavior_signals_;
  int billable_service_number_;
  int64_t billing_customer_number_;
  PaymentsNetworkInterface::UploadCardSource upload_card_source_;
  std::unique_ptr<std::unordered_map<std::string, std::string>> save_result_;
  bool use_invalid_legal_message_ = false;
  bool use_legal_message_with_multiple_lines_ = false;
  std::unique_ptr<base::Value::Dict> LegalMessage();
  std::optional<PaymentsAutofillClient::PaymentsRpcResult>
      select_challenge_option_result_;
  std::optional<PaymentsAutofillClient::PaymentsRpcResult>
      update_virtual_card_enrollment_result_;
  payments::PaymentsNetworkInterface::GetDetailsForEnrollmentRequestDetails
      get_details_for_enrollment_request_details_;
  payments::PaymentsNetworkInterface::UpdateVirtualCardEnrollmentRequestDetails
      update_virtual_card_enrollment_request_details_;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_PAYMENTS_NETWORK_INTERFACE_H_
