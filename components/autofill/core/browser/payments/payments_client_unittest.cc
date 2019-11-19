// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/payments/credit_card_save_manager.h"
#include "components/autofill/core/browser/payments/local_card_migration_manager.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/variations/net/variations_http_headers.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_http_header_provider.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace payments {
namespace {

int kAllDetectableValues =
    CreditCardSaveManager::DetectedValue::CVC |
    CreditCardSaveManager::DetectedValue::CARDHOLDER_NAME |
    CreditCardSaveManager::DetectedValue::ADDRESS_NAME |
    CreditCardSaveManager::DetectedValue::ADDRESS_LINE |
    CreditCardSaveManager::DetectedValue::LOCALITY |
    CreditCardSaveManager::DetectedValue::ADMINISTRATIVE_AREA |
    CreditCardSaveManager::DetectedValue::POSTAL_CODE |
    CreditCardSaveManager::DetectedValue::COUNTRY_CODE |
    CreditCardSaveManager::DetectedValue::HAS_GOOGLE_PAYMENTS_ACCOUNT;

struct CardUnmaskOptions {
  CardUnmaskOptions& with_use_fido(bool b) {
    use_fido = b;
    return *this;
  }

  CardUnmaskOptions& with_cvc(std::string c) {
    cvc = c;
    return *this;
  }

  CardUnmaskOptions& with_reason(AutofillClient::UnmaskCardReason r) {
    reason = r;
    return *this;
  }

  // If true, use FIDO authentication instead of CVC authentication.
  bool use_fido = false;
  // If not using FIDO authentication, the CVC value the user entered, to be
  // sent to Google Payments.
  std::string cvc = "123";
  // The reason for unmasking this card.
  AutofillClient::UnmaskCardReason reason = AutofillClient::UNMASK_FOR_AUTOFILL;
};

}  // namespace

class PaymentsClientTest : public testing::Test {
 public:
  PaymentsClientTest() : result_(AutofillClient::NONE) {}
  ~PaymentsClientTest() override {}

