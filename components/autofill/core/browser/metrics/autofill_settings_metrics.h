// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_AUTOFILL_SETTINGS_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_AUTOFILL_SETTINGS_METRICS_H_

#include "components/autofill/core/browser/metrics/autofill_metrics.h"

namespace autofill::autofill_metrics {

// Sources that set Autofill preferences.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AutofillPreferenceSetter {
  // The pref was set from an unknown source, e.g. via the command line.
  kUnknown = 0,
  // The pref was set by the user.
  kUserSetting = 1,
  // ChromeOS only. The pref is set by a standalone browser (lacros).
  kStandaloneBrowser = 2,
  // The pref was set by an extension.
  kExtension = 3,
  // The pref was set by the custodian of the (supervised) user.
  kCustodian = 4,
  // The pref was set by an admin policy.
  kAdminPolicy = 5,
  kMaxValue = kAdminPolicy
};

// This should be called each time a page containing forms is loaded.
void LogIsAutofillEnabledAtPageLoad(
    bool enabled,
    AutofillMetrics::PaymentsSigninState sync_state);

// This should be called each time a page containing forms is loaded.
void LogIsAutofillProfileEnabledAtPageLoad(
    bool enabled,
    AutofillMetrics::PaymentsSigninState sync_state);

// This should be called each time a page containing forms is loaded.
void LogIsAutofillPaymentMethodsEnabledAtPageLoad(
    bool enabled,
    AutofillMetrics::PaymentsSigninState sync_state);

// This should be called each time a new chrome profile is launched.
void LogIsAutofillEnabledAtStartup(bool enabled);

// This should be called each time a new chrome profile is launched.
void LogIsAutofillProfileEnabledAtStartup(bool enabled);

// This should be called each time a new chrome profile is launched.
void LogIsAutofillPaymentMethodsEnabledAtStartup(bool enabled);

// Logs the source that disabled Autofill Profile, on startup. This should be
// called each time a new chrome profile is launched.
void LogAutofillProfileDisabledReasonAtStartup(const PrefService& pref_service);

// Logs the source that disabled Autofill Profile, on page load for a page
// containing forms.
void LogAutofillProfileDisabledReasonAtPageLoad(
    const PrefService& pref_service);

// Logs the source that disabled payment method Autofill, on startup. This
// should be called each time a new chrome profile is launched.
void LogAutofillPaymentMethodsDisabledReasonAtStartup(
    const PrefService& pref_service);

// Logs the source that disabled payment method Autofill, on page load for a
// page containing forms.
void LogAutofillPaymentMethodsDisabledReasonAtPageLoad(
    const PrefService& pref_service);

// Logs user action "Autofill_ProfileDisabled" if
// `prefs::kAutofillProfileEnabled` is disabled and controlled by the user or an
// extension.
void MaybeLogAutofillProfileDisabled(const PrefService& pref_service);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_AUTOFILL_SETTINGS_METRICS_H_
