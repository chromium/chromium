// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/autofill_settings_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"

namespace autofill::autofill_metrics {

void LogIsAutofillEnabledAtStartup(bool enabled) {
  UMA_HISTOGRAM_BOOLEAN("Autofill.IsEnabled.Startup", enabled);
}

void LogIsAutofillProfileEnabledAtStartup(bool enabled) {
  UMA_HISTOGRAM_BOOLEAN("Autofill.Address.IsEnabled.Startup", enabled);
}

void LogIsAutofillCreditCardEnabledAtStartup(bool enabled) {
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

void LogIsAutofillCreditCardEnabledAtPageLoad(
    bool enabled,
    AutofillMetrics::PaymentsSigninState sync_state) {
  std::string name = "Autofill.CreditCard.IsEnabled.PageLoad";
  UMA_HISTOGRAM_BOOLEAN(name, enabled);
  base::UmaHistogramBoolean(
      base::StrCat(
          {name, AutofillMetrics::GetMetricsSyncStateSuffix(sync_state)}),
      enabled);
}

}  // namespace autofill::autofill_metrics