  void SetUp() override {
    // Silence the warning for mismatching sync and Payments servers.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kWalletServiceUseSandbox, "0");

    result_ = AutofillClient::NONE;
    server_id_.clear();
    unmask_response_details_ = nullptr;
    legal_message_.reset();
    has_variations_header_ = false;

    factory()->SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {
          intercepted_headers_ = request.headers;
          intercepted_body_ = network::GetUploadData(request);
          has_variations_header_ = variations::HasVariationsHeader(request);
        }));
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    client_ = std::make_unique<PaymentsClient>(
        test_shared_loader_factory_, identity_test_env_.identity_manager(),
        &test_personal_data_);
    test_personal_data_.SetAccountInfoForPayments(
        identity_test_env_.MakePrimaryAccountAvailable("example@gmail.com"));
  }

  void TearDown() override { client_.reset(); }

  void EnableAutofillGetPaymentsIdentityFromSync() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kAutofillGetPaymentsIdentityFromSync);
  }

  // Registers a field trial with the specified name and group and an associated
  // google web property variation id.
  void CreateFieldTrialWithId(const std::string& trial_name,
                              const std::string& group_name,
                              int variation_id) {
    variations::AssociateGoogleVariationID(
        variations::GOOGLE_WEB_PROPERTIES, trial_name, group_name,
        static_cast<variations::VariationID>(variation_id));
    base::FieldTrialList::CreateFieldTrial(trial_name, group_name)->group();
  }

  void OnDidGetUnmaskDetails(AutofillClient::PaymentsRpcResult result,
                             AutofillClient::UnmaskDetails& unmask_details) {
    result_ = result;
    unmask_details_ = &unmask_details;
  }

  void OnDidGetRealPan(AutofillClient::PaymentsRpcResult result,
                       PaymentsClient::UnmaskResponseDetails& response) {
    result_ = result;
    unmask_response_details_ = &response;
  }

  void OnDidGetOptChangeResult(
      AutofillClient::PaymentsRpcResult result,
      PaymentsClient::OptChangeResponseDetails& response) {
    result_ = result;
    opt_change_response_.user_is_opted_in = response.user_is_opted_in;
    opt_change_response_.fido_creation_options =
        std::move(response.fido_creation_options);
    opt_change_response_.fido_request_options =
        std::move(response.fido_request_options);
  }

  void OnDidGetUploadDetails(
      AutofillClient::PaymentsRpcResult result,
      const base::string16& context_token,
      std::unique_ptr<base::Value> legal_message,
      std::vector<std::pair<int, int>> supported_card_bin_ranges) {
    result_ = result;
    legal_message_ = std::move(legal_message);
    supported_card_bin_ranges_ = supported_card_bin_ranges;
  }

  void OnDidUploadCard(AutofillClient::PaymentsRpcResult result,
                       const std::string& server_id) {
    result_ = result;
    server_id_ = server_id;
  }

  void OnDidMigrateLocalCards(
      AutofillClient::PaymentsRpcResult result,
      std::unique_ptr<std::unordered_map<std::string, std::string>>
          migration_save_results,
      const std::string& display_text) {
    result_ = result;
    migration_save_results_ = std::move(migration_save_results);
    display_text_ = display_text;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

  // Issue a GetUnmaskDetails request. This requires an OAuth token before
  // starting the request.
  void StartGettingUnmaskDetails() {
    client_->GetUnmaskDetails(
        base::BindOnce(&PaymentsClientTest::OnDidGetUnmaskDetails,
                       weak_ptr_factory_.GetWeakPtr()),
        "language-LOCALE");
  }

  // Issue an UnmaskCard request. This requires an OAuth token before starting
  // the request.
  void StartUnmasking(CardUnmaskOptions options) {
    PaymentsClient::UnmaskRequestDetails request_details;
    request_details.billing_customer_number = 111222333444;
    request_details.reason = options.reason;

    request_details.card = test::GetMaskedServerCard();
    request_details.risk_data = "some risk data";
    if (options.use_fido) {
      request_details.fido_assertion_info =
          base::Value(base::Value::Type::DICTIONARY);
    } else {
      request_details.user_response.cvc = base::ASCIIToUTF16(options.cvc);
    }
    client_->UnmaskCard(request_details,
                        base::BindOnce(&PaymentsClientTest::OnDidGetRealPan,
                                       weak_ptr_factory_.GetWeakPtr()));
  }

  // If |opt_in| is set to true, then opts the user in to use FIDO
  // authentication for card unmasking. Otherwise opts the user out.
  void StartOptChangeRequest(
      PaymentsClient::OptChangeRequestDetails::Reason reason) {
    PaymentsClient::OptChangeRequestDetails request_details;
    request_details.reason = reason;
    client_->OptChange(
        request_details,
        base::BindOnce(&PaymentsClientTest::OnDidGetOptChangeResult,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  // Issue a GetUploadDetails request.
  void StartGettingUploadDetails(
      PaymentsClient::UploadCardSource upload_card_source =
          PaymentsClient::UploadCardSource::UNKNOWN_UPLOAD_CARD_SOURCE) {
    client_->GetUploadDetails(
        BuildTestProfiles(), kAllDetectableValues, std::vector<const char*>(),
        "language-LOCALE",
        base::BindOnce(&PaymentsClientTest::OnDidGetUploadDetails,
                       weak_ptr_factory_.GetWeakPtr()),
        /*billable_service_number=*/12345, upload_card_source);
  }

  // Issue an UploadCard request. This requires an OAuth token before starting
  // the request.
  void StartUploading(bool include_cvc) {
    PaymentsClient::UploadRequestDetails request_details;
    request_details.billing_customer_number = 111222333444;
    request_details.card = test::GetCreditCard();
    if (include_cvc)
      request_details.cvc = base::ASCIIToUTF16("123");
    request_details.context_token = base::ASCIIToUTF16("context token");
    request_details.risk_data = "some risk data";
    request_details.app_locale = "language-LOCALE";
    request_details.profiles = BuildTestProfiles();
    client_->UploadCard(request_details,
                        base::BindOnce(&PaymentsClientTest::OnDidUploadCard,
                                       weak_ptr_factory_.GetWeakPtr()));
  }

  void StartMigrating(bool has_cardholder_name) {
    PaymentsClient::MigrationRequestDetails request_details;
    request_details.context_token = base::ASCIIToUTF16("context token");
    request_details.risk_data = "some risk data";
    request_details.app_locale = "language-LOCALE";

    migratable_credit_cards_.clear();
    CreditCard card1 = test::GetCreditCard();
    CreditCard card2 = test::GetCreditCard2();
    if (!has_cardholder_name) {
      card1.SetRawInfo(CREDIT_CARD_NAME_FULL, base::UTF8ToUTF16(""));
      card2.SetRawInfo(CREDIT_CARD_NAME_FULL, base::UTF8ToUTF16(""));
    }
    migratable_credit_cards_.push_back(MigratableCreditCard(card1));
    migratable_credit_cards_.push_back(MigratableCreditCard(card2));
    client_->MigrateCards(
        request_details, migratable_credit_cards_,
        base::BindOnce(&PaymentsClientTest::OnDidMigrateLocalCards,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  network::TestURLLoaderFactory* factory() { return &test_url_loader_factory_; }

  const std::string& GetUploadData() { return intercepted_body_; }

  bool HasVariationsHeader() { return has_variations_header_; }

  // Issues access token in response to any access token request. This will
  // start the Payments Request which requires the authentication.
  void IssueOAuthToken() {
    identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
        "totally_real_token",
        AutofillClock::Now() + base::TimeDelta::FromDays(10));

    // Verify the auth header.
    std::string auth_header_value;
    EXPECT_TRUE(intercepted_headers_.GetHeader(
        net::HttpRequestHeaders::kAuthorization, &auth_header_value))
        << intercepted_headers_.ToString();
    EXPECT_EQ("Bearer totally_real_token", auth_header_value);
  }

  void ReturnResponse(net::HttpStatusCode response_code,
                      const std::string& response_body) {
    client_->OnSimpleLoaderCompleteInternal(response_code, response_body);
  }

  AutofillClient::PaymentsRpcResult result_;
  AutofillClient::UnmaskDetails* unmask_details_;

  // Server ID of a saved card via credit card upload save.
  std::string server_id_;
  // The OptChangeResponseDetails retrieved from an OptChangeRequest.
  PaymentsClient::OptChangeResponseDetails opt_change_response_;
  // The UnmaskResponseDetails retrieved from an UnmaskRequest.  Includes PAN.
  PaymentsClient::UnmaskResponseDetails* unmask_response_details_ = nullptr;
  // The legal message returned from a GetDetails upload save preflight call.
  std::unique_ptr<base::Value> legal_message_;
  // A list of card BIN ranges supported by Google Payments, returned from a
  // GetDetails upload save preflight call.
  std::vector<std::pair<int, int>> supported_card_bin_ranges_;
  // Credit cards to be upload saved during a local credit card migration call.
  std::vector<MigratableCreditCard> migratable_credit_cards_;
  // A mapping of results from a local credit card migration call.
  std::unique_ptr<std::unordered_map<std::string, std::string>>
      migration_save_results_;
  // A tip message to be displayed during local card migration.
  std::string display_text_;

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  TestPersonalDataManager test_personal_data_;
  std::unique_ptr<PaymentsClient> client_;
  signin::IdentityTestEnvironment identity_test_env_;

  net::HttpRequestHeaders intercepted_headers_;
  bool has_variations_header_;
  std::string intercepted_body_;
  base::WeakPtrFactory<PaymentsClientTest> weak_ptr_factory_{this};

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentsClientTest);

  std::vector<AutofillProfile> BuildTestProfiles() {
    std::vector<AutofillProfile> profiles;
    profiles.push_back(BuildProfile("John", "Smith", "1234 Main St.", "Miami",
                                    "FL", "32006", "212-555-0162"));
    profiles.push_back(BuildProfile("Pat", "Jones", "432 Oak Lane", "Lincoln",
                                    "OH", "43005", "(834)555-0090"));
    return profiles;
  }

  AutofillProfile BuildProfile(base::StringPiece first_name,
                               base::StringPiece last_name,
                               base::StringPiece address_line,
                               base::StringPiece city,
                               base::StringPiece state,
                               base::StringPiece zip,
                               base::StringPiece phone_number) {
    AutofillProfile profile;

    profile.SetInfo(NAME_FIRST, ASCIIToUTF16(first_name), "en-US");
    profile.SetInfo(NAME_LAST, ASCIIToUTF16(last_name), "en-US");
    profile.SetInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16(address_line), "en-US");
    profile.SetInfo(ADDRESS_HOME_CITY, ASCIIToUTF16(city), "en-US");
    profile.SetInfo(ADDRESS_HOME_STATE, ASCIIToUTF16(state), "en-US");
    profile.SetInfo(ADDRESS_HOME_ZIP, ASCIIToUTF16(zip), "en-US");
    profile.SetInfo(PHONE_HOME_WHOLE_NUMBER, ASCIIToUTF16(phone_number),
                    "en-US");

    return profile;
  }
};

TEST_F(PaymentsClientTest, GetUnmaskDetailsSuccess) {
  StartGettingUnmaskDetails();
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK,
                 "{ \"offer_fido_opt_in\": \"false\", "
                 "\"authentication_method\": \"CVC\" }");
  EXPECT_EQ(AutofillClient::SUCCESS, result_);
  EXPECT_EQ(false, unmask_details_->offer_fido_opt_in);
  EXPECT_EQ(AutofillClient::UnmaskAuthMethod::CVC,
            unmask_details_->unmask_auth_method);
}

TEST_F(PaymentsClientTest, GetUnmaskDetailsIncludesChromeUserContext) {
  scoped_feature_list_.InitWithFeatures(
      {features::kAutofillGetPaymentsIdentityFromSync},  // Enabled
      {features::kAutofillEnableAccountWalletStorage});  // Disabled

  StartGettingUnmaskDetails();
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{}");

  // ChromeUserContext was set.
  EXPECT_TRUE(GetUploadData().find("chrome_user_context") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("full_sync_enabled") != std::string::npos);
}

TEST_F(PaymentsClientTest, OAuthError) {
  StartUnmasking(CardUnmaskOptions());
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE));
  EXPECT_EQ(AutofillClient::PERMANENT_FAILURE, result_);
  EXPECT_TRUE(unmask_response_details_->real_pan.empty());
}

