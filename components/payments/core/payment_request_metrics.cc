// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_request_metrics.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/payments/core/payment_prefs.h"
#include "components/prefs/pref_service.h"

namespace payments {

namespace {
// Returns the appropriate `CanMakePaymentPreferenceSetter` entry for `pref`.
CanMakePaymentPreferenceSetter GetCanMakePaymentPreferenceSetter(
    const PrefService::Preference* pref) {
  if (pref->IsUserControlled()) {
    return CanMakePaymentPreferenceSetter::kUserSetting;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  } else if (pref->IsStandaloneBrowserControlled()) {
    return CanMakePaymentPreferenceSetter::kStandaloneBrowser;
#endif
  } else if (pref->IsExtensionControlled()) {
    return CanMakePaymentPreferenceSetter::kExtension;
  } else if (pref->IsManagedByCustodian()) {
    return CanMakePaymentPreferenceSetter::kCustodian;
  } else if (pref->IsManaged() || pref->IsRecommended()) {
    return CanMakePaymentPreferenceSetter::kAdminPolicy;
  } else {
    return CanMakePaymentPreferenceSetter::kUnknown;
  }
}

}  // namespace

void RecordCanMakePaymentPrefMetrics(const PrefService& pref_service,
                                     const std::string& suffix) {
  const PrefService::Preference* pref =
      pref_service.FindPreference(payments::kCanMakePaymentEnabled);
  // The kCanMakePaymentEnabled pref should already be registered, so `pref`
  // must be non-null.
  CHECK(pref);

  const bool can_make_payment_enabled = pref->GetValue()->GetBool();
  base::UmaHistogramBoolean(
      base::StrCat({"PaymentRequest.IsCanMakePaymentAllowedByPref.", suffix}),
      can_make_payment_enabled);

  if (!can_make_payment_enabled) {
    base::UmaHistogramEnumeration(
        base::StrCat({"PaymentRequest.IsCanMakePaymentAllowedByPref.", suffix,
                      ".DisabledReason"}),
        GetCanMakePaymentPreferenceSetter(pref));
  }
}

}  // namespace payments
