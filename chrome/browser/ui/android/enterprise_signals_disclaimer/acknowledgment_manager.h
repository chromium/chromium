// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_ENTERPRISE_SIGNALS_DISCLAIMER_ACKNOWLEDGMENT_MANAGER_H_
#define CHROME_BROWSER_UI_ANDROID_ENTERPRISE_SIGNALS_DISCLAIMER_ACKNOWLEDGMENT_MANAGER_H_

#include "base/containers/span.h"

class PrefRegistrySimple;
class PrefService;
class GaiaId;

// This logic is responsible for setting and getting the information about
// whether a given account has accepted the enterprise signals disclaimer. This
// information is stored in a local state preference as a set of hashed Gaia
// Ids.
namespace enterprise_signals_disclaimer {

// Name of the preference containing set of hashed GaiaIds for accounts which
// have acknowledged the enterprise signals disclaimer.
extern const char kAcknowledgmentSetPrefPath[];

// Marks the enterprise signals disclaimer as acknowledged for the account
// represented by `gaia_id`.
//
// `gaia_id` must not be empty.
void SetAccountAckedSignalsDisclaimer(PrefService& local_state,
                                      const GaiaId& gaia_id);

// Returns true if the enterprise signals disclaimer has been already
// acknowledged by the account represented by `gaia_id`.
//
// `gaia_id` must not be empty.
bool HasAccountAckedSignalsDisclaimer(const PrefService& local_state,
                                      const GaiaId& gaia_id);

// Syncs the acknowledgment set with the given list of accounts.
// If the ack set contains any accounts that are not in `accounts_on_device`,
// they will be removed.
//
// `accounts_on_device` must not contain empty GaiaIds.
void RemoveUnknownAccounts(PrefService& local_state,
                           const base::span<const GaiaId> accounts_on_device);

void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

}  // namespace enterprise_signals_disclaimer

#endif  // CHROME_BROWSER_UI_ANDROID_ENTERPRISE_SIGNALS_DISCLAIMER_ACKNOWLEDGMENT_MANAGER_H_
