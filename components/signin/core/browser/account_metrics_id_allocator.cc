// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_metrics_id_allocator.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_prefs.h"

namespace signin {

namespace {
constexpr int kMaxMetricsAccountId = 99;
constexpr int kHistogramExclusiveMax = kMaxMetricsAccountId + 1;
}  // namespace

std::optional<int> GetOrAllocateAccountMetricsId(SigninPrefs& signin_prefs,
                                                 const GaiaId& gaia_id) {
  std::optional<int> assigned_id = signin_prefs.GetAccountMetricsId(gaia_id);

  if (assigned_id.has_value()) {
    CHECK_LE(assigned_id.value(), kMaxMetricsAccountId);
    return assigned_id.value();
  }

  if (signin_prefs.IsAccountMetricsIdCapped(gaia_id)) {
    return std::nullopt;
  }

  int next_id = signin_prefs.GetNextAccountMetricsUnassignedId();

  if (next_id > kMaxMetricsAccountId) {
    signin_prefs.SetAccountMetricsIdCapped(gaia_id);

    // Still log that a new account was added. Since next_id is 100 and the
    // exclusive max is 100, this sample will fall into the overflow bucket.
    base::UmaHistogramExactLinear("Signin.AccountInPref.AssignedId", next_id,
                                  kHistogramExclusiveMax);
    return std::nullopt;
  }

  signin_prefs.SetNextAccountMetricsUnassignedId(next_id + 1);
  signin_prefs.SetAccountMetricsId(gaia_id, next_id);

  base::UmaHistogramExactLinear("Signin.AccountInPref.AssignedId", next_id,
                                kHistogramExclusiveMax);

  return next_id;
}

}  // namespace signin
