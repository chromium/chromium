// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_PRE_PURCHASE_QUERY_H_
#define COMPONENTS_PAYMENTS_CORE_PRE_PURCHASE_QUERY_H_

namespace payments {

// Enum used for recording a histogram about the type of pre-purchase query.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.payments
enum class PrePurchaseQuery {
  kOtherTypeOfQuery = 0,
  kServiceWorkerEvent = 1,
  kAndroidIntent = 2,
  kMaxValue = kAndroidIntent,
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_PRE_PURCHASE_QUERY_H_