TEST_F(PaymentsClientTest,
       UnmaskRequestIncludesBillingCustomerNumberInRequest) {
  StartUnmasking(CardUnmaskOptions());
  IssueOAuthToken();

  // Verify that the billing customer number is included in the request.
  EXPECT_TRUE(
      GetUploadData().find("%22external_customer_id%22:%22111222333444%22") !=
      std::string::npos);
}

TEST_F(PaymentsClientTest, UnmaskSuccessViaCVC) {
  StartUnmasking(CardUnmaskOptions().with_use_fido(false));
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{ \"pan\": \"1234\" }");
  EXPECT_EQ(AutofillClient::SUCCESS, result_);
  EXPECT_EQ("1234", unmask_response_details_->real_pan);
}

TEST_F(PaymentsClientTest, UnmaskSuccessViaFIDO) {
  StartUnmasking(CardUnmaskOptions().with_use_fido(true));
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{ \"pan\": \"1234\" }");
  EXPECT_EQ(AutofillClient::SUCCESS, result_);
  EXPECT_EQ("1234", unmask_response_details_->real_pan);
}

TEST_F(PaymentsClientTest, UnmaskSuccessViaCVCWithCreationOptions) {
  StartUnmasking(CardUnmaskOptions().with_use_fido(false));
  IssueOAuthToken();
  ReturnResponse(
      net::HTTP_OK,
      "{ \"pan\": \"1234\", \"dcvv\": \"321\", \"fido_creation_options\": "
      "{\"relying_party_id\": \"google.com\"}}");
  EXPECT_EQ(AutofillClient::SUCCESS, result_);
  EXPECT_EQ("1234", unmask_response_details_->real_pan);
  EXPECT_EQ("321", unmask_response_details_->dcvv);
  EXPECT_EQ("google.com",
            *unmask_response_details_->fido_creation_options->FindStringKey(
                "relying_party_id"));
}

TEST_F(PaymentsClientTest, UnmaskSuccessAccountFromSyncTest) {
  EnableAutofillGetPaymentsIdentityFromSync();
  StartUnmasking(CardUnmaskOptions());
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{ \"pan\": \"1234\" }");
  EXPECT_EQ(AutofillClient::SUCCESS, result_);
  EXPECT_EQ("1234", unmask_response_details_->real_pan);
}

TEST_F(PaymentsClientTest, UnmaskIncludesChromeUserContext) {
  scoped_feature_list_.InitWithFeatures(
      {features::kAutofillGetPaymentsIdentityFromSync},  // Enabled
      {features::kAutofillEnableAccountWalletStorage});  // Disabled

  StartUnmasking(CardUnmaskOptions());
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{}");

  // ChromeUserContext was set.
  EXPECT_TRUE(GetUploadData().find("chrome_user_context") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("full_sync_enabled") != std::string::npos);
}

TEST_F(PaymentsClientTest,
       UnmaskIncludesChromeUserContextIfWalletStorageFlagEnabled) {
  scoped_feature_list_.InitWithFeatures(
      {features::kAutofillEnableAccountWalletStorage},    // Enabled
      {features::kAutofillGetPaymentsIdentityFromSync});  // Disabled

  StartUnmasking(CardUnmaskOptions());
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{}");

  // ChromeUserContext was set.
  EXPECT_TRUE(GetUploadData().find("chrome_user_context") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("full_sync_enabled") != std::string::npos);
}

TEST_F(PaymentsClientTest, UnmaskExcludesChromeUserContextIfExperimentsOff) {
  scoped_feature_list_.InitWithFeatures(
      {},  // Enabled
      {features::kAutofillEnableAccountWalletStorage,
       features::kAutofillGetPaymentsIdentityFromSync});  // Disabled

  StartUnmasking(CardUnmaskOptions());
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{}");

  // ChromeUserContext was not set.
  EXPECT_TRUE(!GetUploadData().empty());
  EXPECT_TRUE(GetUploadData().find("chrome_user_context") == std::string::npos);
  EXPECT_TRUE(GetUploadData().find("full_sync_enabled") == std::string::npos);
}

