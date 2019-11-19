// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/test_payments_client.h"

#include <memory>
#include <unordered_map>

#include "base/json/json_reader.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace autofill {

namespace payments {

namespace {
// Base64 encoding of "This is a test challenge".
constexpr char kTestChallenge[] = "VGhpcyBpcyBhIHRlc3QgY2hhbGxlbmdl";
constexpr int kTestTimeoutSeconds = 180;
}  // namespace

TestPaymentsClient::TestPaymentsClient(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_,
    signin::IdentityManager* identity_manager,
    PersonalDataManager* personal_data_manager)
    : PaymentsClient(url_loader_factory_,
                     identity_manager,
                     personal_data_manager) {}

TestPaymentsClient::~TestPaymentsClient() {}

void TestPaymentsClient::GetUnmaskDetails(GetUnmaskDetailsCallback callback,
                                          const std::string& app_locale) {
  if (should_return_unmask_details_)
    std::move(callback).Run(AutofillClient::SUCCESS, unmask_details_);
}

void TestPaymentsClient::GetUploadDetails(
    const std::vector<AutofillProfile>& addresses,
    const int detected_values,
    const std::vector<const char*>& active_experiments,
    const std::string& app_locale,
    base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                            const base::string16&,
                            std::unique_ptr<base::Value>,
                            std::vector<std::pair<int, int>>)> callback,
    const int billable_service_number,
    PaymentsClient::UploadCardSource upload_card_source) {
  upload_details_addresses_ = addresses;
  detected_values_ = detected_values;
  active_experiments_ = active_experiments;
  billable_service_number_ = billable_service_number;
  upload_card_source_ = upload_card_source;
  std::move(callback).Run(
      app_locale == "en-US" ? AutofillClient::SUCCESS
                            : AutofillClient::PERMANENT_FAILURE,
      base::ASCIIToUTF16("this is a context token"),
      TestPaymentsClient::LegalMessage(), supported_card_bin_ranges_);
}

void TestPaymentsClient::UploadCard(
    const payments::PaymentsClient::UploadRequestDetails& request_details,
    base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                            const std::string&)> callback) {
  upload_card_addresses_ = request_details.profiles;
  active_experiments_ = request_details.active_experiments;
  std::move(callback).Run(AutofillClient::SUCCESS, server_id_);
}

void TestPaymentsClient::MigrateCards(
    const MigrationRequestDetails& details,
    const std::vector<MigratableCreditCard>& migratable_credit_cards,
    MigrateCardsCallback callback) {
  std::move(callback).Run(AutofillClient::SUCCESS, std::move(save_result_),
                          "this is display text");
}

void TestPaymentsClient::ShouldReturnUnmaskDetailsImmediately(
    bool should_return_unmask_details) {
  should_return_unmask_details_ = should_return_unmask_details;
}

void TestPaymentsClient::AllowFidoRegistration(bool offer_fido_opt_in) {
  should_return_unmask_details_ = true;
  unmask_details_.offer_fido_opt_in = offer_fido_opt_in;
}

void TestPaymentsClient::AddFidoEligibleCard(std::string server_id,
                                             std::string credential_id,
                                             std::string relying_party_id) {
  should_return_unmask_details_ = true;
  unmask_details_.offer_fido_opt_in = false;
  unmask_details_.unmask_auth_method = AutofillClient::UnmaskAuthMethod::FIDO;
  unmask_details_.fido_eligible_card_ids.insert(server_id);
  unmask_details_.fido_request_options =
      base::Value(base::Value::Type::DICTIONARY);

  // Building the following JSON structure--
  // fido_request_options = {
  //   "challenge": kTestChallenge,
  //   "timeout_millis": kTestTimeoutSeconds,
  //   "relying_party_id": relying_party_id,
  //   "key_info": [{
  //       "credential_id": credential_id,
  //       "authenticator_transport_support": ["INTERNAL"]
  // }]}
  unmask_details_.fido_request_options.SetKey("challenge",
                                              base::Value(kTestChallenge));
  unmask_details_.fido_request_options.SetKey("timeout_millis",
                                              base::Value(kTestTimeoutSeconds));
  unmask_details_.fido_request_options.SetKey("relying_party_id",
                                              base::Value(relying_party_id));

  base::Value key_info(base::Value::Type::DICTIONARY);
  if (!credential_id.empty())
    key_info.SetKey("credential_id", base::Value(credential_id));
  key_info.SetKey("authenticator_transport_support",
                  base::Value(base::Value::Type::LIST));
  key_info
      .FindKeyOfType("authenticator_transport_support", base::Value::Type::LIST)
      ->Append("INTERNAL");
  unmask_details_.fido_request_options.SetKey(
      "key_info", base::Value(base::Value::Type::LIST));
  unmask_details_.fido_request_options
      .FindKeyOfType("key_info", base::Value::Type::LIST)
      ->Append(std::move(key_info));
}

void TestPaymentsClient::SetServerIdForCardUpload(std::string server_id) {
  server_id_ = server_id;
}

void TestPaymentsClient::SetSaveResultForCardsMigration(
    std::unique_ptr<std::unordered_map<std::string, std::string>> save_result) {
  save_result_ = std::move(save_result);
}

void TestPaymentsClient::SetSupportedBINRanges(
    std::vector<std::pair<int, int>> bin_ranges) {
  supported_card_bin_ranges_ = bin_ranges;
}

void TestPaymentsClient::SetUseInvalidLegalMessageInGetUploadDetails(
    bool use_invalid_legal_message) {
  use_invalid_legal_message_ = use_invalid_legal_message;
}

std::unique_ptr<base::Value> TestPaymentsClient::LegalMessage() {
  if (use_invalid_legal_message_) {
    // Legal message is invalid because it's missing the url.
    return std::unique_ptr<base::Value>(
        base::JSONReader::ReadDeprecated("{"
                                         "  \"line\" : [ {"
                                         "     \"template\": \"Panda {0}.\","
                                         "     \"template_parameter\": [ {"
                                         "        \"display_text\": \"bear\""
                                         "     } ]"
                                         "  } ]"
                                         "}"));
    return std::unique_ptr<base::Value>(base::JSONReader::ReadDeprecated("{}"));
  } else {
    return std::unique_ptr<base::Value>(base::JSONReader::ReadDeprecated(
        "{"
        "  \"line\" : [ {"
        "     \"template\": \"The legal documents are: {0} and {1}.\","
        "     \"template_parameter\" : [ {"
        "        \"display_text\" : \"Terms of Service\","
        "        \"url\": \"http://www.example.com/tos\""
        "     }, {"
        "        \"display_text\" : \"Privacy Policy\","
        "        \"url\": \"http://www.example.com/pp\""
        "     } ]"
        "  } ]"
        "}"));
  }
}

}  // namespace payments
}  // namespace autofill
