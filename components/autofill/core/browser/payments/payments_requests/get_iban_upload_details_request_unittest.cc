// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/get_iban_upload_details_request.h"

#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {
namespace {

constexpr char kAppLocale[] = "dummy_locale";
constexpr char kCountryCode[] = "FR";
constexpr int kBillableServiceNumber = 12345678;
constexpr int64_t kBillingCustomerNumber = 111222333;
constexpr char16_t kCapitalizedIbanRegex[] =
    u"^[A-Z]{2}[0-9]{2}[A-Z0-9]{4}[0-9]{7}[A-Z0-9]{0,18}$";

class GetIbanUploadDetailsRequestTest : public testing::Test {
 public:
  void SetUp() override {
    request_ = std::make_unique<GetIbanUploadDetailsRequest>(
        /*full_sync_enabled=*/true, kAppLocale, kBillingCustomerNumber,
        kBillableServiceNumber, kCountryCode, base::DoNothing());
  }

  GetIbanUploadDetailsRequest* GetRequest() { return request_.get(); }

  void ParseResponse(const base::Value::Dict& response) {
    request_->ParseResponse(response);
  }

  bool IsResponseComplete() const { return request_->IsResponseComplete(); }

  std::u16string context_token() const {
    return request_->context_token_for_testing();
  }
  base::Value::Dict* legal_message() const {
    return request_->legal_message_for_testing();
  }

 private:
  std::unique_ptr<GetIbanUploadDetailsRequest> request_;
};

TEST_F(GetIbanUploadDetailsRequestTest,
       GetRequestContent_ContainsExpectedData) {
  EXPECT_EQ(GetRequest()->GetRequestUrlPath(),
            "payments/apis/chromepaymentsservice/"
            "getdetailsforcreatepaymentinstrument");
  EXPECT_FALSE(GetRequest()->GetRequestContent().empty());
  EXPECT_NE(GetRequest()->GetRequestContent().find("language_code"),
            std::string::npos);
  EXPECT_NE(GetRequest()->GetRequestContent().find(kAppLocale),
            std::string::npos);
  EXPECT_NE(GetRequest()->GetRequestContent().find("billable_service"),
            std::string::npos);
  EXPECT_NE(GetRequest()->GetRequestContent().find(
                base::NumberToString(kBillableServiceNumber)),
            std::string::npos);
  EXPECT_NE(GetRequest()->GetRequestContent().find("customer_context"),
            std::string::npos);
  EXPECT_NE(GetRequest()->GetRequestContent().find("external_customer_id"),
            std::string::npos);
  EXPECT_NE(GetRequest()->GetRequestContent().find(
                base::NumberToString(kBillingCustomerNumber)),
            std::string::npos);
  EXPECT_NE(GetRequest()->GetRequestContent().find("chrome_user_context"),
            std::string::npos);
  EXPECT_NE(GetRequest()->GetRequestContent().find("iban_region_code"),
            std::string::npos);
}

TEST_F(GetIbanUploadDetailsRequestTest, ParseResponse_ResponseIsComplete) {
  base::Value::Dict response =
      base::Value::Dict()
          .Set("iban_details", base::Value::Dict().Set("validation_regex",
                                                       kCapitalizedIbanRegex))
          .Set("context_token", base::Value(u"some token"))
          .Set("legal_message",
               base::Value::Dict().Set("terms_of_service", "Terms of Service"));

  ParseResponse(response);

  EXPECT_EQ(context_token(), u"some token");
  EXPECT_TRUE(legal_message());
  EXPECT_TRUE(IsResponseComplete());
}

TEST_F(GetIbanUploadDetailsRequestTest, ParseResponse_MissingContextToken) {
  base::Value::Dict response =
      base::Value::Dict()
          .Set("iban_details", base::Value::Dict().Set("validation_regex",
                                                       kCapitalizedIbanRegex))
          .Set("legal_message",
               base::Value::Dict().Set("terms_of_service", "Terms of Service"));

  ParseResponse(response);

  EXPECT_FALSE(IsResponseComplete());
}

TEST_F(GetIbanUploadDetailsRequestTest, ParseResponse_MissingLegalMessage) {
  base::Value::Dict response =
      base::Value::Dict()
          .Set("iban_details", base::Value::Dict().Set("validation_regex",
                                                       kCapitalizedIbanRegex))
          .Set("context_token", base::Value(u"some token"));

  ParseResponse(response);

  EXPECT_FALSE(IsResponseComplete());
}

TEST_F(GetIbanUploadDetailsRequestTest, ParseResponse_MissingValidationRegex) {
  base::Value::Dict response =
      base::Value::Dict()
          .Set("context_token", base::Value(u"some token"))
          .Set("legal_message",
               base::Value::Dict().Set("terms_of_service", "Terms of Service"));

  ParseResponse(response);

  EXPECT_FALSE(IsResponseComplete());
}

}  // namespace
}  // namespace autofill::payments
