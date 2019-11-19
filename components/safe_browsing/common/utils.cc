// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/common/utils.h"

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/prefs/pref_service.h"
#include "crypto/sha2.h"

#if defined(OS_WIN)
#include "base/enterprise_util.h"
#endif

namespace safe_browsing {

std::string ShortURLForReporting(const GURL& url) {
  std::string spec(url.spec());
  if (url.SchemeIs(url::kDataScheme)) {
    size_t comma_pos = spec.find(',');
    if (comma_pos != std::string::npos && comma_pos != spec.size() - 1) {
      std::string hash_value = crypto::SHA256HashString(spec);
      spec.erase(comma_pos + 1);
      spec += base::HexEncode(hash_value.data(), hash_value.size());
    }
  }
  return spec;
}

ChromeUserPopulation::ProfileManagementStatus GetProfileManagementStatus(
    const policy::BrowserPolicyConnector* bpc) {
#if defined(OS_WIN)
  if (base::IsMachineExternallyManaged())
    return ChromeUserPopulation::ENTERPRISE_MANAGED;
  else
    return ChromeUserPopulation::NOT_MANAGED;
#elif defined(OS_CHROMEOS)
  if (!bpc || !bpc->IsEnterpriseManaged())
    return ChromeUserPopulation::NOT_MANAGED;
  return ChromeUserPopulation::ENTERPRISE_MANAGED;
#else
  return ChromeUserPopulation::UNAVAILABLE;
#endif  // #if defined(OS_WIN) || defined(OS_CHROMEOS)
}

void SetDelayInPref(PrefService* prefs,
                    const char* pref_name,
                    const base::TimeDelta& delay) {
  base::Time next_event = base::Time::Now() + delay;
  int64_t seconds_since_epoch =
      next_event.ToDeltaSinceWindowsEpoch().InSeconds();
  prefs->SetInt64(pref_name, seconds_since_epoch);
}

base::TimeDelta GetDelayFromPref(PrefService* prefs, const char* pref_name) {
  const base::TimeDelta zero_delay;
  if (!prefs->HasPrefPath(pref_name))
    return zero_delay;

  int64_t seconds_since_epoch = prefs->GetInt64(pref_name);
  if (seconds_since_epoch <= 0)
    return zero_delay;

  base::Time next_event = base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromSeconds(seconds_since_epoch));
  base::Time now = base::Time::Now();
  if (now > next_event)
    return zero_delay;
  else
    return next_event - now;
}

}  // namespace safe_browsing
