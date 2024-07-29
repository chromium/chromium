// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/autofill_settings_metrics.h"

#include "base/check_deref.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"

namespace autofill::autofill_metrics {

namespace {

// Returns the appropriate `AutofillPreferenceSetter` entry for `pref`.
AutofillPreferenceSetter GetAutofillPreferenceSetter(
    const PrefService::Preference* pref) {
  if (pref->IsUserControlled()) {
    return AutofillPreferenceSetter::kUserSetting;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  } else if (pref->IsStandaloneBrowserControlled()) {
    return AutofillPreferenceSetter::kStandaloneBrowser;
#endif
  } else if (pref->IsExtensionControlled()) {
    return AutofillPreferenceSetter::kExtension;
  } else if (pref->IsManagedByCustodian()) {
    return AutofillPreferenceSetter::kCustodian;
  } else if (pref->IsManaged() || pref->IsRecommended()) {
    return AutofillPreferenceSetter::kAdminPolicy;
  } else {
    return AutofillPreferenceSetter::kUnknown;
  }
}

// Emits UMA metric `Autofill.Address.DisabledReason.{PageLoad, Startup}`.
void LogAutofillProfileDisabledReason(const PrefService& pref_service,
                                      std::string_view suffix) {
  CHECK(!prefs::IsAutofillProfileEnabled(&pref_service));
  if (const PrefService::Preference* pref =
          pref_service.FindPreference(prefs::kAutofillProfileEnabled)) {
    base::UmaHistogramEnumeration(
        base::StrCat({"Autofill.Address.DisabledReason.", suffix}),
        GetAutofillPreferenceSetter(pref));
  }
}

// Emits UMA metric `Autofill.CreditCard.DisabledReason.{PageLoad, Startup}`.
void LogAutofillPaymentMethodsDisabledReason(const PrefService& pref_service,
                                             std::string_view suffix) {
  CHECK(!prefs::IsAutofillPaymentMethodsEnabled(&pref_service));
  if (const PrefService::Preference* pref =
          pref_service.FindPreference(prefs::kAutofillCreditCardEnabled)) {
    base::UmaHistogramEnumeration(
        base::StrCat({"Autofill.CreditCard.DisabledReason.", suffix}),
        GetAutofillPreferenceSetter(pref));
  }
}

}  // namespace

void LogIsAutofillEnabledAtStartup(bool enabled) {
  UMA_HISTOGRAM_BOOLEAN("Autofill.IsEnabled.Startup", enabled);
}

void LogIsAutofillProfileEnabledAtStartup(bool enabled) {
  UMA_HISTOGRAM_BOOLEAN("Autofill.Address.IsEnabled.Startup", enabled);
}

void LogIsAutofillPaymentMethodsEnabledAtStartup(bool enabled) {
  UMA_HISTOGRAM_BOOLEAN("Autofill.CreditCard.IsEnabled.Startup", enabled);
}

void LogIsAutofillEnabledAtPageLoad(
    bool enabled,
    AutofillMetrics::PaymentsSigninState sync_state) {
  std::string name = "Autofill.IsEnabled.PageLoad";
  UMA_HISTOGRAM_BOOLEAN(name, enabled);
  base::UmaHistogramBoolean(
      base::StrCat(
          {name, AutofillMetrics::GetMetricsSyncStateSuffix(sync_state)}),
      enabled);
}

void LogIsAutofillProfileEnabledAtPageLoad(
    bool enabled,
    AutofillMetrics::PaymentsSigninState sync_state) {
  std::string name = "Autofill.Address.IsEnabled.PageLoad";
  UMA_HISTOGRAM_BOOLEAN(name, enabled);
  base::UmaHistogramBoolean(
      base::StrCat(
          {name, AutofillMetrics::GetMetricsSyncStateSuffix(sync_state)}),
      enabled);
}

void LogIsAutofillPaymentMethodsEnabledAtPageLoad(
    bool enabled,
    AutofillMetrics::PaymentsSigninState sync_state) {
  std::string name = "Autofill.CreditCard.IsEnabled.PageLoad";
  UMA_HISTOGRAM_BOOLEAN(name, enabled);
  base::UmaHistogramBoolean(
      base::StrCat(
          {name, AutofillMetrics::GetMetricsSyncStateSuffix(sync_state)}),
      enabled);
}

void LogAutofillProfileDisabledReasonAtStartup(
    const PrefService& pref_service) {
  LogAutofillProfileDisabledReason(pref_service, "Startup");
}

void LogAutofillProfileDisabledReasonAtPageLoad(
    const PrefService& pref_service) {
  LogAutofillProfileDisabledReason(pref_service, "PageLoad");
}

void LogAutofillPaymentMethodsDisabledReasonAtStartup(
    const PrefService& pref_service) {
  LogAutofillPaymentMethodsDisabledReason(pref_service, "Startup");
}

void LogAutofillPaymentMethodsDisabledReasonAtPageLoad(
    const PrefService& pref_service) {
  LogAutofillPaymentMethodsDisabledReason(pref_service, "PageLoad");
}

void MaybeLogAutofillProfileDisabled(const PrefService& pref_service) {
  if (prefs::IsAutofillProfileEnabled(&pref_service)) {
    return;
  }
  const PrefService::Preference& pref =
      CHECK_DEREF(pref_service.FindPreference(prefs::kAutofillProfileEnabled));
  if (!pref.IsUserControlled() && !pref.IsExtensionControlled()) {
    return;
  }
  base::RecordAction(base::UserMetricsAction("Autofill_ProfileDisabled"));
}

}  // namespace autofill::autofill_metrics
