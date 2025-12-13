// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLET_CORE_COMMON_WALLET_PREFS_H_
#define COMPONENTS_WALLET_CORE_COMMON_WALLET_PREFS_H_

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace wallet::prefs {

// Dictionary pref that stores the opt-in status for walletable pass detection.
inline constexpr char kWalletablePassDetectionOptInStatus[] =
    "wallet.walletable_pass_detection_opt_in_status";

// Registers the preferences for the Wallet component. These preferences are
// not synced.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace wallet::prefs

#endif  // COMPONENTS_WALLET_CORE_COMMON_WALLET_PREFS_H_
