// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_INFO_CORE_PREF_NAMES_H_
#define COMPONENTS_PAGE_INFO_CORE_PREF_NAMES_H_

namespace prefs {

// Un-synced time pref indicating when the user interacted with Merchant Trust
// UI for a minimum required time the last time.
inline constexpr char kMerchantTrustUiLastInteractionTime[] =
    "merchant_trust.ui.last_interaction_time";

// Un-synced time pref indicating when the user opened Page Info with Merchant
// Trust information available (but not shown).
inline constexpr char kMerchantTrustPageInfoLastOpenTime[] =
    "merchant_trust.page_info.last_open_time";

}  // namespace prefs

#endif  // COMPONENTS_PAGE_INFO_CORE_PREF_NAMES_H_
