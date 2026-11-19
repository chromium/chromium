// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/browser/account_switching_promo_logger.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/browser/account_switching_promo_metrics.h"
#include "components/signin/public/browser/account_switching_promo_prefs.h"
#include "google_apis/gaia/gaia_id.h"

namespace signin {

AccountSwitchingPromoLogger::AccountSwitchingPromoLogger(
    feature_engagement::Tracker& tracker,
    PrefService& pref_service,
    const GaiaId& primary_gaia_id,
    const GaiaId& target_gaia_id)
    : tracker_(tracker),
      pref_service_(pref_service),
      primary_gaia_id_(primary_gaia_id),
      target_gaia_id_(target_gaia_id) {
  CHECK(!primary_gaia_id_.empty(), base::NotFatalUntil::M170);
  CHECK(!target_gaia_id_.empty(), base::NotFatalUntil::M170);
  CHECK_NE(primary_gaia_id_, target_gaia_id_, base::NotFatalUntil::M170);
}

AccountSwitchingPromoLogger::~AccountSwitchingPromoLogger() {
  if (promo_triggered_ && !outcome_recorded_) {
    outcome_recorded_ = true;
    base::UmaHistogramEnumeration(kAccountSwitchingPromoEventsHistogram,
                                  AccountSwitchingPromoEvent::kIgnored);
    tracker_->NotifyEvent(
        feature_engagement::events::kAccountSwitchingPromoIgnored);
  }
}

void AccountSwitchingPromoLogger::LogPromoTriggered() {
  CHECK(!promo_triggered_, base::NotFatalUntil::M170);
  CHECK(!outcome_recorded_, base::NotFatalUntil::M170);
  if (promo_triggered_ || outcome_recorded_) {
    return;
  }
  promo_triggered_ = true;
  base::UmaHistogramEnumeration(kAccountSwitchingPromoEventsHistogram,
                                AccountSwitchingPromoEvent::kPromoTriggered);
}

void AccountSwitchingPromoLogger::LogAccepted() {
  CHECK(promo_triggered_, base::NotFatalUntil::M170);
  CHECK(!outcome_recorded_, base::NotFatalUntil::M170);
  if (!promo_triggered_ || outcome_recorded_) {
    return;
  }
  outcome_recorded_ = true;
  base::UmaHistogramEnumeration(kAccountSwitchingPromoEventsHistogram,
                                AccountSwitchingPromoEvent::kAccepted);
  RecordAccountSwitchingPromoAccepted(*pref_service_, target_gaia_id_);
}

void AccountSwitchingPromoLogger::LogExplicitlyDismissed() {
  CHECK(promo_triggered_, base::NotFatalUntil::M170);
  CHECK(!outcome_recorded_, base::NotFatalUntil::M170);
  if (!promo_triggered_ || outcome_recorded_) {
    return;
  }
  outcome_recorded_ = true;
  base::UmaHistogramEnumeration(
      kAccountSwitchingPromoEventsHistogram,
      AccountSwitchingPromoEvent::kExplicitlyDismissed);
  tracker_->NotifyEvent(
      feature_engagement::events::kAccountSwitchingPromoExplicitlyDismissed);
  RecordAccountSwitchingPromoExplicitlyDismissed(
      *pref_service_, primary_gaia_id_, target_gaia_id_);
}

}  // namespace signin
