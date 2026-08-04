// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/autofill_settings_metrics.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/permissions/autofill_policy_service.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"

namespace autofill::autofill_metrics {

namespace {

// Returns the appropriate `AutofillPreferenceSetter` entry for `pref`.
AutofillPreferenceSetter GetAutofillPreferenceSetter(
    const PrefService::Preference* pref) {
  if (pref->IsUserControlled()) {
    return AutofillPreferenceSetter::kUserSetting;
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

std::optional<AutofillPreferenceSetter> GetDisabledReason(
    const PrefService& prefs,
    std::string_view user_pref_name) {
  const PrefService::Preference* user_pref =
      prefs.FindPreference(user_pref_name);
  if (user_pref && !prefs.GetBoolean(user_pref_name)) {
    return GetAutofillPreferenceSetter(user_pref);
  }
  return std::nullopt;
}

std::optional<AutofillPreferenceSetter> GetDisabledReasonAtStartup(
    const PrefService& prefs,
    std::string_view user_pref_name,
    AutofillClient::AutofillPolicyDataCategory category) {
  if (std::optional<AutofillPreferenceSetter> pref_reason =
          GetDisabledReason(prefs, user_pref_name)) {
    return pref_reason;
  }

  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableAutofillSettingsEnterprisePolicy) &&
      AutofillPolicyService::IsAutofillTypeBlockedByPolicyFromPref(
          prefs, GURL(), category)) {
    if (const PrefService::Preference* pref =
            prefs.FindPreference(prefs::kAutofillTypesBlocked)) {
      return GetAutofillPreferenceSetter(pref);
    }
  }
  return std::nullopt;
}

std::optional<AutofillPreferenceSetter> GetDisabledReasonAtPageLoad(
    const AutofillClient& client,
    std::string_view user_pref_name,
    AutofillClient::AutofillPolicyDataCategory category) {
  const PrefService* prefs = client.GetPrefs();
  if (!prefs) {
    return std::nullopt;
  }

  if (std::optional<AutofillPreferenceSetter> pref_reason =
          GetDisabledReason(*prefs, user_pref_name)) {
    return pref_reason;
  }

  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableAutofillSettingsEnterprisePolicy) &&
      client.IsAutofillTypeBlockedByPolicy(
          client.GetLastCommittedPrimaryMainFrameURL(), category)) {
    if (const PrefService::Preference* pref =
            prefs->FindPreference(prefs::kAutofillTypesBlocked)) {
      return GetAutofillPreferenceSetter(pref);
    }
  }
  return std::nullopt;
}

}  // namespace

void LogIsAutofillEnabledAtStartup(bool enabled) {
  UMA_HISTOGRAM_BOOLEAN("Autofill.IsEnabled.Startup", enabled);
}

void LogAutofillAiSettingsAtStartup(const PrefService& prefs) {
  if (std::optional<AutofillPreferenceSetter> reason =
          GetDisabledReasonAtStartup(
              prefs, prefs::kAutofillAiIdentityEntitiesEnabled,
              AutofillClient::AutofillPolicyDataCategory::kIdentityDocs)) {
    base::UmaHistogramEnumeration(
        "Autofill.IdentityDocs.DisabledReason.Startup", *reason);
  }

  if (std::optional<AutofillPreferenceSetter> reason =
          GetDisabledReasonAtStartup(
              prefs, prefs::kAutofillAiTravelEntitiesEnabled,
              AutofillClient::AutofillPolicyDataCategory::kTravel)) {
    base::UmaHistogramEnumeration("Autofill.Travel.DisabledReason.Startup",
                                  *reason);
  }

  if (std::optional<AutofillPreferenceSetter> reason =
          GetDisabledReasonAtStartup(
              prefs, prefs::kAutofillAiShoppingEntitiesEnabled,
              AutofillClient::AutofillPolicyDataCategory::kShopping)) {
    base::UmaHistogramEnumeration("Autofill.Shopping.DisabledReason.Startup",
                                  *reason);
  }
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
  std::optional<AutofillPreferenceSetter> reason = GetDisabledReasonAtStartup(
      pref_service, prefs::kAutofillProfileEnabled,
      AutofillClient::AutofillPolicyDataCategory::kContactInfo);
  if (reason.has_value()) {
    base::UmaHistogramEnumeration("Autofill.Address.DisabledReason.Startup",
                                  *reason);
  }
}

void LogAutofillProfileDisabledReasonAtPageLoad(const AutofillClient& client) {
  std::optional<AutofillPreferenceSetter> reason = GetDisabledReasonAtPageLoad(
      client, prefs::kAutofillProfileEnabled,
      AutofillClient::AutofillPolicyDataCategory::kContactInfo);
  if (reason.has_value()) {
    base::UmaHistogramEnumeration("Autofill.Address.DisabledReason.PageLoad",
                                  *reason);
  }
}

void LogAutofillPaymentMethodsDisabledReasonAtStartup(
    const PrefService& pref_service) {
  std::optional<AutofillPreferenceSetter> reason = GetDisabledReasonAtStartup(
      pref_service, prefs::kAutofillCreditCardEnabled,
      AutofillClient::AutofillPolicyDataCategory::kPayments);
  if (reason.has_value()) {
    base::UmaHistogramEnumeration("Autofill.CreditCard.DisabledReason.Startup",
                                  *reason);
  }
}

void LogAutofillPaymentMethodsDisabledReasonAtPageLoad(
    const AutofillClient& client) {
  std::optional<AutofillPreferenceSetter> reason = GetDisabledReasonAtPageLoad(
      client, prefs::kAutofillCreditCardEnabled,
      AutofillClient::AutofillPolicyDataCategory::kPayments);
  if (reason.has_value()) {
    base::UmaHistogramEnumeration("Autofill.CreditCard.DisabledReason.PageLoad",
                                  *reason);
  }
}

void LogAutofillAiSettingsAtPageLoad(const AutofillClient& client) {
  if (std::optional<AutofillPreferenceSetter> reason =
          GetDisabledReasonAtPageLoad(
              client, prefs::kAutofillAiIdentityEntitiesEnabled,
              AutofillClient::AutofillPolicyDataCategory::kIdentityDocs)) {
    base::UmaHistogramEnumeration(
        "Autofill.IdentityDocs.DisabledReason.PageLoad", *reason);
  }

  if (std::optional<AutofillPreferenceSetter> reason =
          GetDisabledReasonAtPageLoad(
              client, prefs::kAutofillAiTravelEntitiesEnabled,
              AutofillClient::AutofillPolicyDataCategory::kTravel)) {
    base::UmaHistogramEnumeration("Autofill.Travel.DisabledReason.PageLoad",
                                  *reason);
  }

  if (std::optional<AutofillPreferenceSetter> reason =
          GetDisabledReasonAtPageLoad(
              client, prefs::kAutofillAiShoppingEntitiesEnabled,
              AutofillClient::AutofillPolicyDataCategory::kShopping)) {
    base::UmaHistogramEnumeration("Autofill.Shopping.DisabledReason.PageLoad",
                                  *reason);
  }
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

void LogAutofillPaymentsSyncDisabled(SyncDisabledReason reason) {
  base::UmaHistogramEnumeration("Autofill.CreditCard.SyncDisabledReason",
                                reason);
}

}  // namespace autofill::autofill_metrics
