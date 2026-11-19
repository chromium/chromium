// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BROWSER_ACCOUNT_SWITCHING_PROMO_LOGGER_H_
#define COMPONENTS_SIGNIN_PUBLIC_BROWSER_ACCOUNT_SWITCHING_PROMO_LOGGER_H_

#include "base/memory/raw_ref.h"
#include "google_apis/gaia/gaia_id.h"

class PrefService;

namespace feature_engagement {
class Tracker;
}  // namespace feature_engagement

namespace signin {

// Records the presentation and the outcome (accepted, explicitly dismissed, or
// ignored) of the account switching promo in the
// `Signin.AccountSwitchingPromo.Events` histogram, the feature engagement
// tracker, and the account switching promo prefs. If `LogPromoTriggered()` is
// called, exactly one outcome is recorded by the time this logger is destroyed.
class AccountSwitchingPromoLogger {
 public:
  // `tracker` and `pref_service` must outlive this logger. `pref_service` must
  // be a profile `PrefService` (on multi-profile platforms where personal
  // accounts share a single personal profile, such as iOS, it must be the
  // personal profile's `PrefService`). `primary_gaia_id` is the primary
  // account when the promo is presented, and `target_gaia_id` is the account
  // offered by the promo. They must be non-empty and distinct.
  AccountSwitchingPromoLogger(feature_engagement::Tracker& tracker,
                              PrefService& pref_service,
                              const GaiaId& primary_gaia_id,
                              const GaiaId& target_gaia_id);

  AccountSwitchingPromoLogger(const AccountSwitchingPromoLogger&) = delete;
  AccountSwitchingPromoLogger& operator=(const AccountSwitchingPromoLogger&) =
      delete;

  // Records that the promo was dismissed without an explicit user decision if
  // `LogPromoTriggered()` was called and no outcome was recorded yet.
  ~AccountSwitchingPromoLogger();

  // Records that presentation of the promo was triggered. Must be called at
  // most once, before any outcome is recorded. If it is never called, nothing
  // is recorded.
  void LogPromoTriggered();

  // Records that the user switched to the target account. Must be called after
  // `LogPromoTriggered()` and at most once across all outcome methods.
  void LogAccepted();

  // Records that the user explicitly dismissed the promo. Must be called after
  // `LogPromoTriggered()` and at most once across all outcome methods.
  void LogExplicitlyDismissed();

 private:
  const raw_ref<feature_engagement::Tracker> tracker_;
  const raw_ref<PrefService> pref_service_;
  const GaiaId primary_gaia_id_;
  const GaiaId target_gaia_id_;
  bool promo_triggered_ = false;
  bool outcome_recorded_ = false;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_BROWSER_ACCOUNT_SWITCHING_PROMO_LOGGER_H_
