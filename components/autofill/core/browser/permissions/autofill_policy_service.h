// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PERMISSIONS_AUTOFILL_POLICY_SERVICE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PERMISSIONS_AUTOFILL_POLICY_SERVICE_H_

#include <vector>

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

class GURL;
class PrefService;

namespace autofill {

// Evaluates whether Autofill is allowed for a given data category and URL,
// combining both user settings and enterprise policies.
class AutofillPolicyService : public KeyedService {
 public:
  explicit AutofillPolicyService(PrefService* prefs);
  AutofillPolicyService(const AutofillPolicyService&) = delete;
  AutofillPolicyService& operator=(const AutofillPolicyService&) = delete;
  ~AutofillPolicyService() override;

  // Returns true if the specified Autofill data category is blocked by either
  // user settings or enterprise policy for the given `url`. This static method
  // is used where an `AutofillPolicyService` instance or `AutofillClient` is
  // not accessible (e.g., in data managers or at startup), parsing the policy
  // list directly from `prefs` without caching.
  [[nodiscard]] static bool IsAutofillTypeBlockedByPolicyFromPref(
      const PrefService& prefs,
      const GURL& url,
      AutofillClient::AutofillPolicyDataCategory category);

  // Returns true if the specified Autofill data category is disabled
  // specifically by enterprise policy (`kAutofillTypesBlocked`) for the given
  // `url`. This does not check user setting preferences.
  [[nodiscard]] static bool IsAutofillTypeDisabledByEnterprisePolicy(
      const PrefService& prefs,
      const GURL& url,
      AutofillClient::AutofillPolicyDataCategory category);

  // Evaluates the policy using the cached patterns in this service.
  [[nodiscard]] bool IsAutofillTypeBlockedByPolicy(
      const GURL& url,
      AutofillClient::AutofillPolicyDataCategory category) const;

 private:
  void OnAutofillPolicyChanged();

  const base::raw_ref<const PrefService> prefs_;
  PrefChangeRegistrar autofill_types_blocked_change_registrar_;

  struct BlockedPatternEntry {
    ContentSettingsPattern pattern;
    std::vector<AutofillClient::AutofillPolicyDataCategory> blocked_categories;
  };
  std::vector<BlockedPatternEntry> blocked_patterns_cache_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PERMISSIONS_AUTOFILL_POLICY_SERVICE_H_
