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

  result_ = AutofillClient::PaymentsRpcResult::kNone;
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
  std::string auth_header_value;
  EXPECT_TRUE(intercepted_headers_.GetHeader(
      net::HttpRequestHeaders::kAuthorization, &auth_header_value))
      << intercepted_headers_.ToString();
  EXPECT_EQ("Bearer totally_real_token", auth_header_value);
}

void PaymentsNetworkInterfaceTestBase::ReturnResponse(
    PaymentsNetworkInterfaceBase* payments_network_interface_base,
    net::HttpStatusCode response_code,
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

std::vector<AutofillProfile>
PaymentsNetworkInterfaceTestBase::BuildTestProfiles() {
  std::vector<AutofillProfile> profiles;
  profiles.push_back(BuildProfile("John", "Smith", "1234 Main St.", "Miami",
                                  "FL", "32006", "212-555-0162"));
  profiles.push_back(BuildProfile("Pat", "Jones", "432 Oak Lane", "Lincoln",
                                  "OH", "43005", "(834)555-0090"));
  return profiles;
}

AutofillProfile PaymentsNetworkInterfaceTestBase::BuildProfile(
    std::string_view first_name,
    std::string_view last_name,
    std::string_view address_line,
    std::string_view city,
    std::string_view state,
    std::string_view zip,
    std::string_view phone_number) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);

  profile.SetInfo(NAME_FIRST, base::ASCIIToUTF16(first_name), "en-US");
  profile.SetInfo(NAME_LAST, base::ASCIIToUTF16(last_name), "en-US");
  profile.SetInfo(ADDRESS_HOME_LINE1, base::ASCIIToUTF16(address_line),
                  "en-US");
  profile.SetInfo(ADDRESS_HOME_CITY, base::ASCIIToUTF16(city), "en-US");
  profile.SetInfo(ADDRESS_HOME_STATE, base::ASCIIToUTF16(state), "en-US");
  profile.SetInfo(ADDRESS_HOME_ZIP, base::ASCIIToUTF16(zip), "en-US");
  profile.SetInfo(PHONE_HOME_WHOLE_NUMBER, base::ASCIIToUTF16(phone_number),
                  "en-US");
  profile.FinalizeAfterImport();
  return profile;
}

}  // namespace autofill::payments