TEST_F(PaymentsClientTest, UnmaskLogsCvcLengthForAutofill) {
  base::HistogramTester histogram_tester;
  StartUnmasking(CardUnmaskOptions()
                     .with_reason(AutofillClient::UNMASK_FOR_AUTOFILL)
                     .with_cvc("1234"));
  IssueOAuthToken();

  histogram_tester.ExpectBucketCount(
      "Autofill.CardUnmask.CvcLength.ForAutofill", 4, 1);
}

TEST_F(PaymentsClientTest, UnmaskLogsCvcLengthForPaymentRequest) {
  base::HistogramTester histogram_tester;
  StartUnmasking(CardUnmaskOptions()
                     .with_reason(AutofillClient::UNMASK_FOR_PAYMENT_REQUEST)
                     .with_cvc("56789"));
  IssueOAuthToken();

  histogram_tester.ExpectBucketCount(
      "Autofill.CardUnmask.CvcLength.ForPaymentRequest", 5, 1);
}

TEST_F(PaymentsClientTest, OptInSuccess) {
  StartOptChangeRequest(
      PaymentsClient::OptChangeRequestDetails::ENABLE_FIDO_AUTH);
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK,
                 "{ \"fido_authentication_info\": { \"user_status\": "
                 "\"FIDO_AUTH_ENABLED\"}}");
  EXPECT_EQ(AutofillClient::SUCCESS, result_);
  EXPECT_TRUE(opt_change_response_.user_is_opted_in.value());
}

TEST_F(PaymentsClientTest, OptInServerUnresponsive) {
  StartOptChangeRequest(
      PaymentsClient::OptChangeRequestDetails::ENABLE_FIDO_AUTH);
  IssueOAuthToken();
  ReturnResponse(net::HTTP_REQUEST_TIMEOUT, "");
  EXPECT_EQ(AutofillClient::NETWORK_ERROR, result_);
  EXPECT_FALSE(opt_change_response_.user_is_opted_in.has_value());
}

TEST_F(PaymentsClientTest, OptOutSuccess) {
  StartOptChangeRequest(
      PaymentsClient::OptChangeRequestDetails::DISABLE_FIDO_AUTH);
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK,
                 "{ \"fido_authentication_info\": { \"user_status\": "
                 "\"FIDO_AUTH_DISABLED\"}}");
  EXPECT_EQ(AutofillClient::SUCCESS, result_);
  EXPECT_FALSE(opt_change_response_.user_is_opted_in.value());
}

TEST_F(PaymentsClientTest, EnrollAttemptReturnsCreationOptions) {
  StartOptChangeRequest(
      PaymentsClient::OptChangeRequestDetails::ENABLE_FIDO_AUTH);
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK,
                 "{ \"fido_authentication_info\": { \"user_status\": "
                 "\"FIDO_AUTH_DISABLED\","
                 "\"fido_creation_options\": {"
                 "\"relying_party_id\": \"google.com\"}}}");
  EXPECT_EQ(AutofillClient::SUCCESS, result_);
  EXPECT_FALSE(opt_change_response_.user_is_opted_in.value());
  EXPECT_EQ("google.com",
            *opt_change_response_.fido_creation_options->FindStringKey(
                "relying_party_id"));
}

TEST_F(PaymentsClientTest, GetDetailsSuccess) {
  StartGettingUploadDetails();
  ReturnResponse(
      net::HTTP_OK,
      "{ \"context_token\": \"some_token\", \"legal_message\": {} }");
  EXPECT_EQ(AutofillClient::SUCCESS, result_);
  EXPECT_NE(nullptr, legal_message_.get());
}

TEST_F(PaymentsClientTest, GetDetailsRemovesNonLocationData) {
  StartGettingUploadDetails();

  // Verify that the recipient name field and test names appear nowhere in the
  // upload data.
  EXPECT_TRUE(GetUploadData().find(PaymentsClient::kRecipientName) ==
              std::string::npos);
  EXPECT_TRUE(GetUploadData().find("John") == std::string::npos);
  EXPECT_TRUE(GetUploadData().find("Smith") == std::string::npos);
  EXPECT_TRUE(GetUploadData().find("Pat") == std::string::npos);
  EXPECT_TRUE(GetUploadData().find("Jones") == std::string::npos);

  // Verify that the phone number field and test numbers appear nowhere in the
  // upload data.
  EXPECT_TRUE(GetUploadData().find(PaymentsClient::kPhoneNumber) ==
              std::string::npos);
  EXPECT_TRUE(GetUploadData().find("212") == std::string::npos);
  EXPECT_TRUE(GetUploadData().find("555") == std::string::npos);
  EXPECT_TRUE(GetUploadData().find("0162") == std::string::npos);
  EXPECT_TRUE(GetUploadData().find("834") == std::string::npos);
  EXPECT_TRUE(GetUploadData().find("0090") == std::string::npos);
}

TEST_F(PaymentsClientTest, GetDetailsIncludesDetectedValuesInRequest) {
  StartGettingUploadDetails();

  // Verify that the detected values were included in the request.
  std::string detected_values_string =
      "\"detected_values\":" + std::to_string(kAllDetectableValues);
  EXPECT_TRUE(GetUploadData().find(detected_values_string) !=
              std::string::npos);
}

TEST_F(PaymentsClientTest, GetDetailsIncludesChromeUserContext) {
  scoped_feature_list_.InitWithFeatures(
      {features::kAutofillGetPaymentsIdentityFromSync},  // Enabled
      {features::kAutofillEnableAccountWalletStorage});  // Disabled

  StartGettingUploadDetails();

  // ChromeUserContext was set.
  EXPECT_TRUE(GetUploadData().find("chrome_user_context") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("full_sync_enabled") != std::string::npos);
}

TEST_F(PaymentsClientTest,
       GetDetailsIncludesChromeUserContextIfWalletStorageFlagEnabled) {
  scoped_feature_list_.InitWithFeatures(
      {features::kAutofillEnableAccountWalletStorage},    // Enabled
      {features::kAutofillGetPaymentsIdentityFromSync});  // Disabled

  StartGettingUploadDetails();

  // ChromeUserContext was set.
  EXPECT_TRUE(GetUploadData().find("chrome_user_context") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("full_sync_enabled") != std::string::npos);
}

