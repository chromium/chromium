// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_ENTERPRISE_SIGNALS_DISCLAIMER_ACKNOWLEDGMENT_MANAGER_H_
#define CHROME_BROWSER_UI_ANDROID_ENTERPRISE_SIGNALS_DISCLAIMER_ACKNOWLEDGMENT_MANAGER_H_

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

// TODO(b/527872237): On startup, sync the ack set with accounts on the
// device.

// Marks the enterprise signals disclaimer as acknowledged for the account
// represented by `gaia_id`.
//
// `local_state` must not be null. `gaia_id` must not be empty.
void SetAccountAcknowledgedSignalsDisclaimer(PrefService* local_state,
                                             const GaiaId& gaia_id);

// Returns true if the enterprise signals disclaimer has been already
// acknowledged by the account represented by `gaia_id`.
//
// `local_state` must not be null. `gaia_id` must not be empty.
bool HasAccountAcknowledgedSignalsDisclaimer(const PrefService* local_state,
                                             const GaiaId& gaia_id);

void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

}  // namespace enterprise_signals_disclaimer

#endif  // CHROME_BROWSER_UI_ANDROID_ENTERPRISE_SIGNALS_DISCLAIMER_ACKNOWLEDGMENT_MANAGER_H_
