// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USER_EDUCATION_NTP_PROMO_IDENTIFIERS_H_
#define CHROME_BROWSER_USER_EDUCATION_NTP_PROMO_IDENTIFIERS_H_

// Unique identifiers for New Tab Page user education promos. These values are
// also used as keys in stored user prefs.

// LINT.IfChange(NtpPromoIdentifiers)
inline constexpr char kNtpCustomizationPromoId[] = "Customization";
inline constexpr char kNtpExtensionsPromoId[] = "Extensions";
inline constexpr char kNtpSignInPromoId[] = "SignIn";
// LINT.ThenChange(//tools/metrics/histograms/metadata/user_education/histograms.xml:NtpPromoIdentifiers)

#endif  // CHROME_BROWSER_USER_EDUCATION_NTP_PROMO_IDENTIFIERS_H_