TEST_F(PaymentsClientTest,
       GetDetailsExcludesChromeUserContextIfExperimentsOff) {
  scoped_feature_list_.InitWithFeatures(
      {},  // Enabled
      {features::kAutofillEnableAccountWalletStorage,
       features::kAutofillGetPaymentsIdentityFromSync});  // Disabled

  StartGettingUploadDetails();

  // ChromeUserContext was not set.
  EXPECT_TRUE(!GetUploadData().empty());
  EXPECT_TRUE(GetUploadData().find("chrome_user_context") == std::string::npos);
  EXPECT_TRUE(GetUploadData().find("full_sync_enabled") == std::string::npos);
}

TEST_F(PaymentsClientTest,
       GetDetailsIncludesUpstreamCheckoutFlowUploadCardSourceInRequest) {
  StartGettingUploadDetails(
      PaymentsClient::UploadCardSource::UPSTREAM_CHECKOUT_FLOW);

  // Verify that the correct upload card source was included in the request.
  EXPECT_TRUE(GetUploadData().find("UPSTREAM_CHECKOUT_FLOW") !=
              std::string::npos);
}

TEST_F(PaymentsClientTest,
       GetDetailsIncludesUpstreamSettingsPageUploadCardSourceInRequest) {
  StartGettingUploadDetails(
      PaymentsClient::UploadCardSource::UPSTREAM_SETTINGS_PAGE);

  // Verify that the correct upload card source was included in the request.
  EXPECT_TRUE(GetUploadData().find("UPSTREAM_SETTINGS_PAGE") !=
              std::string::npos);
}

TEST_F(PaymentsClientTest,
       GetDetailsIncludesUpstreamCardOcrUploadCardSourceInRequest) {
  StartGettingUploadDetails(
      PaymentsClient::UploadCardSource::UPSTREAM_CARD_OCR);

  // Verify that the correct upload card source was included in the request.
  EXPECT_TRUE(GetUploadData().find("UPSTREAM_CARD_OCR") != std::string::npos);
}

TEST_F(
    PaymentsClientTest,
    GetDetailsIncludesLocalCardMigrationCheckoutFlowUploadCardSourceInRequest) {
  StartGettingUploadDetails(
      PaymentsClient::UploadCardSource::LOCAL_CARD_MIGRATION_CHECKOUT_FLOW);

  // Verify that the correct upload card source was included in the request.
  EXPECT_TRUE(GetUploadData().find("LOCAL_CARD_MIGRATION_CHECKOUT_FLOW") !=
              std::string::npos);
}

TEST_F(
    PaymentsClientTest,
    GetDetailsIncludesLocalCardMigrationSettingsPageUploadCardSourceInRequest) {
  StartGettingUploadDetails(
      PaymentsClient::UploadCardSource::LOCAL_CARD_MIGRATION_SETTINGS_PAGE);

  // Verify that the correct upload card source was included in the request.
  EXPECT_TRUE(GetUploadData().find("LOCAL_CARD_MIGRATION_SETTINGS_PAGE") !=
              std::string::npos);
}

TEST_F(PaymentsClientTest, GetDetailsIncludesUnknownUploadCardSourceInRequest) {
  StartGettingUploadDetails();

  // Verify that the absence of an upload card source results in UNKNOWN.
  EXPECT_TRUE(GetUploadData().find("UNKNOWN_UPLOAD_CARD_SOURCE") !=
              std::string::npos);
}

TEST_F(PaymentsClientTest, GetUploadDetailsVariationsTest) {
  // Register a trial and variation id, so that there is data in variations
  // headers. Also, the variations header provider may have been registered to
  // observe some other field trial list, so reset it.
  variations::VariationsHttpHeaderProvider::GetInstance()->ResetForTesting();
  CreateFieldTrialWithId("AutofillTest", "Group", 369);
  StartGettingUploadDetails();

  // Note that experiment information is stored in X-Client-Data.
  EXPECT_TRUE(HasVariationsHeader());

  variations::VariationsHttpHeaderProvider::GetInstance()->ResetForTesting();
}

TEST_F(PaymentsClientTest, GetDetailsIncludeBillableServiceNumber) {
  StartGettingUploadDetails();

  // Verify that billable service number was included in the request.
  EXPECT_TRUE(GetUploadData().find("\"billable_service\":12345") !=
              std::string::npos);
}

TEST_F(PaymentsClientTest, GetDetailsFollowedByUploadSuccess) {
  StartGettingUploadDetails();
  ReturnResponse(
      net::HTTP_OK,
      "{ \"context_token\": \"some_token\", \"legal_message\": {} }");
  EXPECT_EQ(AutofillClient::SUCCESS, result_);

  result_ = AutofillClient::NONE;

  StartUploading(/*include_cvc=*/true);
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{}");
  EXPECT_EQ(AutofillClient::SUCCESS, result_);
}

TEST_F(PaymentsClientTest, GetDetailsFollowedByMigrationSuccess) {
  StartGettingUploadDetails();
  ReturnResponse(
      net::HTTP_OK,
      "{ \"context_token\": \"some_token\", \"legal_message\": {} }");
  EXPECT_EQ(AutofillClient::SUCCESS, result_);

  result_ = AutofillClient::NONE;

  StartMigrating(/*has_cardholder_name=*/true);
  IssueOAuthToken();
  ReturnResponse(
      net::HTTP_OK,
      "{\"save_result\":[],\"value_prop_display_text\":\"display text\"}");
  EXPECT_EQ(AutofillClient::SUCCESS, result_);
}

TEST_F(PaymentsClientTest, GetDetailsMissingContextToken) {
  StartGettingUploadDetails();
  ReturnResponse(net::HTTP_OK, "{ \"legal_message\": {} }");
  EXPECT_EQ(AutofillClient::PERMANENT_FAILURE, result_);
}

TEST_F(PaymentsClientTest, GetDetailsMissingLegalMessage) {
  StartGettingUploadDetails();
  ReturnResponse(net::HTTP_OK, "{ \"context_token\": \"some_token\" }");
  EXPECT_EQ(AutofillClient::PERMANENT_FAILURE, result_);
  EXPECT_EQ(nullptr, legal_message_.get());
}

