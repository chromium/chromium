// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/test_payments_network_interface.h"

#include <memory>
#include <optional>
#include <unordered_map>

#include "base/json/json_reader.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {
namespace {

using PaymentsRpcResult = PaymentsAutofillClient::PaymentsRpcResult;

// Base64 encoding of "This is a test challenge".
constexpr char kTestChallenge[] = "VGhpcyBpcyBhIHRlc3QgY2hhbGxlbmdl";
constexpr int kTestTimeoutSeconds = 180;

}  // namespace

TestPaymentsNetworkInterface::TestPaymentsNetworkInterface(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_,
    signin::IdentityManager* identity_manager,
    PersonalDataManager* personal_data_manager)
    : PaymentsNetworkInterface(
          url_loader_factory_,
          identity_manager,
          personal_data_manager
              ? &personal_data_manager->payments_data_manager()
              : nullptr) {
  // Default value should be CVC.
  unmask_details_.unmask_auth_method =
      PaymentsAutofillClient::UnmaskAuthMethod::kCvc;
}

TestPaymentsNetworkInterface::~TestPaymentsNetworkInterface() = default;

void TestPaymentsNetworkInterface::GetUnmaskDetails(
    base::OnceCallback<void(PaymentsRpcResult,
                            PaymentsNetworkInterface::UnmaskDetails&)> callback,
    const std::string& app_locale) {
  if (should_return_unmask_details_)
    std::move(callback).Run(PaymentsRpcResult::kSuccess, unmask_details_);
}

void TestPaymentsNetworkInterface::UnmaskCard(
    const UnmaskRequestDetails& unmask_request,
    base::OnceCallback<void(PaymentsRpcResult, const UnmaskResponseDetails&)>
        callback) {
  unmask_request_ = unmask_request;
}

void TestPaymentsNetworkInterface::GetCardUploadDetails(
    const std::vector<AutofillProfile>& addresses,
    const int detected_values,
    const std::vector<ClientBehaviorConstants>& client_behavior_signals,
    const std::string& app_locale,
    base::OnceCallback<void(PaymentsRpcResult,
                            const std::u16string&,
                            std::unique_ptr<base::Value::Dict>,
                            std::vector<std::pair<int, int>>)> callback,
    const int billable_service_number,
    const int64_t billing_customer_number,
    PaymentsNetworkInterface::UploadCardSource upload_card_source) {
  upload_details_addresses_ = addresses;
  detected_values_ = detected_values;
  client_behavior_signals_ = client_behavior_signals;
  billable_service_number_ = billable_service_number;
  billing_customer_number_ = billing_customer_number;
  upload_card_source_ = upload_card_source;
  std::move(callback).Run(
      app_locale == "en-US" ? PaymentsRpcResult::kSuccess
                            : PaymentsRpcResult::kPermanentFailure,
      u"this is a context token", TestPaymentsNetworkInterface::LegalMessage(),
      supported_card_bin_ranges_);
}

