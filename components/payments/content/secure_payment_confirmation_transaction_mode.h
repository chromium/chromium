// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_TRANSACTION_MODE_H_
#define COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_TRANSACTION_MODE_H_

namespace payments {

// Represents the WebDriver automation 'modes' that the SPC UX can be put into.
// See https://w3c.github.io/secure-payment-confirmation/#sctn-automation
//
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.payments
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: SPCTransactionMode
enum class SPCTransactionMode {
  kNone,
  kAutoAccept,
  kAutoAuthAnotherWay,
  kAutoReject,
  kAutoOptOut,
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_TRANSACTION_MODE_H_
