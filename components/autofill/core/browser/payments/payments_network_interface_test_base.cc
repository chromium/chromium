// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/autofill/core/browser/payments/payments_network_interface_test_base.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/values.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::HasSubstr;

namespace autofill::payments {

PaymentsNetworkInterfaceTestBase::PaymentsNetworkInterfaceTestBase() = default;
PaymentsNetworkInterfaceTestBase::~PaymentsNetworkInterfaceTestBase() = default;

void PaymentsNetworkInterfaceTestBase::SetUpTest() {
  // Silence the warning for mismatching sync and Payments servers.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kWalletServiceUseSandbox, "0");

  result_ = PaymentsAutofillClient::PaymentsRpcResult::kNone;
  has_variations_header_ = false;

  factory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        intercepted_headers_ = request.headers;
        intercepted_body_ = network::GetUploadData(request);
        has_variations_header_ = variations::HasVariationsHeader(request);
      }));
  test_shared_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_);
  test_personal_data_.test_payments_data_manager().SetAccountInfoForPayments(
      identity_test_env_.MakePrimaryAccountAvailable(
          "example@gmail.com", signin::ConsentLevel::kSync));
}

void PaymentsNetworkInterfaceTestBase::CreateFieldTrialWithId(
    const std::string& trial_name,
    const std::string& group_name,
    int variation_id) {
  variations::AssociateGoogleVariationID(
      variations::GOOGLE_WEB_PROPERTIES_ANY_CONTEXT, trial_name, group_name,
      static_cast<variations::VariationID>(variation_id));
  base::FieldTrialList::CreateFieldTrial(trial_name, group_name)->Activate();
}

void PaymentsNetworkInterfaceTestBase::IssueOAuthToken() {
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "totally_real_token", AutofillClock::Now() + base::Days(10));

  // Verify the auth header.
  EXPECT_THAT(
      intercepted_headers_.GetHeader(net::HttpRequestHeaders::kAuthorization),
      testing::Optional(std::string("Bearer totally_real_token")))
      << intercepted_headers_.ToString();
}

void PaymentsNetworkInterfaceTestBase::ReturnResponse(
    PaymentsNetworkInterfaceBase* payments_network_interface_base,
    int response_code,
    const std::string& response_body) {
  payments_network_interface_base->OnSimpleLoaderCompleteInternal(
      response_code, response_body);
}

void PaymentsNetworkInterfaceTestBase::assertIncludedInRequest(
    std::string field_name_or_value) {
  EXPECT_TRUE(GetUploadData().find(field_name_or_value) != std::string::npos);
}

void PaymentsNetworkInterfaceTestBase::assertNotIncludedInRequest(
    std::string field_name_or_value) {
  EXPECT_TRUE(GetUploadData().find(field_name_or_value) == std::string::npos);
}

}  // namespace autofill::payments