void TestPaymentsNetworkInterface::UploadCard(
    const payments::PaymentsNetworkInterface::UploadCardRequestDetails&
        request_details,
    base::OnceCallback<void(
        PaymentsRpcResult,
        const PaymentsNetworkInterface::UploadCardResponseDetails&)> callback) {
  upload_card_addresses_ = request_details.profiles;
  client_behavior_signals_ = request_details.client_behavior_signals;
  std::move(callback).Run(PaymentsRpcResult::kSuccess,
                          upload_card_response_details_);
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
void TestPaymentsNetworkInterface::MigrateCards(
    const MigrationRequestDetails& details,
    const std::vector<MigratableCreditCard>& migratable_credit_cards,
    MigrateCardsCallback callback) {
  std::move(callback).Run(PaymentsRpcResult::kSuccess, std::move(save_result_),
                          "this is display text");
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

void TestPaymentsNetworkInterface::SelectChallengeOption(
    const SelectChallengeOptionRequestDetails& details,
    base::OnceCallback<void(PaymentsRpcResult, const std::string&)> callback) {
  select_challenge_option_request_ = details;
  // If select_challenge_option_result_ is set, use the provided result.
  // Otherwise, always return success with fake context token.
  if (select_challenge_option_result_) {
    std::move(callback).Run(select_challenge_option_result_.value(),
                            /*context_token=*/"");
    return;
  }
  std::move(callback).Run(PaymentsRpcResult::kSuccess,
                          "context_token from SelectChallengeOption");
}

void TestPaymentsNetworkInterface::GetVirtualCardEnrollmentDetails(
    const GetDetailsForEnrollmentRequestDetails& request_details,
    base::OnceCallback<void(PaymentsRpcResult,
                            const payments::PaymentsNetworkInterface::
                                GetDetailsForEnrollmentResponseDetails&)>
        callback) {
  get_details_for_enrollment_request_details_ = std::move(request_details);
}

void TestPaymentsNetworkInterface::UpdateVirtualCardEnrollment(
    const TestPaymentsNetworkInterface::
        UpdateVirtualCardEnrollmentRequestDetails& request_details,
    base::OnceCallback<void(PaymentsRpcResult)> callback) {
  update_virtual_card_enrollment_request_details_ = std::move(request_details);
  std::move(callback).Run(update_virtual_card_enrollment_result_.value_or(
      PaymentsRpcResult::kSuccess));
}

void TestPaymentsNetworkInterface::ShouldReturnUnmaskDetailsImmediately(
    bool should_return_unmask_details) {
  should_return_unmask_details_ = should_return_unmask_details;
}

void TestPaymentsNetworkInterface::AllowFidoRegistration(bool offer_fido_opt_in) {
  should_return_unmask_details_ = true;
  unmask_details_.offer_fido_opt_in = offer_fido_opt_in;
}

void TestPaymentsNetworkInterface::AddFidoEligibleCard(std::string server_id,
                                             std::string credential_id,
                                             std::string relying_party_id) {
  should_return_unmask_details_ = true;
  unmask_details_.offer_fido_opt_in = false;
  unmask_details_.unmask_auth_method =
      PaymentsAutofillClient::UnmaskAuthMethod::kFido;
  unmask_details_.fido_eligible_card_ids.insert(server_id);

  SetFidoRequestOptionsInUnmaskDetails(credential_id, relying_party_id);
}

void TestPaymentsNetworkInterface::SetFidoRequestOptionsInUnmaskDetails(
    std::string_view credential_id,
    std::string_view relying_party_id) {
  // Building the following JSON structure--
  // fido_request_options = {
  //   "challenge": kTestChallenge,
  //   "timeout_millis": kTestTimeoutSeconds,
  //   "relying_party_id": relying_party_id,
  //   "key_info": [{
  //       "credential_id": credential_id,
  //       "authenticator_transport_support": ["INTERNAL"]
  // }]}

  auto key_info =
      base::Value::Dict().Set("authenticator_transport_support",
                              base::Value::List().Append("INTERNAL"));
  if (!credential_id.empty()) {
    key_info.Set("credential_id", base::Value(credential_id));
  }

  unmask_details_.fido_request_options =
      base::Value::Dict()
          .Set("challenge", base::Value(kTestChallenge))
          .Set("timeout_millis", base::Value(kTestTimeoutSeconds))
          .Set("relying_party_id", base::Value(relying_party_id))
          .Set("key_info", base::Value::List().Append(std::move(key_info)));
}

void TestPaymentsNetworkInterface::SetUploadCardResponseDetailsForUploadCard(
    const PaymentsNetworkInterface::UploadCardResponseDetails&
        upload_card_response_details) {
  upload_card_response_details_ = upload_card_response_details;
}

void TestPaymentsNetworkInterface::SetSaveResultForCardsMigration(
    std::unique_ptr<std::unordered_map<std::string, std::string>> save_result) {
  save_result_ = std::move(save_result);
}

void TestPaymentsNetworkInterface::SetSupportedBINRanges(
    std::vector<std::pair<int, int>> bin_ranges) {
  supported_card_bin_ranges_ = bin_ranges;
}

void TestPaymentsNetworkInterface::SetUseInvalidLegalMessageInGetUploadDetails(
    bool use_invalid_legal_message) {
  use_invalid_legal_message_ = use_invalid_legal_message;
}

void TestPaymentsNetworkInterface::SetUseLegalMessageWithMultipleLinesInGetUploadDetails(
    bool use_legal_message_with_multiple_lines) {
  use_legal_message_with_multiple_lines_ =
      use_legal_message_with_multiple_lines;
}

std::unique_ptr<base::Value::Dict> TestPaymentsNetworkInterface::LegalMessage() {
  std::optional<base::Value> parsed_json;
  if (use_invalid_legal_message_) {
    // Legal message is invalid because it's missing the url.
    parsed_json = base::JSONReader::Read(
        "{"
        "  \"line\" : [ {"
        "     \"template\": \"Panda {0}.\","
        "     \"template_parameter\": [ {"
        "        \"display_text\": \"bear\""
        "     } ]"
        "  } ]"
        "}");
    DCHECK(parsed_json);
  } else if (use_legal_message_with_multiple_lines_) {
    parsed_json = base::JSONReader::Read(
        "{"
        "  \"line\": ["
        "    {"
        "      \"template\": \"The legal documents are: {0} and {1}.\","
        "      \"template_parameter\": ["
        "        {"
        "          \"display_text\": \"Terms of Service\","
        "          \"url\": \"http://www.example.com/tos\""
        "        },"
        "        {"
        "          \"display_text\": \"Privacy Policy\","
        "          \"url\": \"http://www.example.com/pp\""
        "        }"
        "      ]"
        "    },"
        "    {"
        "      \"template\": \"The legal documents are: {0} and {1}.\","
        "      \"template_parameter\": ["
        "        {"
        "          \"display_text\": \"Terms of Service\","
        "          \"url\": \"http://www.example.com/tos\""
        "        },"
        "        {"
        "          \"display_text\": \"Privacy Policy\","
        "          \"url\": \"http://www.example.com/pp\""
        "        }"
        "      ]"
        "    }"
        "  ]"
        "}");
    DCHECK(parsed_json);
  } else {
    parsed_json = base::JSONReader::Read(
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
        "}");
    DCHECK(parsed_json);
  }
  // TODO(crbug.com/40826246): Refactor when `base::JSONReader::Read` is updated
  // to return a Dict.
  return std::make_unique<base::Value::Dict>(std::move(parsed_json->GetDict()));
}

}  // namespace autofill::payments
