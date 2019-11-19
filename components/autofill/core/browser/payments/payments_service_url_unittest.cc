// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "components/autofill/core/browser/payments/payments_service_url.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace autofill {
namespace payments {

TEST(PaymentsServiceSandboxUrl, CheckSandboxUrls) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kWalletServiceUseSandbox, "1");

  const char kExpectedSandboxURL[] =
      "https://pay.sandbox.google.com/payments/"
      "home?utm_source=chrome&utm_medium=settings&utm_campaign=payment-methods#"
      "paymentMethods";

  EXPECT_EQ(kExpectedSandboxURL, GetManageInstrumentsUrl().spec());
  EXPECT_EQ(kExpectedSandboxURL, GetManageAddressesUrl().spec());
}

TEST(PaymentsServiceSandboxUrl, CheckProdUrls) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kWalletServiceUseSandbox, "0");

  const char kExpectedURL[] =
      "https://pay.google.com/payments/"
      "home?utm_source=chrome&utm_medium=settings&utm_campaign=payment-methods#"
      "paymentMethods";

  EXPECT_EQ(kExpectedURL, GetManageInstrumentsUrl().spec());
  EXPECT_EQ(kExpectedURL, GetManageAddressesUrl().spec());
}

}  // namespace payments
}  // namespace autofill
