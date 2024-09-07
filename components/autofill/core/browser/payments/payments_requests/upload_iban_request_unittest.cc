// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/upload_iban_request.h"

#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {
namespace {

constexpr char kAppLocale[] = "pt-BR";
constexpr int kBillableServiceNumber = 12345678;
constexpr int64_t kBillingCustomerNumber = 111222333;
constexpr char16_t kContextToken[] = u"somecontexttoken";
constexpr char16_t kValue[] = u"CH5604835012345678009";
constexpr char16_t kNickname[] = u"My IBAN";

class UploadIbanRequestTest : public testing::Test {
 public:
  void SetUp() override {
    PaymentsNetworkInterface::UploadIbanRequestDetails request_details;
    request_details.app_locale = kAppLocale;
    request_details.billable_service_number = kBillableServiceNumber;
    request_details.billing_customer_number = kBillingCustomerNumber;
    request_details.context_token = kContextToken;
    request_details.value = kValue;
    request_details.nickname = kNickname;
    request_ = std::make_unique<UploadIbanRequest>(
        request_details, /*full_sync_enabled=*/true, base::DoNothing());
  }

  UploadIbanRequest& GetRequest() { return *request_.get(); }

 private:
  std::unique_ptr<UploadIbanRequest> request_;
};

TEST_F(UploadIbanRequestTest, GetRequestContent) {
  EXPECT_EQ(GetRequest().GetRequestUrlPath(),
            "payments/apis-secure/chromepaymentsservice/createpaymentinstrument"
            "?s7e_suffix=chromewallet");
  ASSERT_TRUE(!GetRequest().GetRequestContent().empty());
  EXPECT_NE(GetRequest().GetRequestContent().find("language_code"),
            std::string::npos);
  EXPECT_NE(GetRequest().GetRequestContent().find(kAppLocale),
            std::string::npos);
  EXPECT_NE(GetRequest().GetRequestContent().find("billable_service"),
            std::string::npos);
  EXPECT_NE(GetRequest().GetRequestContent().find(
                base::NumberToString(kBillableServiceNumber)),
            std::string::npos);
  EXPECT_NE(GetRequest().GetRequestContent().find("customer_context"),
            std::string::npos);
  EXPECT_NE(GetRequest().GetRequestContent().find("external_customer_id"),
            std::string::npos);
  EXPECT_NE(GetRequest().GetRequestContent().find(
                base::NumberToString(kBillingCustomerNumber)),
            std::string::npos);
  EXPECT_NE(GetRequest().GetRequestContent().find("context_token"),
            std::string::npos);
  EXPECT_NE(
      GetRequest().GetRequestContent().find(base::UTF16ToUTF8(kContextToken)),
      std::string::npos);
  EXPECT_NE(GetRequest().GetRequestContent().find("chrome_user_context"),
            std::string::npos);
  EXPECT_NE(GetRequest().GetRequestContent().find("__param:s7e_443_value"),
            std::string::npos);
  EXPECT_NE(GetRequest().GetRequestContent().find(base::UTF16ToUTF8(kValue)),
            std::string::npos);
  EXPECT_NE(GetRequest().GetRequestContent().find("nickname"),
            std::string::npos);
}

}  // namespace
}  // namespace autofill::payments
