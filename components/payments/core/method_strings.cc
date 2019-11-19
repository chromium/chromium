// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/method_strings.h"

namespace payments {
namespace methods {

// Please keep the list alphabetized.
// Each string must be on a single line to correctly generate
// MethodStrings.java.

const char kAndroidPay[] = "https://android.com/pay";
const char kBasicCard[] = "basic-card";
const char kGooglePay[] = "https://google.com/pay";
const char kInterledger[] = "interledger";
const char kPayeeCreditTransfer[] = "payee-credit-transfer";
const char kPayerCreditTransfer[] = "payer-credit-transfer";
const char kTokenizedCard[] = "tokenized-card";

}  // namespace methods
}  // namespace payments
