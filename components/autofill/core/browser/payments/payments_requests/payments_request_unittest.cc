// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"

#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/payments/payments_requests/create_card_request.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request_test_api.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/version_info/version_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {
namespace {

class PaymentsRequestTest : public testing::Test {
 public:
  void SetUp() override {
    // PaymentsRequest is an abstract class and can't be created on its own.
    // To test its base functionality, here we arbitrarily pick one of its
    // subclasses to instantiate.
    payments_request_ = std::make_unique<CreateCardRequest>(
        UploadCardRequestDetails(), /*callback=*/base::DoNothing());
  }

  PaymentsRequest* GetPaymentsRequest() { return payments_request_.get(); }

 private:
  std::unique_ptr<PaymentsRequest> payments_request_;
};

TEST_F(PaymentsRequestTest, BuildChromeUserContext_ContainsClientType) {
  base::test::ScopedFeatureList feature_list(
      autofill::features::kAutofillAddChromeUserContextFields);

  base::DictValue chrome_user_context =
      test_api(GetPaymentsRequest()).BuildChromeUserContext();

  EXPECT_EQ(
      chrome_user_context.FindInt("client_type").value(),
      static_cast<int>(
          test_api(GetPaymentsRequest()).GetChromeUserContextClientType()));
}

TEST_F(PaymentsRequestTest,
       BuildChromeUserContext_DoesNotContainClientType_FlagOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      autofill::features::kAutofillAddChromeUserContextFields);

  base::DictValue chrome_user_context =
      test_api(GetPaymentsRequest()).BuildChromeUserContext();

  EXPECT_FALSE(chrome_user_context.FindInt("client_type").has_value());
}

TEST_F(PaymentsRequestTest, BuildChromeUserContext_ContainsMajorVersion) {
  base::test::ScopedFeatureList feature_list(
      autofill::features::kAutofillAddChromeUserContextFields);

  base::DictValue chrome_user_context =
      test_api(GetPaymentsRequest()).BuildChromeUserContext();

  EXPECT_EQ(chrome_user_context.FindInt("chrome_major_version").value(),
            version_info::GetMajorVersionNumberAsInt());
}

TEST_F(PaymentsRequestTest,
       BuildChromeUserContext_DoesNotContainMajorVersion_FlagOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      autofill::features::kAutofillAddChromeUserContextFields);

  base::DictValue chrome_user_context =
      test_api(GetPaymentsRequest()).BuildChromeUserContext();

  EXPECT_FALSE(chrome_user_context.FindInt("chrome_major_version").has_value());
}

}  // namespace
}  // namespace autofill::payments
