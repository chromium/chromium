// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_service_url.h"

#include "base/command_line.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace autofill {
namespace payments {

TEST(PaymentsServiceSandboxUrl, CheckSandboxUrls) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kWalletServiceUseSandbox, "1");

  const char kExpectedURL[] =
      "https://pay.sandbox.google.com/"
      "pay?p=paymentmethods&utm_source=chrome&utm_medium=settings&utm_campaign="
      "payment_methods";

  EXPECT_EQ(kExpectedURL, GetManageInstrumentsUrl().spec());
  EXPECT_EQ(kExpectedURL, GetManageAddressesUrl().spec());
}

TEST(PaymentsServiceSandboxUrl, CheckProdUrls) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kWalletServiceUseSandbox, "0");

  const char kExpectedURL[] =
      "https://pay.google.com/"
      "pay?p=paymentmethods&utm_source=chrome&utm_medium=settings&utm_campaign="
      "payment_methods";

  EXPECT_EQ(kExpectedURL, GetManageInstrumentsUrl().spec());
  EXPECT_EQ(kExpectedURL, GetManageAddressesUrl().spec());
}

TEST(PaymentsServiceUrl, UrlWithInstrumentId) {
  const char kExpectedURL[] =
      "https://pay.google.com/"
      "pay?p=paymentmethods&utm_source=chrome&utm_medium=settings&utm_campaign="
      "payment_methods&id=123";

  EXPECT_EQ(kExpectedURL, GetManageInstrumentUrl(/*instrument_id=*/123).spec());
}

}  // namespace payments
}  // namespace autofill
