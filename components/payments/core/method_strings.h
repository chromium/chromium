// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_METHOD_STRINGS_H_
#define COMPONENTS_PAYMENTS_CORE_METHOD_STRINGS_H_

namespace payments {
namespace methods {

// Non-translatable payment method identifier strings.

// These strings are referenced from both C++ and Java (through the
// auto-generated file MethodStrings.java).

// Please keep the list alphabetized.

// Android Pay method name.
extern const char kAndroidPay[];

// Basic Card method name. https://w3c.github.io/payment-method-basic-card/
extern const char kBasicCard[];

// Google Pay method name.
// https://developers.google.com/pay/api/web/guides/tutorial
extern const char kGooglePay[];

// Interledger method name.
// https://w3c.github.io/webpayments/proposals/interledger/
extern const char kInterledger[];

// Credit Transfer method name.
// https://w3c.github.io/payment-method-credit-transfer/
extern const char kPayeeCreditTransfer[];

// Credit Transfer method name.
// https://w3c.github.io/payment-method-credit-transfer/
extern const char kPayerCreditTransfer[];

// Tokenized Card method name.
// https://w3c.github.io/webpayments-methods-tokenization/
extern const char kTokenizedCard[];

}  // namespace methods
}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_METHOD_STRINGS_H_
