// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_PAYMENT_PREFS_H_
#define COMPONENTS_PAYMENTS_CORE_PAYMENT_PREFS_H_

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace payments {

// True if the profile has already successfully completed at least one payment
// request transaction.
extern const char kPaymentsFirstTransactionCompleted[];

// True if the user has allowed canMakePayment to return a truthful value, false
// if canMakePayment should always return false regardless.
extern const char kCanMakePaymentEnabled[];

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_PAYMENT_PREFS_H_
