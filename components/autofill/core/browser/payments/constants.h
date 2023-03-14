// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CONSTANTS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CONSTANTS_H_

namespace autofill {

// Contains constants specific to the payments Autofill used in the browser
// process.

// Credit card issuer ids. These are server-generated values that must be
// consistent between server and client.
constexpr char kAmexCardIssuerId[] = "amex";
constexpr char kCapitalOneCardIssuerId[] = "capitalone";
constexpr char kChaseCardIssuerId[] = "chase";
constexpr char kMarqetaCardIssuerId[] = "marqeta";

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CONSTANTS_H_
