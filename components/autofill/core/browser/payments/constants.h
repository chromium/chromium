// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CONSTANTS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CONSTANTS_H_

namespace autofill {
//TODO(crbug.com/1500345) replace char[] with std::string_view

// Contains constants specific to the payments Autofill used in the browser
// process.

// Credit card issuer ids. These are server-generated values that must be
// consistent between server and client.
inline constexpr char kAmexCardIssuerId[] = "amex";
inline constexpr char kAnzCardIssuerId[] = "anz";
inline constexpr char kCapitalOneCardIssuerId[] = "capitalone";
inline constexpr char kChaseCardIssuerId[] = "chase";
inline constexpr char kCitiCardIssuerId[] = "citi";
inline constexpr char kDiscoverCardIssuerId[] = "discover";
inline constexpr char kLloydsCardIssuerId[] = "lloyds";
inline constexpr char kMarqetaCardIssuerId[] = "marqeta";
inline constexpr char kNabCardIssuerId[] = "nab";
inline constexpr char kNatwestCardIssuerId[] = "natwest";

// The urls to the static card art images used by Capital One cards.
inline constexpr char kCapitalOneCardArtUrl[] =
    "https://www.gstatic.com/autofill/virtualcard/icon/capitalone.png";
inline constexpr char kCapitalOneLargeCardArtUrl[] =
    "https://www.gstatic.com/autofill/virtualcard/icon/capitalone_40_24.png";

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CONSTANTS_H_