TEST_F(PaymentsClientTest, SupportedCardBinRangesParsesCorrectly) {
  StartGettingUploadDetails();
  ReturnResponse(
      net::HTTP_OK,
      "{"
      "  \"context_token\" : \"some_token\","
      "  \"legal_message\" : {},"
      "  \"supported_card_bin_ranges_string\" : \"1234,300000-555555,765\""
      "}");
  EXPECT_EQ(AutofillClient::SUCCESS, result_);
  // Check that |supported_card_bin_ranges_| has the two entries specified in
  // ReturnResponse(~) above.
  ASSERT_EQ(3U, supported_card_bin_ranges_.size());
  EXPECT_EQ(1234, supported_card_bin_ranges_[0].first);
  EXPECT_EQ(1234, supported_card_bin_ranges_[0].second);
  EXPECT_EQ(300000, supported_card_bin_ranges_[1].first);
  EXPECT_EQ(555555, supported_card_bin_ranges_[1].second);
  EXPECT_EQ(765, supported_card_bin_ranges_[2].first);
  EXPECT_EQ(765, supported_card_bin_ranges_[2].second);
}

TEST_F(PaymentsClientTest, GetUploadAccountFromSyncTest) {
  EnableAutofillGetPaymentsIdentityFromSync();
  // Set up a different account.
  const AccountInfo& secondary_account_info =
      identity_test_env_.MakeAccountAvailable("secondary@gmail.com");
  test_personal_data_.SetAccountInfoForPayments(secondary_account_info);

  StartUploading(/*include_cvc=*/true);
  ReturnResponse(net::HTTP_OK, "{}");

  // Issue a token for the secondary account.
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      secondary_account_info.account_id, "secondary_account_token",
      AutofillClock::Now() + base::TimeDelta::FromDays(10));

  // Verify the auth header.
  std::string auth_header_value;
  EXPECT_TRUE(intercepted_headers_.GetHeader(
      net::HttpRequestHeaders::kAuthorization, &auth_header_value))
      << intercepted_headers_.ToString();
  EXPECT_EQ("Bearer secondary_account_token", auth_header_value);
}

TEST_F(PaymentsClientTest, UploadCardVariationsTest) {
  // Register a trial and variation id, so that there is data in variations
  // headers. Also, the variations header provider may have been registered to
  // observe some other field trial list, so reset it.
  variations::VariationsHttpHeaderProvider::GetInstance()->ResetForTesting();
  CreateFieldTrialWithId("AutofillTest", "Group", 369);
  StartUploading(/*include_cvc=*/true);
  IssueOAuthToken();

  // Note that experiment information is stored in X-Client-Data.
  EXPECT_TRUE(HasVariationsHeader());

  variations::VariationsHttpHeaderProvider::GetInstance()->ResetForTesting();
}

TEST_F(PaymentsClientTest, UnmaskCardVariationsTest) {
  // Register a trial and variation id, so that there is data in variations
  // headers. Also, the variations header provider may have been registered to
  // observe some other field trial list, so reset it.
  variations::VariationsHttpHeaderProvider::GetInstance()->ResetForTesting();
  CreateFieldTrialWithId("AutofillTest", "Group", 369);
  StartUnmasking(CardUnmaskOptions());
  IssueOAuthToken();

  // Note that experiment information is stored in X-Client-Data.
  EXPECT_TRUE(HasVariationsHeader());

  variations::VariationsHttpHeaderProvider::GetInstance()->ResetForTesting();
}

TEST_F(PaymentsClientTest, MigrateCardsVariationsTest) {
  // Register a trial and variation id, so that there is data in variations
  // headers. Also, the variations header provider may have been registered to
  // observe some other field trial list, so reset it.
  variations::VariationsHttpHeaderProvider::GetInstance()->ResetForTesting();
  CreateFieldTrialWithId("AutofillTest", "Group", 369);
  StartMigrating(/*has_cardholder_name=*/true);
  IssueOAuthToken();

  // Note that experiment information is stored in X-Client-Data.
  EXPECT_TRUE(HasVariationsHeader());

  variations::VariationsHttpHeaderProvider::GetInstance()->ResetForTesting();
}

TEST_F(PaymentsClientTest, UploadSuccessWithoutServerId) {
  StartUploading(/*include_cvc=*/true);
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{}");
  EXPECT_EQ(AutofillClient::SUCCESS, result_);
  EXPECT_TRUE(server_id_.empty());
}

TEST_F(PaymentsClientTest, UploadSuccessWithServerId) {
  StartUploading(/*include_cvc=*/true);
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{ \"credit_card_id\": \"InstrumentData:1\" }");
  EXPECT_EQ(AutofillClient::SUCCESS, result_);
  EXPECT_EQ("InstrumentData:1", server_id_);
}

TEST_F(PaymentsClientTest, UploadIncludesNonLocationData) {
  StartUploading(/*include_cvc=*/true);
  IssueOAuthToken();

  // Verify that the recipient name field and test names do appear in the upload
  // data.
  EXPECT_TRUE(GetUploadData().find(PaymentsClient::kRecipientName) !=
              std::string::npos);
  EXPECT_TRUE(GetUploadData().find("John") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("Smith") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("Pat") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("Jones") != std::string::npos);

  // Verify that the phone number field and test numbers do appear in the upload
  // data.
  EXPECT_TRUE(GetUploadData().find(PaymentsClient::kPhoneNumber) !=
              std::string::npos);
  EXPECT_TRUE(GetUploadData().find("212") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("555") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("0162") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("834") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("0090") != std::string::npos);
}

TEST_F(PaymentsClientTest,
       UploadRequestIncludesBillingCustomerNumberInRequest) {
  StartUploading(/*include_cvc=*/true);
  IssueOAuthToken();

  // Verify that the billing customer number is included in the request.
  EXPECT_TRUE(
      GetUploadData().find("%22external_customer_id%22:%22111222333444%22") !=
      std::string::npos);
}

