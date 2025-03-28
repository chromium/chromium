// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ACTIVE_PRIMARY_ACCOUNTS_METRICS_RECORDER_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ACTIVE_PRIMARY_ACCOUNTS_METRICS_RECORDER_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "components/signin/public/base/persistent_repeating_timer.h"
#include "components/signin/public/identity_manager/tribool.h"

class GaiaId;
class PrefService;
class PrefRegistrySimple;

namespace signin {

// Tracks the set of accounts (by Gaia ID) that have been active in the last 7
// and 28 days, and emits the counts to histograms once per day. An account is
// considered "active" if it is the primary account in any profile.
class ActivePrimaryAccountsMetricsRecorder {
 public:
  explicit ActivePrimaryAccountsMetricsRecorder(PrefService& local_state);
  ~ActivePrimaryAccountsMetricsRecorder();

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Should be called when an account becomes the primary account in any
  // profile, and then with some frequency while the account is being used. (The
  // exact points in time or frequency with which this is called don't matter
  // much, as long as there's at least one update every 24 hours or so.)
  void MarkAccountAsActiveNow(const GaiaId& gaia_id,
                              Tribool is_managed_account);

  // Should be called when an account's managed-ness is determined, if it wasn't
  // known yet at the time MarkAccountAsActiveNow() was called. (Duplicate calls
  // are fine, and calls for not-recently-active accounts are no-ops.)
  void MarkAccountAsManaged(const GaiaId& gaia_id, bool is_managed_account);

#if BUILDFLAG(IS_IOS)
  // Should be called when the user explicitly switches to a different account.
  // Used to track the number of account switches per client.
  void AccountWasSwitched();
#endif  // BUILDFLAG(IS_IOS)

  // Returns the last know active-time for the given account. If the account has
  // never been marked as active, or it was too long ago so that the entry has
  // expired, returns nullopt.
  std::optional<base::Time> GetLastActiveTimeForAccount(
      const GaiaId& gaia_id) const;

 private:
  void OnTimerFired();

  void EmitMetrics();

  void CleanUpExpiredEntries();

  const raw_ref<PrefService> local_state_;

  std::unique_ptr<signin::PersistentRepeatingTimer> timer_;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ACTIVE_PRIMARY_ACCOUNTS_METRICS_RECORDER_H_
