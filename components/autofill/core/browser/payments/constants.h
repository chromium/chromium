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
inline constexpr std::string_view kAmexCardIssuerId = "amex";
inline constexpr std::string_view kAnzCardIssuerId = "anz";
inline constexpr std::string_view kCapitalOneCardIssuerId = "capitalone";
inline constexpr std::string_view kChaseCardIssuerId = "chase";
inline constexpr std::string_view kCitiCardIssuerId = "citi";
inline constexpr std::string_view kDiscoverCardIssuerId = "discover";
inline constexpr std::string_view kLloydsCardIssuerId = "lloyds";
inline constexpr std::string_view kMarqetaCardIssuerId = "marqeta";
inline constexpr std::string_view kNabCardIssuerId = "nab";
inline constexpr std::string_view kNatwestCardIssuerId = "natwest";

// The urls to the static card art images used by Capital One cards.
inline constexpr std::string_view kCapitalOneCardArtUrl =
    "https://www.gstatic.com/autofill/virtualcard/icon/capitalone.png";
inline constexpr std::string_view kCapitalOneLargeCardArtUrl =
    "https://www.gstatic.com/autofill/virtualcard/icon/capitalone_40_24.png";

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CONSTANTS_H_