TEST_F(PaymentsClientTest, UploadIncludesCvcInRequestIfProvided) {
  StartUploading(/*include_cvc=*/true);
  IssueOAuthToken();

  // Verify that the encrypted_cvc and s7e_13_cvc parameters were included in
  // the request.
  EXPECT_TRUE(GetUploadData().find("encrypted_cvc") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("__param:s7e_13_cvc") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("&s7e_13_cvc=") != std::string::npos);
}

TEST_F(PaymentsClientTest, UploadIncludesChromeUserContext) {
  scoped_feature_list_.InitWithFeatures(
      {features::kAutofillGetPaymentsIdentityFromSync},  // Enabled
      {features::kAutofillEnableAccountWalletStorage});  // Disabled

  StartUploading(/*include_cvc=*/true);
  IssueOAuthToken();

  // ChromeUserContext was set.
  EXPECT_TRUE(GetUploadData().find("chrome_user_context") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("full_sync_enabled") != std::string::npos);
}

TEST_F(PaymentsClientTest,
       UploadIncludesChromeUserContextIfWalletStorageFlagEnabled) {
  scoped_feature_list_.InitWithFeatures(
      {features::kAutofillEnableAccountWalletStorage},    // Enabled
      {features::kAutofillGetPaymentsIdentityFromSync});  // Disabled

  StartUploading(/*include_cvc=*/true);
  IssueOAuthToken();

  // ChromeUserContext was set.
  EXPECT_TRUE(GetUploadData().find("chrome_user_context") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("full_sync_enabled") != std::string::npos);
}

TEST_F(PaymentsClientTest, UploadExcludesChromeUserContextIfExperimentsOff) {
  scoped_feature_list_.InitWithFeatures(
      {},  // Enabled
      {features::kAutofillEnableAccountWalletStorage,
       features::kAutofillGetPaymentsIdentityFromSync});  // Disabled

  StartUploading(/*include_cvc=*/true);
  IssueOAuthToken();

  // ChromeUserContext was not set.
  EXPECT_TRUE(!GetUploadData().empty());
  EXPECT_TRUE(GetUploadData().find("chrome_user_context") == std::string::npos);
  EXPECT_TRUE(GetUploadData().find("full_sync_enabled") == std::string::npos);
}

TEST_F(PaymentsClientTest, UploadDoesNotIncludeCvcInRequestIfNotProvided) {
  StartUploading(/*include_cvc=*/false);
  IssueOAuthToken();

  EXPECT_TRUE(!GetUploadData().empty());
  // Verify that the encrypted_cvc and s7e_13_cvc parameters were not included
  // in the request.
  EXPECT_TRUE(GetUploadData().find("encrypted_cvc") == std::string::npos);
  EXPECT_TRUE(GetUploadData().find("__param:s7e_13_cvc") == std::string::npos);
  EXPECT_TRUE(GetUploadData().find("&s7e_13_cvc=") == std::string::npos);
}

TEST_F(PaymentsClientTest, MigrationRequestIncludesUniqueId) {
  StartMigrating(/*has_cardholder_name=*/true);
  IssueOAuthToken();

  // Verify that the unique id was included in the request.
  EXPECT_TRUE(GetUploadData().find("unique_id") != std::string::npos);
  EXPECT_TRUE(
      GetUploadData().find(migratable_credit_cards_[0].credit_card().guid()) !=
      std::string::npos);
  EXPECT_TRUE(
      GetUploadData().find(migratable_credit_cards_[1].credit_card().guid()) !=
      std::string::npos);
}

TEST_F(PaymentsClientTest, MigrationRequestIncludesEncryptedPan) {
  StartMigrating(/*has_cardholder_name=*/true);
  IssueOAuthToken();

  // Verify that the encrypted_pan and s7e_1_pan parameters were included
  // in the request.
  EXPECT_TRUE(GetUploadData().find("encrypted_pan") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("__param:s7e_1_pan0") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("&s7e_1_pan0=4111111111111111") !=
              std::string::npos);
  EXPECT_TRUE(GetUploadData().find("__param:s7e_1_pan1") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("&s7e_1_pan1=378282246310005") !=
              std::string::npos);
}

TEST_F(PaymentsClientTest, MigrationRequestIncludesCardholderNameWhenItExists) {
  StartMigrating(/*has_cardholder_name=*/true);
  IssueOAuthToken();

  EXPECT_TRUE(!GetUploadData().empty());
  // Verify that the cardholder name is sent if it exists.
  EXPECT_TRUE(GetUploadData().find("cardholder_name") != std::string::npos);
}

TEST_F(PaymentsClientTest,
       MigrationRequestExcludesCardholderNameWhenItDoesNotExist) {
  StartMigrating(/*has_cardholder_name=*/false);
  IssueOAuthToken();

  EXPECT_TRUE(!GetUploadData().empty());
  // Verify that the cardholder name is not sent if it doesn't exist.
  EXPECT_TRUE(GetUploadData().find("cardholder_name") == std::string::npos);
}

TEST_F(PaymentsClientTest, MigrationRequestIncludesChromeUserContext) {
  scoped_feature_list_.InitWithFeatures(
      {features::kAutofillGetPaymentsIdentityFromSync},  // Enabled
      {features::kAutofillEnableAccountWalletStorage});  // Disabled

  StartMigrating(/*has_cardholder_name=*/true);
  IssueOAuthToken();

  // ChromeUserContext was set.
  EXPECT_TRUE(GetUploadData().find("chrome_user_context") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("full_sync_enabled") != std::string::npos);
}

TEST_F(PaymentsClientTest,
       MigrationRequestIncludesChromeUserContextIfWalletStorageFlagEnabled) {
  scoped_feature_list_.InitWithFeatures(
      {features::kAutofillEnableAccountWalletStorage},    // Enabled
      {features::kAutofillGetPaymentsIdentityFromSync});  // Disabled

  StartMigrating(/*has_cardholder_name=*/true);
  IssueOAuthToken();

  // ChromeUserContext was set.
  EXPECT_TRUE(GetUploadData().find("chrome_user_context") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("full_sync_enabled") != std::string::npos);
}

