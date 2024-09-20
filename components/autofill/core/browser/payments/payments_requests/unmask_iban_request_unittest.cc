// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/unmask_iban_request.h"

#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {
namespace {

constexpr int kBillableServiceNumber = 12345678;
constexpr int64_t kBillingCustomerNumber = 111222333;
constexpr int64_t kInstrumentId = 1122334455;

class UnmaskIbanRequestTest : public testing::Test {
 public:
  void SetUp() override {
    PaymentsNetworkInterface::UnmaskIbanRequestDetails request_details;
    request_details.billable_service_number = kBillableServiceNumber;
    request_details.billing_customer_number = kBillingCustomerNumber;
    request_details.instrument_id = kInstrumentId;
    request_ = std::make_unique<UnmaskIbanRequest>(
        request_details, /*full_sync_enabled=*/true,
        /*callback=*/base::DoNothing());
  }

  UnmaskIbanRequest* GetRequest() { return request_.get(); }

  void ParseResponse(const base::Value::Dict& response) {
    request_->ParseResponse(response);
  }

  bool IsResponseComplete() const { return request_->IsResponseComplete(); }

  std::u16string value() const { return request_->value_for_testing(); }

 private:
  std::unique_ptr<UnmaskIbanRequest> request_;
};

TEST_F(UnmaskIbanRequestTest, GetRequestContent) {
  EXPECT_EQ(GetRequest()->GetRequestUrlPath(),
            "payments/apis-secure/chromepaymentsservice/"
            "getpaymentinstrument?s7e_suffix=chromewallet");
  ASSERT_FALSE(GetRequest()->GetRequestContent().empty());
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
  EXPECT_NE(GetRequest()->GetRequestContent().find("instrument_id"),
            std::string::npos);
  // iban_info must always be set, even if blank, so that the Payments server
  // knows this is an UnmaskIbanRequest.
  EXPECT_NE(GetRequest()->GetRequestContent().find("iban_info"),
            std::string::npos);
  EXPECT_NE(GetRequest()->GetRequestContent().find(
                base::NumberToString(kInstrumentId)),
            std::string::npos);
}

TEST_F(UnmaskIbanRequestTest, ParseResponse_ResponseIsComplete) {
  base::Value::Dict response = base::Value::Dict().Set(
      "iban_info", base::Value::Dict().Set("value", u"DE75512108001245126199"));

  ParseResponse(response);

  EXPECT_EQ(value(), u"DE75512108001245126199");
}

TEST_F(UnmaskIbanRequestTest, ParseResponse_MissingValue) {
  base::Value::Dict response;

  ParseResponse(response);

  EXPECT_FALSE(IsResponseComplete());
}

}  // namespace
}  // namespace autofill::payments
