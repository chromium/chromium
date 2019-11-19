// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_PAYMENTS_CLIENT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_PAYMENTS_CLIENT_H_

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "components/autofill/core/browser/payments/payments_client.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace autofill {
namespace payments {

class TestPaymentsClient : public payments::PaymentsClient {
 public:
  TestPaymentsClient(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_,
      signin::IdentityManager* identity_manager,
      PersonalDataManager* personal_data_manager);

  ~TestPaymentsClient() override;

  void GetUnmaskDetails(GetUnmaskDetailsCallback callback,
                        const std::string& app_locale) override;

  void GetUploadDetails(
      const std::vector<AutofillProfile>& addresses,
      const int detected_values,
      const std::vector<const char*>& active_experiments,
      const std::string& app_locale,
      base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                              const base::string16&,
                              std::unique_ptr<base::Value>,
                              std::vector<std::pair<int, int>>)> callback,
      const int billable_service_number,
      UploadCardSource upload_card_source =
          UploadCardSource::UNKNOWN_UPLOAD_CARD_SOURCE) override;

  void UploadCard(
      const payments::PaymentsClient::UploadRequestDetails& request_details,
      base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                              const std::string&)> callback) override;

  void MigrateCards(
      const MigrationRequestDetails& details,
      const std::vector<MigratableCreditCard>& migratable_credit_cards,
      MigrateCardsCallback callback) override;

  // Some metrics are affected by the latency of GetUnmaskDetails, so it is
  // useful to control whether or not GetUnmaskDetails() is responded to.
  void ShouldReturnUnmaskDetailsImmediately(bool should_return_unmask_details);

  void AllowFidoRegistration(bool offer_fido_opt_in = true);

  void AddFidoEligibleCard(std::string server_id,
                           std::string credential_id,
                           std::string relying_party_id);

  void SetServerIdForCardUpload(std::string);

  void SetSaveResultForCardsMigration(
      std::unique_ptr<std::unordered_map<std::string, std::string>>
          save_result);

  void SetSupportedBINRanges(std::vector<std::pair<int, int>> bin_ranges);

  void SetUseInvalidLegalMessageInGetUploadDetails(
      bool use_invalid_legal_message);

  int detected_values_in_upload_details() const { return detected_values_; }
  const std::vector<AutofillProfile>& addresses_in_upload_details() const {
    return upload_details_addresses_;
  }
  const std::vector<AutofillProfile>& addresses_in_upload_card() const {
    return upload_card_addresses_;
  }
  const std::vector<const char*>& active_experiments_in_request() const {
    return active_experiments_;
  }
  int billable_service_number_in_request() const {
    return billable_service_number_;
  }
  PaymentsClient::UploadCardSource upload_card_source_in_request() const {
    return upload_card_source_;
  }

 private:
  std::string server_id_;
  // Some metrics are affected by the latency of GetUnmaskDetails, so it is
  // useful to control whether or not GetUnmaskDetails() is responded to.
  bool should_return_unmask_details_ = true;
  AutofillClient::UnmaskDetails unmask_details_;
  std::vector<std::pair<int, int>> supported_card_bin_ranges_;
  std::vector<AutofillProfile> upload_details_addresses_;
  std::vector<AutofillProfile> upload_card_addresses_;
  int detected_values_;
  std::string pan_first_six_;
  std::vector<const char*> active_experiments_;
  int billable_service_number_;
  PaymentsClient::UploadCardSource upload_card_source_;
  std::unique_ptr<std::unordered_map<std::string, std::string>> save_result_;
  bool use_invalid_legal_message_ = false;
  std::unique_ptr<base::Value> LegalMessage();

  DISALLOW_COPY_AND_ASSIGN(TestPaymentsClient);
};

}  // namespace payments
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_PAYMENTS_CLIENT_H_