TEST_F(PaymentsClientTest,
       MigrationRequestExcludesChromeUserContextIfExperimentsOff) {
  scoped_feature_list_.InitWithFeatures(
      {},  // Enabled
      {features::kAutofillEnableAccountWalletStorage,
       features::kAutofillGetPaymentsIdentityFromSync});  // Disabled

  StartMigrating(/*has_cardholder_name=*/true);
  IssueOAuthToken();

  // ChromeUserContext was not set.
  EXPECT_TRUE(!GetUploadData().empty());
  EXPECT_TRUE(GetUploadData().find("chrome_user_context") == std::string::npos);
  EXPECT_TRUE(GetUploadData().find("full_sync_enabled") == std::string::npos);
}

TEST_F(PaymentsClientTest, MigrationSuccessWithSaveResult) {
  StartMigrating(/*has_cardholder_name=*/true);
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK,
                 "{\"save_result\":[{\"unique_id\":\"0\",\"status\":"
                 "\"SUCCESS\"},{\"unique_id\":\"1\",\"status\":\"TEMPORARY_"
                 "FAILURE\"}],\"value_prop_display_text\":\"display text\"}");

  EXPECT_EQ(AutofillClient::SUCCESS, result_);
  EXPECT_TRUE(migration_save_results_.get());
  EXPECT_TRUE(migration_save_results_->find("0") !=
              migration_save_results_->end());
  EXPECT_TRUE(migration_save_results_->at("0") == "SUCCESS");
  EXPECT_TRUE(migration_save_results_->find("1") !=
              migration_save_results_->end());
  EXPECT_TRUE(migration_save_results_->at("1") == "TEMPORARY_FAILURE");
}

TEST_F(PaymentsClientTest, MigrationMissingSaveResult) {
  StartMigrating(/*has_cardholder_name=*/true);
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK,
                 "{\"value_prop_display_text\":\"display text\"}");
  EXPECT_EQ(AutofillClient::PERMANENT_FAILURE, result_);
  EXPECT_EQ(nullptr, migration_save_results_.get());
}

TEST_F(PaymentsClientTest, MigrationSuccessWithDisplayText) {
  StartMigrating(/*has_cardholder_name=*/true);
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK,
                 "{\"save_result\":[{\"unique_id\":\"0\",\"status\":"
                 "\"SUCCESS\"}],\"value_prop_display_text\":\"display text\"}");
  EXPECT_EQ(AutofillClient::SUCCESS, result_);
  EXPECT_EQ("display text", display_text_);
}

TEST_F(PaymentsClientTest, UnmaskMissingPan) {
  StartUnmasking(CardUnmaskOptions());
  ReturnResponse(net::HTTP_OK, "{}");
  EXPECT_EQ(AutofillClient::PERMANENT_FAILURE, result_);
}

TEST_F(PaymentsClientTest, RetryFailure) {
  StartUnmasking(CardUnmaskOptions());
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{ \"error\": { \"code\": \"INTERNAL\" } }");
  EXPECT_EQ(AutofillClient::TRY_AGAIN_FAILURE, result_);
  EXPECT_EQ("", unmask_response_details_->real_pan);
}

TEST_F(PaymentsClientTest, PermanentFailure) {
  StartUnmasking(CardUnmaskOptions());
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK,
                 "{ \"error\": { \"code\": \"ANYTHING_ELSE\" } }");
  EXPECT_EQ(AutofillClient::PERMANENT_FAILURE, result_);
  EXPECT_EQ("", unmask_response_details_->real_pan);
}

TEST_F(PaymentsClientTest, MalformedResponse) {
  StartUnmasking(CardUnmaskOptions());
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{ \"error_code\": \"WRONG_JSON_FORMAT\" }");
  EXPECT_EQ(AutofillClient::PERMANENT_FAILURE, result_);
  EXPECT_EQ("", unmask_response_details_->real_pan);
}

TEST_F(PaymentsClientTest, ReauthNeeded) {
  {
    StartUnmasking(CardUnmaskOptions());
    IssueOAuthToken();
    ReturnResponse(net::HTTP_UNAUTHORIZED, "");
    // No response yet.
    EXPECT_EQ(AutofillClient::NONE, result_);
    EXPECT_EQ(nullptr, unmask_response_details_);

    // Second HTTP_UNAUTHORIZED causes permanent failure.
    IssueOAuthToken();
    ReturnResponse(net::HTTP_UNAUTHORIZED, "");
    EXPECT_EQ(AutofillClient::PERMANENT_FAILURE, result_);
    EXPECT_EQ("", unmask_response_details_->real_pan);
  }

  result_ = AutofillClient::NONE;
  unmask_response_details_ = nullptr;

  {
    StartUnmasking(CardUnmaskOptions());
    // NOTE: Don't issue an access token here: the issuing of an access token
    // first waits for the access token request to be received, but here there
    // should be no access token request because PaymentsClient should reuse the
    // access token from the previous request.
    ReturnResponse(net::HTTP_UNAUTHORIZED, "");
    // No response yet.
    EXPECT_EQ(AutofillClient::NONE, result_);
    EXPECT_EQ(nullptr, unmask_response_details_);

    // HTTP_OK after first HTTP_UNAUTHORIZED results in success.
    IssueOAuthToken();
    ReturnResponse(net::HTTP_OK, "{ \"pan\": \"1234\" }");
    EXPECT_EQ(AutofillClient::SUCCESS, result_);
    EXPECT_EQ("1234", unmask_response_details_->real_pan);
  }
}

TEST_F(PaymentsClientTest, NetworkError) {
  StartUnmasking(CardUnmaskOptions());
  IssueOAuthToken();
  ReturnResponse(net::HTTP_REQUEST_TIMEOUT, std::string());
  EXPECT_EQ(AutofillClient::NETWORK_ERROR, result_);
  EXPECT_EQ("", unmask_response_details_->real_pan);
}

TEST_F(PaymentsClientTest, OtherError) {
  StartUnmasking(CardUnmaskOptions());
  IssueOAuthToken();
  ReturnResponse(net::HTTP_FORBIDDEN, std::string());
  EXPECT_EQ(AutofillClient::PERMANENT_FAILURE, result_);
  EXPECT_EQ("", unmask_response_details_->real_pan);
}

}  // namespace payments
}  // namespace autofill
