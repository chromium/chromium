// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_service_url.h"

#include "base/command_line.h"
#include "base/test/gtest_util.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace autofill {
namespace payments {

using IssuerId = autofill::BnplIssuer::IssuerId;

TEST(PaymentsServiceSandboxUrl, CheckSandboxUrls) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kWalletServiceUseSandbox, "1");

  const char kExpectedURL[] =
      "https://wallet-web.sandbox.google.com/wallet?"
      "p=paymentmethods&utm_source=chrome&utm_medium=settings&utm_campaign="
      "paymentmethods";

  EXPECT_EQ(kExpectedURL, GetManageInstrumentsUrl().spec());
  EXPECT_EQ(kExpectedURL, GetManageAddressesUrl().spec());
}

TEST(PaymentsServiceSandboxUrl, CheckProdUrls) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kWalletServiceUseSandbox, "0");

  const char kExpectedURL[] =
      "https://wallet.google.com/wallet?"
      "p=paymentmethods&utm_source=chrome&utm_medium=settings&utm_campaign="
      "paymentmethods";

  EXPECT_EQ(kExpectedURL, GetManageInstrumentsUrl().spec());
  EXPECT_EQ(kExpectedURL, GetManageAddressesUrl().spec());
}

TEST(PaymentsServiceUrl, UrlWithInstrumentId) {
  const char kExpectedURL[] =
      "https://wallet.google.com/wallet?"
      "p=paymentmethods&utm_source=chrome&utm_medium=settings&utm_campaign="
      "paymentmethods&id=123";

  EXPECT_EQ(kExpectedURL, GetManageInstrumentUrl(/*instrument_id=*/123).spec());
}

TEST(PaymentsServiceUrl, BnplTermsUrl) {
  const char kExpectedURL[] =
      "https://support.google.com/googlepay?p=bnpl_autofill_chrome";

  EXPECT_EQ(kExpectedURL, GetBnplTermsUrl(IssuerId::kBnplAffirm));
  EXPECT_EQ(kExpectedURL, GetBnplTermsUrl(IssuerId::kBnplZip));
  EXPECT_EQ(kExpectedURL, GetBnplTermsUrl(IssuerId::kBnplKlarna));
  EXPECT_NOTREACHED_DEATH(GetBnplTermsUrl(IssuerId::kBnplAfterpay));
}

}  // namespace payments
}  // namespace autofill
